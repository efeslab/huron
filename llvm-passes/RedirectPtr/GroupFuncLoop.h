//
// Created by yifanz on 10/26/18.
//

#ifndef HURON_GROUPFUNCLOOP_H
#define HURON_GROUPFUNCLOOP_H

#include <llvm/Analysis/LoopInfo.h>
#include <llvm/ADT/SetVector.h>
#include <llvm/IR/DebugInfoMetadata.h>
#include <unordered_set>

#include "Utils.h"

class GroupFuncLoop {
public:
    explicit GroupFuncLoop(ModulePass *MP, Module *M, Function *F, PostCloneT &insts);

    std::unordered_set<DebugLoc *> runOnFunction();

private:
    void getAllLoops();

    void replaceCallFunc(Instruction *inst, Function *newFunc) const;

    void offsetSimplInst(Instruction *inst, long offset) const;

    void adjustMalloc(Instruction *inst, const MallocInfo &malloc) const;

    void resolveExtDep(Instruction *inst, size_t depMallocId) const;

    Module *module{};
    LLVMContext *context{};
    DataLayout *layout{};
    Function *func{};
    LoopInfo *loopInfo{};
    Type *intPtrType, *sizeType, *callFuncType;
    PostCloneT *instsPtr;
    std::unordered_map<Loop *, PostCloneT> unrollInsts;
    PostUnrollT finalTable;
};

#endif //HURON_GROUPFUNCLOOP_H
