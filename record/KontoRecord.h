#ifndef KONTORECORD_H
#define KONTORECORD_H

#include "../filesystem/bufmanager/BufPageManager.h"
#include "../filesystem/fileio/FileManager.h"
#include "../KontConst.h"
#include <vector>
#include <string>
#include <functional>

using std::vector;
using std::string;

typedef BufPageManager KontoPageManager;
typedef FileManager KontoFileManager;
typedef unsigned int* KontoPage;

typedef unsigned int KontoKeyIndex; 

struct KontoRPos {
    int page; int id;
    KontoRPos(int pageID, int pos):page(pageID),id(pos){}
    KontoRPos(){page=id=0;}
    bool operator <(const KontoRPos& b){return page<b.page || (page==b.page && id<b.id);}
    bool operator ==(const KontoRPos& b){return page==b.page && id==b.id;}
};

struct KontoQueryResult {
private:
    friend class KontoTableFile;
    // 应当保持严格升序，主关键字page，副关键字id
    vector<KontoRPos> items;
    void push(const KontoRPos& p){items.push_back(p);}
public:
    KontoQueryResult(){items=vector<KontoRPos>();}
    KontoQueryResult(const KontoQueryResult& r);
    KontoQueryResult join(const KontoQueryResult& b);
    KontoQueryResult meet(const KontoQueryResult& b);
    KontoQueryResult substract(const KontoQueryResult& b);
    uint size() const {return items.size();}
    void clear(){items.clear();}
    KontoRPos& get(int id){return items[id];}
};

typedef KontoQueryResult KontoQRes;

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
    KontoResult checkPosition(KontoRPos& pos);
    uint* getDataPointer(KontoRPos& pos, KontoKeyIndex key, bool write);
    KontoResult defineField(int size, KontoKeyType type, const char* key); 
    KontoResult finishDefineField();
    KontoResult close();
    KontoResult insertEntry(KontoRPos* pos);
    KontoResult deleteEntry(KontoRPos& pos);
    KontoResult editEntryInt(KontoRPos& pos, KontoKeyIndex key, int datum);
    KontoResult editEntryString(KontoRPos& pos, KontoKeyIndex key, char* data);
    KontoResult editEntryFloat(KontoRPos& pos, KontoKeyIndex key, double datum);
    KontoResult readEntryInt(KontoRPos& pos, KontoKeyIndex key, int& out);
    KontoResult readEntryString(KontoRPos& pos, KontoKeyIndex key, char* out);
    KontoResult readEntryFloat(KontoRPos& pos, KontoKeyIndex key, double& out);
    KontoResult queryEntryInt(KontoQRes& from, KontoKeyIndex key, function<bool(int)> cond, KontoQRes& out);
    KontoResult queryEntryString(KontoQRes& from, KontoKeyIndex key, function<bool(char*)> cond, KontoQRes& out);
    KontoResult queryEntryFloat(KontoQRes& from, KontoKeyIndex key, function<bool(double)> cond, KontoQRes& out);
    KontoResult allEntries(KontoQRes& out);
    KontoResult getKeyIndex(const char* key, KontoKeyIndex& out);
    void debugtest();
};

#endif