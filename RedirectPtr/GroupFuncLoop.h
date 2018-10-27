//
// Created by yifanz on 10/26/18.
//

#ifndef HURON_GROUPFUNCLOOP_H
#define HURON_GROUPFUNCLOOP_H

#include "UnrollLoopPass.h"

class GroupFuncLoop {
public:
    explicit GroupFuncLoop(ModulePass *MP, Module *M, Function *F, PostCloneT &insts);

    void runOnFunction();

private:
    void getAllLoops();

    void replaceCallFunc(Instruction *inst, Function *newFunc) const;

    void offsetSimplInst(Instruction *inst, long offset) const;

    void adjustMalloc(Instruction *inst, const MallocInfo &malloc) const;

    LLVMContext *context{};
    DataLayout *layout{};
    Function *func{};
    LoopInfo *loopInfo{};
    Type *intPtrType, *sizeType;
    PostCloneT *instsPtr;
    std::unordered_map<Loop *, PostCloneT> unrollInsts;
    PostUnrollT finalTable;
};

#endif //HURON_GROUPFUNCLOOP_H
