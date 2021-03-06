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
        dbgs() << "Working on function " << func.getName() << '\n';
        dbgs() << "  Gathering the loops.\n";
        LoopInfo *li = &(pass->getAnalysis<LoopInfoWrapperPass>(func).getLoopInfo());
        getAllLoops(func, li);
        for (const auto &p: unrollInsts) {
            UnrollLoopPass pass(layout, &func, li);
            PostUnrollT unrolled = pass.runOnInstGroup(p.second);
            finalTable.insert(unrolled.begin(), unrolled.end());
        }
        dbgs() << "  Processing instructions.\n";
        size_t c = 0, m = 0, o = 0;

        for (const auto &p2: finalTable) {
            auto changeOffset = [this, p2, &o](std::pair<size_t, size_t> fromTo) {
                long offset = (long)fromTo.second - (long)fromTo.first;
                offsetSimplInst(p2.first, offset);
                o++;
            };
            auto changeCallee = [this, p2, &c](Function *callee) {
                replaceCallFunc(p2.first, callee);
                c++;
            };
            auto changeMalloc = [this, p2, &m](long malloc) {
                adjustMalloc(p2.first, malloc);
                m++;
            };
            p2.second.actOn(changeOffset, changeCallee, changeMalloc);
        }
        dbgs() << "  (c, m, o) = (" << c << ", " << m << ", " << o << ")\n";
    }

private:
    void replaceCallFunc(Instruction *inst, Function *newFunc) const {
        inst->dump();
        // pthread_create cannot be invoked.
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

    void adjustMalloc(Instruction *inst, long sizeAdd) const {
        // allocs are no-throw and won't be invoked.
        CallInst *call = cast<CallInst>(inst);
        StringRef name = call->getCalledFunction()->getName();
        Value *origSize = nullptr;
        if (name == "malloc")
            origSize = call->getArgOperand(0);
        else if (name == "calloc")
            assert(false && "Not implemented");
        else if (name == "realloc")
            origSize = call->getArgOperand(1);
        IRBuilder<> IRB(inst);
        Constant *addValue = ConstantInt::get(sizeType, sizeAdd, /*isSigned=*/true);
        Value *newSize = IRB.CreateAdd(origSize, addValue);
        call->setArgOperand(0, newSize);
    }

    void getAllLoops(Function &func, LoopInfo *li) {
        for (const auto &p : *instsPtr) {
            if (p.second.getSize() != 1) {
                BasicBlock *bb = p.first->getParent();
                Loop *loop = li->getLoopFor(bb);
                assert(loop && "A multiple-offset inst is not directly in a loop");
                while (Loop *parent = loop->getParentLoop())
                    loop = parent;
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
