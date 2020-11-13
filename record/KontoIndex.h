#ifndef KONTOINDEX_H
#define KONTOINDEX_H

#include "../filesystem/bufmanager/BufPageManager.h"
#include "../filesystem/fileio/FileManager.h"
#include "../KontoConst.h"
#include "KontoRecord.h"
#include <vector>
#include <string>
#include <functional>

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
    BufPageManager* pmgr;
    FileManager* fmgr;
    vector<KontoKeyType> keyTypes;
    vector<uint> keyPositions;
    vector<uint> keySizes;
    string filename;
    uint indexSize;
    KontoIndex(BufPageManager* Pmgr, FileManager* Fmgr);
    int fileID;
    int pageCount;
    static int compare(uint* d1, uint* d2, KontoKeyType type);
    int compare(uint* record, uint* index);
    void setKey(uint* dest, uint* record, KontoRPos& pos);
    KontoResult insertRecur(uint* record, KontoRPos& pos, uint pageID);
    KontoResult split(uint pageID);
    KontoResult queryIposRecur(uint* record, KontoIPos& out, uint pageID, bool equal);
    KontoResult queryIpos(uint* record, KontoIPos& out, bool equal);
    KontoRPos getRPos(KontoIPos& pos);
    KontoResult getNext(KontoIPos& pos, KontoIPos& out);
public:
    static KontoResult createIndex(string filename, KontoIndex** handle, BufPageManager* pManager, FileManager* fManager, 
        vector<KontoKeyType> ktypes, vector<uint> kposs, vector<uint> ksizes);
    static KontoResult loadIndex(string filename, KontoIndex** handle, BufPageManager* pManager, FileManager* fManager);
    static string getIndexFilename(string database, vector<string> keyNames); 
    KontoResult insert(uint* record, KontoRPos& pos);
    KontoResult remove(uint* record); // TODO: delete unique
    KontoResult queryLE(uint* record, KontoRPos& out); // 返回不大于key的最大记录
    KontoResult queryL(uint* record, KontoRPos& out);
    KontoResult close();

    void debugPrintKey(uint* ptr);
    void debugPrintPage(int pageID);
    void debugPrint();
};

#endif