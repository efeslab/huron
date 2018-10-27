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
    if (isa<GetElementPtrInst>(inst)) {
        isWrite = false;
        return GetElementPtrInst::getPointerOperandIndex();
    }
    errs() << "Instruction is not pointer operation!";
    assert(false);
}

unsigned int getPointerOperandIndex(Instruction *inst) {
    bool _dummy;
    return getPointerOperandIndex(inst, _dummy);
}

PCInfo::PCInfo(const std::vector<std::tuple<size_t, size_t, size_t>> &lines)
        : which(0) {
    for (const auto &tup : lines) {
        size_t tid, from, to;
        std::tie(tid, from, to) = tup;
        allRedirects[tid].emplace_back(from, to);
    }
    for (auto &p: allRedirects)
        std::sort(p.second.begin(), p.second.end());
}

PCInfo::PCInfo(size_t id, long mallocSizeDelta): malloc(id, mallocSizeDelta), which(1) {}

PCInfo::PCInfo(size_t depId): depAllocId(depId), which(2) {}

std::set<size_t> PCInfo::getThreads() const {
    if (which == 0) {
        std::set<size_t> threads;
        for (const auto &p: allRedirects)
            threads.insert(p.first);
        return threads;
    } else return std::set<size_t>();
}

bool PCInfo::isCorrectInst(Instruction *inst) const {
    if (which == 0) {
        return isa<LoadInst>(inst) || isa<StoreInst>(inst) ||
               isa<AtomicRMWInst>(inst) || isa<AtomicCmpXchgInst>(inst);
    } else if (which == 1) {
        // allocs are no-throw and won't be invoked.
        auto *ci = dyn_cast<CallInst>(inst);
        if (!ci)
            return false;
        StringRef name = ci->getCalledFunction()->getName();
        return name == "malloc" || name == "calloc" || name == "realloc";
    } else return true;
}

ThreadedPCInfo PCInfo::operator[](size_t tid) const {
    ThreadedPCInfo tpcInfo;
    if (which == 0) {
        auto it = allRedirects.find(tid);
        if (it != allRedirects.end())
            tpcInfo.redirects = it->second;
    } else if (which == 1) {
        tpcInfo.malloc = malloc;
    } else if (which == 2) {
        tpcInfo.depAllocId = depAllocId;
    }
    tpcInfo.which = which;
    return tpcInfo;
}

ThreadedPCInfo::ThreadedPCInfo(std::vector<Function *> dupFuncs) :
        dupFuncs(std::move(dupFuncs)), which(3) {}

size_t ThreadedPCInfo::getSize() const {
    switch (which) {
        case 0:
            return redirects.size();
        case 1:
            return 1;
        case 2:
            return 1;
        case 3:
            return dupFuncs.size();
        default:
            assert(false);
    }
}

ExpandedPCInfo ThreadedPCInfo::operator[](size_t loopid) const {
    ExpandedPCInfo epcInfo;
    switch (which) {
        case 0:
            assert(redirects.size() > loopid);
            epcInfo.redirect = redirects[loopid];
            break;
        case 1:
            epcInfo.malloc = malloc;
            break;
        case 2:
            epcInfo.depAllocId = depAllocId;
            break;
        case 3:
            assert(dupFuncs.size() > loopid);
            epcInfo.callee = dupFuncs[loopid];
            break;
        default:
            assert(false);
    }
    epcInfo.which = which;
    return epcInfo;
}
