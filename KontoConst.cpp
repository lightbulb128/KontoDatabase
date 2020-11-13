#include <iostream>
#include <cstring>

using std::string;

string get_filename(string filename) {
    return filename+".txt";
}

void debug_assert(bool assertion, string message) {
    if (!assertion) printf("Assertion failed: %s\n", message);
}