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

#include "llvm-c/Initialization.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/InitializePasses.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/DataTypes.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Instrumentation.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"

#include <algorithm>
#include <map>
#include <string>

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

        void getAnalysisUsage(AnalysisUsage &AU) const override;

        static char ID; // Pass identification, replacement for typeid

    private:
        Function *checkInterfaceFunction(Constant *FuncOrBitcast);

        DataLayout *TD;
        Function *accessCallback, *modifiedMalloc;
        Type *intptrType, *voidptrType, *int64Type, *boolType;
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
    voidptrType = Type::getInt8PtrTy(context);
    int64Type = Type::getInt64Ty(context);
    boolType = Type::getInt8Ty(context);

    // We insert an empty inline asm after __asan_report* to avoid callback
    // merge.
    noopAsm = InlineAsm::get(FunctionType::get(Type::getVoidTy(context), false),
                             StringRef(""), StringRef(""), /*hasSideEffects=*/true);

    accessCallback = checkInterfaceFunction(M.getOrInsertFunction(
            "handle_access", Type::getVoidTy(context),
            intptrType, int64Type, int64Type, int64Type, boolType
    ));

    modifiedMalloc = checkInterfaceFunction(M.getOrInsertFunction(
            "malloc_inst", voidptrType, int64Type, int64Type, int64Type
    ));
    return true;
}


void Instrumenter::getAnalysisUsage(AnalysisUsage &AU) const {
    AU.addRequired<LoopInfoWrapperPass>();
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

void printLoop(Loop *L) {
    errs() << "Loop: \n";
    L->dump();
    auto *phinode = L->getCanonicalInductionVariable();
    if (phinode) {
        errs() << "CIV of this loop: ";
        phinode->print(errs());
        errs() << "\n";
    }
    for (BasicBlock *BB : L->getBlocks()) {
        errs() << "basicb name: " << BB->getName() << "\n";
    }
    for (Loop *SL : L->getSubLoops()) {
        printLoop(SL);
    }
}

bool Instrumenter::runOnModule(Module &M) {
    std::map<int, StringRef> funcNames;
    size_t funcCounter = (size_t)startFrom, numInsted = 0;
    for (Module::iterator fb = M.begin(), fe = M.end(); fb != fe;
         ++fb, ++funcCounter) {
        if (&*fb == accessCallback || &*fb == modifiedMalloc)
            continue;
        funcNames.insert(std::make_pair(funcCounter, fb->getName()));
        // Fill the set of memory operations to instrument.
        unsigned long instCounter = 0;
        std::vector<std::pair<Instruction *, size_t>> mallocLocs;
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
                    Function *callee = ci->getCalledFunction();
                    if (!callee)
                        continue;
                    StringRef name = callee->getName();
                    if (name == "malloc")
                        mallocLocs.emplace_back(Inst, instCounter);
                }
            }
        }
        for (unsigned i = 0; i < mallocLocs.size(); i++) {
            Instruction *Inst;
            size_t instIndex;
            std::tie(Inst, instIndex) = mallocLocs[i];
            CallInst *instToReplace = cast<CallInst>(Inst);
            Value *size_arg = instToReplace->getArgOperand(0);
            BasicBlock::iterator ii(instToReplace);
            std::vector<Value *> arguments;
            arguments.push_back(size_arg);
            arguments.push_back(ConstantInt::get(int64Type, funcCounter));
            arguments.push_back(ConstantInt::get(int64Type, instIndex));
            ReplaceInstWithInst(instToReplace->getParent()->getInstList(), ii,
                                CallInst::Create(modifiedMalloc, ArrayRef<Value *>(arguments)));
            numInsted++;
        }
    }
    for (const auto &p: funcNames)
        dbgs() << p.first << " " << p.second << "\n";
    return numInsted > 0;
}
