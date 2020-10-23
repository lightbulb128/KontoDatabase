#include "record/KontoRecord.h"
#include <iostream>
#include <string.h>
#include <math.h>
using std::cout;
using std::endl; 
int main() {
    FileManager* fm = new FileManager();
	BufPageManager* bpm = new BufPageManager(fm);
    KontoTableFile* table;
    //fm->createFile("filename.txt");
    int k;
    //fm->openFile("filename.txt", k);
    int res = KontoTableFile::createFile("testfile2.txt", &table, bpm, fm);
    table->defineField(8, KT_STRING, "strfield");
    table->defineField(1, KT_INT, "intfield");
    table->defineField(2, KT_FLOAT, "floatfield");
    table->finishDefineField();
    KontoKeyIndex key1, key2, key3;
    table->getKeyIndex("strfield", key1);
    table->getKeyIndex("intfield", key2);
    table->getKeyIndex("floatfield", key3);
    int c = 10000;
    KontoRecordPosition krp[c];
    for (int i=0;i<c;i++) {
        table->insertEntry(&krp[i]);
        string str = to_string(i)+"hel";
        char cstr[20]; strcpy(cstr, str.c_str());
        table->editEntry(krp[i], key2, i%2==0?10:i*i*i);
        table->editEntryString(krp[i], key1, cstr);
        table->editEntryFloat(krp[i], key3, sqrt(i));
    }
    cout << "finished" << endl;
    for (int i=0;i<c;i++) {
        char cstr[20]; unsigned int x; double s;
        table->readEntryString(krp[i], key1, cstr);
        table->readEntryFloat(krp[i], key3, s);
        table->readEntry(krp[i], key2, x);
        cout << "i=" << i << ":" << cstr << "," << x << "," << s << endl;
    }
    table->close();
    res = KontoTableFile::loadFile("testfile2.txt", &table, bpm, fm);
    table->debugtest();
    for (int i=0;i<c;i++) {
        char cstr[20]; unsigned int x; double s;
        table->readEntryString(krp[i], key1, cstr);
        table->readEntryFloat(krp[i], key3, s);
        table->readEntry(krp[i], key2, x);
        cout << "i=" << i << ":" << cstr << "," << x << "," << s << endl;
    }
    table->close();
}