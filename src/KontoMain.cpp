#include <iostream>
#include <string.h>
#include <math.h>
#include "KontoTerm.h"

using std::cout;
using std::endl; 
using std::to_string;

KontoTerminal* terminal;

int main(int argc, const char* argv[]){
    bool flag = true;
    if (argc>1 && (strcmp(argv[1], "-F"))==0) flag = false;
    terminal = KontoTerminal::getInstance();
    terminal->main(flag);
    return 0;
}