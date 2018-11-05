#ifndef SEGMENT_H
#define SEGMENT_H

#include <limits>
#include <atomic>

template<typename T>
class Segment {
    T start, end;
public:
    Segment() = default;

    Segment(T _start, T _end) : start(_start), end(_end) {}

    static Segment empty() {
        return Segment(std::numeric_limits<T>::max(), std::numeric_limits<T>::min());
    }

    void insert(const Segment &rhs) {
        start = std::min(start, rhs.start);
        end = std::max(end, rhs.end);
    }

    void shrink(const Segment &rhs) {
        if (rhs.start <= start && rhs.end > start) {
            start = rhs.end;
        } else if (rhs.start < end && rhs.end >= start) {
            end = rhs.start;
        }
    }

    inline bool contain(T val) const {
        return start <= val && val < end;
    }

    T get_start() const {
        return start;
    }

    T get_size() const {
        return end - start;
    }

    friend class Segment<std::atomic<uintptr_t>>;
};

typedef Segment<uintptr_t> AddrSeg;

template<>
class Segment<std::atomic<uintptr_t>> {
    std::atomic<uintptr_t> start, end;
public:
    Segment() : start(std::numeric_limits<uintptr_t>::max()), end(std::numeric_limits<uintptr_t>::min()) {}

    Segment(uintptr_t _start, uintptr_t _end) : start(_start), end(_end) {}

    void insert(const Segment<uintptr_t> &rhs) {
        start = std::min(start.load(), rhs.start);
        end = std::max(end.load(), rhs.end);
    }

    void shrink(const Segment<uintptr_t> &rhs) {
        if (rhs.start <= start && rhs.end > start) {
            start = rhs.end;
        } else if (rhs.start < end && rhs.end >= start) {
            end = rhs.start;
        }
    }

    bool contain(uintptr_t val) const {
        return start <= val && val < end;
    }

    uintptr_t get_start() const {
        return start;
    }

    uintptr_t get_size() const {
        return end - start;
    }
};


#endif
