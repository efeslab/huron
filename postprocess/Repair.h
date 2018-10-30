//
// Created by yifanz on 7/10/18.
//

#ifndef POSTPROCESS_REPAIR_H
#define POSTPROCESS_REPAIR_H

#include <unordered_map>
#include "Utils.h"
#include "Detect.h"

class AnalysisResult {
public:
    AnalysisResult() = default;

    void read_from_file(std::ifstream &is);

    bool empty() const;

    bool get_size_by_pc(PC pc, size_t &size) const;

    bool get_seg_by_pc(PC pc, Segment &seg) const;

private:
    std::unordered_map<PC, Segment> pc_replace;
    std::unordered_map<PC, size_t> malloc_single_size;
};

struct FixedMalloc {
    PC pc;
    size_t origSize, newSize;
    std::vector<size_t> translations;

    FixedMalloc(PC pc, size_t origSize, size_t newSize, const std::map<size_t, Segment> &remap);
};

class RepairPass {
public:
    static const char *optionals;
    static const size_t n_opt;

    RepairPass(const std::string &in, const std::vector<std::string> &rest);

    RepairPass(DetectPass::ApiT &&in, const std::vector<std::string> &rest);

    void compute();

    void print_result(const std::string &path);

private:
    void read_from_file(std::ifstream &is);

    void print_malloc(std::ofstream &layout_stream);

    void print_layout(std::ofstream &layout_stream);

    std::vector<FixedMalloc> all_fixed_mallocs;
    std::multimap<PC, std::tuple<size_t, size_t, size_t>> all_pcs_layout;
    int target_thread_count;
    DetectPass::ApiT input;
    AnalysisResult analysis;
};

#endif //POSTPROCESS_REPAIR_H
