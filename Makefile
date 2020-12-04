build/ktdb.out : build/ build/KontoRecord.o build/KontoIndex.o build/KontoLexer.o build/KontoTerm.o build/KontoConst.o build/KontoMain.o
	g++ -std=c++17 build/KontoRecord.o build/KontoIndex.o build/KontoConst.o build/KontoLexer.o build/KontoTerm.o build/KontoMain.o -o build/ktdb.out

build/: 
	mkdir build

build/KontoRecord.o: src/KontoRecord.cpp src/KontoRecord.h
	g++ -std=c++17 src/KontoRecord.cpp -c -o build/KontoRecord.o

build/KontoIndex.o: src/KontoIndex.cpp src/KontoIndex.h
	g++ -std=c++17 src/KontoIndex.cpp -c -o build/KontoIndex.o

build/KontoConst.o: src/KontoConst.cpp src/KontoConst.h
	g++ -std=c++17 src/KontoConst.cpp -c -o build/KontoConst.o

build/KontoLexer.o: src/KontoLexer.cpp src/KontoLexer.h
	g++ -std=c++17 src/KontoLexer.cpp -c -o build/KontoLexer.o

build/KontoTerm.o: src/KontoTerm.cpp src/KontoTerm.h
	g++ -std=c++17 src/KontoTerm.cpp -c -o build/KontoTerm.o

build/KontoMain.o: src/KontoMain.cpp
	g++ -std=c++17 src/KontoMain.cpp -c -o build/KontoMain.o

clean:
	rm build/*
	rmdir build