#define DEBUG_TYPE "redirectptr"

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Transforms/Utils/Cloning.h"

#include <fstream>
#include <set>
#include <unordered_map>
#include <llvm/IR/InstIterator.h>

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

        void resolveThreadedFunc(Function *func, const PreCloneT &instInfos);

        void replaceThreadedFuncCall(Function *func, size_t tid);

        LLVMContext *context{};
        DataLayout *layout{};
        // Table read and parsed from input profile file.
        std::unordered_map<std::pair<size_t, size_t>, PCInfo> profile{};
        // Profile with offset replaced by pointers and PCInfo splitted (partially applied)
        std::unordered_map<Function *, PostCloneT> absPosProfile{};
        // All cloned functions.
        std::unordered_map<Function *, std::unordered_map<size_t, Function *>> clonedFuncs{};
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
        assert(to >= from && "Malloc size becomes smaller after padding?");
        if (to == from) continue;  // no change needed for this malloc
        PCInfo value(to - from);
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
    layout = new DataLayout(&M);
    context = &(M.getContext());
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

void RedirectPtr::resolveThreadedFunc(Function *func, const PreCloneT &instInfos) {
    // Get thread usage for each function,
    // combined from all instructions in it.
    // If used by more than 1 threads, provide copies.
    std::set<size_t> funcUserThreads;
    for (const auto &instInfoP : instInfos) {
        std::set<size_t> threads = instInfoP.second.getThreads();
        funcUserThreads.insert(threads.begin(), threads.end());
    }
    if (funcUserThreads.size() != 1) {
        std::vector<std::pair<Function *, size_t>> dupFuncs;
        CallInst *creator = getCallToPThread(func);
        for (size_t tid : funcUserThreads) {
            // name: `func` -> `func_1` (thread 1 version)
            std::string newName = func->getName().str() + "_" + std::to_string(tid);
            ValueToValueMapTy map;
            Function *newFunc = CloneFunction(func, map);
            newFunc->setName(newName);
            dupFuncs.emplace_back(newFunc, tid);
            this->absPosProfile[newFunc] = getEquivalentInsts(tid, map, instInfos);
        }
        if (creator) {
            // Directly created by pthread_create.
            // Remember to replace the function name in pthread_create call,
            // potentially unrolling the loop around pthread_create.
            Function *creatorFunc = creator->getParent()->getParent();
            std::vector<Function *> cloned;
            for (const auto &p: dupFuncs)
                cloned.push_back(p.first);
            this->absPosProfile[creatorFunc].emplace(creator, ThreadedPCInfo(cloned));
        }
        for (const auto &p: dupFuncs)
            this->clonedFuncs[func].emplace(p.second, p.first);
    } else {
        size_t tid = *funcUserThreads.begin();
        for (const auto &p: instInfos)
            this->absPosProfile[func].emplace(p.first, ThreadedPCInfo(p.second, tid));
    }
}

void RedirectPtr::replaceThreadedFuncCall(Function *func, size_t tid) {
    dbgs() << "Replacing calls in threaded function " << func->getName() << '\n';
    for (auto ins = inst_begin(func), ie = inst_begin(func); ins != ie; ++ins) {
        CallInst *call = cast<CallInst>(&*ins);
        if (!call) continue;
        Function *callee = call->getFunction();
        auto it = this->clonedFuncs.find(callee);
        if (it == this->clonedFuncs.end()) continue;
        const auto &clones = it->second;
        auto it2 = clones.find(tid);
        assert(it2 != clones.end());
        call->setCalledFunction(it2->second);
        dbgs() << callee->getName() << "->" << it2->second->getName() << '\n';
    }
}

bool RedirectPtr::runOnModule(Module &M) {
    // Expand functions thread-wise
    // and use pointer to locate objects (instead of offset)
    size_t funcCounter = 0;
    dbgs() << "Searching for instructions:\n";
    for (Module::iterator fb = M.begin(), fe = M.end(); fb != fe;
         ++fb, ++funcCounter) {
        size_t instCounter = 0;
        PreCloneT instInfos;
        dbgs() << funcCounter << ' ' << fb->getName() << '\n';
        for (Function::iterator bb = fb->begin(), be = fb->end(); bb != be;
             ++bb) {
            for (BasicBlock::iterator ins = bb->begin(), instE = bb->end();
                 ins != instE; ++ins, ++instCounter) {
                auto thisLoc = std::make_pair(funcCounter, instCounter);
                auto it = profile.find(thisLoc);
                if (it == profile.end()) continue;
                ins->dump();
                assert(it->second.isCorrectInst(&*ins));
                instInfos[&*ins] = it->second;
            }
        }
        if (instInfos.empty())
            continue;
        this->resolveThreadedFunc(&*fb, instInfos);
    }
    
    dbgs() << "\nWorking on functions.\n";

    dbgs() << "\n";
    for (const auto &p: this->clonedFuncs)
        for (const auto &p2: p.second)
            replaceThreadedFuncCall(p2.second, p2.first);
    dbgs() << "\n";

    for (auto &p: absPosProfile) {
        GroupFuncLoop funcPass(this, this->context, this->layout, p.second);
        funcPass.runOnFunction(*p.first);
    }

    return true;
}
