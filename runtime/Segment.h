#ifndef SEGMENT_H
#define SEGMENT_H

template <typename T>
class Segment {
    T begin, end;
public:
    Segment() = default;

    Segment(T offset, T size): begin(offset), end(offset + size) {}

    Segment &insert(const Segment &rhs) {
        begin = std::min(begin, rhs.begin);
        end = std::max(end, rhs.end);
        return *this;
    }

    Segment &shrink(const Segment &rhs) {
        if (rhs.begin <= begin && rhs.end > begin) {
            begin = rhs.end;
        }
        else if (rhs.begin < end && rhs.end >= begin) {
            end = rhs.begin;
        }
        return *this;
    }

    inline bool contain(T val) {
        return begin <= val && val < end;
    }
};

typedef Segment<uintptr_t> AddrSeg;

#endif
