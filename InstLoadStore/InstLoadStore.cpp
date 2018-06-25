#define DEBUG_TYPE "instloadstore"

#include "llvm-c/Initialization.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Transforms/Instrumentation.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"

#include <unordered_map>
#include <unordered_set>
#include <fstream>
#include <llvm/Support/Debug.h>
#include <llvm/Analysis/LoopInfo.h>

using namespace llvm;

// We only support 5 sizes(powers of two): 1, 2, 4, 8, 16.
static const size_t numAccessesSizes = 5;

namespace {

/// Instrumenter: instrument the memory accesses.
struct InstLoadStore : public FunctionPass {
    InstLoadStore();

    StringRef getPassName() const override;

    bool runOnFunction(Function &F) override;

    bool doInitialization(Module &M) override;

    bool doFinalization(Module &M) override;

    void getAnalysisUsage(AnalysisUsage& AU) const override;

    static char ID; // Pass identification, replacement for typeid

private:
    LLVMContext *context;
    DataLayout *TD;
    Function *accessCallback, *ctorFunction;
    Type *intptrType, *boolType; // *int64Type, *intptrPtrType;
    InlineAsm *noopAsm;
    unsigned int funcId;

    std::unordered_map<unsigned int, std::unordered_set<unsigned int>> locInfo;

    void loadLocInfo();

    void instrumentMemoryAccess(BasicBlock::iterator &ins);

    Function *checkInterfaceFunction(Constant *FuncOrBitcast);

    Instruction *insertAccessCallback(BasicBlock::iterator &insertBefore, Value *addr, bool isWrite);
};

} // namespace

char InstLoadStore::ID = 0;
static RegisterPass<InstLoadStore> instloadstore(
        "instloadstore", "Instrumenting READ/WRITE pass", false, false
);
static cl::opt<std::string> locfile(
        "locfile", cl::desc("load location from given file"), cl::Hidden, cl::init("")
);

InstLoadStore::InstLoadStore() : FunctionPass(ID), funcId(0) {}

StringRef InstLoadStore::getPassName() const { return "InstLoadStore"; }

// virtual: define some initialization for the whole module
bool InstLoadStore::doInitialization(Module &M) {

    errs() << "\n^^^^^^^^^^^^^^^^^^InstLoadStore initialization^^^^^^^^^^^^^^^^^^^^^^^^^" << "\n";
    InstLoadStore::loadLocInfo();

    TD = new DataLayout(&M);
    context = &(M.getContext());

    unsigned int LongSize = TD->getPointerSizeInBits();
    intptrType = Type::getIntNTy(*context, LongSize);
    boolType = Type::getInt8Ty(*context);

    // Creating the constructor module "instrumenter"
    ctorFunction = Function::Create(FunctionType::get(Type::getVoidTy(*context), false),
                                    GlobalValue::InternalLinkage, "instrumenter", &M);

    BasicBlock *ctorBB = BasicBlock::Create(*context, "", ctorFunction);
    Instruction *ctorinsertBefore = ReturnInst::Create(*context, ctorBB);
    IRBuilder<> IRB(ctorinsertBefore);

    // We insert an empty inline asm after __asan_report* to avoid callback
    // merge.
    noopAsm = InlineAsm::get(FunctionType::get(IRB.getVoidTy(), false),
                             StringRef(""), StringRef(""),
                             /*hasSideEffects=*/true);

    // Create instrumentation callbacks.
    accessCallback = checkInterfaceFunction(
            M.getOrInsertFunction("redirect_ptr", intptrType, intptrType, boolType)
    );
    return true;
}

void InstLoadStore::loadLocInfo() {
    dbgs() << "Loading from file: " << locfile << '\n';
    std::ifstream fin(locfile.c_str());
    unsigned int funcId, instId;
    while (fin >> funcId >> instId)
        locInfo[funcId].insert(instId);
    for (const auto& p: locInfo) {
        dbgs() << p.first << '\n';
        for (const auto &x: p.second)
            dbgs() << x << ' ';
        dbgs() << '\n';
    }
}

void InstLoadStore::getAnalysisUsage(AnalysisUsage& AU) const {
    AU.addRequired<LoopInfoWrapperPass>();
}

static unsigned int getPointerOperandIndex(BasicBlock::iterator &ins, bool *isWrite) {
    if (LoadInst *LI = dyn_cast<LoadInst>(ins)) {
        *isWrite = false;
        return LI->getPointerOperandIndex();
    }
    if (StoreInst *SI = dyn_cast<StoreInst>(ins)) {
        *isWrite = true;
        return SI->getPointerOperandIndex();
    }
    if (AtomicRMWInst *RMW = dyn_cast<AtomicRMWInst>(ins)) {
        *isWrite = true;
        return RMW->getPointerOperandIndex();
    }
    if (AtomicCmpXchgInst *XCHG = dyn_cast<AtomicCmpXchgInst>(ins)) {
        *isWrite = true;
        return XCHG->getPointerOperandIndex();
    }
    errs() << "Instruction is not load/store!";
    assert(false);
}

void InstLoadStore::instrumentMemoryAccess(BasicBlock::iterator &ins) {
    bool isWrite = false;
    unsigned int index = getPointerOperandIndex(ins, &isWrite);
    Value *addr = ins->getOperand(index);

    Type *OrigPtrTy = addr->getType();
    Type *OrigTy = cast<PointerType>(OrigPtrTy)->getElementType();

    assert(OrigTy->isSized());
    uint64_t typeSize = TD->getTypeStoreSizeInBits(OrigTy);

    if (typeSize != 8 && typeSize != 16 && typeSize != 32 && typeSize != 64 &&
        typeSize != 128) {
        assert(false);
    }

    IRBuilder<> IRB(&*ins);
    Value *actualAddr = IRB.CreatePointerCast(addr, intptrType);
    // replace the load/store with a redirected one.
    Value *redirectAddr = insertAccessCallback(ins, actualAddr, isWrite);
    Value *redirectPtr = IRB.CreateIntToPtr(redirectAddr, OrigPtrTy);  // sustain original type.
    ins->setOperand(index, redirectPtr);
    // We don't do Call->setDoesNotReturn() because the BB already has
    // UnreachableInst at the end.
    // This noopAsm is required to avoid callback merge.
    IRB.CreateCall(noopAsm);
}

// General function call before some given instruction
Instruction *InstLoadStore::insertAccessCallback(BasicBlock::iterator &insertBefore, Value *addr, bool isWrite) {
    IRBuilder<> IRB(&*insertBefore);

    std::vector<Value*> arguments;
    arguments.push_back(addr);
    arguments.push_back(ConstantInt::get(boolType, (uint64_t)isWrite));

    CallInst *Call = IRB.CreateCall(accessCallback, ArrayRef<Value *>(arguments));
    errs() << "Generated function call:\n\t";
    Call->print(errs());
    errs() << "\n";

    return Call;
}

// Validate the result of Module::getOrInsertFunction called for an interface
// function of InstLoadStore. If the instrumented module defines a function
// with the same name, their prototypes must match, otherwise
// getOrInsertFunction returns a bitcast.
Function *InstLoadStore::checkInterfaceFunction(Constant *FuncOrBitcast) {
    if (isa<Function>(FuncOrBitcast))
        return cast<Function>(FuncOrBitcast);
    //  FuncOrBitcast->dump();
    report_fatal_error("trying to redefine an InstLoadStore "
                       "interface function");
}

bool InstLoadStore::doFinalization(Module &M) {
    delete TD;
    return false;
}

bool InstLoadStore::runOnFunction(Function &F) {
    // If the input function is the function added by myself, don't do anything.
    if (&F == ctorFunction)
        return false;

    // Fill the set of memory operations to instrument.
    unsigned int thisFuncId = funcId++;
    auto instIdsIt = locInfo.find(thisFuncId);
    if (instIdsIt == locInfo.end())
        return false;

    unsigned int instCounter = 0;
    for (Function::iterator bb = F.begin(), FE = F.end(); bb != FE; ++bb) {
        for (BasicBlock::iterator ins = bb->begin(), BE = bb->end(); ins != BE;
             ++ins, ++instCounter) {
            const auto &instIds = instIdsIt->second;
            if (instIds.find(instCounter) == instIds.end())
                continue;
            ins->print(errs());
            instrumentMemoryAccess(ins);
        }
    }
//    dbgs() << "Exiting...\n";
//    for (Function::iterator bb = F.begin(), FE = F.end(); bb != FE; ++bb) {
//        for (BasicBlock::iterator ins = bb->begin(), BE = bb->end(); ins != BE;
//             ++ins, ++instCounter) {
//            ins->dump();
//        }
//    }
    return true;
}
