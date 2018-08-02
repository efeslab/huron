//
// Created by yifanz on 7/29/18.
//

#include "Utils.h"

using namespace std;

size_t to_address(const string_view &str) {
    if (!(str.length() > 2 && str[0] == '0' && str[1] == 'x'))
        throw invalid_argument("Invalid address format");
    size_t val = 0;
    for (size_t i = 2; i < str.size(); i++)
        val = val * 16 + (str[i] >= 'a' ? str[i] - 'a' + 10 : str[i] - '0');
    return val;
}

string insert_suffix(const string &path, const string &suffix) {
    size_t slash = path.rfind('/');
    slash = slash == string::npos ? 0 : slash + 1;
    string basename = path.substr(slash);
    size_t dot = basename.find('.');
    if (dot == string::npos || dot == 0) // hidden file, not extension
        return path + suffix;
    else {
        dot += slash;
        return path.substr(0, dot) + suffix + path.substr(dot);
    }
}
