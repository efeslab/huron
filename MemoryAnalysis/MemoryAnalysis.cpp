#define DEBUG_TYPE "memory-analysis"

#include "llvm/IR/Instructions.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Debug.h"

#include <algorithm>
#include <fstream>
#include <unordered_map>
#include <unordered_set>

using namespace llvm;

struct PC {
    size_t func, inst;

    PC(size_t func, size_t inst): func(func), inst(inst){}

    PC() = default;

    bool operator==(const PC &rhs) const {
        return func == rhs.func && inst == rhs.inst;
    }

    bool operator<(const PC &rhs) const {
        return func < rhs.func || (func == rhs.func && inst < rhs.inst);
    }

    friend raw_ostream &operator<<(raw_ostream &os, const PC &pc) {
        os << pc.func << ' ' << pc.inst;
        return os;
    }
};

namespace std {
    template <>
    struct hash<PC> {
        std::size_t operator()(const PC &x) const {
            auto hash = std::hash<size_t>();
            return hash(x.func) ^ hash(x.inst);
        }
    };
}  // namespace std

struct Segment {
    size_t begin, end;

    Segment(size_t begin, size_t end): begin(begin), end(end) {}

    Segment() = default;

    friend raw_ostream &operator<<(raw_ostream &os, const Segment &seg) {
        os << seg.begin << ' ' << seg.end;
        return os;
    }
};

namespace {
    class MemoryAnalysis : public ModulePass {
    public:
        MemoryAnalysis();

        StringRef getPassName() const override;

        bool runOnModule(Module &M) override;

        bool doInitialization(Module &M) override;

        void getAnalysisUsage(AnalysisUsage &AU) const override;

        static char ID;  // Pass identification, replacement for typeid

    private:
        Function *findPtrMaxRange(Value *ptr, Segment &seg);

        void loadPCs();

        void collectPrintResult(const std::map<PC, Segment> &pcResult);

        LLVMContext *context{};
        const DataLayout *layout{};
        std::unordered_map<PC, PC> pcToMalloc{};
        std::unordered_map<Argument *, Instruction *> funcArgToMalloc{};
        std::unordered_map<PC, size_t> mallocTypeSize{};
    };

}  // namespace

char MemoryAnalysis::ID = 0;
static RegisterPass<MemoryAnalysis> redirectptr(
        "memoryanalysis", "Analyze max memory dependency for each PC of interest", false, false);
static cl::opt<std::string> pcfile("pcfile", cl::desc("Specify PC file path"),
                                    cl::value_desc("filename"), cl::Required);

void MemoryAnalysis::loadPCs() {
    dbgs() << "Loading from file: " << pcfile << "\n\n";
    std::ifstream fin(pcfile.c_str());
    if (fin.fail()) {
        errs() << "Open file failed! Exiting.\n";
        exit(1);
    }
    size_t mfunc, minst, n;
    while (fin >> mfunc >> minst >> n) {
        PC mpc(mfunc, minst);
        for (size_t i = 0; i < n; i++) {
            size_t ifunc, iinst;
            fin >> ifunc >> iinst;
            PC ipc(ifunc, iinst);
            pcToMalloc.emplace(ipc, mpc);
        }
    }
}

MemoryAnalysis::MemoryAnalysis() : ModulePass(ID) {}

StringRef MemoryAnalysis::getPassName() const { return "MemoryAnalysis"; }

bool MemoryAnalysis::doInitialization(Module &M) {
    layout = &(M.getDataLayout());
    context = &(M.getContext());
    loadPCs();
    return true;
}

void MemoryAnalysis::getAnalysisUsage(AnalysisUsage &AU) const {}

Value *getPointerOperand(Instruction *inst) {
    if (auto *LI = dyn_cast<LoadInst>(inst))
        return LI->getPointerOperand();
    if (auto *SI = dyn_cast<StoreInst>(inst))
        return SI->getPointerOperand();
    if (auto *RMW = dyn_cast<AtomicRMWInst>(inst))
        return RMW->getPointerOperand();
    if (auto *XCHG = dyn_cast<AtomicCmpXchgInst>(inst))
        return XCHG->getPointerOperand();
    assert(false && "Instruction is not load/store!");
}

Value *findAliasPtr(Value *ptr) {
    Value *current = ptr;
    dbgs() << "Finding alias for pointer type " << *ptr << '\n';
    dbgs() << "    with type " << *(ptr->getType()) << '\n';
    while (true) {
        Value *upPtr = current->stripPointerCasts();
        dbgs() << "  Continue to " << *upPtr << " by going up the tree.\n";
        if (isa<GetElementPtrInst>(upPtr)) {
            dbgs() << "  Hit a GEP: " << *upPtr << ". Returning with it.\n";
            return upPtr;
        }
        else if (isa<Argument>(upPtr)) {
            dbgs() << "  Hit an argument: " << *upPtr << ". Returning.\n";
            return upPtr;
        }
        else if (isa<CallInst>(upPtr)) {
            dbgs() << "  Hit a CallInst: " << *upPtr << ". Returning.\n";
            return upPtr;
        }
        else if (auto *li = dyn_cast<LoadInst>(upPtr)) {
            Value *loadArg = li->getPointerOperand();
            dbgs() << "  Hit a LoadInst. Searching for a store to the load argument "
                   << *loadArg << ".\n";
            StoreInst *storeInst = nullptr;
            for (auto ubegin = loadArg->user_begin(), uend = loadArg->user_end(); 
                ubegin != uend; ++ubegin)
                if (auto *userInst = dyn_cast<StoreInst>(*ubegin)) {
                    assert(!storeInst && "Multiple StoreInsts");
                    storeInst = userInst;
                }
            assert(storeInst && "No store inst");
            dbgs() << "  Found StoreInst: " << *storeInst << '\n';
            current = storeInst->getOperand(0);
            dbgs() << "  Continue to value provider of StoreInst: " << *current << '\n';
        }
        else assert(false);
    }
}

std::unordered_map<PC, Instruction *> findpcs(
        Module &M, const std::unordered_set<PC> &pcs) {
    std::unordered_map<PC, Instruction *> ret;
    size_t funcCounter = 0;
    for (auto fb = M.begin(), fe = M.end(); fb != fe; ++fb, ++funcCounter) {
        size_t instCounter = 0;
        bool funcPrinted = false;
        for (auto insb = inst_begin(&*fb), inse = inst_end(&*fb); 
             insb != inse; ++insb, ++instCounter) {
            auto thisPC = PC(funcCounter, instCounter);
            auto it = pcs.find(thisPC);
            if (it == pcs.end()) continue;
            if (!funcPrinted) {
                dbgs() << funcCounter << ' ' << fb->getName() << '\n';
                funcPrinted = true;
            }
            dbgs() << funcCounter << ' ' << instCounter << *insb << '\n';
            ret.emplace(*it, &*insb);
        }
    }
    assert(ret.size() == pcs.size() && "Some pcs not found!");
    return ret;
}

Value *getPthreadArg(Function *f) {
    CallInst *callInst = nullptr;
    for (auto ubegin = f->user_begin(), uend = f->user_end(); 
        ubegin != uend; ++ubegin)
        if (auto *userInst = dyn_cast<CallInst>(*ubegin)) {
            StringRef funcName = userInst->getCalledFunction()->getName();
            assert(funcName == "pthread_create" && 
                   "called by function other than pthread_create");
            assert(!callInst && "Multiple CallInsts");
            callInst = userInst;
        }
    if (!callInst)
        return nullptr;
    return callInst->getOperand(3);
}

Type *getMallocType(Value *malloced) {
    Type *type = nullptr;
    for (auto ubegin = malloced->user_begin(), uend = malloced->user_end(); 
        ubegin != uend; ++ubegin)
        if (auto *userInst = dyn_cast<BitCastInst>(*ubegin)) {
            assert((!type || type == userInst->getDestTy()) && 
                    "Malloc ptr casted to multiple types?");
            type = userInst->getDestTy();
        }
    assert(type && "Didn't see bitcast");
    return cast<PointerType>(type)->getElementType();
}

Function *MemoryAnalysis::findPtrMaxRange(Value *ptr, Segment &seg) {
    dbgs() << "Pointer operand: " << *ptr << '\n';
    dbgs() << "    with type " << *(ptr->getType()) << '\n';
    // The access range of a ptr is by default the size of its type, 
    // unless there's proof that it's a pointer into an array.
    Type *ptrElemTy = cast<PointerType>(ptr->getType())->getElementType();
    size_t range = this->layout->getTypeStoreSizeInBits(ptrElemTy) >> 3;
    size_t offset = 0;
    bool isArray = false;
    while (true) {
        Value *barrier = findAliasPtr(ptr);
        if (auto *arg = dyn_cast<Argument>(barrier)) {
            dbgs() << "Successfully related to argument: " << *arg << ".\n";
            seg = Segment(offset, offset + range);
            return arg->getParent();
        }
        if (auto *call = dyn_cast<CallInst>(barrier)) {
            dbgs() << "Successfully related to malloc call: " << *call << ".\n";
            seg = Segment(offset, offset + range);
            return nullptr;
        }
        auto *barrierGEP = cast<GetElementPtrInst>(barrier);
        Value *provider = barrierGEP->getOperand(0);
        if (barrierGEP->getType() != ptr->getType()) {
            // Alias with unequal type could mean
            // we've stripped an all-zero GEP that is an array decay.
            // Take a note of the array size.
            auto *aliasTy = cast<PointerType>(barrierGEP->getType());
            auto *arrayTy = dyn_cast<ArrayType>(aliasTy->getElementType());
            if (arrayTy) {
                assert(!isArray && "Nested array unsupported");
                Type *elemTy = arrayTy->getElementType();
                size_t nElem = arrayTy->getNumElements();
                assert(elemTy->getPointerTo() == ptr->getType());
                dbgs() << "Array decay found. The array has elem type " << *elemTy 
                    << " and #elem = " << nElem << ".\n";
                range *= nElem, isArray = true;
            }
        }
        if (provider->getType() != barrierGEP->getType()) {
            // The type changes within a GEP, so the RHS should be a
            // pointer to a struct.
            // Take a note of offset.
            APInt size(64, 0);
            assert(barrierGEP->accumulateConstantOffset(*(this->layout), size));
            dbgs() << "Struct found. The offset of the start of current field "
                      "from its struct is " << size << ".\n";
            offset += size.getLimitedValue();
        }
        ptr = provider;
    }
}

void MemoryAnalysis::collectPrintResult(const std::map<PC, Segment> &pcResult) {
    std::map<PC, std::map<PC, Segment>> collected;
    for (const auto &p: pcResult) {
        const PC &key = this->pcToMalloc.find(p.first)->second;
        collected[key].emplace(p);
    }
    for (const auto &p: collected) {
        size_t msize = this->mallocTypeSize.find(p.first)->second;
        dbgs() << p.first << ' ' << msize << '\n';
        for (const auto &p2: p.second)
            dbgs() << p2.first << ' ' << p2.second << '\n';
    }
}

bool MemoryAnalysis::runOnModule(Module &M) {
    dbgs() << "Searching for instructions:\n";
    std::unordered_set<PC> instPCs;
    std::transform(
        pcToMalloc.begin(), pcToMalloc.end(),
        std::inserter(instPCs, instPCs.end()),
        [](const std::pair<PC, PC> &p) { return p.first; }
    );
    auto iPC2Inst = findpcs(M, instPCs);
    dbgs() << "\n";

    dbgs() << "Searching for malloc calls:\n";
    std::unordered_set<PC> mallocPCs;
    std::transform(
        pcToMalloc.begin(), pcToMalloc.end(),
        std::inserter(mallocPCs, mallocPCs.end()),
        [](const std::pair<PC, PC> &p) { return p.second; }
    );
    auto mPC2Inst = findpcs(M, mallocPCs);
    dbgs() << "\n";

    std::map<PC, Segment> pcResult;
    for (const auto &p: iPC2Inst) {
        Value *ptr = getPointerOperand(p.second);
        dbgs() << "Target instruction: " << *(p.second) << '\n';
        Segment &seg = pcResult[p.first];
        findPtrMaxRange(ptr, seg);
        dbgs() << "(start, end) = " << seg << "\n\n";
    }

    for (const auto &p: mPC2Inst) {
        Type *mTy = getMallocType(p.second);
        size_t size = this->layout->getTypeStoreSizeInBits(mTy) >> 3;
        mallocTypeSize[p.first] = size;
    }

    collectPrintResult(pcResult);
    return false;
}
