#include <cstdint>
#include <atomic>
#include <algorithm>

class CacheLine {
private:
    std::atomic<uint16_t> bitmap;
public:
    CacheLine() : bitmap(0x7fff) {}

    bool isInstrumented() const {
        return bool(bitmap >> 15);
    }

    bool store(int threadId) {
        if (this->isInstrumented()) {
            return true;
        } else if (bitmap == 0x7fff) {
            bitmap = (uint16_t) threadId;
        } else if (bitmap != (uint16_t) threadId) {
            bitmap |= (1 << 15);
            return true;
        } // else do nothing
        return false;
    }

    bool load(int threadId) {
        if (this->isInstrumented()) {
            return true;
        } else if (bitmap != (uint16_t) threadId) {
            bitmap |= (1 << 15);
            return true;
        } // else do nothing
        return false;
    }
};
