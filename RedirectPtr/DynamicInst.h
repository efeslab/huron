#define DEBUG_TYPE "instloadstore"

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/IntrinsicInst.h"

#include <unordered_map>

using namespace llvm;

class InstLoadStore {
public:
    InstLoadStore(Module &M) {
        DataLayout *dl = new DataLayout(&M);
        LLVMContext &context = M.getContext();
        unsigned int LongSize = dl->getPointerSizeInBits();
        intptrType = Type::getIntNTy(context, LongSize);
        boolType = Type::getInt8Ty(context);
        redirectPtr = M.getOrInsertFunction("redirect_ptr", intptrType, intptrType, boolType);
    }

    bool dynInstFunction(Function *func, std::unordered_set<unsigned int> &locInfo) {
        unsigned int instCounter = 0;
        for (auto bb = func->begin(), FE = func->end(); bb != FE; ++bb) {
            for (auto ins = bb->begin(), BE = bb->end(); ins != BE; ++ins, ++instCounter) {
                const auto &instIds = instIdsIt->second;
                if (instIds.find(instCounter) == instIds.end())
                    continue;
                instrumentMemoryAccess(ins);
            }
        }
        return true;
    }

private:
    Instruction *insertAccessCallback(Instruction *insertBefore, Value *addr, bool isWrite) {
        IRBuilder<> IRB(insertBefore);

        std::vector<Value*> arguments;
        arguments.push_back(addr);
        arguments.push_back(ConstantInt::get(boolType, (uint64_t)isWrite));

        CallInst *Call = IRB.CreateCall(accessCallback, ArrayRef<Value *>(arguments));

        return Call;
    }

    void InstLoadStore::instrumentMemoryAccess(Instruction *inst) {
        bool isWrite = false;
        unsigned int index = getPointerOperandIndex(inst, &isWrite);
        Value *addr = inst->getOperand(index);

        Type *OrigPtrTy = addr->getType();
        Type *OrigTy = cast<PointerType>(OrigPtrTy)->getElementType();

        assert(OrigTy->isSized());
        uint64_t typeSize = TD->getTypeStoreSizeInBits(OrigTy);

        if (typeSize != 8 && typeSize != 16 && typeSize != 32 && typeSize != 64 &&
            typeSize != 128) {
            assert(false);
        }

        IRBuilder<> IRB(inst);
        Value *actualAddr = IRB.CreatePointerCast(addr, intptrType);
        // replace the load/store with a redirected one.
        Value *redirectAddr = insertAccessCallback(inst, actualAddr, isWrite);
        Value *redirectPtr = IRB.CreateIntToPtr(redirectAddr, OrigPtrTy);  // sustain original type.
        inst->setOperand(index, redirectPtr);
        // We don't do Call->setDoesNotReturn() because the BB already has
        // UnreachableInst at the end.
        // This noopAsm is required to avoid callback merge.
        IRB.CreateCall(noopAsm);
    }

    Type *intptrType, boolType;
    Function *redirectPtr;
};
