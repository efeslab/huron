//
// Created by yifanz on 6/27/18.
//

#ifndef LLVM_UNROLLLOOPPASS_H
#define LLVM_UNROLLLOOPPASS_H

#include "llvm/Transforms/Utils/LoopRotationUtils.h"
#include "llvm/Analysis/InstructionSimplify.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "Utils.h"
#include "CustomUnrollLoop.h"

class UnrollLoopPass {
public:
    explicit UnrollLoopPass(DataLayout *layout, Function *parent, LoopInfo *li)
            : layout(layout), parent(parent), li(li) {}

    PostUnrollT runOnInstGroup(const PostCloneT &insts) {
        Loop *loop = inferLoop(insts);
        size_t count = getLoopUnrollCount(insts);
        dbgs() << "unrolling the following loop for " << count << " times: \n";
        loop->dump();
        for (const auto &p: insts)
            hook.emplace(p.first, std::vector<Instruction *>());
        unrollLoop(insts, loop, count);
        // After unrolling the original instruction should exist;
        // they do n-1 copies and reuse the original one.
        // Thus we'll add all of them into the table of offset task.
        return storeHookInfoToTable(insts);
    }

private:
    void unrollLoop(const PostCloneT &insts, Loop *loop, size_t count) {
        // First "rotate" the loop so that unrolling could work.
        /* Loop *L, LoopInfo *LI, const TargetTransformInfo *TTI,
           AssumptionCache *AC, DominatorTree *DT, ScalarEvolution *SE, const
           SimplifyQuery &SQ, bool RotationOnly = true, unsigned Threshold =
           unsigned(-1), bool IsUtilMode = true */
        AssumptionCache cache(*parent);
        SimplifyQuery query(*layout);
        TargetTransformInfo transinfo(*layout);
        bool rotated = LoopRotation(loop, li, &transinfo, &cache, nullptr,
                                    nullptr, query, true, unsigned(-1), true);
        assert(rotated);
        // Subscribe to instuction clone information for instructions of interest,
        // and then do the unrolling.
        CustomLoopUnrollResult result = DefaultUnrollLoop(loop, count, count, li, &cache, hook);
        assert(result == CustomLoopUnrollResult::FullyUnrolled);
    }

    Loop *inferLoop(const PostCloneT &insts) const {
        assert(!insts.empty());
        BasicBlock *sampleBB = insts.begin()->first->getParent();
        return li->getLoopFor(sampleBB);
    }

    size_t getLoopUnrollCount(const PostCloneT &insts) const {
        bool compare = false;
        size_t size;
        for (const auto &instP : insts) {
            size_t nextSize = instP.second.getSize();
            if (!compare) {
                size = nextSize;
                compare = true;
            } else
                assert(size == nextSize);
        }
        return size;
    }

    PostUnrollT storeHookInfoToTable(const PostCloneT &insts) {
        PostUnrollT instOps;
        for (const auto &instP : hook) {
            auto it = insts.find(instP.first);
            assert(it != insts.end());
            instOps[instP.first] = ExpandedPCInfo(it->second, 0);
            for (size_t i = 0; i < instP.second.size(); i++)
                instOps[instP.second[i]] = ExpandedPCInfo(it->second, i + 1);
        }
        return instOps;
    }

    DataLayout *layout;
    Function *parent;
    LoopInfo *li;
    std::unordered_map<Instruction *, std::vector<Instruction *>> hook;
};

#endif //LLVM_UNROLLLOOPPASS_H
