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
    size_t id{};
    long sizeDelta{};

    MallocInfo(size_t id, long sizeDelta): id(id), sizeDelta(sizeDelta) {}

    MallocInfo() = default;
};

class PCInfo {
public:
    explicit PCInfo(const std::vector<std::tuple<size_t, size_t, size_t>> &lines);

    explicit PCInfo(size_t id, long mallocSizeDelta);

    PCInfo() = default;

    std::set<size_t> getThreads() const;

    bool isCorrectInst(Instruction *inst) const;

    bool getThreaded(size_t tid, MallocInfo &mloc,
                     std::vector<std::pair<size_t, size_t>> &re) const;

private:
    std::unordered_map<size_t, std::vector<std::pair<size_t, size_t>>> allRedirects;
    MallocInfo malloc;
    bool isRedirect;
};

class ThreadedPCInfo {
public:
    explicit ThreadedPCInfo(const PCInfo &pc, size_t tid);

    explicit ThreadedPCInfo(std::vector<Function *> dupFuncs);

    ThreadedPCInfo() = default;

    size_t getSize() const;

    uint8_t getLoopWise(size_t loopid,
                        std::pair<size_t, size_t> &redirect,
                        Function *&callee, MallocInfo &mloc) const;

private:
    std::vector<std::pair<size_t, size_t>> redirects{};
    std::vector<Function *> dupFuncs{};
    MallocInfo malloc{};
    uint8_t triSwitch{};
};

class ExpandedPCInfo {
public:
    ExpandedPCInfo() = default;

    explicit ExpandedPCInfo(const ThreadedPCInfo &info, size_t loopid);

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

unsigned int getPointerOperandIndex(Instruction *inst, bool &isWrite);

unsigned int getPointerOperandIndex(Instruction *inst);

namespace std {
    template<typename T1, typename T2>
    struct hash<std::pair<T1, T2>> {
        std::size_t operator()(const std::pair<T1, T2> &x) const {
            return std::hash<T1>()(x.first) ^ std::hash<T2>()(x.second);
        }
    };
}  // namespace std

#endif
