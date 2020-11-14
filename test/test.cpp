#include "../record/KontoRecord.h"
#include <iostream>
#include <string.h>
#include <math.h>

using std::cout;
using std::endl; 
using std::to_string;

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
    res = KontoTableFile::loadFile("testfile", &table);
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
    // when testing with a single num as key, use SPLIT_UPPERBOUND = 80
    // when testing with a single str as key, use SPLIT_UPPERBOUND = 100
    // create a table and set up 3 indices
    // add records and simultaneously add them into the indices
    KontoTableFile* table;
    // create table
    KontoResult result = KontoTableFile::createFile("testfile", &table);
    table->defineField(10, KT_STRING, "str");
    table->defineField(1, KT_INT, "num");
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
        char p[20]; strcpy(p, datastr[i].c_str());
        table->editEntryString(pos, keyStr, p);
        table->insertIndex(pos);
    }
    KontoIndex* ptr = table->getIndex(2);
    ptr->debugPrint();
    table->close();
    // reopen the file
    KontoTableFile::loadFile("testfile", &table);
    ptr = table->getIndex(2);
    ptr->debugPrint();
    // recreate the indices
    for (int i=20;i<40;i++) {
        printf("i=%d\n",i);
        KontoRPos pos; result = table->insertEntry(&pos);
        table->editEntryInt(pos, keyNum, data[i]*data[i]);
        char p[20]; strcpy(p, datastr[i].c_str());
        table->editEntryString(pos, keyStr, p);
    }
    table->recreateIndices();
    ptr = table->getIndex(2);
    ptr->debugPrint();
    // test query
    uint* record = new uint[table->getRecordSize()];
    table->setEntryInt(record, keyNum, 4);
    table->setEntryString(record, keyStr, "Scotland");
    KontoRPos qpos;
    ptr->queryLE(record, qpos);
    table->printRecord(qpos); cout << endl;
    ptr->queryL(record, qpos);
    table->printRecord(qpos); cout << endl;
    delete[] record;
    uint* rec1 = new uint[table->getRecordSize()];
    uint* rec2 = new uint[table->getRecordSize()];
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
    // when testing with a single num as key, use SPLIT_UPPERBOUND = 80
    // when testing with a single str as key, use SPLIT_UPPERBOUND = 100
    // create a table and set up 3 indices
    // add records and simultaneously add them into the indices
    KontoTableFile* table;
    // create table
    KontoResult result = KontoTableFile::createFile("testfile", &table);
    table->defineField(10, KT_STRING, "str");
    table->defineField(1, KT_INT, "num");
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
    uint* rec1 = new uint[table->getRecordSize()];
    uint* rec2 = new uint[table->getRecordSize()];
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

int test_debug() {
    auto p = get_files("testfile.index.");
    for (auto name: p) {
        auto k = get_index_key_names(name);
        cout << "filename " << name << endl;
        for (auto key: k) cout << " " << key;
        cout << endl;
    }
    return 0;
}


int main(){
    test_index_2();
    return 0;
}