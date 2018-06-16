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

// We only support 5 sizes(powers of two): 1, 2, 4, 8, 16.
static const size_t numAccessesSizes = 5;

namespace {

/// Instrumenter: instrument the memory accesses.
struct Instrumenter : public FunctionPass {
    Instrumenter();
    virtual StringRef getPassName() const;
    void instrumentMemoryAccess(Instruction *ins, unsigned long funcId,
                                unsigned long instCounter);
    void instrumentAddress(Instruction *origIns, IRBuilder<> &IRB, Value *addr,
                           uint32_t typeSize, bool isWrite,
                           unsigned long funcId, unsigned long instCounter);
    bool instrumentMemIntrinsic(MemIntrinsic *mInst);
    void instrumentMemIntrinsicParam(Instruction *origIns, Value *addr,
                                     Value *size, Instruction *insertBefore,
                                     bool isWrite);
    Instruction *insertAccessCallback(Instruction *insertBefore, Value *addr,
                                      bool isWrite, size_t accessSizeArrayIndex,
                                      unsigned long funcId,
                                      unsigned long instCounter);

    bool runOnFunction(Function &F);
    virtual bool doInitialization(Module &M);
    virtual bool doFinalization(Module &M);
    virtual void getAnalysisUsage(AnalysisUsage& AU) const override;
    static char ID; // Pass identification, replacement for typeid

  private:
    Function *checkInterfaceFunction(Constant *FuncOrBitcast);

    LLVMContext *context;
    DataLayout *TD;
    int LongSize;
    // READ/WRITE access
    Function *accessCallback;
    Function *ctorFunction;
    Type *intptrType, *int64Type, *boolType;
    InlineAsm *noopAsm;
    std::map<int, StringRef> funcNames;
    int funcCounter;
};

} // namespace

char Instrumenter::ID = 0;
static RegisterPass<Instrumenter>
    instrumenter("instrumenter", "Instrumenting READ/WRITE pass", false, false);
static cl::opt<int> startFrom(
    "start-from", cl::init(0), cl::desc("Start function id from N"), cl::Hidden);
static cl::opt<int> maxInsPerBB(
    "max-ins-per-bb", cl::init(10000),
    cl::desc("maximal number of instructions to instrument in any given BB"),
    cl::Hidden);
static cl::opt<bool> toInstrumentStack("instrument-stack-variables",
                                       cl::desc("instrument stack variables"),
                                       cl::Hidden, cl::init(false));
static cl::opt<bool> toInstrumentReads("instrument-reads",
                                       cl::desc("instrument read instructions"),
                                       cl::Hidden, cl::init(true));
static cl::opt<bool>
    toInstrumentWrites("instrument-writes",
                       cl::desc("instrument write instructions"), cl::Hidden,
                       cl::init(true));
static cl::opt<bool> toInstrumentAtomics(
    "instrument-atomics",
    cl::desc("instrument atomic instructions (rmw, cmpxchg)"), cl::Hidden,
    cl::init(true));

// INITIALIZE_PASS(Instrumenter, "Instrumenter",
//                 "Instrumenting Read/Write accesses",
//                 false, false)
Instrumenter::Instrumenter() : FunctionPass(ID) {}

StringRef Instrumenter::getPassName() const { return "Instrumenter"; }

// virtual: define some initialization for the whole module
bool Instrumenter::doInitialization(Module &M) {

    errs() << "\n^^^^^^^^^^^^^^^^^^Instrumenter "
              "initialization^^^^^^^^^^^^^^^^^^^^^^^^^"
           << "\n";
    // TD = getAnalysisIfAvailable<DataLayout>();
    TD = new DataLayout(&M);
    // if (!TD)
    //   return false;

    funcCounter = startFrom;

    context = &(M.getContext());
    LongSize = TD->getPointerSizeInBits();
    intptrType = Type::getIntNTy(*context, LongSize);
    int64Type = Type::getInt64Ty(*context);
    boolType = Type::getInt8Ty(*context);

    // Creating the contrunctor module "instrumenter"
    ctorFunction =
        Function::Create(FunctionType::get(Type::getVoidTy(*context), false),
                         GlobalValue::InternalLinkage, "instrumenter", &M);

    BasicBlock *ctorBB = BasicBlock::Create(*context, "", ctorFunction);
    Instruction *ctorinsertBefore = ReturnInst::Create(*context, ctorBB);

    IRBuilder<> IRB(ctorinsertBefore);

    // We insert an empty inline asm after __asan_report* to avoid callback
    // merge.
    noopAsm = InlineAsm::get(FunctionType::get(IRB.getVoidTy(), false),
                             StringRef(""), StringRef(""),
                             /*hasSideEffects=*/true);

    // Create instrumenation callbacks.
    // for (size_t isWriteAccess = 0; isWriteAccess <= 1; isWriteAccess++) {
    //     for (size_t accessSizeArrayIndex = 0;
    //          accessSizeArrayIndex < numAccessesSizes; accessSizeArrayIndex++) {
    //         // isWrite and typeSize are encoded in the function name.
    //         std::string funcName;
    //         if (isWriteAccess) {
    //             funcName = std::string("store_") +
    //                        itostr(1 << accessSizeArrayIndex) + "bytes";
    //         } else {
    //             funcName = std::string("load_") +
    //                        itostr(1 << accessSizeArrayIndex) + "bytes";
    //         }
    //         // If we are merging crash callbacks, they have two parameters.
    //         accessCallback[isWriteAccess][accessSizeArrayIndex] =
    //             checkInterfaceFunction(
    //                 M.getOrInsertFunction(funcName, IRB.getVoidTy(), intptrType,
    //                                       int64Type, int64Type));
    //     }
    // }

    accessCallback = checkInterfaceFunction(M.getOrInsertFunction(
        "handle_access", IRB.getVoidTy(), intptrType, int64Type, int64Type, int64Type, boolType
    ));

    // We insert an empty inline asm after __asan_report* to avoid callback
    // merge.
    // noopAsm = InlineAsm::get(FunctionType::get(IRB.getVoidTy(), false),
    //                          StringRef(""), StringRef(""), true);
    return true;
}


void Instrumenter::getAnalysisUsage(AnalysisUsage& AU) const {
    AU.addRequired<LoopInfoWrapperPass>();
}

// and set isWrite. Otherwise return nullptr.
static Value *isInterestingMemoryAccess(Instruction *ins, bool *isWrite) {
    if (LoadInst *LI = dyn_cast<LoadInst>(ins)) {

        if (!toInstrumentReads)
            return nullptr;
        //    errs() << "instruction is a load instruction\n\n";
        *isWrite = false;
        return LI->getPointerOperand();
    }
    if (StoreInst *SI = dyn_cast<StoreInst>(ins)) {
        if (!toInstrumentWrites)
            return nullptr;
        //   errs() << "instruction is a store instruction\n\n";
        *isWrite = true;
        return SI->getPointerOperand();
    }
    if (AtomicRMWInst *RMW = dyn_cast<AtomicRMWInst>(ins)) {
        if (!toInstrumentAtomics)
            return nullptr;
        //  errs() << "instruction is a atomic READ and Write instruction\n\n";
        *isWrite = true;
        return RMW->getPointerOperand();
    }
    if (AtomicCmpXchgInst *XCHG = dyn_cast<AtomicCmpXchgInst>(ins)) {
        if (!toInstrumentAtomics)
            return nullptr;
        //  errs() << "instruction is a atomic cmpXchg instruction\n\n";
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
    uint32_t typeSize = TD->getTypeStoreSizeInBits(OrigTy);

    if (typeSize != 8 && typeSize != 16 && typeSize != 32 && typeSize != 64 &&
        typeSize != 128) {
        errs() << "ignored typesize is " << typeSize << "at: ";
        return;
    }

    IRBuilder<> IRB(ins);
    instrumentAddress(ins, IRB, addr, typeSize, isWrite, funcId, instCounter);
}

// General function call before some given instruction
Instruction *Instrumenter::insertAccessCallback(Instruction *insertBefore,
                                                Value *addr, bool isWrite,
                                                size_t typeSize,
                                                unsigned long funcId,
                                                unsigned long instCounter) {
    IRBuilder<> IRB(insertBefore);

    std::vector<Value *> arguments;
    arguments.push_back(addr);
    arguments.push_back(ConstantInt::get(int64Type, funcId));
    arguments.push_back(ConstantInt::get(int64Type, instCounter));
    arguments.push_back(ConstantInt::get(int64Type, typeSize));
    arguments.push_back(ConstantInt::get(boolType, isWrite));

    CallInst *Call = IRB.CreateCall(accessCallback, ArrayRef<Value *>(arguments));

    // We don't do Call->setDoesNotReturn() because the BB already has
    // UnreachableInst at the end.
    // This noopAsm is required to avoid callback merge.

    IRB.CreateCall(noopAsm);
    return Call;
}

void Instrumenter::instrumentAddress(Instruction *origIns, IRBuilder<> &IRB,
                                     Value *addr, uint32_t typeSize,
                                     bool isWrite, unsigned long funcId,
                                     unsigned long instCounter) {

    Value *actualAddr = IRB.CreatePointerCast(addr, intptrType);

    dbgs() << "Generated function call: " 
           << (isWrite ? "store" : "load")
           << " size = " << typeSize
           << "funcId, instId = " << funcId << ", " << instCounter << "\n";

    // Insert the callback function here.
    insertAccessCallback(origIns, actualAddr, isWrite, typeSize,
                         funcId, instCounter);
}

// Validate the result of Module::getOrInsertFunction called for an interface
// function of Instrumenter. If the instrumented module defines a function
// with the same name, their prototypes must match, otherwise
// getOrInsertFunction returns a bitcast.
Function *Instrumenter::checkInterfaceFunction(Constant *FuncOrBitcast) {
    if (isa<Function>(FuncOrBitcast))
        return cast<Function>(FuncOrBitcast);
    //  FuncOrBitcast->dump();
    report_fatal_error("trying to redefine an Instrumenter "
                       "interface function");
}

bool Instrumenter::doFinalization(Module &M) {
    for (std::map<int, StringRef>::iterator it = funcNames.begin();
         it != funcNames.end(); it++)
        errs() << it->first << " " << it->second << "\n";
    delete TD;
    return false;
}

bool isLocalVariable(Value *value) {
    return dyn_cast<AllocaInst>(value);
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
        errs() << "basicb name: "<< BB->getName() <<"\n";
    }
    for (Loop *SL : L->getSubLoops()) {
        printLoop(SL);
    }
}

bool Instrumenter::runOnFunction(Function &F) {
    // If the input function is the function added by myself, don't do anything.
    if (&F == ctorFunction)
        return false;

    // Get loop info for this function.
    // if (!F.isDeclaration()) {
    //     // generate the LoopInfoBase for the current function
    //     LoopInfo &LI = getAnalysis<LoopInfoWrapperPass>().getLoopInfo();
    //     for (Loop *L : LI) 
    //         printLoop(L);
    // }

    int thisFuncId = funcCounter++;
    funcNames.insert(std::make_pair(thisFuncId, F.getName()));

    // Fill the set of memory operations to instrument.
    unsigned long instCounter = 0;
    int NumInstrumented = 0;
    bool isWrite;
    for (Function::iterator bb = F.begin(), FE = F.end(); bb != FE; ++bb) {
        for (BasicBlock::iterator ins = bb->begin(), BE = bb->end(); ins != BE;
             ++ins, ++instCounter) {
            Instruction *Inst = &*ins;
            if (Value *addr = isInterestingMemoryAccess(Inst, &isWrite)) {
                if (isLocalVariable(addr)) {
                    continue;
                }
                instrumentMemoryAccess(Inst, thisFuncId, instCounter);
                NumInstrumented++;
            }
        }
    }
    // If the function is modified, runOnFunction should return True.
    return NumInstrumented > 0;
}
