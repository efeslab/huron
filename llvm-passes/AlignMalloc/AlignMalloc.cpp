#include "llvm/Pass.h"
#include "llvm-c/Initialization.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/InitializePasses.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/DataTypes.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Instrumentation.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"

#include <vector>

using namespace llvm;

namespace {
  struct AddFunction : public ModulePass {
    static char ID;
    AddFunction() : ModulePass(ID) {}
    bool runOnModule(Module &M) override {
      errs() << "Module pass working" << '\n';
      std::vector<Instruction *> modifiedOnes;
      for(auto fi = M.begin(); fi != M.end(); fi++)
      {
        for(auto bi = fi->begin(); bi != fi->end(); bi++)
        {
          for(auto ii = bi->begin(); ii != bi->end(); ii++)
          {
            Instruction *Inst = &(*ii);
            CallInst *callInst = dyn_cast<CallInst>(Inst);
            if(callInst != NULL)
            {
              Function *callee = callInst->getCalledFunction();
              if(callee != NULL)
              {
                if(callee->getName() == "malloc")
                {
                  modifiedOnes.push_back(Inst);
                }
              }
            }
          }
        }
      }
      Function *alignedAlloc = cast<Function>(M.getOrInsertFunction(
        "aligned_alloc", Type::getInt8PtrTy(M.getContext()), Type::getInt64Ty(M.getContext()), Type::getInt64Ty(M.getContext())
      ));
      for(unsigned i = 0; i < modifiedOnes.size(); i++)
      {
        Instruction *Inst = modifiedOnes[i];
        CallInst *instToReplace = dyn_cast<CallInst>(Inst);
        Value *size_arg = instToReplace->getArgOperand(0);
        BasicBlock::iterator ii(instToReplace);
        std::vector<Value *> arguments;
        long sixtyFour = 64;
        arguments.push_back(ConstantInt::get(Type::getInt64Ty(M.getContext()), sixtyFour, false));
        arguments.push_back(size_arg);
        ReplaceInstWithInst(instToReplace->getParent()->getInstList(), ii, CallInst::Create(alignedAlloc, ArrayRef<Value *>(arguments)));
      }
      return modifiedOnes.size() > 0;
    }
  private:
  };
}

char AddFunction::ID = 0;

static RegisterPass<AddFunction> X("alignmalloc", "Align Malloc Module Pass", false, false);
