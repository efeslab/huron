#ifndef SEGMENT_H
#define SEGMENT_H

template <typename T>
class Segment {
    T start, end;
public:
    Segment() = default;

    Segment(T _start, T _end): start(_start), end(_end) {}

    void insert(const Segment &rhs) {
        start = std::min(start, rhs.start);
        end = std::max(end, rhs.end);
    }

    void shrink(const Segment &rhs) {
        if (rhs.start <= start && rhs.end > start) {
            start = rhs.end;
        }
        else if (rhs.start < end && rhs.end >= start) {
            end = rhs.start;
        }
    }

    inline bool contain(T val) {
        return start <= val && val < end;
    }
};

typedef Segment<uintptr_t> AddrSeg;

#endif
