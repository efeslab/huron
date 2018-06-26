#define DEBUG_TYPE "redirectptr"

#include "llvm-c/Initialization.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/DataTypes.h"
#include "llvm/Support/Debug.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"
#include "llvm/Transforms/Utils/Cloning.h"

#include <algorithm>
#include <fstream>
#include <set>
#include <string>
#include <sstream>
#include <unordered_map>

using namespace llvm;

struct RedirectInfo {
    /* Profile info at one program location (determined by (func, inst)) */
    std::vector<std::vector<std::pair<size_t, size_t>>> threadedFromTo;
    RedirectInfo(const std::vector<std::tuple<size_t, size_t, size_t>> &lines) {
        for (const auto &tup: lines) {
            size_t tid, from, to;
            std::tie(tid, from, to) = tup;
            if (threadedFromTo.size() <= tid)
                threadedFromTo.resize(tid + 1);
            threadedFromTo[tid].emplace_back(from, to);
        }
    }

    RedirectInfo() = default;

    std::set<size_t> getThreads() const {
        std::set<size_t> threads;
        for (size_t i = 0; i < threadedFromTo.size(); i++)
            if (!threadedFromTo[i].empty())
                threads.insert(i);
        return threads;
    }
};

struct SingleThreadRedirInfo {
    /* Profile info at one PC by one thread. */
    std::vector<std::pair<size_t, size_t>> offsets;

    SingleThreadRedirInfo(const RedirectInfo &pci, size_t tid) {
        assert(pci.threadedFromTo.size() > tid);
        offsets = pci.threadedFromTo[tid];
        assert(!offsets.empty());
    }

    bool needUnrolling() const {
        return offsets.size() != 1;
    }

    long long getUniqueOffset() const {
        assert(offsets.size() == 1);
        size_t from, to;
        std::tie(from, to) = offsets[0];
        return (long long)to - (long long)from;
    }
};

struct PCInfo {
    RedirectInfo ri;
    size_t malloc;
    bool isRedir;

    PCInfo(const std::vector<std::tuple<size_t, size_t, size_t>> &lines): ri(lines), isRedir(true) {}

    PCInfo(size_t mallocSizeAdd): malloc(mallocSizeAdd), isRedir(false) {}

    PCInfo(): malloc(), isRedir() {}

    std::set<size_t> getThreads() const {
        return isRedir ? ri.getThreads() : std::set<size_t>();
    }

    bool isCorrectInst(Instruction *inst) {
        if (isRedir)
            return isa<LoadInst>(inst) || isa<StoreInst>(inst);
        else {
            StringRef name = cast<CallInst>(inst)->getCalledFunction()->getName();
            return name == "malloc" || name == "calloc" || name == "realloc";
        }
    }
};

namespace std {
    template<typename T1, typename T2>
    struct hash<std::pair<T1, T2>> {
        std::size_t operator()(const std::pair<T1, T2> &x) const {
            return std::hash<T1>()(x.first) ^ std::hash<T2>()(x.second);
        }
    };
}

namespace {

struct RedirectPtr : public ModulePass {
    RedirectPtr();
    virtual StringRef getPassName() const;

    bool runOnModule(Module &M);
    virtual bool doInitialization(Module &M);
    static char ID; // Pass identification, replacement for typeid

private:
    // Data exchange format between some functions.
    typedef std::unordered_map<size_t, PCInfo> InstInfoT;

    std::unordered_map<size_t, Instruction*> batchGetInstFromFunc(
        Function *func, const InstInfoT &instInfos);
    void duplicateFuncIfNeeded(Function *func, const InstInfoT &instInfos);
    void loadProfile();
    void loadMallocInfo(std::istream &is);
    void loadLocInfo(std::istream &is);
    void buildThreadExpandedTables(size_t threadId, Function *newFunc, const InstInfoT &instInfos);
    void offsetUnrollInst(Instruction *inst, const SingleThreadRedirInfo &info);
    void adjustMallocSize(Instruction *inst, size_t extraSize);

    LLVMContext *context;
    Type *intPtrType, *sizeType;
    // Table read and parsed from input profile file.
    std::unordered_map<std::pair<size_t, size_t>, PCInfo> profileTable;
    // `instruction to its info` table. 
    // Use pointer instead of id since some functions have been copied, and index is invalid now.
    std::unordered_map<Instruction*, SingleThreadRedirInfo> redirInfo;
    std::unordered_map<Instruction*, size_t> mallocInfo;
    // `function to its copied version and thread ids` table.
    std::unordered_map<Function*, std::vector<std::pair<Function*, size_t>>> duplicatedFunctions;
};

} // namespace

char RedirectPtr::ID = 0;
static RegisterPass<RedirectPtr> redirectptr(
    "redirectptr", "Redirect load/stores according to a profile", false, false
);
static cl::opt<std::string> locfile(
    "locfile", cl::desc("Specify profile path"), cl::value_desc("filename"), cl::Required
);

void RedirectPtr::loadProfile() {
    dbgs() << "Loading from file: " << locfile << '\n';
    std::ifstream fin(locfile.c_str());
    loadMallocInfo(fin);
    loadLocInfo(fin);
}

void RedirectPtr::loadMallocInfo(std::istream &is) {
    size_t n;
    is >> n;
    for (size_t i = 0; i < n; i++) {
        size_t func, inst, from, to;
        is >> func >> inst >> from >> to;
        auto key = std::make_pair(func, inst);
        assert(to >= from);
        if (to == from)
            continue;  // no change needed for this malloc
        PCInfo value(to - from);
        profileTable.emplace(key, std::move(value));
    }
}

void RedirectPtr::loadLocInfo(std::istream &is) {
    size_t func, inst, n;
    while (is >> func >> inst >> n) {
        std::vector<std::tuple<size_t, size_t, size_t>> pc_lines;
        for (size_t i = 0; i < n; i++) {
            size_t tid, original, modified;
            is >> tid >> original >> modified;
            pc_lines.emplace_back(tid, original, modified);
        }
        auto key = std::make_pair(func, inst);
        PCInfo value(pc_lines);
        profileTable.emplace(key, std::move(value));
    }
    // Print what was just read in to verify correctness.
    // for (const auto &p: profileTable) {
    //     dbgs() << p.first.first << ' ' << p.first.second << '\n';
    //     auto &vv = p.second.threadedFromTo;
    //     for (size_t i = 0; i < vv.size(); i++) {
    //         dbgs() << i << ": ";
    //         for (const auto &ll: vv[i])
    //             dbgs() << ll << ' ';
    //         dbgs() << '\n';
    //     }
    //     dbgs() << '\n';
    // }
}

RedirectPtr::RedirectPtr() : ModulePass(ID) {}

StringRef RedirectPtr::getPassName() const { return "RedirectPtr"; }

// virtual: define some initialization for the whole module
bool RedirectPtr::doInitialization(Module &M) {
    DataLayout *layout = new DataLayout(&M);
    context = &(M.getContext());
    unsigned int ptrSize = layout->getPointerSizeInBits();
    sizeType = intPtrType = Type::getIntNTy(*context, ptrSize);
    loadProfile();
    return true;
}

std::unordered_map<size_t, Instruction*> RedirectPtr::batchGetInstFromFunc(
    Function *func, const InstInfoT &instInfos
) {
    std::unordered_map<size_t, Instruction*> ret;
    size_t instCounter = 0;
    for (Function::iterator bb = func->begin(), be = func->end(); bb != be; ++bb)
        for (BasicBlock::iterator ins = bb->begin(), instE = bb->end(); ins != instE;
            ++ins, ++instCounter)
            if (instInfos.find(instCounter) != instInfos.end())
                ret.emplace(instCounter, &*ins);
    return ret;
}

void RedirectPtr::buildThreadExpandedTables(
    size_t threadId, Function *newFunc, const InstInfoT &instInfos
) {
    // Add all instructions (redirected load/store or malloc)
    // and their infos in `newFunc` into `redirsInfo.
    // Steps:
    // 1. Find "the same instruction in the cloned function as the one in the old function"
    //    (we do this in a batch in order to traverse the newFunc only once)
    // 2. Extract SingleThreadRedirInfo from RedirectInfo by providing threadId.
    // 3. With function ptr, inst ptr, info at hand, emplace them in the table.
    auto instPtrs = batchGetInstFromFunc(newFunc, instInfos);
    for (const auto &instInfoP: instInfos) {
        size_t instId = instInfoP.first;
        Instruction *inst = instPtrs[instId];
        if (instInfoP.second.isRedir) {
            SingleThreadRedirInfo instThreadInfo(instInfoP.second.ri, threadId);
            redirInfo.emplace(inst, instThreadInfo);
        }
        else {
            mallocInfo.emplace(inst, instInfoP.second.malloc);
        }
    }
}

void RedirectPtr::duplicateFuncIfNeeded(Function *func, const InstInfoT &instInfos) {
    // Get thread usage for each function, 
    // combined from all instructions in it.
    // If used by more than 1 threads, provide copies.
    std::set<size_t> funcUserThreads;
    for (const auto &instInfoP: instInfos) {
        std::set<size_t> threads = instInfoP.second.getThreads();
        funcUserThreads.insert(threads.begin(), threads.end());
    }
    if (funcUserThreads.size() != 1) {
        for (size_t tid: funcUserThreads) {
            // name: `func` -> `func_1` (thread 1 version)
            std::string newName = func->getName().str() + "_" + std::to_string(tid);
            ValueToValueMapTy map;
            Function *newFunc = CloneFunction(func, map);
            newFunc->setName(newName);
            duplicatedFunctions[func].emplace_back(newFunc, tid);
            buildThreadExpandedTables(tid, newFunc, instInfos);
        }
    }
    else
        buildThreadExpandedTables(*funcUserThreads.begin(), func, instInfos);
}

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

void RedirectPtr::offsetUnrollInst(Instruction *inst, const SingleThreadRedirInfo &info) {
    if (info.needUnrolling())
        return;  // TODO
    long long offset = info.getUniqueOffset();
    unsigned int index = getPointerOperandIndex(inst);
    Value *pointer = inst->getOperand(index);
    Type *origPtrTy = pointer->getType();
    IRBuilder<> IRB(inst);
    // Add offset to uint64_t value of the pointer using an `add` instruction.
    Constant *offsetValue = ConstantInt::get(intPtrType, offset, /*isSigned=*/true);
    Value *actualAddrInt = IRB.CreatePointerCast(pointer, intPtrType);
    Value *redirectAddrInt = IRB.CreateAdd(actualAddrInt, offsetValue);
    Value *redirectPtr = IRB.CreateIntToPtr(redirectAddrInt, origPtrTy);  // sustain original type.
    inst->setOperand(index, redirectPtr);
}

void RedirectPtr::adjustMallocSize(Instruction *inst, size_t extraSize) {
    CallInst *call = cast<CallInst>(inst);
    Value *origSize = call->getArgOperand(0);  // size is always 0th argument in allocs.
    IRBuilder<> IRB(inst);
    Constant *addValue = ConstantInt::get(sizeType, extraSize, /*isSigned=*/false);
    Value *newSize = IRB.CreateAdd(origSize, addValue);
    call->setArgOperand(0, newSize);
}

bool RedirectPtr::runOnModule(Module &M) {
    size_t funcCounter = 0;
    for (Module::iterator fb = M.begin(), fe = M.end(); fb != fe; ++fb, ++funcCounter) {
        size_t instCounter = 0;
        InstInfoT instInfos;
        for (Function::iterator bb = fb->begin(), be = fb->end(); bb != be; ++bb) {
            for (BasicBlock::iterator ins = bb->begin(), instE = bb->end(); ins != instE;
                ++ins, ++instCounter) {
                auto thisLoc = std::make_pair(funcCounter, instCounter);
                auto it = profileTable.find(thisLoc);
                if (it == profileTable.end())
                    continue;
                assert(it->second.isCorrectInst(&*ins));
                instInfos[instCounter] = it->second;
            }
        }
        this->duplicateFuncIfNeeded(&*fb, instInfos);
    }
    for (const auto &instInfoP: redirInfo)
        this->offsetUnrollInst(instInfoP.first, instInfoP.second);
    for (const auto &instInfoP: mallocInfo)
        this->adjustMallocSize(instInfoP.first, instInfoP.second);
    return true;
}
