rm folder/*
g++ -std=c++17 ../record/KontoRecord.cpp ../record/KontoIndex.cpp ../KontoConst.cpp ../db/KontoTerm.cpp ../db/KontoLexer.cpp test.cpp
./a.out