#include "../record/KontoRecord.h"
#include <iostream>

using namespace std;

int main() {
	MyBitMap::initConst();   //新加的初始化
	FileManager* fm = new FileManager();
	BufPageManager* bpm = new BufPageManager(fm);
	fm->createFile("testfile.txt"); //新建文件
	fm->createFile("testfile2.txt");
	int fileID, f2;
    MyBitMap::initConst();
	fm->openFile("testfile.txt", fileID); //打开文件，fileID是返回的文件id
    fm->openFile("testfile2.txt", f2);
	cout << fileID << " " << f2 << endl;
}