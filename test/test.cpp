#include "../record/KontoRecord.h"
#include <iostream>
#include <string.h>
#include <math.h>
#include "../db/KontoTerm.h"

using std::cout;
using std::endl; 
using std::to_string;

/*
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
    KontoTableFile* table;
    int k;
    int res = KontoTableFile::createFile("testfile", &table);
    cout << "file created\n" << endl;
    KontoCDef def = KontoCDef("strfield", KT_STRING, 32);
    table->defineField(def);
    def = KontoCDef("intfield", KT_INT, 4);
    table->defineField(def);
    def = KontoCDef("floatfield", KT_FLOAT, 8);
    table->defineField(def);
    table->finishDefineField();
    KontoKeyIndex key1, key2, key3;
    table->getKeyIndex("strfield", key1);
    table->getKeyIndex("intfield", key2);
    table->getKeyIndex("floatfield", key3);
    int c = 100;
    KontoRPos krp[c*2];
    char buffer[table->getRecordSize()];
    for (int i=0;i<c;i++) {
        printf("i=%d\n",i);
        string str = to_string(i)+"hel";
        char cstr[20]; strcpy(cstr, str.c_str());
        table->setEntryInt(buffer, key2, i*i*i);
        table->setEntryString(buffer, key1, cstr);
        table->setEntryFloat(buffer, key3, sqrt(i));
        table->insertEntry(buffer, &krp[i]);
    }
    table->printTable(true, true);
    table->close();
    res = KontoTableFile::loadFile("testfile", &table);
    table->debugtest();
    for (int i=c;i<c*2;i++) {
        printf("i=%d\n",i);
        table->insertEntry(&krp[i]);
        cout << krp[i].page << " " << krp[i].id << endl;
        string str = to_string(i)+"hel";
        char cstr[20]; strcpy(cstr, str.c_str());
        table->editEntryInt(krp[i], key2, i*i*i);
        table->editEntryString(krp[i], key1, cstr);
        table->editEntryFloat(krp[i], key3, sqrt(i));
    }
    for (int i=0;i<c*2;i++) {
        cout << "i=" << i << ":"; table->printRecord(krp[i], true); cout << endl;
    }
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
        cout << "i=" << i << ":"; table->printRecord(p); cout << endl;
    }
    table->close();
    res = KontoTableFile::loadFile("testfile", &table);
    table->debugtest();
    res = table->allEntries(query1);
    qs = query1.size();
    for (int i=0;i<qs;i++) {
        KontoRPos p = query1.get(i);
        cout << "i=" << i << ":"; table->printRecord(p); cout << endl;
    }
    table->close();
    return 0;
}

void test_load(){
    KontoTableFile* table;
    KontoResult res = KontoTableFile::loadFile("testfile", &table);
    table->debugtest();
    KontoQRes query1;
    res = table->allEntries(query1);
    int qs = query1.size();
    for (int i=0;i<qs;i++) {
        KontoRPos p = query1.get(i);
        cout << "i=" << i << ":"; table->printRecord(p); cout << endl;
    }
    table->close();
}

int test_index() {
    // use a small SPLIT_UPPERBOUND, e.g. 500
    // create a table and set up 3 indices
    // add records and simultaneously add them into the indices
    KontoTableFile* table;
    // create table
    KontoResult result = KontoTableFile::createFile("testfile", &table);
    KontoCDef def = KontoCDef("str", KT_STRING, 40);
    table->defineField(def);
    def = KontoCDef("num", KT_INT, 4);
    table->defineField(def);
    table->finishDefineField();
    KontoKeyIndex keyNum; result = table->getKeyIndex("num", keyNum);
    KontoKeyIndex keyStr; result = table->getKeyIndex("str", keyStr);
    printf("Table created\n");
    // create indices
    KontoIndex *ptr1, *ptr2, *ptr3;
    vector<uint> keyList = vector<uint>(); keyList.push_back(keyNum);
    result = table->createIndex(keyList, &ptr1);
    keyList = vector<uint>(); keyList.push_back(keyStr);
    result = table->createIndex(keyList, &ptr2);
    keyList = vector<uint>();
    keyList.push_back(keyNum); keyList.push_back(keyStr);
    result = table->createIndex(keyList, &ptr3);
    printf("Index created\n");
    printf("record size = %d\n", table->getRecordSize());
    KontoIndex* ptr = table->getIndex(2);
    // repetitve keys
    int data[40] = {
        1,3,2,4,1,4,3,2,1,2,3,4,4,3,1,2,2,3,1,4,
        1,4,3,2,4,1,2,3,1,3,2,4,4,1,2,3,3,1,2,4};
    string datastr[40] = {
        "Alice", "Bob", "Cindy", "David", "Eric",
        "Frank", "Gina", "Helen", "Illa", "Jack",
        "Kelvin", "Louis", "Mike", "Nancy", "Owen",
        "Peter", "Quie", "Rose", "Sarah", "Tina",
        "America", "Belgium", "Canada", "Denmark", "Estonia",
        "France", "Germany", "Holland", "Iran", "Jordan",
        "Korea", "Laos", "Mexico", "Nigeria", "Orthodox",
        "Panama", "Queensland", "Russia", "Scotland", "Turkey"
    };
    for (int i=0;i<20;i++) {
        printf("i=%d\n", i);
        KontoRPos pos; result = table->insertEntry(&pos);
        table->editEntryInt(pos, keyNum, data[i]*data[i]);
        char p[40]; strcpy(p, datastr[i].c_str());
        table->editEntryString(pos, keyStr, p);
        table->insertIndex(pos);
    }
    //ptr3->debugPrint();
    table->close();
    // reopen the file
    KontoTableFile::loadFile("testfile", &table);
    //table->debugtest();
    ptr = table->getIndex(2);
    //ptr->debugPrint();
    // recreate the indices
    for (int i=20;i<40;i++) {
        printf("i=%d\n",i);
        KontoRPos pos; result = table->insertEntry(&pos);
        table->editEntryInt(pos, keyNum, data[i]*data[i]);
        char p[20]; strcpy(p, datastr[i].c_str());
        table->editEntryString(pos, keyStr, p);
    }
    table->recreateIndices();
    cout << "recreated\n";
    keyList = vector<uint>();
    //cout << "keynum = " << keyNum << " keystr = " << keyStr << endl;
    keyList.push_back(keyNum); keyList.push_back(keyStr);
    ptr = table->getIndex(keyList);
    assert(ptr != nullptr);
    cout << "got index\n"; 
    //ptr->debugPrint();
    // test query
    char* record = new char[table->getRecordSize()];
    table->setEntryInt(record, keyNum, 4);
    table->setEntryString(record, keyStr, "Scotland");
    KontoRPos qpos;
    ptr->queryLE(record, qpos);
    table->printRecord(qpos); cout << endl;
    ptr->queryL(record, qpos);
    table->printRecord(qpos); cout << endl;
    delete[] record;
    char* rec1 = new char[table->getRecordSize()];
    char* rec2 = new char[table->getRecordSize()];
    table->setEntryInt(rec1, keyNum, 4);
    table->setEntryString(rec1, keyStr, "Scotland");
    table->setEntryInt(rec2, keyNum, 16);
    table->setEntryString(rec2, keyStr, "Mike");
    KontoQRes qres;
    cout << "query interval 1" << endl;
    ptr->queryInterval(rec1, rec2, qres);
    for (int i=0;i<qres.size();i++) {
        table->printRecord(qres.get(i)); cout << endl;
    }
    cout << "query interval 2" << endl;
    ptr->queryInterval(rec1, nullptr, qres);
    for (int i=0;i<qres.size();i++) {
        table->printRecord(qres.get(i)); cout << endl;
    }
    cout << "query interval 3" << endl;
    ptr->queryInterval(nullptr, rec2, qres);
    for (int i=0;i<qres.size();i++) {
        table->printRecord(qres.get(i)); cout << endl;
    }
    cout << "query interval 4" << endl;
    ptr->queryInterval(nullptr, nullptr, qres);
    for (int i=0;i<qres.size();i++) {
        table->printRecord(qres.get(i)); cout << endl;
    }
    cout << "query interval 5" << endl;
    ptr->queryInterval(rec2, rec1, qres);
    for (int i=0;i<qres.size();i++) {
        table->printRecord(qres.get(i)); cout << endl;
    }
    cout << "query interval 6" << endl;
    ptr->queryInterval(rec1, rec1, qres);
    for (int i=0;i<qres.size();i++) {
        table->printRecord(qres.get(i)); cout << endl;
    }
    cout << "query interval 7" << endl;
    ptr->queryInterval(rec1, rec2, qres);
    for (int i=0;i<qres.size();i++) {
        table->printRecord(qres.get(i)); cout << endl;
    }
    cout << "query interval 8" << endl;
    ptr->queryInterval(rec1, rec2, qres, true, true);
    for (int i=0;i<qres.size();i++) {
        table->printRecord(qres.get(i)); cout << endl;
    }
    cout << "query interval 9" << endl;
    ptr->queryInterval(rec1, rec2, qres, false, true);
    for (int i=0;i<qres.size();i++) {
        table->printRecord(qres.get(i)); cout << endl;
    }
    cout << "query interval 10" << endl;
    ptr->queryInterval(rec1, rec2, qres, false, false);
    for (int i=0;i<qres.size();i++) {
        table->printRecord(qres.get(i)); cout << endl;
    }
    table->close();
    return 0;
}

int test_index_2() {
    // use a small SPLIT_UPPERBOUND, e.g. 500
    KontoTableFile* table;
    // create table
    KontoResult result = KontoTableFile::createFile("testfile", &table);
    KontoCDef def = KontoCDef("str", KT_STRING, 40);
    table->defineField(def);
    def = KontoCDef("num", KT_INT, 4);
    table->defineField(def);
    table->finishDefineField();
    KontoKeyIndex keyNum; result = table->getKeyIndex("num", keyNum);
    KontoKeyIndex keyStr; result = table->getKeyIndex("str", keyStr);
    printf("Table created\n");
    // create indices
    vector<uint> keyList = vector<uint>(); keyList.push_back(keyNum);
    result = table->createIndex(keyList, nullptr);
    keyList = vector<uint>(); keyList.push_back(keyStr);
    result = table->createIndex(keyList, nullptr);
    keyList = vector<uint>();
    keyList.push_back(keyNum); keyList.push_back(keyStr);
    result = table->createIndex(keyList, nullptr);
    printf("Index created\n");
    printf("record size = %d\n", table->getRecordSize());
    // repetitve keys
    int data[20] = {
        1,3,2,4,1,  4,3,2,1,2,  3,4,4,3,1,  2,2,3,1,4};
    string datastr[20] = {
        "Alice", "Bob", "Cindy", "David", "Eric",
        "Frank", "Gina", "Helen", "Illa", "Jack",
        "Kelvin", "Louis", "Mike", "Nancy", "Owen",
        "Peter", "Quie", "Rose", "Sarah", "Tina"
    };
    for (int i=0;i<20;i++) {
        printf("i=%d\n", i);
        KontoRPos pos; result = table->insertEntry(&pos);
        table->editEntryInt(pos, keyNum, data[i]*data[i]);
        char p[20]; strcpy(p, datastr[i].c_str());
        table->editEntryString(pos, keyStr, p);
        table->insertIndex(pos);
    }
    KontoIndex* ptr = table->getIndex(2);
    ptr->debugPrint();
    char* rec1 = new char[table->getRecordSize()];
    char* rec2 = new char[table->getRecordSize()];
    table->setEntryInt(rec1, keyNum, 4);
    table->setEntryString(rec1, keyStr, "Jack");
    table->setEntryInt(rec2, keyNum, 16);
    table->setEntryString(rec2, keyStr, "Mike");
    KontoRPos query; 
    ptr->queryLE(rec1, query);
    table->deleteIndex(query);
    ptr->debugPrint();
    KontoQRes qres;
    cout << "query interval 1" << endl;
    ptr->queryInterval(nullptr, nullptr, qres);
    for (int i=0;i<qres.size();i++) {
        table->printRecord(qres.get(i)); cout << endl;
    }
    ptr->queryLE(rec1, query);
    cout << "query LE" << endl;
    table->printRecord(query); cout << endl;
    delete[] rec1; delete[] rec2;
    table->close();
    return 0;
}

void test_alter () {
    KontoTableFile* table;
    // create table
    KontoResult result = KontoTableFile::createFile("testfile", &table);
    KontoCDef def = KontoCDef("str", KT_STRING, 100);
    table->defineField(def);
    def = KontoCDef("num", KT_INT, 4);
    table->defineField(def);
    table->finishDefineField();
    KontoKeyIndex keyNum; result = table->getKeyIndex("num", keyNum);
    KontoKeyIndex keyStr; result = table->getKeyIndex("str", keyStr);
    printf("Table created\n");
    string datastr[40] = {
        "Alice", "Bob", "Cindy", "David", "Eric",
        "Frank", "Gina", "Helen", "Illa", "Jack",
        "Kelvin", "Louis", "Mike", "Nancy", "Owen",
        "Peter", "Quie", "Rose", "Sarah", "Tina",
        "America", "Belgium", "Canada", "Denmark", "Estonia",
        "France", "Germany", "Holland", "Iran", "Jordan",
        "Korea", "Laos", "Mexico", "Nigeria", "Orthodox",
        "Panama", "Queensland", "Russia", "Scotland", "Turkey"
    };
    char buffer[table->getRecordSize()];
    for (int i=0;i<100;i++) {
        table->setEntryInt(buffer, keyNum, i + 100 * (int((1+sin(i))*100)%10));
        table->setEntryString(buffer, keyStr, datastr[i%40].c_str());
        table->insertEntry(buffer, nullptr);
    }
    vector<uint> primaryKeys; primaryKeys.push_back(keyNum);
    table->alterAddPrimaryKey(primaryKeys);
    KontoIndex* indexPtr = table->getIndex(0);
    assert(indexPtr != nullptr);
    cout << "PRINT ALPHA" << endl;
    table->debugtest();
    indexPtr->debugPrint();
    assert(table->hasPrimaryKey());

    char defaultValue[8]; 
    *(double*)(defaultValue) = 3.1415;
    table->alterAddColumn(KontoCDef("float", KT_FLOAT, 8, true, false, "", "", defaultValue));
    cout << "PRINT BETA" << endl;
    indexPtr = table->getIndex(0);
    assert(indexPtr != nullptr);
    indexPtr->debugPrint();
    assert(table->hasPrimaryKey());

    uint keyFloat;
    result = table->getKeyIndex("float", keyFloat);
    KontoQRes q; table->allEntries(q);
    cout << q.size() << endl;
    for (int i=0;i<q.size();i++) {
        KontoRPos pos = q.get(i);
        table->editEntryFloat(pos, keyFloat, sqrt(i));
    }
    cout << "PRINT GAMMA" << endl;
    table->debugtest();
    indexPtr->debugPrint();
    assert(table->hasPrimaryKey());

    table->alterDropColumn("float");
    indexPtr = table->getIndex(0);
    assert(indexPtr != nullptr);
    cout << "PRINT DELTA" << endl;
    table->debugtest();
    indexPtr->debugPrint();
    assert(table->hasPrimaryKey());

    table->alterRenameColumn("str", "name");
    table->alterRenameColumn("num", "index");
    cout << "PRINT EPSILON" << endl;
    table->debugtest();
    indexPtr->debugPrint();
    assert(table->hasPrimaryKey());

    table->alterChangeColumn("name", KontoCDef("numeric", KT_FLOAT, 8, true, false, "", "", defaultValue));
    cout << "PRINT ZETA" << endl;
    table->debugtest();
    indexPtr->debugPrint();

    table->alterDropPrimaryKey();
    assert(!table->hasPrimaryKey());
    primaryKeys.clear(); 
    table->getKeyIndex("index", keyNum); table->getKeyIndex("numeric", keyFloat);
    primaryKeys.push_back(keyNum); primaryKeys.push_back(keyFloat);
    result = table->alterAddPrimaryKey(primaryKeys);
    cout << result << endl;
    indexPtr = table->getPrimaryIndex(); 
    assert(table->hasPrimaryKey());
    assert(indexPtr != nullptr);
    cout << "PRINT ETA" << endl;
    indexPtr->debugPrint();
    table->close();
}

int test_debug() {
    get_files("folder/__t");
    return 0;
}
*/

KontoTerminal terminal;

int main(int argc, const char* argv[]){
    bool flag = true;
    if (argc>1 && (strcmp(argv[1], "-F"))==0) flag = false;
    terminal.main(flag);
    //test_alter();
    return 0;
}