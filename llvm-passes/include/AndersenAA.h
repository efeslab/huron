#ifndef TCFS_ANDERSEN_AA_H
#define TCFS_ANDERSEN_AA_H

#include "Andersen.h"

#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Pass.h"

class AndersenAAResult : public llvm::AAResultBase<AndersenAAResult> {
private:
  friend llvm::AAResultBase<AndersenAAResult>;

  Andersen anders;
  llvm::AliasResult andersenAlias(const llvm::Value *, const llvm::Value *);

public:
  AndersenAAResult(const llvm::Module &);
  llvm::AliasResult publicAlias(const llvm::Value *, const llvm::Value *);
  llvm::AliasResult alias(const llvm::MemoryLocation &,
                          const llvm::MemoryLocation &);
  bool pointsToConstantMemory(const llvm::MemoryLocation &, bool);
};

#endif
