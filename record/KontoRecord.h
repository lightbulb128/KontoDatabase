#ifndef KONTORECORD_H
#define KONTORECORD_H

#include "../filesystem/bufmanager/BufPageManager.h"
#include "../filesystem/fileio/FileManager.h"
#include "../KontConst.h"
#include <vector>
#include <string>

using std::vector;
using std::string;

typedef BufPageManager KontoPageManager;
typedef FileManager KontoFileManager;
typedef unsigned int* KontoPage;

typedef unsigned int KontoKeyIndex; 

struct KontoRecordPosition {
    int page; int id;
    KontoRecordPosition(int pageID, int pos):page(pageID),id(pos){}
    KontoRecordPosition(){page=id=0;}
};

class KontoTableFile {
private:
    KontoPageManager* pmgr;
    KontoFileManager* fmgr;
    KontoTableFile(KontoPageManager* pManager, KontoFileManager* fManager);
    bool fieldDefined; // 当前表的属性是否已经定义
    vector<uint> keyPosition; // 记录各个属性在一条记录中对应的位置
    vector<uint> keySize; // 记录各个属性的空间（以int大小=4为单位）
    vector<KontoKeyType> keyType;
    vector<string> keyNames;
    uint recordCount; 
    int fileID;
    int pageCount;
    int recordSize; 
public:
    ~KontoTableFile();
    static KontoResult createFile(const char* filename, KontoTableFile** handle, KontoPageManager* pManager, KontoFileManager* fManager);
    static KontoResult loadFile(const char* filename, KontoTableFile** handle, KontoPageManager* pManager, KontoFileManager* fManager);
    KontoResult checkPosition(KontoRecordPosition& pos);
    uint* getDataPointer(KontoRecordPosition& pos, KontoKeyIndex key, bool write);
    KontoResult defineField(int size, KontoKeyType type, const char* key); 
    KontoResult finishDefineField();
    KontoResult close();
    KontoResult insertEntry(KontoRecordPosition* pos);
    KontoResult deleteEntry(KontoRecordPosition& pos);
    KontoResult editEntry(KontoRecordPosition& pos, KontoKeyIndex key, uint datum);
    KontoResult editEntryString(KontoRecordPosition& pos, KontoKeyIndex key, char* data);
    KontoResult editEntryFloat(KontoRecordPosition& pos, KontoKeyIndex key, double datum);
    KontoResult readEntry(KontoRecordPosition& pos, KontoKeyIndex key, uint& out);
    KontoResult readEntryString(KontoRecordPosition& pos, KontoKeyIndex key, char* out);
    KontoResult readEntryFloat(KontoRecordPosition& pos, KontoKeyIndex key, double& out);
    KontoResult getKeyIndex(const char* key, KontoKeyIndex& out);
    void debugtest();
};

#endif