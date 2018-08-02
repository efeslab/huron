//
// Created by yifanz on 7/29/18.
//

#ifndef POSTPROCESS_DETECT_H
#define POSTPROCESS_DETECT_H

#include "Stats.h"

typedef std::tuple<Segment, PC, size_t> RecT;

struct MallocOutput {
    std::vector<RecT> accesses;
    PC pc;
    size_t size;

    MallocOutput(std::vector<std::tuple<Segment, PC, size_t>> &&accesses,
                 std::pair<PC, size_t> &&malloc);

    MallocOutput() = default;

    friend std::ostream &operator<<(std::ostream &os, const MallocOutput &mo);

    friend std::istream &operator>>(std::istream &is, MallocOutput &mo);
};

class MallocStorageT;

class DetectPass {
public:
    static const char *optionals;
    static const size_t n_opt;

    typedef std::vector<MallocOutput> ApiT;

    DetectPass(const std::string &in, const std::vector<std::string> &rest);

    void compute();

    ApiT get_api_output() const;

    void print_result(const std::string &out);

    ~DetectPass();

private:
    void check_in_files();

    std::ifstream log_file, malloc_file;
    std::ofstream summary_file;
    size_t threshold;
    FSRankStat fsrStat;
    std::map<int, MallocStorageT *> data;
};

#endif //POSTPROCESS_DETECT_H
