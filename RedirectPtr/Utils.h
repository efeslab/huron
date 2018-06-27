
#include "llvm/IR/Instruction.h"
#include <cassert>

using namespace llvm;

unsigned int getPointerOperandIndex(Instruction *inst) {
    if (LoadInst *LI = dyn_cast<LoadInst>(inst)) {
        // *isWrite = false;
        return LI->getPointerOperandIndex();
    }
    if (StoreInst *SI = dyn_cast<StoreInst>(inst)) {
        // *isWrite = true;
        return SI->getPointerOperandIndex();
    }
    if (AtomicRMWInst *RMW = dyn_cast<AtomicRMWInst>(inst)) {
        // *isWrite = true;
        return RMW->getPointerOperandIndex();
    }
    if (AtomicCmpXchgInst *XCHG = dyn_cast<AtomicCmpXchgInst>(inst)) {
        // *isWrite = true;
        return XCHG->getPointerOperandIndex();
    }
    errs() << "Instruction is not load/store!";
    assert(false);
}
