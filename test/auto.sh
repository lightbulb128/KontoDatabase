rm testDB/*
g++ -std=c++17 ../src/KontoRecord.cpp ../src/KontoIndex.cpp ../src/KontoConst.cpp ../src/KontoTerm.cpp ../src/KontoLexer.cpp ../src/KontoMain.cpp
./a.out