//===- Hello.cpp - Example code from "Writing an LLVM Pass" ---------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements two versions of the LLVM "Hello World" pass described
// in docs/WritingAnLLVMPass.html
//
//===----------------------------------------------------------------------===//

#include "llvm/IR/Constants.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/DerivedTypes.h" 
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Pass.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/DepthFirstIterator.h"
#include "llvm/ADT/SetOperations.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/AliasSetTracker.h"
#include "llvm/Analysis/CaptureTracking.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/PostDominators.h"
#include "llvm/IR/CFG.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/IR/ConstantRange.h"
#include "llvm/Support/Debug.h"
#include "llvm/IR/GetElementPtrTypeIterator.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/Local.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/Analysis/ScalarEvolutionExpressions.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Function.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
using namespace llvm;

namespace {
  // Hello - The first implementation, without getAnalysisUsage.
  struct TMI : public FunctionPass {
    Function *BeginFunction;
    Function *EndFunction;
    static char ID; // Pass identification, replacement for typeid
    TMI() : FunctionPass(ID) {}

    bool doInitialization(Module &M) override {
      Constant *InstFuncConst =
	M.getOrInsertFunction("TMI_beginASM",FunctionType::get(Type::getVoidTy(M.getContext()),
							       false));
      assert(InstFuncConst != NULL);
      BeginFunction = dyn_cast<Function>(InstFuncConst);
      assert(BeginFunction != NULL);

      InstFuncConst =
	M.getOrInsertFunction("TMI_endASM",FunctionType::get(Type::getVoidTy(M.getContext()),
							       false));
      assert(InstFuncConst != NULL);
      EndFunction = dyn_cast<Function>(InstFuncConst);
      assert(EndFunction != NULL);

      return false;
    }
    
    bool runOnFunction(Function &F) override {
      for(auto BB = F.begin(), BBEND = F.end(); BB != BBEND; ++BB){
	for(auto I = BB->begin(), IEND = BB->end(); I != IEND; ++I){
	  Instruction *first = nullptr;
	  if(CallInst *CI = dyn_cast<CallInst>(I)){
	    Value *cv = CI->getCalledValue();
	    if(dyn_cast<InlineAsm>(cv)){
	      errs() << "InlineASM!\n";
	      CallInst::Create(BeginFunction,"",I);
	      CallInst::Create(EndFunction,"",++I);
	    }
	  }
	}
      }
      
      return true;
    }
  };
}

char TMI::ID = 0;
static RegisterPass<TMI> X("tmi", "TMI Assembly Pass");
