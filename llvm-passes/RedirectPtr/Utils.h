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

class PCInfo;
class ThreadedPCInfo;

class ExpandedPCInfo {
public:
    ExpandedPCInfo() = default;

    template<typename Func1, typename Func2, typename Func3, typename Func4>
    void actOn(Func1 f1, Func2 f2, Func3 f3, Func4 f4) const {
        switch (which) {
            case 0:
                f1(redirect);
                return;
            case 1:
                f2(malloc);
                return;
            case 2:
                f3(depAllocId);
                return;
            case 3:
                f4(callee);
                return;
            default:
                assert(false);
        }
    }

private:
    friend ThreadedPCInfo;

    std::pair<size_t, size_t> redirect;
    MallocInfo malloc{};
    size_t depAllocId{};
    Function *callee{};
    uint8_t which{};
};

class ThreadedPCInfo {
public:
    explicit ThreadedPCInfo(std::vector<Function *> dupFuncs);

    ThreadedPCInfo() = default;

    size_t getSize() const;

    ExpandedPCInfo operator[](size_t loopid) const;

private:
    friend PCInfo;

    std::vector<std::pair<size_t, size_t>> redirects{};
    MallocInfo malloc{};
    size_t depAllocId{};
    std::vector<Function *> dupFuncs{};
    uint8_t which{};
};

class PCInfo {
public:
    explicit PCInfo(const std::vector<std::tuple<size_t, size_t, size_t>> &lines);

    explicit PCInfo(size_t id, long mallocSizeDelta);

    explicit PCInfo(size_t depId);

    PCInfo() = default;

    std::set<size_t> getThreads() const;

    bool isCorrectInst(Instruction *inst) const;

    ThreadedPCInfo operator[](size_t tid) const;

private:
    std::unordered_map<size_t, std::vector<std::pair<size_t, size_t>>> allRedirects;
    MallocInfo malloc{};
    size_t depAllocId{};
    uint8_t which{};
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
