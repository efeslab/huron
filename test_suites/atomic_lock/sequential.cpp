#include <iostream>
#include <thread>
#include <chrono>
#include "spinlock.h"

using namespace std;

spinlock lock;

void thread_print(size_t tid) {
    lock.lock();
    cout << "This is thread " << tid << "! Sleeping for 1 second.\n";
    this_thread::sleep_for(chrono::seconds(1));
    cout << "Thread " << tid << " is quitting.\n";
    lock.unlock();
}

int main() {
    thread th[2];
    th[0] = thread(thread_print, 0);
    th[1] = thread(thread_print, 1);
    for (thread &t: th)
        t.join();
    return 0;
}
