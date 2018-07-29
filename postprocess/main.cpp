#include <iostream>
#include "Repair.h"

using namespace std;

void print_usage(const string &arg0) {
    cerr << arg0 << " \"detect\" logfile output " << DetectPass::optionals << endl
         << arg0 << " \"repair\" detectfile output " << RepairPass::optionals << endl
         << arg0 << " \"all\" logfile output " << DetectPass::optionals
         << " " << RepairPass::optionals << endl;
    exit(1);
}

int main(int argc, char *argv[]) {
    ios_base::sync_with_stdio(false);
    vector<string> args(argv, argv + argc);
    if (argc < 4)
        print_usage(args[0]);
    const auto &subcmd = args[1];
    if (subcmd == "detect") {
        DetectPass dpass(args[2], vector<string>(args.begin() + 4, args.end()));
        dpass.compute();
        dpass.print_result(args[3]);
    }
    else if (subcmd == "repair") {
        RepairPass rpass(args[2], vector<string>(args.begin() + 4, args.end()));
        rpass.compute();
        rpass.print_result(args[3]);
    }
    else if (subcmd == "all") {
        size_t opt1 = min(args.size() - 4, DetectPass::n_opt);
        size_t opt2 = min(args.size() - 4 - opt1, RepairPass::n_opt);
        auto opt1_begin = args.begin() + 4, opt1_end = opt1_begin + opt1, 
             opt2_end = opt1_end + opt2;
        DetectPass dpass(args[2], vector<string>(opt1_begin, opt1_end));
        dpass.compute();
        RepairPass rpass(dpass.get_api_output(), vector<string>(opt1_end, opt2_end));
        rpass.compute();
        rpass.print_result(args[3]);
    }
    else print_usage(args[0]);

    return 0;
}
