//
// Created by yifanz on 6/23/18.
//

#ifndef RUNTIME_SHAREDSPINLOCK_H
#define RUNTIME_SHAREDSPINLOCK_H

#include <atomic>
#include <thread>

class SharedSpinLock {
    std::atomic<int> readers_count{0};
    char _padding[64];
    std::atomic<bool> write_now{false};
public:
    void lock() noexcept {
        while (write_now.exchange(true, std::memory_order_acquire)) {
            std::this_thread::yield();
        }

        // wait for readers to exit
        while (readers_count != 0) {
            std::this_thread::yield();
        }
    }

    void unlock() noexcept {
        write_now.store(false, std::memory_order_release);
    }

    void lock_shared() noexcept {
        // writers have priority
        while (true) {
            while (write_now.load(std::memory_order_seq_cst)) {     // wait for unlock
                std::this_thread::yield();
            }

            readers_count.fetch_add(1, std::memory_order_acquire);

            if (write_now.load(std::memory_order_seq_cst)) {
                // locked while transaction? Fallback. Go another round
                readers_count.fetch_sub(1, std::memory_order_release);
            } else {
                // all ok
                return;
            }
        }
    }

    void unlock_shared() noexcept {
        readers_count.fetch_sub(1, std::memory_order_release);
    }
};

#endif //RUNTIME_SHAREDSPINLOCK_H
