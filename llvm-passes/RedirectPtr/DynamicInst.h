#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/IntrinsicInst.h"

#include "Utils.h"

#include <unordered_map>
#include <unordered_set>

using namespace llvm;

class InstLoadStore {
public:
    explicit InstLoadStore(Module &M) {
        layout = new DataLayout(&M);
        LLVMContext &context = M.getContext();
        unsigned int LongSize = layout->getPointerSizeInBits();
        intptrType = Type::getIntNTy(context, LongSize);
        boolType = Type::getInt8Ty(context);
        redirectPtr = cast<Function>(M.getOrInsertFunction("redirect_ptr", intptrType, intptrType, boolType));
        noopAsm = InlineAsm::get(
                FunctionType::get(Type::getVoidTy(context), false),
                StringRef(""), StringRef(""),
                /*hasSideEffects=*/true
        );
    }

    void dynInstFunction(Function *func, std::unordered_set<unsigned int> &locInfo) {
        unsigned int instCounter = 0;
        for (auto bb = func->begin(), FE = func->end(); bb != FE; ++bb) {
            for (auto ins = bb->begin(), BE = bb->end(); ins != BE; ++ins, ++instCounter) {
                if (locInfo.find(instCounter) == locInfo.end())
                    continue;
                instrumentMemoryAccess(&*ins);
            }
        }
    }

private:
    Instruction *insertAccessCallback(Instruction *insertBefore, Value *addr, bool isWrite) {
        IRBuilder<> IRB(insertBefore);

        std::vector<Value*> arguments;
        arguments.push_back(addr);
        arguments.push_back(ConstantInt::get(boolType, (uint64_t)isWrite));

        CallInst *Call = IRB.CreateCall(redirectPtr, ArrayRef<Value *>(arguments));

        return Call;
    }

    void instrumentMemoryAccess(Instruction *inst) {
        bool isWrite = false;
        unsigned int index = getPointerOperandIndex(inst, isWrite);
        Value *addr = inst->getOperand(index);

        Type *OrigPtrTy = addr->getType();
        Type *OrigTy = cast<PointerType>(OrigPtrTy)->getElementType();

        assert(OrigTy->isSized());
        uint64_t typeSize = layout->getTypeStoreSizeInBits(OrigTy);

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

    DataLayout *layout;
    InlineAsm *noopAsm;
    Type *intptrType, *boolType;
    Function *redirectPtr;
};
