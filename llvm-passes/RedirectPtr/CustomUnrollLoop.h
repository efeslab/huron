#ifndef CUSTOMLOOPUNROLL_H
#define CUSTOMLOOPUNROLL_H

#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/Analysis/OptimizationRemarkEmitter.h"
#include "llvm/Analysis/ScalarEvolution.h"

#include <unordered_map>

/// Represents the result of a \c UnrollLoop invocation.
enum class CustomLoopUnrollResult {
    /// The loop was not modified.
    Unmodified,

    /// The loop was partially unrolled -- we still have a loop, but with a
    /// smaller trip count.  We may also have emitted epilogue loop if the loop
    /// had a non-constant trip count.
    PartiallyUnrolled,

    /// The loop was fully unrolled into straight-line code.  We no longer have
    /// any back-edges.
    FullyUnrolled
};

CustomLoopUnrollResult CustomUnrollLoop(
    llvm::Loop *L, unsigned Count, unsigned TripCount, bool Force,
    bool AllowRuntime, bool AllowExpensiveTripCount, bool PreserveCondBr,
    bool PreserveOnlyFirst, unsigned TripMultiple, unsigned PeelCount,
    bool UnrollRemainder, llvm::LoopInfo *LI, llvm::ScalarEvolution *SE,
    llvm::DominatorTree *DT, llvm::AssumptionCache *AC,
    llvm::OptimizationRemarkEmitter *ORE, bool PreserveLCSSA,
    std::unordered_map<llvm::Instruction *, std::vector<llvm::Instruction *>>
        &instHooks);

CustomLoopUnrollResult DefaultUnrollLoop(
    llvm::Loop *L, unsigned Count, unsigned TripCount, llvm::LoopInfo *LI,
    llvm::AssumptionCache *AC,
    std::unordered_map<llvm::Instruction *, std::vector<llvm::Instruction *>>
        &instHooks);

#endif
