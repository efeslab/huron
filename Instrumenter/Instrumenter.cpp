//===-- Instrumenter.cpp - memory error detector ------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file is a part of Instrumenter, an address sanity checker.
// Details of the algorithm:
//  http://code.google.com/p/address-sanitizer/wiki/InstrumenterAlgorithm
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "instrumenter"

#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/Debug.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

#include <map>

using namespace llvm;

namespace {

/// Instrumenter: instrument the memory accesses.
    struct Instrumenter : public ModulePass {
        Instrumenter();

        StringRef getPassName() const override;

        void instrumentMemoryAccess(Instruction *ins, unsigned long funcId,
                                    unsigned long instCounter);

        void instrumentAddress(Instruction *origIns, IRBuilder<> &IRB, Value *addr,
                               uint32_t typeSize, bool isWrite,
                               unsigned long funcId, unsigned long instCounter);

        Instruction *insertAccessCallback(Instruction *insertBefore, Value *addr,
                                          bool isWrite, uint32_t typeBytes,
                                          unsigned long funcId,
                                          unsigned long instCounter);

        bool runOnModule(Module &M) override;

        bool doInitialization(Module &M) override;

        bool doFinalization(Module &M) override;

        static char ID; // Pass identification, replacement for typeid

    private:
        Instruction *getAllocsReplace(CallInst *ci, size_t fid, size_t iid);

        Function *checkInterfaceFunction(Constant *FuncOrBitcast);

        DataLayout *TD;
        Function *accessCallback;
        StringMap<Function*> modifiedAllocs;
        Type *intptrType, *int64Type, *boolType;
        InlineAsm *noopAsm;
    };

} // namespace

char Instrumenter::ID = 0;
static RegisterPass<Instrumenter> instrumenter(
        "instrumenter", "Instrumenting READ/WRITE pass", false, false
);
static cl::opt<unsigned> startFrom(
        "start-from", cl::init(0), cl::desc("Start function id from N")
);
static cl::opt<bool> toInstrumentReads(
        "instrument-reads", cl::desc("instrument read instructions"),
        cl::Hidden, cl::init(true)
);
static cl::opt<bool> toInstrumentWrites(
        "instrument-writes", cl::desc("instrument write instructions"),
        cl::Hidden, cl::init(true)
);
static cl::opt<bool> toInstrumentAtomics(
        "instrument-atomics", cl::desc("instrument atomic instructions (rmw, cmpxchg)"),
        cl::Hidden, cl::init(true)
);

Instrumenter::Instrumenter() : ModulePass(ID) {}

StringRef Instrumenter::getPassName() const { return "Instrumenter"; }

// virtual: define some initialization for the whole module
bool Instrumenter::doInitialization(Module &M) {
    dbgs() << "\n^^^^^^^^^^^^^^^^^^Instrumenter initialization^^^^^^^^^^^^^^^^^^^^^^^^^\n";
    TD = new DataLayout(&M);

    LLVMContext &context = M.getContext();
    uint32_t LongSize = TD->getPointerSizeInBits();
    intptrType = Type::getIntNTy(context, LongSize);
    int64Type = Type::getInt64Ty(context);
    boolType = Type::getInt8Ty(context);

    Type *voidPtrType = Type::getInt8PtrTy(context), *intType = Type::getInt32Ty(context), 
         *voidPtrPtrType = voidPtrType->getPointerTo();

    // We insert an empty inline asm after __asan_report* to avoid callback
    // merge.
    noopAsm = InlineAsm::get(FunctionType::get(Type::getVoidTy(context), false),
                             StringRef(""), StringRef(""), /*hasSideEffects=*/true);

    accessCallback = checkInterfaceFunction(M.getOrInsertFunction(
            "handle_access", Type::getVoidTy(context),
            intptrType, int64Type, int64Type, int64Type, boolType
    ));

    modifiedAllocs["malloc"] = checkInterfaceFunction(M.getOrInsertFunction(
            "malloc_inst", voidPtrType, int64Type, int64Type, int64Type
    ));

    modifiedAllocs["calloc"] = checkInterfaceFunction(M.getOrInsertFunction(
            "calloc_inst", voidPtrType, int64Type, int64Type, int64Type, int64Type
    ));

    modifiedAllocs["realloc"] = checkInterfaceFunction(M.getOrInsertFunction(
        "realloc_inst", voidPtrType, voidPtrType, int64Type, int64Type, int64Type
    ));

    modifiedAllocs["posix_memalign"] = checkInterfaceFunction(M.getOrInsertFunction(
        "posix_memalign_inst", intType, voidPtrPtrType, int64Type, int64Type, int64Type, int64Type
    ));
    return true;
}

// and set isWrite. Otherwise return nullptr.
static Value *isInterestingMemoryAccess(Instruction *ins, bool *isWrite) {
    if (LoadInst *LI = dyn_cast<LoadInst>(ins)) {
        if (!toInstrumentReads)
            return nullptr;
        *isWrite = false;
        return LI->getPointerOperand();
    }
    if (StoreInst *SI = dyn_cast<StoreInst>(ins)) {
        if (!toInstrumentWrites)
            return nullptr;
        *isWrite = true;
        return SI->getPointerOperand();
    }
    if (AtomicRMWInst *RMW = dyn_cast<AtomicRMWInst>(ins)) {
        if (!toInstrumentAtomics)
            return nullptr;
        *isWrite = true;
        return RMW->getPointerOperand();
    }
    if (AtomicCmpXchgInst *XCHG = dyn_cast<AtomicCmpXchgInst>(ins)) {
        if (!toInstrumentAtomics)
            return nullptr;
        *isWrite = true;
        return XCHG->getPointerOperand();
    }
    return nullptr;
}

void Instrumenter::instrumentMemoryAccess(Instruction *ins,
                                          unsigned long funcId,
                                          unsigned long instCounter) {
    bool isWrite = false;
    Value *addr = isInterestingMemoryAccess(ins, &isWrite);
    assert(addr);

    Type *OrigPtrTy = addr->getType();
    Type *OrigTy = cast<PointerType>(OrigPtrTy)->getElementType();

    assert(OrigTy->isSized());
    auto typeSizeBits = static_cast<uint32_t>(TD->getTypeStoreSizeInBits(OrigTy));

    IRBuilder<> IRB(ins);
    instrumentAddress(ins, IRB, addr, typeSizeBits >> 3, isWrite, funcId, instCounter);
}

// General function call before some given instruction
Instruction *Instrumenter::insertAccessCallback(Instruction *insertBefore,
                                                Value *addr, bool isWrite,
                                                uint32_t typeBytes,
                                                unsigned long funcId,
                                                unsigned long instCounter) {
    IRBuilder<> IRB(insertBefore);

    std::vector<Value *> arguments;
    arguments.push_back(addr);
    arguments.push_back(ConstantInt::get(int64Type, funcId));
    arguments.push_back(ConstantInt::get(int64Type, instCounter));
    arguments.push_back(ConstantInt::get(int64Type, typeBytes));
    arguments.push_back(ConstantInt::get(boolType, static_cast<uint64_t>(isWrite)));

    CallInst *Call = IRB.CreateCall(accessCallback, ArrayRef<Value *>(arguments));

    // We don't do Call->setDoesNotReturn() because the BB already has
    // UnreachableInst at the end.
    // This noopAsm is required to avoid callback merge.

    IRB.CreateCall(noopAsm);
    return Call;
}

void Instrumenter::instrumentAddress(Instruction *origIns, IRBuilder<> &IRB,
                                     Value *addr, uint32_t typeBytes,
                                     bool isWrite, unsigned long funcId,
                                     unsigned long instCounter) {

    Value *actualAddr = IRB.CreatePointerCast(addr, intptrType);

    dbgs() << "Generated function call: "
           << (isWrite ? "store" : "load")
           << " size = " << typeBytes
           << " funcId, instId = " << funcId << ", " << instCounter << "\n";

    // Insert the callback function here.
    insertAccessCallback(origIns, actualAddr, isWrite, typeBytes,
                         funcId, instCounter);
}

// Validate the result of Module::getOrInsertFunction called for an interface
// function of Instrumenter. If the instrumented module defines a function
// with the same name, their prototypes must match, otherwise
// getOrInsertFunction returns a bitcast.
Function *Instrumenter::checkInterfaceFunction(Constant *FuncOrBitcast) {
    if (isa<Function>(FuncOrBitcast))
        return cast<Function>(FuncOrBitcast);
    report_fatal_error("trying to redefine an Instrumenter "
                               "interface function");
}

bool Instrumenter::doFinalization(Module &M) {
    delete TD;
    return false;
}

bool isLocalVariable(Value *value) {
    return isa<AllocaInst>(value);
}

Instruction *Instrumenter::getAllocsReplace(CallInst *ci, size_t fid, size_t iid) {
    Function *callee = ci->getCalledFunction();
    if (!callee)
        return nullptr;
    StringRef name = callee->getName();
    if (name == "malloc") {
        SmallVector<Value *, 3> arguments {
            ci->getArgOperand(0), 
            ConstantInt::get(int64Type, fid), 
            ConstantInt::get(int64Type, iid)
        };
        return CallInst::Create(modifiedAllocs[name], ArrayRef<Value *>(arguments));
    } else if (name == "calloc" || name == "realloc") {
        SmallVector<Value *, 4> arguments {
            ci->getArgOperand(0), 
            ci->getArgOperand(1), 
            ConstantInt::get(int64Type, fid), 
            ConstantInt::get(int64Type, iid)
        };
        return CallInst::Create(modifiedAllocs[name], ArrayRef<Value *>(arguments));
    } else if (name == "posix_memalign") {
        SmallVector<Value *, 5> arguments {
            ci->getArgOperand(0),
            ci->getArgOperand(1),
            ci->getArgOperand(2),
            ConstantInt::get(int64Type, fid),
            ConstantInt::get(int64Type, iid)
        };
        return CallInst::Create(modifiedAllocs[name], ArrayRef<Value *>(arguments));
    } else return nullptr;
}

bool Instrumenter::runOnModule(Module &M) {
    std::map<int, StringRef> funcNames;
    size_t funcCounter = (size_t)startFrom, numInsted = 0;
    for (Module::iterator fb = M.begin(), fe = M.end(); fb != fe;
         ++fb, ++funcCounter) {
        if (fb->isDeclaration())
            continue;
        funcNames.insert(std::make_pair(funcCounter, fb->getName()));
        // Fill the set of memory operations to instrument.
        unsigned long instCounter = 0;
        std::vector<std::pair<Instruction *, Instruction *>> allocsReplace;
        for (Function::iterator bb = fb->begin(), FE = fb->end(); bb != FE; ++bb) {
            for (BasicBlock::iterator ins = bb->begin(), BE = bb->end(); ins != BE;
                 ++ins, ++instCounter) {
                Instruction *Inst = &*ins;
                bool isWrite;
                if (Value *addr = isInterestingMemoryAccess(Inst, &isWrite)) {
                    if (isLocalVariable(addr))
                        continue;
                    instrumentMemoryAccess(Inst, funcCounter, instCounter);
                    numInsted++;
                } else if (CallInst *ci = dyn_cast<CallInst>(Inst)) {
                    Instruction *rep = getAllocsReplace(ci, funcCounter, instCounter);
                    if (rep)
                        allocsReplace.emplace_back(&*ins, rep);
                    numInsted++;
                }
            }
        }
        for (const auto &p: allocsReplace)
            ReplaceInstWithInst(p.first, p.second);
    }
    for (const auto &p: funcNames)
        dbgs() << p.first << " " << p.second << "\n";
    return numInsted > 0;
}
