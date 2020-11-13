#ifndef KONTORECORD_H
#define KONTORECORD_H

#include "../filesystem/bufmanager/BufPageManager.h"
#include "../filesystem/fileio/FileManager.h"
#include "../KontoConst.h"
#include "KontoIndex.h"
#include <vector>
#include <string>
#include <functional>

using std::vector;
using std::string;

// 一条记录在一个表中的位置，用页编号和页中记录编号来表示
struct KontoRPos {
    int page; int id;
    // 初始化
    KontoRPos(int pageID, int pos):page(pageID),id(pos){}
    // 默认构造函数
    KontoRPos(){page=id=0;}
    // 主关键字为页编号，次关键字为页中记录编号
    bool operator <(const KontoRPos& b){return page<b.page || (page==b.page && id<b.id);}
    bool operator ==(const KontoRPos& b){return page==b.page && id==b.id;}
};

// 查询结果，用向量实现。应保持严格升序。
struct KontoQueryResult {
private:
    friend class KontoTableFile;
    // 应当保持严格升序，主关键字page，副关键字id
    vector<KontoRPos> items;
    // 向末尾插入
    void push(const KontoRPos& p){items.push_back(p);}
public:
    // 初始化空的向量。
    KontoQueryResult(){items=vector<KontoRPos>();}
    // 拷贝构造。
    KontoQueryResult(const KontoQueryResult& r);
    // 两个查询结果求并。
    KontoQueryResult join(const KontoQueryResult& b);
    // 两个查询结果求交。
    KontoQueryResult meet(const KontoQueryResult& b);
    // 本查询结果减去另一个查询结果，即求集合的差。
    KontoQueryResult substract(const KontoQueryResult& b);
    // 查询结果中的记录数目。
    uint size() const {return items.size();}
    // 清空查询结果向量。
    void clear(){items.clear();}
    // 获取查询结果。
    KontoRPos& get(int id){return items[id];}
};

typedef KontoQueryResult KontoQRes;

class KontoTableFile {
private:
    BufPageManager* pmgr;
    FileManager* fmgr;
    KontoTableFile(BufPageManager* pManager, FileManager* fManager);
    bool fieldDefined; // 当前表的属性是否已经定义
    vector<uint> keyPosition; // 记录各个属性在一条记录中对应的位置
    vector<uint> keySize; // 记录各个属性的空间（以int大小=4为单位）
    vector<KontoKeyType> keyType; // 各个属性的类别
    vector<string> keyNames; // 各个属性的名称
    uint recordCount; // 当前表中的记录条数（包括已删除的）
    int fileID;
    int pageCount; // 页的数量
    int recordSize; // 一条记录所占用空间大小（以int=4为单位）
    string filename;
public:
    ~KontoTableFile();
    // 创建新的表。创建后应该调用defineField声明各个属性，finishDefineField结束声明。
    static KontoResult createFile(string filename, KontoTableFile** handle, BufPageManager* pManager, FileManager* fManager);
    // 载入已有的表文件。
    static KontoResult loadFile(string filename, KontoTableFile** handle, BufPageManager* pManager, FileManager* fManager);
    // 检查一个记录位置是否有效。
    KontoResult checkPosition(KontoRPos& pos);
    // 获取指向记录位置数据的指针，并指出接下来是读取还是写入。
    uint* getDataPointer(KontoRPos& pos, KontoKeyIndex key, bool write);
    // 定义表的一个属性域。
    KontoResult defineField(int size, KontoKeyType type, const char* key); 
    // 结束属性域的定义，之后若再尝试定义将会出错。
    KontoResult finishDefineField();
    // 关闭文件。
    KontoResult close();
    // 插入记录，插入后记录的位置通过pos返回。
    KontoResult insertEntry(KontoRPos* pos);
    // 删除指定位置的记录。
    KontoResult deleteEntry(KontoRPos& pos);
    // 修改指定位置的记录的int域。
    KontoResult editEntryInt(KontoRPos& pos, KontoKeyIndex key, int datum);
    // 修改指定位置的记录的string域。
    KontoResult editEntryString(KontoRPos& pos, KontoKeyIndex key, char* data);
    // 修改指定位置的记录的float域。
    KontoResult editEntryFloat(KontoRPos& pos, KontoKeyIndex key, double datum);
    // 读取指定位置记录的int域。
    KontoResult readEntryInt(KontoRPos& pos, KontoKeyIndex key, int& out);
    // 读取指定位置记录的string域。
    KontoResult readEntryString(KontoRPos& pos, KontoKeyIndex key, char* out);
    // 读取指定位置记录的float域。
    KontoResult readEntryFloat(KontoRPos& pos, KontoKeyIndex key, double& out);
    // 查询表中某个int域满足某条件的结果，其中判定条件由cond指定，查询结果储存在out中。
    KontoResult queryEntryInt(KontoQRes& from, KontoKeyIndex key, function<bool(int)> cond, KontoQRes& out);
    // 查询表中某个string域满足某条件的结果，其中判定条件由cond指定，查询结果储存在out中。
    KontoResult queryEntryString(KontoQRes& from, KontoKeyIndex key, function<bool(char*)> cond, KontoQRes& out);
    // 查询表中某个float域满足某条件的结果，其中判定条件由cond指定，查询结果储存在out中。
    KontoResult queryEntryFloat(KontoQRes& from, KontoKeyIndex key, function<bool(double)> cond, KontoQRes& out);
    // 获取所有记录位置组成的向量，用于新的查询。
    KontoResult allEntries(KontoQRes& out);
    // 获取某属性对应的属性编号。
    KontoResult getKeyIndex(const char* key, KontoKeyIndex& out);

    uint getRecordSize();

    KontoResult getDataCopied(KontoRPos& pos, uint* dest);
    
    KontoResult createIndex(vector<KontoKeyIndex>& keys, KontoIndex** handle);
    void debugtest();
};

#endif