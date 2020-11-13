#include "../record/KontoRecord.h"
#include <iostream>
#include <string.h>
#include <math.h>
using std::cout;
using std::endl; 

bool isd2(int i){
    return (i/10)%10==2;
}
bool ish3(double i){
    return (i-int(i)<0.3);
}

int test_record() {
    // create a record file with three fields: strfield, intfield and floatfield.
    // add 1000 records into the record and then reload the file
    // do a query and output the results.
    FileManager* fm = new FileManager();
	BufPageManager* bpm = new BufPageManager(fm);
    KontoTableFile* table;
    int k;
    int res = KontoTableFile::createFile("testfile", &table, bpm, fm);
    table->defineField(8, KT_STRING, "strfield");
    table->defineField(1, KT_INT, "intfield");
    table->defineField(2, KT_FLOAT, "floatfield");
    table->finishDefineField();
    KontoKeyIndex key1, key2, key3;
    table->getKeyIndex("strfield", key1);
    table->getKeyIndex("intfield", key2);
    table->getKeyIndex("floatfield", key3);
    int c = 100;
    KontoRPos krp[c*2];
    for (int i=0;i<c;i++) {
        table->insertEntry(&krp[i]);
        string str = to_string(i)+"hel";
        char cstr[20]; strcpy(cstr, str.c_str());
        table->editEntryInt(krp[i], key2, i*i*i);
        table->editEntryString(krp[i], key1, cstr);
        table->editEntryFloat(krp[i], key3, sqrt(i));
    }
    cout << "finished1" << endl;
    for (int i=0;i<c;i++) {
        char cstr[20]; int x; double s;
        table->readEntryString(krp[i], key1, cstr);
        table->readEntryFloat(krp[i], key3, s);
        table->readEntryInt(krp[i], key2, x);
        cout << "i=" << i << ":" << cstr << "," << x << "," << s << endl;
    }
    table->close();
    res = KontoTableFile::loadFile("testfile", &table, bpm, fm);
    table->debugtest();
    KontoQRes query1, query2;
    res = table->allEntries(query1);
    table->queryEntryInt(query1, key2, isd2, query1);
    res = table->allEntries(query2);
    table->queryEntryFloat(query2, key3, ish3, query2);
    query1 = query1.substract(query2);
    int qs = query1.size();
    for (int i=0;i<qs;i++) {
        KontoRPos p = query1.get(i);
        char cstr[20]; int x; double s;
        table->readEntryString(p, key1, cstr);
        table->readEntryFloat(p, key3, s);
        table->readEntryInt(p, key2, x);
        cout << "i=" << i << ":" << cstr << "," << x << "," << s << endl;
    }
    table->close();
    return 0;
}

int test_index() {
    FileManager* fm = new FileManager();
	BufPageManager* bpm = new BufPageManager(fm);
    KontoTableFile* table;
    KontoResult result = KontoTableFile::createFile("testfile", &table, bpm, fm);
    table->defineField(1, KT_INT, "num");
    table->finishDefineField();
    printf("Table created.\n");
    KontoKeyIndex keyNum; result = table->getKeyIndex("num", keyNum);
    vector<uint> keyList = vector<uint>(); keyList.push_back(keyNum);
    KontoIndex* index; result = table->createIndex(keyList, &index);
    printf("Index created\n");
    printf("record size = %d\n", table->getRecordSize());
    uint* dataBuffer = new uint[table->getRecordSize()];
    index->debugPrint();
    for (int i=0;i<3;i++) {
        printf("i=%d\n", i);
        KontoRPos pos; result = table->insertEntry(&pos);
        table->editEntryInt(pos, keyNum, i*i);
        table->getDataCopied(pos, dataBuffer);
        printf("beforeinsert\n");
        index->insert(dataBuffer, pos);
    }
    delete[] dataBuffer;
    index->close();
    table->close();
    index->debugPrint();
    return 0;
}

int test_debug() {
    FileManager* fm = new FileManager();
	BufPageManager* bpm = new BufPageManager(fm);
    int f1, f2;
    fm->openFile("testfile.txt", f1);
    fm->openFile("testfile.index.num.txt", f2);
    cout << f1 << " " << f2 << endl;
    return 0;
}

int test_debug_2() {
    MyBitMap::initConst();   //新加的初始化
	FileManager* fm = new FileManager();
	BufPageManager* bpm = new BufPageManager(fm);
	fm->createFile("testfile.txt"); //新建文件
	fm->createFile("testfile2.txt");
	int fileID, f2;
	fm->openFile("testfile.txt", fileID); //打开文件，fileID是返回的文件id
    fm->openFile("testfile2.txt", f2);
	cout << fileID << " " << f2 << endl;
    return 0;
}

int main(){
    test_debug_2();
    return 0;
}