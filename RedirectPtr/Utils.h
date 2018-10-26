#include <utility>

//
// Created by yifanz on 6/27/18.
//

#ifndef LLVM_UTILS_H
#define LLVM_UTILS_H

#include "llvm/IR/Instruction.h"
#include "llvm/IR/IRBuilder.h"

#include <cassert>
#include <set>
#include <unordered_map>

using namespace llvm;

#ifndef LLVM_DEBUG
#define LLVM_DEBUG(X) {X;}
#endif

struct MallocInfo {
    long sizeDelta{};
    std::vector<size_t> remaps;

    MallocInfo(long sizeDelta, std::vector<size_t> &&remaps):
        sizeDelta(sizeDelta), remaps(move(remaps)) {}

    MallocInfo() = default;
};

class PCInfo {
public:
    explicit PCInfo(const std::vector<std::tuple<size_t, size_t, size_t>> &lines)
            : isRedirect(true) {
        for (const auto &tup : lines) {
            size_t tid, from, to;
            std::tie(tid, from, to) = tup;
            allRedirects[tid].emplace_back(from, to);
        }
        for (auto &p: allRedirects)
            std::sort(p.second.begin(), p.second.end());
    }

    explicit PCInfo(long mallocSizeDelta, std::vector<size_t> &&mallocRemaps) :
            malloc(mallocSizeDelta, move(mallocRemaps)), isRedirect(false) {}

    PCInfo() = default;

    std::set<size_t> getThreads() const {
        if (isRedirect) {
            std::set<size_t> threads;
            for (const auto &p: allRedirects)
                threads.insert(p.first);
            return threads;
        } else return std::set<size_t>();
    }

    bool isCorrectInst(Instruction *inst) const {
        if (isRedirect) {
            return isa<LoadInst>(inst) || isa<StoreInst>(inst) ||
                   isa<AtomicRMWInst>(inst) || isa<AtomicCmpXchgInst>(inst);
        } else {
            // allocs are no-throw and won't be invoked.
            auto *ci = dyn_cast<CallInst>(inst);
            if (!ci)
                return false;
            StringRef name = ci->getCalledFunction()->getName();
            return name == "malloc" || name == "calloc" || name == "realloc";
        }
    }

    bool getThreaded(size_t tid, MallocInfo &mloc,
                     std::vector<std::pair<size_t, size_t>> &re) const {
        if (isRedirect) {
            auto it = allRedirects.find(tid);
            if (it != allRedirects.end())
                re = it->second;
            else
                re.clear();
        } else
            mloc = malloc;
        return isRedirect;
    }

private:
    std::unordered_map<
            size_t,
            std::vector<std::pair<size_t, size_t>>
    > allRedirects;
    MallocInfo malloc;
    bool isRedirect;
};

class ThreadedPCInfo {
public:
    explicit ThreadedPCInfo(const PCInfo &pc, size_t tid) {
        bool isRedirect = pc.getThreaded(tid, malloc, redirects);
        triSwitch = (uint8_t) (isRedirect ? 0 : 2);
    }

    explicit ThreadedPCInfo(std::vector<Function *> dupFuncs) :
            dupFuncs(std::move(dupFuncs)), triSwitch(1) {}

    ThreadedPCInfo() = default;

    size_t getSize() const {
        switch (triSwitch) {
            case 0:
                return redirects.size();
            case 1:
                return dupFuncs.size();
            case 2:
                return 1;
            default:
                assert(false);
        }
    }

    uint8_t getLoopWise(size_t loopid,
                        std::pair<size_t, size_t> &redirect,
                        Function *&callee, MallocInfo &mloc) const {
        switch (triSwitch) {
            case 0:
                assert(redirects.size() > loopid);
                redirect = redirects[loopid];
                break;
            case 1:
                assert(dupFuncs.size() > loopid);
                callee = dupFuncs[loopid];
                break;
            case 2:
                mloc = malloc;
                break;
            default:
                assert(false);
        }
        return triSwitch;
    }

private:
    std::vector<std::pair<size_t, size_t>> redirects{};
    std::vector<Function *> dupFuncs{};
    MallocInfo malloc{};
    uint8_t triSwitch{};
};

class ExpandedPCInfo {
public:
    ExpandedPCInfo() = default;

    explicit ExpandedPCInfo(const ThreadedPCInfo &info, size_t loopid) {
        triSwitch = info.getLoopWise(loopid, redirect, callee, malloc);
    }

    template<typename Func1, typename Func2, typename Func3>
    void actOn(Func1 f1, Func2 f2, Func3 f3) const {
        switch (triSwitch) {
            case 0:
                f1(redirect);
                return;
            case 1:
                f2(callee);
                return;
            case 2:
                f3(malloc);
                return;
            default:
                assert(false);
        }
    }

private:
    std::pair<size_t, size_t> redirect;
    Function *callee{};
    MallocInfo malloc{};
    uint8_t triSwitch{};
};

typedef std::unordered_map<Instruction *, PCInfo> PreCloneT;
typedef std::unordered_map<Instruction *, ThreadedPCInfo> PostCloneT;
typedef std::unordered_map<Instruction *, ExpandedPCInfo> PostUnrollT;

unsigned int getPointerOperandIndex(Instruction *inst, bool &isWrite) {
    if (isa<LoadInst>(inst)) {
        isWrite = false;
        return LoadInst::getPointerOperandIndex();
    }
    if (isa<StoreInst>(inst)) {
        isWrite = true;
        return StoreInst::getPointerOperandIndex();
    }
    if (isa<AtomicRMWInst>(inst)) {
        isWrite = true;
        return AtomicRMWInst::getPointerOperandIndex();
    }
    if (isa<AtomicCmpXchgInst>(inst)) {
        isWrite = true;
        return AtomicCmpXchgInst::getPointerOperandIndex();
    }
    errs() << "Instruction is not load/store!";
    assert(false);
}

unsigned int getPointerOperandIndex(Instruction *inst) {
    bool _dummy;
    return getPointerOperandIndex(inst, _dummy);
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
