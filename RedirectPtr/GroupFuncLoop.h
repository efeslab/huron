//
// Created by yifanz on 10/26/18.
//

#ifndef HURON_GROUPFUNCLOOP_H
#define HURON_GROUPFUNCLOOP_H

#include "UnrollLoopPass.h"

class GroupFuncLoop {
public:
    explicit GroupFuncLoop(ModulePass *pass, LLVMContext *context, DataLayout *layout, PostCloneT &insts);

    void runOnFunction(Function &func);

private:
    void replaceCallFunc(Instruction *inst, Function *newFunc) const;

    void offsetSimplInst(Instruction *inst, long offset) const;

    void adjustMalloc(Instruction *inst, long sizeAdd) const;

    void getAllLoops(Function &func, LoopInfo *li);

    ModulePass *pass;
    Type *intPtrType, *sizeType;
    DataLayout *layout;
    PostCloneT *instsPtr;
    std::unordered_map<Loop *, PostCloneT> unrollInsts;
    PostUnrollT finalTable;
};

#endif //HURON_GROUPFUNCLOOP_H
