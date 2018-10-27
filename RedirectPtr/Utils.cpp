//
// Created by yifanz on 10/26/18.
//

#include "Utils.h"

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

PCInfo::PCInfo(const std::vector<std::tuple<size_t, size_t, size_t>> &lines)
        : isRedirect(true) {
    for (const auto &tup : lines) {
        size_t tid, from, to;
        std::tie(tid, from, to) = tup;
        allRedirects[tid].emplace_back(from, to);
    }
    for (auto &p: allRedirects)
        std::sort(p.second.begin(), p.second.end());
}

PCInfo::PCInfo(size_t id, long mallocSizeDelta): malloc(id, mallocSizeDelta), isRedirect(false) {}

std::set<size_t> PCInfo::getThreads() const {
    if (isRedirect) {
        std::set<size_t> threads;
        for (const auto &p: allRedirects)
            threads.insert(p.first);
        return threads;
    } else return std::set<size_t>();
}

bool PCInfo::isCorrectInst(Instruction *inst) const {
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

bool PCInfo::getThreaded(size_t tid, MallocInfo &mloc, std::vector<std::pair<size_t, size_t>> &re) const {
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

ThreadedPCInfo::ThreadedPCInfo(const PCInfo &pc, size_t tid) {
    bool isRedirect = pc.getThreaded(tid, malloc, redirects);
    triSwitch = (uint8_t) (isRedirect ? 0 : 2);
}

ThreadedPCInfo::ThreadedPCInfo(std::vector<Function *> dupFuncs) :
        dupFuncs(std::move(dupFuncs)), triSwitch(1) {}

size_t ThreadedPCInfo::getSize() const {
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

uint8_t ThreadedPCInfo::getLoopWise(size_t loopid, std::pair<size_t, size_t> &redirect, Function *&callee,
                                    MallocInfo &mloc) const {
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

ExpandedPCInfo::ExpandedPCInfo(const ThreadedPCInfo &info, size_t loopid) {
    triSwitch = info.getLoopWise(loopid, redirect, callee, malloc);
}
