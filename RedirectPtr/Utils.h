//
// Created by yifanz on 6/27/18.
//

#ifndef LLVM_UTILS_H
#define LLVM_UTILS_H

#include "llvm/IR/Instruction.h"
#include <cassert>
#include <llvm/IR/IRBuilder.h>

using namespace llvm;

struct PCInfo {
private:
    struct Redirect {
        /* Profile info at one program location (determined by (func, inst)) */
        std::vector<std::vector<std::pair<size_t, size_t>>> allRedirects;

        explicit Redirect(const std::vector<std::tuple<size_t, size_t, size_t>> &lines) {
            for (const auto &tup : lines) {
                size_t tid, from, to;
                std::tie(tid, from, to) = tup;
                if (allRedirects.size() <= tid) allRedirects.resize(tid + 1);
                allRedirects[tid].emplace_back(from, to);
            }
            for (auto &vec: allRedirects)
                std::sort(vec.begin(), vec.end());
        }

        Redirect() = default;

        std::set<size_t> getThreads() const {
            std::set<size_t> threads;
            for (size_t i = 0; i < allRedirects.size(); i++)
                if (!allRedirects[i].empty()) threads.insert(i);
            return threads;
        }
    };

public:
    explicit PCInfo(const std::vector<std::tuple<size_t, size_t, size_t>> &lines)
            : ri(new Redirect(lines)) {}

    explicit PCInfo(size_t mallocSizeAdd) : malloc(new size_t(mallocSizeAdd)) {}

    PCInfo() = default;

    std::set<size_t> getThreads() const {
        return ri ? ri->getThreads() : std::set<size_t>();
    }

    size_t getSize(size_t tid) const {
        if (ri) {
            assert(ri->allRedirects.size() > tid);
            return ri->allRedirects[tid].size();
        } else
            return 1;
    }

    bool isCorrectInst(Instruction *inst) {
        if (malloc) {
            StringRef name =
                    cast<CallInst>(inst)->getCalledFunction()->getName();
            return name == "malloc" || name == "calloc" || name == "realloc";
        } else
            return isa<LoadInst>(inst) || isa<StoreInst>(inst);
    }

    Redirect *ri{};
    size_t *malloc{};
};

struct ThreadedPCInfo {
    const PCInfo *pc{};
    size_t tid{};
    std::vector<Function *> *dupFuncs{};

    explicit ThreadedPCInfo(PCInfo *pc, size_t tid) : pc(pc), tid(tid) {}

    explicit ThreadedPCInfo(const std::vector<Function *> &dupFuncs) :
            dupFuncs(new std::vector<Function *>(dupFuncs)) {}

    ThreadedPCInfo() = default;

    size_t getSize() const {
        return pc ? pc->getSize(tid) : dupFuncs->size();
    }
};

struct ExpandedPCInfo {
    const ThreadedPCInfo *tpc;
    size_t loopid;

    ExpandedPCInfo() = default;

    explicit ExpandedPCInfo(const ThreadedPCInfo &info, size_t loopid) :
            tpc(&info), loopid(loopid) {}
};

struct ActionType {
    enum Action { changeMalloc, changeOffset, changeCallee };
    size_t malloc;
    long offset;
    Function *callee;
    Action action;

    explicit ActionType(const ExpandedPCInfo &info) {
        const PCInfo *pc = info.tpc->pc;
        if (pc) {
            if (pc->ri) {
                size_t from, to;
                std::tie(from, to) = pc->ri->allRedirects[info.tpc->tid][info.loopid];
                offset = (long)to - (long)from;
                action = changeOffset;
            }
            else
                malloc = *pc->malloc, action = changeMalloc;
        }
        else {
            auto *funcsVec = info.tpc->dupFuncs;
            assert(funcsVec->size() > info.loopid);
            callee = (*funcsVec)[info.loopid];
            action = changeCallee;
        }
    }
};

typedef std::unordered_map<Instruction *, PCInfo *> PreCloneT;
typedef std::unordered_map<Instruction *, ThreadedPCInfo> PostCloneT;
typedef std::unordered_map<Instruction *, ExpandedPCInfo> PostUnrollT;

unsigned int getPointerOperandIndex(Instruction *inst) {
    if (LoadInst *LI = dyn_cast<LoadInst>(inst)) {
        // *isWrite = false;
        return LI->getPointerOperandIndex();
    }
    if (StoreInst *SI = dyn_cast<StoreInst>(inst)) {
        // *isWrite = true;
        return SI->getPointerOperandIndex();
    }
    if (AtomicRMWInst *RMW = dyn_cast<AtomicRMWInst>(inst)) {
        // *isWrite = true;
        return RMW->getPointerOperandIndex();
    }
    if (AtomicCmpXchgInst *XCHG = dyn_cast<AtomicCmpXchgInst>(inst)) {
        // *isWrite = true;
        return XCHG->getPointerOperandIndex();
    }
    errs() << "Instruction is not load/store!";
    assert(false);
}

CallInst *getCallToPThread(Function *orig) {
    std::vector<CallInst *> ret;
    for (User *user : orig->users()) {
        CallInst *call = cast<CallInst>(user);
        if (!call) continue;
        StringRef name = call->getCalledFunction()->getName();
        assert(name == "pthread_create");
        ret.push_back(call);
    }
    assert(ret.size() == 1);
    return ret[0];
}

namespace std {
    template<typename T1, typename T2>
    struct hash<std::pair<T1, T2>> {
        std::size_t operator()(const std::pair<T1, T2> &x) const {
            return std::hash<T1>()(x.first) ^ std::hash<T2>()(x.second);
        }
    };
}  // namespace std

#endif
