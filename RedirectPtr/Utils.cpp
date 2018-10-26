//
// Created by yifanz on 10/26/18.
//

#include "Utils.h"

unsigned int getPointerOperandIndex(Instruction *inst, bool &isWrite) {
    if (isa<LoadInst>(inst)) {
        isWrite = false;
        return LoadInst::getPointerOperandIndex();
    }
    if (isa<StoreInst>(inst)) {
        isWrite = true;
        return StoreInst::getPointerOperandIndex();
    }
    if (isa<AtomicRMWInst>(inst)) {
        isWrite = true;
        return AtomicRMWInst::getPointerOperandIndex();
    }
    if (isa<AtomicCmpXchgInst>(inst)) {
        isWrite = true;
        return AtomicCmpXchgInst::getPointerOperandIndex();
    }
    errs() << "Instruction is not load/store!";
    assert(false);
}

unsigned int getPointerOperandIndex(Instruction *inst) {
    bool _dummy;
    return getPointerOperandIndex(inst, _dummy);
}
