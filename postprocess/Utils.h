//
// Created by yifanz on 6/13/18.
//

#ifndef POSTPROCESS_UTILS_H
#define POSTPROCESS_UTILS_H

#include <iostream>
#include <set>
#include <experimental/string_view>
#include <cassert>
#include <cstring>
#include <vector>

using std::experimental::string_view;

const int CACHELINE_BIT = 6;

template<class T>
inline void hash_combine(std::size_t &s, const T &v) {
    std::hash<T> h;
    s ^= h(v) + 0x9e3779b9 + (s << 6) + (s >> 2);
}

std::string insert_suffix(const std::string &path, const std::string &suffix);

struct RW {
    uint32_t r, w;

    explicit RW(uint32_t _r = 0, uint32_t _w = 0) : r(_r), w(_w) {}

    RW operator+(const RW &rhs) {
        return RW(r + rhs.r, w + rhs.w);
    }

    RW &operator+=(const RW &rhs) {
        r += rhs.r;
        w += rhs.w;
        return *this;
    }

    void dump(std::ostream &os) const {
        os << '(' << r << ", " << w << ')';
    }
};

struct PC {
    uint16_t func, inst;

    PC(uint16_t f, uint16_t i) : func(f), inst(i) {}

    PC() {
        *this = null();
    }

    bool operator==(const PC &rhs) const {
        return func == rhs.func && inst == rhs.inst;
    }

    bool operator<(const PC &rhs) const {
        if (func < rhs.func)
            return true;
        if (func > rhs.func)
            return false;
        return inst < rhs.inst;
    }

    static PC null() {
        uint16_t max16 = (1 << 16) - 1;
        return PC(max16, max16);
    }

    friend std::ostream &operator<<(std::ostream &os, const PC &pc) {
        os << pc.func << ' ' << pc.inst;
        return os;
    }

    friend std::istream &operator>>(std::istream &is, PC &pc) {
        is >> pc.func >> pc.inst;
        return is;
    }

    void dump(std::ostream &os) const {
        os << '(' << func << ", " << inst << ')';
    }
};

namespace std {
    template<>
    struct hash<PC> {
        std::size_t operator()(PC const &pc) const {
            std::size_t res = 0;
            hash_combine(res, pc.func);
            hash_combine(res, pc.inst);
            return res;
        }
    };
}

struct Segment {
    size_t start, end;

    explicit Segment(size_t _start = 0, size_t _end = 0) : start(_start), end(_end) {}

    bool operator==(const Segment &rhs) const {
        return start == rhs.start && end == rhs.end;
    }

    bool operator<(const Segment &rhs) const {
        return start < rhs.start || (start == rhs.start && end < rhs.end);
    }

    bool overlap(const Segment &rhs) const {
        return (start < rhs.end && end > rhs.start) || (rhs.start < end && rhs.end > start);
    }

    friend std::ostream &operator<<(std::ostream &os, const Segment &seg) {
        os << seg.start << ' ' << seg.end;
        return os;
    }

    friend std::istream &operator>>(std::istream &is, Segment &seg) {
        is >> seg.start >> seg.end;
        return is;
    }

    void dump(std::ostream &os) const {
        os << "(" << start << ", " << end << ")";
    }

    Segment shift_by(size_t sz, bool positive) const {
        return positive ? Segment(start + sz, end + sz) : Segment(start - sz, end - sz);
    }

    bool touch(const Segment &seg) const {
        return end == seg.start;
    }

    static std::vector<Segment> merge_neighbors(const std::vector<Segment> &segs) {
        if (segs.empty())
            return std::vector<Segment>();
        std::vector<Segment> ret;
        Segment last = segs[0];
        for (size_t i = 1; i < segs.size(); i++) {
            if (last.touch(segs[i]))
                last.end = segs[i].end;
            else {
                ret.push_back(last);
                last = segs[i];
            }

        }
        ret.push_back(last);
        return ret;
    }
};

namespace std {
    template<>
    struct hash<Segment> {
        std::size_t operator()(Segment const &seg) const {
            std::size_t res = 0;
            hash_combine(res, seg.start);
            hash_combine(res, seg.end);
            return res;
        }
    };
}

namespace std {
    template<class T>
    struct hash<std::set<T>> {
        std::size_t operator()(std::set<T> const &vec) const {
            std::size_t seed = vec.size();
            for (auto &i : vec) {
                seed ^= i + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            }
            return seed;
        }
    };

    template<typename T1, typename T2>
    struct hash<std::pair<T1, T2>> {
        std::size_t operator()(const std::pair<T1, T2> &x) const {
            return std::hash<T1>()(x.first) ^ std::hash<T2>()(x.second);
        }
    };
}

struct CSVParser {
    std::vector<string_view> fields;
    char delim;

    explicit CSVParser(size_t n_fields, char _delim = ',') : fields(n_fields), delim(_delim) {}

    inline const std::vector<string_view> &read_csv_line(const std::string &line) {
        size_t start = 0, end = 0;
        for (auto &field : fields) {
            if (end == std::string::npos)
                throw std::invalid_argument("csv malformed");
            const void *end_ptr = memchr(line.data() + start, delim, line.length() - start);
            end = end_ptr ? (char *) end_ptr - line.data() : std::string::npos;
            field = string_view(line).substr(start, end - start);
            start = end + 1;
        }
        return fields;
    }
};


template<typename T>
inline T to_unsigned(const string_view &str) {
    T val = 0;
    for (char i: str)
        val = val * 10 + (i - '0');
    return val;
}

template<typename T>
inline T to_signed(const string_view &str) {
    T val = 0;
    bool negative = str.front() == '-';
    for (size_t i = (negative ? 1 : 0); i < str.size(); i++)
        val = val * 10 + (str[i] - '0');
    return (negative ? -val : val);
}

size_t to_address(const string_view &str);

template<typename T>
class AllEqual {
public:
    AllEqual() : is_first(true), is_unequal(false) {}

    void next(const T &t) {
        if (is_unequal)
            return;
        if (is_first)
            prev = t, is_first = false;
        else if (t != prev)
            is_unequal = true;
    }

    bool is_all_equal() const {
        return !is_unequal;
    }

private:
    T prev;
    bool is_first, is_unequal;
};

#endif //POSTPROCESS_UTILS_H
