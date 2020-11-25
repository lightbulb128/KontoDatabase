#include <iostream>
#include <cstring>
#include <vector>
#include <filesystem>
#include "KontoConst.h"

using std::vector;
using std::string;
using std::cout;
using std::endl;

string get_filename(string filename) {
    return filename+".txt";
}

void debug_assert(bool assertion, string message) {
    if (!assertion) printf("Assertion failed: %s\n", message);
}

bool get_files_start_with(string& str, string& prefix) {
    if (str.length() < prefix.length()) return false;
    int len = prefix.length();
    for (int i=0;i<len;i++) if (str[i]!=prefix[i]) return false;
    return true;
}

vector<string> get_files(string prefix) {
    vector<string> ret = vector<string>();
    std::filesystem::path path(".");
    std::filesystem::directory_iterator list(path);
    for (auto& iter : list) {
        string filename = iter.path().filename().string();
        if (get_files_start_with(filename, prefix)) ret.push_back(filename);
    }
    return ret;
}

vector<string> get_index_key_names(string fullFilename) {
    vector<string> ret = vector<string>();
    int len = fullFilename.length();
    int i = 0; while (fullFilename[i]!='.') i++; i++;
    while (fullFilename[i]!='.') i++;
    while (i < len) {
        int j = i+1; while (j < len && fullFilename[j]!='.') j++;
        if (j==len) break;
        ret.push_back(fullFilename.substr(i+1, j-i-1));
        i=j;
    }
    return ret;
}

void remove_file(string filename) {
    std::filesystem::path path(filename);
    std::filesystem::remove(path);
}

string strip_filename(string filename) {
    int p = filename.find_last_of('.');
    return filename.substr(0, p);
}

void rename_file(string old, string newname) {
    std::filesystem::path path(old);
    std::filesystem::path newpath(newname);
    std::filesystem::rename(path, newpath);
}