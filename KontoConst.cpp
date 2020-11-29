#include <iostream>
#include <cstring>
#include <vector>
#include <fstream>
#include <filesystem>
#include "KontoConst.h"

using std::vector;
using std::string;
using std::cout;
using std::endl;

void PT(int tabs, const string& prom) {
    cout << TABS[tabs] << prom << endl;
}

string SS(int t, const string& s, bool right) {
    int l = s.length();
    if (l > t) return s.substr(0, t - 3) + "...";
    return (!right) ? (s + SPACES.substr(0, t-l)) : (SPACES.substr(0, t-l) + s);
}

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
    int f = prefix.find('/');
    if (f == -1) {
        vector<string> ret = vector<string>();
        std::filesystem::path path(".");
        std::filesystem::directory_iterator list(path);
        for (auto& iter : list) {
            string filename = iter.path().filename().string();
            if (get_files_start_with(filename, prefix)) ret.push_back(filename);
        }
        //cout << "get files:" << endl;
        //for (auto& str : ret) cout << str << endl;
        return ret;
    } else {
        vector<string> ret = vector<string>();
        string inner = prefix.substr(f+1, prefix.length() - f - 1);
        string outer = prefix.substr(0, f);
        std::filesystem::path path(outer);
        std::filesystem::directory_iterator list(path);
        for (auto& iter : list) {
            string filename = iter.path().filename().string();
            if (get_files_start_with(filename, inner))
                ret.push_back(outer + "/" + filename);
        }
        //cout << "get files:" << endl;
        //for (auto& str : ret) cout << str << endl;
        return ret;
    }
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

vector<string> get_directories() {
    std::filesystem::path path(".");
    std::filesystem::directory_iterator list(path);
    vector<string> ret; ret.clear();
    for (auto& iter: list) {
        if (iter.is_directory()) ret.push_back(iter.path().filename().string());
    }
    return ret;
}

bool directory_exist(string str) {
    std::filesystem::path path(str);
    return std::filesystem::exists(path);
}

void create_directory(string str) {
    std::filesystem::create_directory(str);
}

void remove_directory(string str) {
    std::filesystem::remove(str);
}

bool file_exist(string dir, string file) {
    return directory_exist(dir + "/" + file);
}

vector<string> get_lines(string dir, string file) {
    std::ifstream fin(dir+"/"+file);
    vector<string> ret; ret.clear();
    while (!fin.eof()) {
        string s; fin >> s;
        ret.push_back(s);
    }
    fin.close();
    return ret;
}

void save_lines(string dir, string file, const vector<string>& lines) {
    std::ofstream fout(dir+"/"+file);
    for (auto& l : lines) {
        fout << l << endl;
    }
    fout.close();
}

vector<string> single_string_vector(string str){
    vector<string> ret; ret.clear(); ret.push_back(str); return ret;
}

vector<uint> single_uint_vector(uint num) {
    vector<uint> ret; ret.clear(); ret.push_back(num); return ret;
}