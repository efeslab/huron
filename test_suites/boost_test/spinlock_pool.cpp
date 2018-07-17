#include <iostream>
#include <string>
#include <vector>
#include <cassert>
#include <algorithm>

#include "spinlock_pool.hpp"

using namespace std;

class cacheline_t {
    size_t data[8];

public:
    cacheline_t(): data() {}

    void iter() {
        for (size_t i = 0; i < 8; i++)
            data[i] = data[i] * data[i] % 16777923 + 3;
    }
};

cacheline_t *ibuffer;
size_t bufferSize = 1 << 10, iter;
const size_t nThreads = 4, cacheline = 64, spinlocks = 41;
vector<size_t> indices[nThreads * 3];

void *threadFunc(void *threadIdArg) {
    size_t tid = *(size_t *)threadIdArg;
    size_t ibufferCL = (size_t)ibuffer / cacheline;
    for (size_t id: indices[tid * 3])
        for (size_t i = 0; i < iter; i++) {
            spinlock_pool<0>::scoped_lock lock((void*)(ibufferCL + id));
            ibuffer[id].iter();
        }
    return nullptr;
}

void distribute() {
    size_t ibufferCL = (size_t)ibuffer / cacheline;
    size_t lockWorkLoads[spinlocks];
    for (size_t lock = 0; lock < spinlocks; lock++) {
        long start = (((long)lock - (long)ibufferCL) % spinlocks + spinlocks) % spinlocks;
        long nSpace = (long)bufferSize - start - 1;
        if (nSpace < 0)
            lockWorkLoads[lock] = 0;
        else
            lockWorkLoads[lock] = (size_t)(nSpace / spinlocks + 1);
    }
    size_t tid = 0, sum = 0;
    for (size_t lock = 0; lock < spinlocks; lock++) {
        if (sum > bufferSize / nThreads)
            tid++, sum = 0;
        size_t start = (size_t)(((long)lock - (long)ibufferCL) % spinlocks + spinlocks) % spinlocks;
        for (size_t i = start; i < bufferSize; i += spinlocks)
            indices[tid * 3].push_back(i);
        sum += lockWorkLoads[lock];
    }
    for (size_t tid = 0; tid < nThreads; tid++)
        sort(indices[tid * 3].begin(), indices[tid * 3].end());
}

inline void *roundUp(void *addr, size_t align) {
    size_t rem = (size_t)addr % align;
    if (rem == 0)
        return addr;
    return (void*)((size_t)addr + align - rem);
}

int main(int argc, char *argv[]) {
    iter = argc > 1 ? stoi(argv[1]) : 100;
    void *mem = malloc(bufferSize * cacheline + cacheline);
    ibuffer = new (roundUp(mem, cacheline)) cacheline_t[bufferSize]();
    distribute();
    pthread_t threads[nThreads];
    size_t indices[nThreads];
    for (size_t i = 0; i < nThreads; i++) {
        indices[i] = i;
        pthread_create(&threads[i], nullptr, threadFunc, (void *)&indices[i]);
    }
    for (size_t i = 0; i < nThreads; i++) {
        pthread_join(threads[i], nullptr);
    }
    return 0;
}
