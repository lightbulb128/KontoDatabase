#ifndef KONTOINDEX_H
#define KONTOINDEX_H

#include "../KontoConst.h"
#include "KontoRecord.h"
#include <vector>
#include <string>
#include <functional>

struct KontoQueryResult;
typedef KontoQueryResult KontoQRes;

using std::vector;
using std::string;

struct KontoIPos {
    int page, id;
    KontoIPos(){}
    KontoIPos(int page, int id):page(page),id(id){}
    bool operator ==(const KontoIPos& b){
        return page==b.page && id==b.id;
    }
};

class KontoIndex {
private:
    BufPageManager& pmgr;
    vector<KontoKeyType> keyTypes;
    vector<uint> keyPositions;
    vector<uint> keySizes;
    string filename;
    uint indexSize;
    KontoIndex();
    int fileID;
    int pageCount;
    static int compare(uint* d1, uint* d2, KontoKeyType type);
    int compare(uint* record, uint* index);
    int compareRecords(uint* r1, uint* r2);
    void setKey(uint* dest, uint* record, KontoRPos& pos);
    KontoResult insertRecur(uint* record, KontoRPos& pos, uint pageID);
    KontoResult split(uint pageID);
    KontoResult queryIposRecur(uint* record, KontoIPos& out, uint pageID, bool equal);
    KontoResult queryIpos(uint* record, KontoIPos& out, bool equal);
    KontoResult queryIposFirstRecur(KontoIPos& out, uint pageID);
    KontoResult queryIposFirst(KontoIPos& out);
    KontoResult queryIposLastRecur(KontoIPos& out, uint pageID);
    KontoResult queryIposLast(KontoIPos& out);
    KontoRPos getRPos(KontoIPos& pos);
    KontoResult getNext(KontoIPos& pos, KontoIPos& out);
    KontoResult getPrevious(KontoIPos& pos, KontoIPos& out);
    KontoResult getNext(KontoIPos& pos);
    KontoResult getPrevious(KontoIPos& pos);
    bool isDeleted(KontoIPos& pos);
public:
    static KontoResult createIndex(string filename, KontoIndex** handle, 
        vector<KontoKeyType> ktypes, vector<uint> kposs, vector<uint> ksizes);
    static KontoResult loadIndex(string filename, KontoIndex** handle);
    static string getIndexFilename(string database, vector<string> keyNames); 
    KontoResult insert(uint* record, KontoRPos& pos);
    KontoResult remove(uint* record, KontoRPos& pos); // TODO: delete unique
    KontoResult queryLE(uint* record, KontoRPos& out); // 返回不大于key的最大记录
    KontoResult queryL(uint* record, KontoRPos& out);
    KontoResult queryInterval(uint* lower, uint* upper, KontoQRes& out, 
        bool lowerIncluded = true, 
        bool upperIncluded = false);
    KontoResult close();
    static KontoResult recreate(KontoIndex* original, KontoIndex** handle);

    void debugPrintKey(uint* ptr);
    void debugPrintRecord(uint* ptr);
    void debugPrintPage(int pageID);
    void debugPrint();
};

#endif