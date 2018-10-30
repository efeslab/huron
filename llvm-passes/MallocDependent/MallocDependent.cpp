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
#include "llvm/ADT/SetVector.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#include <string>
#include <vector>
#include <map>
#include <stdio.h>
#include <utility>
#include <iostream>
#include <fstream>
#include <sstream>

#include "AndersenAA.h"

using namespace llvm;

namespace {
  struct AddFunction : public ModulePass {
    static char ID;
    std::unique_ptr<AndersenAAResult> result;
    AddFunction() : ModulePass(ID) {}
    AndersenAAResult &getResult() {return *result;}
    const AndersenAAResult &getResult() const {return *result;}
    void getAnalysisUsage(AnalysisUsage &AU) const override {
      AU.setPreservesAll();
    }
    static inline bool isInterestingPointer(Value *V) {
      return V->getType()->isPointerTy()
          && !isa<ConstantPointerNull>(V);
    }
    static void PrintResults(AliasResult AR, bool P, const Value *V1, const Value *V2, const Module *M)
    {
      std::string o1, o2;
      {
        raw_string_ostream os1(o1), os2(o2);
        V1->printAsOperand(os1, true, M);
        V2->printAsOperand(os2, true, M);
      }

      if (o2 < o1)std::swap(o1, o2);
      errs() << "  " << AR << ":\t" << o1 << ", " << o2 << "\n";
    }
    void runInternal(Instruction &Malloc, Module &M, unsigned curr_func_id, unsigned curr_inst_id) {
      errs() << "Checking dependency for the instruction->\n" << Malloc << " " << "\nIs a Pointer: " << Malloc.getType()->isPointerTy() << "\n";
      Value *pointer = &Malloc;
      unsigned functionId = 0;
      for(auto fi = M.begin(); fi != M.end(); fi++)
      {
        unsigned instructionId=0;
        for(auto bi = fi->begin(); bi != fi->end(); bi++)
        {
          for(auto ii = bi->begin(); ii != bi->end(); ii++)
          {
            Instruction *Inst = &(*ii);
            if( (curr_func_id != functionId || curr_inst_id != instructionId) && (isa<LoadInst>(&*Inst) || isa<StoreInst>(&*Inst)))
            {
              const MemoryLocation &l1 = (isa<LoadInst>(&*Inst)) ?  MemoryLocation::get(cast<LoadInst>(Inst)): MemoryLocation::get(cast<StoreInst>(Inst)); 
              const Value *v1 = (l1.Ptr)->stripPointerCasts();
              if(v1->getType()->isPointerTy())
              {
                if(v1 == pointer || result->publicAlias(v1, pointer) == MayAlias || result->publicAlias(v1, pointer) == MustAlias)
                {
                  errs() << functionId << " " << instructionId << " " << curr_func_id << " " << curr_inst_id << "\n" << *Inst << "\n";
                }
              }
            } 
            instructionId+=1;
          }
        }
        functionId+=1;
      }
    }
    bool runOnModule(Module &M) override {
      errs() << "Loading malloc func insts from mallocRuntimeIDs.txt file...\n";
      std::ifstream fp("mallocRuntimeIDs.txt");
      std::map<std::pair<unsigned, unsigned>, bool> malloc_func_inst_id;
      unsigned tmp_f, tmp_i;
      if (fp.is_open())
      {
        std::string line;
        while(std::getline(fp, line))
        {
          std::stringstream ss(line);
          std::string tmp_string;
          std::getline(ss, tmp_string, ',');
          std::getline(ss, tmp_string, ',');
          std::getline(ss, tmp_string, ',');
          std::getline(ss, tmp_string, ',');//out of 5 columns, last two columns are func_id and inst_id
          int tmp = std::stoi(tmp_string, nullptr);
          if (tmp < 0)continue;
          tmp_f = tmp;
          std::getline(ss, tmp_string, ',');
          tmp = std::stoi(tmp_string, nullptr);
          if (tmp < 0)continue;
          tmp_i = tmp;
          malloc_func_inst_id[std::make_pair(tmp_f, tmp_i)]=true;
        }
      }
      //errs() << "Module pass working" << '\n';
      std::vector<Instruction *> modifiedOnes;
      std::vector<unsigned> functionIds;
      std::vector<unsigned> instructionIds;
      unsigned functionId = 0;
      for(auto fi = M.begin(); fi != M.end(); fi++)
      {
        unsigned instructionId=0;
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
                  if(malloc_func_inst_id.find(std::make_pair(functionId, instructionId)) != malloc_func_inst_id.end())
                  {
                    modifiedOnes.push_back(Inst);
                    functionIds.push_back(functionId);
                    instructionIds.push_back(instructionId);
                    errs() << functionId << " " << instructionId << " " << *Inst  << "\n";
                  }
                }
              }
            }
            instructionId+=1;
          }
        }
        functionId+=1;
      } 
      /*Function *alignedAlloc = cast<Function>(M.getOrInsertFunction(
        "aligned_alloc", Type::getInt8PtrTy(M.getContext()), Type::getInt64Ty(M.getContext()), Type::getInt64Ty(M.getContext())
        ));*/
      /*for(unsigned i = 0; i < modifiedOnes.size(); i++)
        {
        Instruction *Inst = modifiedOnes[i];
        if(malloc_func_inst_id.find(std::make_pair(functionIds[i], instructionIds[i])) != malloc_func_inst_id.end())
        {
        }
        }*/
      result.reset(new AndersenAAResult(M));
      for(unsigned i = 0; i < modifiedOnes.size(); i++)
      {
        Instruction *Inst = modifiedOnes[i];
        runInternal(*Inst, M, functionIds[i], instructionIds[i]);
      }
      //for(auto fi = M.begin(); fi != M.end(); fi++)runInternal(*fi);
      return false;
    }
    private:
  };
}

char AddFunction::ID = 0;

static RegisterPass<AddFunction> X("mallocdependent", "genereate list of pointers dependent on the malloc instruction", false, false);
