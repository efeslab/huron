//
// Created by yifanz on 10/26/18.
//

#include <llvm/Transforms/Utils/BasicBlockUtils.h>
#include <llvm/IR/Instructions.h>

#include "GroupFuncLoop.h"
#include "UnrollLoopPass.h"

GroupFuncLoop::GroupFuncLoop(ModulePass *MP, Module *M, Function *F, PostCloneT &insts) :
        module(M), instsPtr(&insts), func(F) {
    context = &M->getContext();
    layout = new DataLayout(M);
    unsigned int ptrSize = layout->getPointerSizeInBits();
    sizeType = intPtrType = Type::getIntNTy(*context, ptrSize);
    loopInfo = &(MP->getAnalysis<LoopInfoWrapperPass>(*F).getLoopInfo());
}

void GroupFuncLoop::runOnFunction() {
    dbgs() << "Working on function " << func->getName() << '\n';
    dbgs() << "  Gathering the loops.\n";
    getAllLoops();
    for (const auto &p: unrollInsts) {
        UnrollLoopPass pass(layout, func, loopInfo);
        PostUnrollT unrolled = pass.runOnInstGroup(p.second);
        finalTable.insert(unrolled.begin(), unrolled.end());
    }
    dbgs() << "  Processing instructions.\n";
    size_t c = 0, m = 0, d = 0, o = 0;

    for (const auto &p2: finalTable) {
        auto changeOffset = [this, p2, &o](std::pair<size_t, size_t> fromTo) {
            long offset = (long)fromTo.second - (long)fromTo.first;
            offsetSimplInst(p2.first, offset);
            o++;
        };
        auto changeMalloc = [this, p2, &m](const MallocInfo &malloc) {
            adjustMalloc(p2.first, malloc);
            m++;
        };
        auto externalDep = [this, p2, &d](size_t depId) {
            resolveExtDep(p2.first, depId);
            d++;
        };
        auto changeCallee = [this, p2, &c](Function *callee) {
            replaceCallFunc(p2.first, callee);
            c++;
        };
        p2.second.actOn(changeOffset, changeMalloc, externalDep, changeCallee);
    }
    dbgs() << "  (c, m, o, d) = (" << c << ", " << m << ", " << o << ", " << d << ")\n";
}

void GroupFuncLoop::replaceCallFunc(Instruction *inst, Function *newFunc) const {
    //inst->dump();
    // pthread_create cannot be invoked.
    auto *call = cast<CallInst>(inst);
    call->setArgOperand(2, newFunc);
}

void GroupFuncLoop::offsetSimplInst(Instruction *inst, long offset) const {
    unsigned int index = getPointerOperandIndex(inst);
    Value *pointer = inst->getOperand(index);
    Type *origPtrTy = pointer->getType();
    IRBuilder<> IRB(inst);
    // Add offset to uint64_t value of the pointer using an `add` instruction.
    Constant *offsetValue =
            ConstantInt::get(intPtrType, static_cast<uint64_t>(offset), /*isSigned=*/true);
    Value *actualAddrInt = IRB.CreatePointerCast(pointer, intPtrType);
    Value *redirectAddrInt = IRB.CreateAdd(actualAddrInt, offsetValue);
    Value *redirectPtr = IRB.CreateIntToPtr(
            redirectAddrInt, origPtrTy);  // sustain original type.
    inst->setOperand(index, redirectPtr);
}

void GroupFuncLoop::adjustMalloc(Instruction *inst, const MallocInfo &malloc) const {
    // allocs are no-throw and won't be invoked.
    auto *call = cast<CallInst>(inst);
    StringRef name = call->getCalledFunction()->getName();
    Value *origSize = nullptr;
    if (name == "malloc")
        origSize = call->getArgOperand(0);
    else if (name == "calloc")
        assert(false && "Not implemented");
    else if (name == "realloc")
        origSize = call->getArgOperand(1);
    Constant *addValue = ConstantInt::get(sizeType, static_cast<uint64_t>(malloc.sizeDelta), /*isSigned=*/true);
    Value *newSize = BinaryOperator::CreateAdd(origSize, addValue, "", inst);
    call->setArgOperand(0, newSize);

    Instruction *next = inst->getNextNode();
    GlobalVariable *mallocStartT = module->getGlobalVariable("__malloc_start_table");
    GlobalVariable *mallocSizeT = module->getGlobalVariable("__malloc_start_table");
    SmallVector<Value *, 2> indices({
        ConstantInt::get(sizeType, 0), ConstantInt::get(sizeType, malloc.id)
    });
    IRBuilder<> irb(next);
    Value *allocAddr = irb.CreatePtrToInt(call, sizeType);
    Value *startTableEntry = irb.CreateGEP(mallocStartT, indices);
    Value *sizeTableEntry = irb.CreateGEP(mallocSizeT, indices);
    irb.CreateStore(allocAddr, startTableEntry);
    irb.CreateStore(newSize, sizeTableEntry);
}

void GroupFuncLoop::resolveExtDep(Instruction *inst, size_t depMallocId) const {
    unsigned idx = getPointerOperandIndex(inst);
    Value *pointerOpr = inst->getOperand(idx);
    GlobalVariable *mallocStartT = module->getGlobalVariable("__malloc_start_table");
    GlobalVariable *mallocSizeT = module->getGlobalVariable("__malloc_size_table");
    GlobalVariable *mallocOffsetT = module->getGlobalVariable("__malloc_offset_table");
    SmallVector<Value *, 2> msIndices({
        ConstantInt::get(sizeType, 0), ConstantInt::get(sizeType, depMallocId)
    });

    IRBuilder<> checkInboundIRB(inst);
    Value *allocStartPtr = checkInboundIRB.CreateGEP(mallocStartT, msIndices);
    Value *allocStartAddr = checkInboundIRB.CreateLoad(allocStartPtr);
    Value *pointerAddr = checkInboundIRB.CreatePtrToInt(pointerOpr, sizeType);
    Value *offset = checkInboundIRB.CreateSub(pointerAddr, allocStartAddr);
    Value *allocSizePtr = checkInboundIRB.CreateGEP(mallocSizeT, msIndices);
    Value *allocSize = checkInboundIRB.CreateLoad(allocSizePtr);
    Value *lhsInbound = checkInboundIRB.CreateICmpUGE(pointerAddr, allocStartAddr);
    Value *rhsInbound = checkInboundIRB.CreateICmpULT(offset, allocSize);
    Value *inbound = checkInboundIRB.CreateAnd(lhsInbound, rhsInbound);
    TerminatorInst *thenTerm = SplitBlockAndInsertIfThen(inbound, inst, false);

    IRBuilder<> redirectIRB(thenTerm);
    SmallVector<Value *, 3> moIndices({
        ConstantInt::get(sizeType, 0), ConstantInt::get(sizeType, depMallocId), offset
    });
    Value *mappedOffsetEntry = redirectIRB.CreateGEP(mallocOffsetT, moIndices);
    Value *mappedOffset = redirectIRB.CreateLoad(mappedOffsetEntry);
    Value *mappedPointerAddr = redirectIRB.CreateAdd(allocStartAddr, mappedOffset);
    Value *mappedPointer = redirectIRB.CreateIntToPtr(mappedPointerAddr, pointerOpr->getType());

    PHINode *pn = PHINode::Create(pointerOpr->getType(), 2, "", inst);
    pn->addIncoming(pointerOpr, cast<Instruction>(pointerOpr)->getParent());
    pn->addIncoming(mappedPointer, thenTerm->getParent());
    inst->setOperand(idx, pn);
}

void GroupFuncLoop::getAllLoops() {
    for (const auto &p : *instsPtr) {
        if (p.second.getSize() != 1) {
            BasicBlock *bb = p.first->getParent();
            Loop *loop = loopInfo->getLoopFor(bb);
            assert(loop && "A multiple-offset inst is not directly in a loop");
            while (Loop *parent = loop->getParentLoop())
                loop = parent;
            unrollInsts[loop].emplace(p.first, p.second);
        } else
            finalTable.emplace(p.first, p.second[0]);
    }
}
