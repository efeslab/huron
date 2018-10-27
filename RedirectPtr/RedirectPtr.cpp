#define DEBUG_TYPE "redirectptr"

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/IR/InstIterator.h"

#include <fstream>
#include <set>
#include <unordered_map>

#include "CallGraph.h"
#include "GroupFuncLoop.h"
#include "Utils.h"

using namespace llvm;

namespace {
    class RedirectPtr : public ModulePass {
    public:
        RedirectPtr();

        StringRef getPassName() const override;

        bool runOnModule(Module &M) override;

        bool doInitialization(Module &M) override;

        void getAnalysisUsage(AnalysisUsage &AU) const override;

        static char ID;  // Pass identification, replacement for typeid

    private:
        void loadProfile();

        void loadMallocInfo(std::istream &is);

        void loadLocInfo(std::istream &is);

        void replaceThreadedFuncCall(size_t tid, Function *func);

        void cloneFunc(Function *func, Instruction *pthread, const std::set<size_t> &threads);

        void makeMallocTable(Module &M);

        size_t mallocN{};
        CallGraphT cg{};
        std::vector<std::vector<size_t>> mallocOffsets{};
        // Table read and parsed from input profile file.
        std::unordered_map<std::pair<size_t, size_t>, PCInfo> profile{};
        // Profile with all instructions found and related to Function*.
        std::unordered_map<Function *, PreCloneT> preCloneProfile{};
        // Profile with offset replaced by pointers and PCInfo splitted (partially applied)
        std::unordered_map<Function *, PostCloneT> absPosProfile{};
        // All cloned functions.
        std::unordered_map<Function *, std::unordered_map<size_t, Function *>> clonedFuncs{};
        // Functions' thread signatures (unique since cloned).
        std::unordered_map<Function *, size_t> funcThread{};
    };

}  // namespace

char RedirectPtr::ID = 0;
static RegisterPass<RedirectPtr> redirectptr(
        "redirectptr", "Redirect load/stores according to a profile", false, false);
static cl::opt<std::string> locfile("locfile", cl::desc("Specify profile path"),
                                    cl::value_desc("filename"), cl::Required);

void RedirectPtr::loadProfile() {
    dbgs() << "Loading from file: " << locfile << "\n\n";
    std::ifstream fin(locfile.c_str());
    if (fin.fail()) {
        errs() << "Open file failed! Exiting.\n";
        exit(1);
    }
    loadMallocInfo(fin);
    loadLocInfo(fin);
}

void RedirectPtr::loadMallocInfo(std::istream &is) {
    is >> mallocN;
    for (size_t i = 0; i < mallocN; i++) {
        size_t func, inst, from, to;
        is >> func >> inst >> from >> to;
        std::vector<size_t> remaps;
        for (size_t j = 0; j < from; j++) {
            size_t next;
            is >> next;
            remaps.push_back(next);
        }
        auto key = std::make_pair(func, inst);
        if (to == from) continue;  // no change needed for this malloc
        mallocOffsets.push_back(remaps);
        PCInfo value(i, (long)to - (long)from, std::move(remaps));
        profile.emplace(key, std::move(value));
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
        profile.emplace(key, std::move(value));
    }
}

RedirectPtr::RedirectPtr() : ModulePass(ID) {}

StringRef RedirectPtr::getPassName() const { return "RedirectPtr"; }

// virtual: define some initialization for the whole module
bool RedirectPtr::doInitialization(Module &M) {
    loadProfile();
    return true;
}

void RedirectPtr::getAnalysisUsage(AnalysisUsage &AU) const {
    AU.addRequired<LoopInfoWrapperPass>();
}

PostCloneT getEquivalentInsts(size_t tid, const ValueToValueMapTy &map, const PreCloneT &oldInsts) {
    PostCloneT ret;
    for (const auto &p: oldInsts) {
        auto it = map.find(p.first);
        assert(it != map.end());
        Instruction *newInst = cast<Instruction>(it->second);
        assert(newInst);
        ret.emplace(newInst, ThreadedPCInfo(p.second, tid));
    }
    return ret;
}

void RedirectPtr::cloneFunc(Function *func, Instruction *pthread, const std::set<size_t> &threads) {
    std::vector<std::pair<Function *, size_t>> dupFuncs;
    auto it = this->preCloneProfile.find(func);
    for (size_t tid : threads) {
        // name: `func` -> `func_1` (thread 1 version)
        std::string newName = func->getName().str() + "_" + std::to_string(tid);
        dbgs() << func->getName() << " -> " << newName << '\n';
        ValueToValueMapTy map;
        Function *newFunc = CloneFunction(func, map);
        newFunc->setName(newName);
        dupFuncs.emplace_back(newFunc, tid);
        if (it != this->preCloneProfile.end())
            this->absPosProfile[newFunc] = getEquivalentInsts(tid, map, it->second);
    }
    if (pthread) {
        // Directly created by pthread_create.
        // Remember to replace the function name in pthread_create call,
        // potentially unrolling the loop around pthread_create.
        Function *pthreadInFunc = pthread->getParent()->getParent();
        std::vector<Function *> cloned;
        for (const auto &p: dupFuncs)
            cloned.push_back(p.first);
        this->absPosProfile[pthreadInFunc].emplace(pthread, ThreadedPCInfo(cloned));
    }
    for (const auto &p: dupFuncs) {
        this->clonedFuncs[func].emplace(p.second, p.first);
        this->funcThread[p.first] = p.second;
    }
}

void RedirectPtr::replaceThreadedFuncCall(size_t tid, Function *func) {
    dbgs() << "Replacing calls in threaded(" << tid << ") function " << func->getName() << '\n';
    auto findReplace = [tid, this](Function *callee) -> Function* {
        if (!callee)
            return nullptr;
        auto it = this->clonedFuncs.find(callee);
        if (it == this->clonedFuncs.end())
            return nullptr;
        const auto &clones = it->second;
        auto it2 = clones.find(tid);
        assert(it2 != clones.end());
        dbgs() << callee->getName() << "->" << it2->second->getName() << '\n';
        return it2->second;
    };
    for (auto ins = inst_begin(func), ie = inst_end(func); ins != ie; ++ins) {
        if (auto *call = dyn_cast<CallInst>(&*ins)) {
            Function *replaceCallee = findReplace(call->getCalledFunction());
            if (!replaceCallee)
                continue;
            call->setCalledFunction(replaceCallee);
        }
        else if (auto *invoke = dyn_cast<InvokeInst>(&*ins)) {
            Function *replaceCallee = findReplace(invoke->getCalledFunction());
            if (!replaceCallee)
                continue;
            invoke->setCalledFunction(replaceCallee);
        }
    }
}

template <typename CallInvoke>
inline Function *getThreadFuncFrom(CallInvoke *ci) {
    const static StringRef pthread = "pthread_create";
    Function *callee = ci->getCalledFunction();
    if (!callee)
        return nullptr;
    StringRef calledName = callee->getName();
    if (calledName == pthread)
        return cast<Function>(ci->getArgOperand(2));
    else
        return nullptr;
}

bool RedirectPtr::runOnModule(Module &M) {
    makeMallocTable(M);

    // Search for all pthread_create calls and create call graphs 
    // for threaded functions.
    std::unordered_map<Function *, Instruction *> startersPthreads;
    dbgs() << "Searching for pthread_create:\n";
    for (auto &fb : M)
        for (auto insb = inst_begin(&fb), inse = inst_end(&fb); insb != inse; ++insb) {
            Function *pthread_func = nullptr;
            if (auto *ci = dyn_cast<CallInst>(&*insb))
                pthread_func = getThreadFuncFrom(ci);
            else if (auto *ii = dyn_cast<InvokeInst>(&*insb))
                pthread_func = getThreadFuncFrom(ii);
            if (pthread_func) {
                dbgs() << "Found " << *insb << "\n";
                this->cg.addStartFunc(pthread_func);
                startersPthreads.emplace(pthread_func, &*insb);
            }
        }
    dbgs() << "\n";

    // Expand functions thread-wise
    // and use pointer to locate objects (instead of offset)
    size_t funcCounter = 0;
    dbgs() << "Searching for instructions:\n";
    for (auto fb = M.begin(), fe = M.end(); fb != fe; ++fb, ++funcCounter) {
        size_t instCounter = 0;
        PreCloneT instInfos;
        dbgs() << funcCounter << ' ' << fb->getName() << '\n';
        for (auto insb = inst_begin(&*fb), inse = inst_end(&*fb); 
             insb != inse; ++insb, ++instCounter) {
                auto thisLoc = std::make_pair(funcCounter, instCounter);
                auto it = profile.find(thisLoc);
                if (it == profile.end()) continue;
            dbgs() << funcCounter << ' ' << instCounter << *insb << '\n';
            assert(it->second.isCorrectInst(&*insb));
            instInfos[&*insb] = it->second;
        }
        if (instInfos.empty())
            continue;
        // Get thread usage of function from instructions with an entry in profile
        // and push that in the call graph.
        std::set<size_t> funcUserThreads;
        for (const auto &instInfoP : instInfos) {
            std::set<size_t> threads = instInfoP.second.getThreads();
            funcUserThreads.insert(threads.begin(), threads.end());
        }
        this->cg.hintFuncThreads(&*fb, funcUserThreads);
        this->preCloneProfile.emplace(&*fb, instInfos);
    }
    this->cg.propagateFuncThreads();
    dbgs() << this->cg << '\n';
    dbgs() << "\n";

    // The call graph may contain some functions not in the profile, 
    // so we walk through it first.
    dbgs() << "Cloning functions.\n";
    auto cloneFunctions = this->cg.getFunctions();
    for (const auto &f: cloneFunctions) {
        const auto &threads = this->cg.getFuncThreads(f);
        assert(threads.size() > 1);
        auto it = startersPthreads.find(f);
        Instruction *pthread = it == startersPthreads.end() ? nullptr : it->second;
        this->cloneFunc(f, pthread, threads);
    }
    dbgs() << "\n";

    dbgs() << "Correcting function calls.\n";
    for (const auto &p: clonedFuncs)
        for (const auto &p2: p.second)
            this->replaceThreadedFuncCall(p2.first, p2.second);
    dbgs() << "\n";
    
    dbgs() << "Getting instructions in non-cloned functions.\n";
    for (auto &fb : M) {
        if (cloneFunctions.find(&fb) != cloneFunctions.end())
            continue;
        auto it = this->preCloneProfile.find(&fb);
        if (it == this->preCloneProfile.end())
            continue;
        // By default, thread 0.
        for (const auto &p: it->second)
            this->absPosProfile[&fb].emplace(p.first, ThreadedPCInfo(p.second, 0));
    }
    dbgs() << "\n";

    for (auto &p: absPosProfile) {
        GroupFuncLoop funcPass(this, &M, p.first, p.second);
        funcPass.runOnFunction();
    }

    return true;
}

void RedirectPtr::makeMallocTable(Module &M) {
    auto *dl = new DataLayout(&M);
    Type *sizeType = Type::getIntNTy(M.getContext(), dl->getPointerSizeInBits());
    size_t maxSubSize = 0;
    for (const auto &sub: mallocOffsets)
        maxSubSize = std::max(maxSubSize, sub.size());
    ArrayType *subArrayType = ArrayType::get(sizeType, maxSubSize);
    ArrayType* arrayType = ArrayType::get(subArrayType, mallocN);

    std::vector<Constant *> subArrayInits;
    for (const auto &sub: mallocOffsets) {
        std::vector<Constant *> remapConsts;
        for (size_t m: sub) {
            Constant *c = ConstantInt::get(sizeType, m, /*isSigned=*/false);
            remapConsts.push_back(c);
        }
        Constant *init = ConstantArray::get(subArrayType, remapConsts);
        subArrayInits.push_back(init);
    }
    Constant *init = ConstantArray::get(arrayType, subArrayInits);
    new GlobalVariable(
            M, arrayType, /*isConstant=*/true, GlobalValue::PrivateLinkage, init, "__malloc_offset_table"
    );
}
