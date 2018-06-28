//
// Created by yifanz on 6/27/18.
//

#ifndef LLVM_GROUPFUNCLOOP_H
#define LLVM_GROUPFUNCLOOP_H

#include "UnrollLoopPass.h"
#include "Utils.h"

class GroupFuncLoop {
public:
    explicit GroupFuncLoop(ModulePass *pass, LLVMContext *context, DataLayout *layout, PostCloneT &insts) :
            pass(pass), layout(layout), instsPtr(&insts) {
        unsigned int ptrSize = layout->getPointerSizeInBits();
        sizeType = intPtrType = Type::getIntNTy(*context, ptrSize);
    }

    void runOnFunction(Function &func) {
        LoopInfo *li = &(pass->getAnalysis<LoopInfoWrapperPass>(func).getLoopInfo());
        getAllLoops(func, li);
        for (const auto &p: unrollInsts) {
            UnrollLoopPass pass(layout, &func, li);
            PostUnrollT unrolled = pass.runOnInstGroup(p.second);
            finalTable.insert(unrolled.begin(), unrolled.end());
        }
        size_t c = 0, m = 0, o = 0;
        for (const auto &p2: finalTable) {
            ActionType action(p2.second);
            switch (action.action) {
                case ActionType::changeCallee:
                    replaceCallFunc(p2.first, action.callee);
                    c++;
                    break;
                case ActionType::changeMalloc:
                    adjustMalloc(p2.first, action.malloc);
                    m++;
                    break;
                case ActionType::changeOffset:
                    offsetSimplInst(p2.first, action.offset);
                    o++;
                    break;
            }
        }
        dbgs() << "(c, m, o) = (" << c << ", " << m << ", " << o << "), in function " << func.getName() << '\n';
    }

private:
    void replaceCallFunc(Instruction *inst, Function *newFunc) const {
        CallInst *call = cast<CallInst>(inst);
        call->setArgOperand(2, newFunc);
    }

    void offsetSimplInst(Instruction *inst, long offset) const {
        unsigned int index = getPointerOperandIndex(inst);
        Value *pointer = inst->getOperand(index);
        Type *origPtrTy = pointer->getType();
        IRBuilder<> IRB(inst);
        // Add offset to uint64_t value of the pointer using an `add` instruction.
        Constant *offsetValue =
                ConstantInt::get(intPtrType, offset, /*isSigned=*/true);
        Value *actualAddrInt = IRB.CreatePointerCast(pointer, intPtrType);
        Value *redirectAddrInt = IRB.CreateAdd(actualAddrInt, offsetValue);
        Value *redirectPtr = IRB.CreateIntToPtr(
                redirectAddrInt, origPtrTy);  // sustain original type.
        inst->setOperand(index, redirectPtr);
    }

    void adjustMalloc(Instruction *inst, size_t sizeAdd) const {
        CallInst *call = cast<CallInst>(inst);
        Value *origSize =
                call->getArgOperand(0);  // size is always 0th argument in allocs.
        IRBuilder<> IRB(inst);
        Constant *addValue =
                ConstantInt::get(sizeType, sizeAdd, /*isSigned=*/false);
        Value *newSize = IRB.CreateAdd(origSize, addValue);
        call->setArgOperand(0, newSize);
    }

    void getAllLoops(Function &func, LoopInfo *li) {
        for (const auto &p : *instsPtr) {
            if (p.second.getSize() != 1) {
                BasicBlock *bb = p.first->getParent();
                Loop *loop = li->getLoopFor(bb);
                assert(loop);
                unrollInsts[loop].emplace(p.first, p.second);
            } else
                finalTable.emplace(p.first, ExpandedPCInfo(p.second, 0));
        }
    }

    ModulePass *pass;
    Type *intPtrType, *sizeType;
    DataLayout *layout;
    PostCloneT *instsPtr;
    std::unordered_map<Loop *, PostCloneT> unrollInsts;
    PostUnrollT finalTable;
};

#endif //LLVM_GROUPFUNCLOOP_H
