#ifndef KONTORECORD_H
#define KONTORECORD_H

#include "../KontoConst.h"
#include "KontoIndex.h"
#include <vector>
#include <string>
#include <functional>

using std::vector;
using std::string;
using std::function;

struct KontoColumnDefinition {
    string name;
    KontoKeyType type;
    uint size;
    bool nullable;
    char* defaultValue;
    uint position;
    KontoColumnDefinition(string _name, KontoKeyType _type, uint _size, bool _nullable = true, 
        bool _isForeign = false, string _foreignTable = "", string _foreignName = "", 
        char* _defaultValue = nullptr): name(_name), type(_type), size(_size), nullable(_nullable)
    {
        if (_defaultValue == nullptr) {
            defaultValue = new char[size]; 
            switch (type) {
                case KT_INT:
                    *(int*)(defaultValue) = DEFAULT_INT_VALUE;
                    break;
                case KT_FLOAT:
                    *(double*)(defaultValue) = DEFAULT_FLOAT_VALUE;
                    break;
                case KT_STRING:
                    memset(defaultValue, 0, size);
                    break;
                case KT_DATE:
                    *(Date*)(defaultValue) = DEFAULT_DATE_VALUE;
            }
        } else {
            defaultValue = new char[size]; 
            memcpy(defaultValue, _defaultValue, size); 
        }
    }
    KontoColumnDefinition(){}
    KontoColumnDefinition(const KontoColumnDefinition& def):
        name(def.name), type(def.type), size(def.size), nullable(def.nullable),
        position(def.position) 
    {
        assert(def.defaultValue != nullptr);
        defaultValue = new char[size]; 
        memcpy(defaultValue, def.defaultValue, def.size); 
    }
};

typedef KontoColumnDefinition KontoCDef;

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
    bool operator !=(const KontoRPos& b){return page!=b.page || id!=b.id;}
};

// 查询结果，用向量实现。应保持严格升序。
struct KontoQueryResult {
private:
    friend class KontoTableFile;
    friend class KontoIndex;
    // 应当保持严格升序，主关键字page，副关键字id
    vector<KontoRPos> items;
    bool sorted;
    // 向末尾插入
    void push(const KontoRPos& p){items.push_back(p);}
public:
    // 初始化空的向量。
    KontoQueryResult(){items=vector<KontoRPos>(); sorted = false;}
    // 拷贝构造。
    KontoQueryResult(const KontoQueryResult& r);
    // 两个查询结果求并。
    KontoQueryResult join(KontoQueryResult& b);
    // 两个查询结果求交。
    KontoQueryResult meet(KontoQueryResult& b);
    // 本查询结果减去另一个查询结果，即求集合的差。
    KontoQueryResult substract(KontoQueryResult& b);

    KontoQueryResult append(const KontoQueryResult& b);
    // 查询结果中的记录数目。
    uint size() const {return items.size();}
    // 清空查询结果向量。
    void clear(){items.clear();}
    // 获取查询结果。
    const KontoRPos& get(int id) const {return items[id];}

    void sort();
};

typedef KontoQueryResult KontoQRes;

class KontoTableFile {
    friend class KontoTerminal;
private:
    BufPageManager& pmgr;
    KontoTableFile();
    bool fieldDefined; // 当前表的属性是否已经定义
    vector<KontoCDef> keys;
    vector<KontoIndex*> indices;
    uint recordCount; // 当前表中的记录条数（包括已删除的）
    int fileID;
    int pageCount; // 页的数量
    int recordSize; // 一条记录所占用空间大小（以char=1为单位）
    string filename;
    KontoIndex* primaryIndex;
    // 在当前目录下查找已有的索引文件并加载。
    void loadIndices(); 

    void recreatePrimaryIndex();
    
public:
    ~KontoTableFile();
    // 创建新的表。创建后应该调用defineField声明各个属性，finishDefineField结束声明。
    static KontoResult createFile(string filename, KontoTableFile** handle);
    // 载入已有的表文件。
    static KontoResult loadFile(string filename, KontoTableFile** handle);
    // 获取指向记录位置数据的指针，并指出接下来是读取还是写入。
    char* getDataPointer(const KontoRPos& pos, KontoKeyIndex key, bool write);
    // 定义表的一个属性域。
    KontoResult defineField(KontoCDef& def); 
    // 结束属性域的定义，之后若再尝试定义将会出错。
    KontoResult finishDefineField();
    // 关闭文件。
    KontoResult close();
    // 插入记录，插入后记录的位置通过pos返回。
    KontoResult insertEntry(KontoRPos* pos);
    // 删除指定位置的记录。
    KontoResult deleteEntry(const KontoRPos& pos);
    // 修改指定位置的记录的int域。
    KontoResult editEntryInt(const KontoRPos& pos, KontoKeyIndex key, int datum);
    // 修改指定位置的记录的string域。
    KontoResult editEntryString(const KontoRPos& pos, KontoKeyIndex key, const char* data);
    // 修改指定位置的记录的float域。
    KontoResult editEntryFloat(const KontoRPos& pos, KontoKeyIndex key, double datum);
    
    KontoResult editEntryDate(const KontoRPos& pos, KontoKeyIndex key, Date datum);
    // 读取指定位置记录的int域。
    KontoResult readEntryInt(const KontoRPos& pos, KontoKeyIndex key, int& out);
    // 读取指定位置记录的string域。
    KontoResult readEntryString(const KontoRPos& pos, KontoKeyIndex key, char* out);
    // 读取指定位置记录的float域。
    KontoResult readEntryFloat(const KontoRPos& pos, KontoKeyIndex key, double& out);

    KontoResult readEntryDate(const KontoRPos& pos, KontoKeyIndex key, Date& out);
    // 查询表中某个int域满足某条件的结果，其中判定条件由cond指定，查询结果储存在out中。
    KontoResult queryEntryInt(const KontoQRes& from, KontoKeyIndex key, function<bool(int)> cond, KontoQRes& out);
    // 查询表中某个string域满足某条件的结果，其中判定条件由cond指定，查询结果储存在out中。
    KontoResult queryEntryString(const KontoQRes& from, KontoKeyIndex key, function<bool(const char*)> cond, KontoQRes& out);
    // 查询表中某个float域满足某条件的结果，其中判定条件由cond指定，查询结果储存在out中。
    KontoResult queryEntryFloat(const KontoQRes& from, KontoKeyIndex key, function<bool(double)> cond, KontoQRes& out);
    
    KontoResult queryEntryDate(const KontoQRes& from, KontoKeyIndex key, function<bool(Date)> cond, KontoQRes& out);
    // 获取所有记录位置组成的向量，用于新的查询。
    KontoResult allEntries(KontoQRes& out);
    // 获取某属性对应的属性编号。
    KontoResult getKeyIndex(const char* key, KontoKeyIndex& out);
    // 获取一条记录的大小，以char=1为单位
    uint getRecordSize();
    // 获取某一条记录，以pos指定，将数据存储到dest中
    KontoResult getDataCopied(const KontoRPos& pos, char* dest);
    // 创建索引表并与该数据表绑定，handle非空时将存储创建的索引表的指针
    KontoResult createIndex(const vector<KontoKeyIndex>& keyIndices, KontoIndex** handle);
    // 删除所有索引表
    void removeIndices();
    // 向所有已经关联的索引表中添加记录
    KontoResult insertIndex(const KontoRPos& pos);
    // 从已经关联的索引表中删除记录
    KontoResult deleteIndex(const KontoRPos& pos);
    // 重新生成索引表
    KontoResult recreateIndices();
    // 获取索引表的指针
    KontoIndex* getIndex(uint id);

    uint getIndexCount();

    bool hasPrimaryKey();
    
    KontoIndex* getIndex(const vector<KontoKeyIndex>& keyIndices);
    
    KontoIndex* getPrimaryIndex();
    // 设置某一条记录的某一个int域
    KontoResult setEntryInt(char* record, KontoKeyIndex key, int datum);
    // 设置某一条记录的某一个string域
    KontoResult setEntryString(char* record, KontoKeyIndex key, const char* data);
    // 设置某一条记录的某一个float域
    KontoResult setEntryFloat(char* record, KontoKeyIndex key, double datum);

    KontoResult setEntryDate(char* record, KontoKeyIndex key, Date datum);

    // The first two bytes will be ignored for they record id and flags
    KontoResult insertEntry(char* record, KontoRPos* pos);

    char* getRecordPointer(const KontoRPos& pos, bool write);

    KontoResult getKeyNames(const vector<uint>& keyIndices, vector<string>& out); 

    KontoResult insertIndex(const KontoRPos& pos, KontoIndex* dest);

    void rewriteKeyDefinitions();

    KontoResult alterAddPrimaryKey(const vector<uint>& primaryKeys);
    KontoResult alterDropPrimaryKey();
    KontoResult alterAddForeignKey(string name, const vector<uint>& foreignKeys, string foreignTable, const vector<string>& foreignName);
    KontoResult alterDropForeignKey(string name);
    KontoResult alterAddColumn(const KontoCDef& def);
    KontoResult alterDropColumn(string name);
    KontoResult alterRenameColumn(string old, string newname);
    KontoResult alterChangeColumn(string original, const KontoCDef& newdef);

    void getPrimaryKeys(vector<uint>& cols);

    void getForeignKeys(
        vector<string>& fknames, 
        vector<vector<uint>>& cols, 
        vector<string>& foreignTable, 
        vector<vector<string>>& foreignName);

    void drop();

    KontoResult insert(char* record);

    KontoResult dropIndex(const vector<uint>& cols);

    void debugIndex(const vector<uint>& cols);

    void printTableHeader(bool pos = false);
    void printTableEntry(const KontoRPos& item, bool pos = false);
    void printTable(bool meta = false, bool pos = false);
    void printTable(const KontoQRes& list, bool pos);

    void queryEntryInt(const KontoQRes& from, KontoKeyIndex key, OperatorType op, int rvalue, KontoQRes& out);
    void queryEntryFloat(const KontoQRes& from, KontoKeyIndex key, OperatorType op, double rvalue, KontoQRes& out);
    void queryEntryString(const KontoQRes& from, KontoKeyIndex key, OperatorType op, const char* rvalue, KontoQRes& out);
    void queryEntryDate(const KontoQRes& from, KontoKeyIndex key, OperatorType op, Date rvalue, KontoQRes& out);

    void queryEntryInt(const KontoQRes& from, KontoKeyIndex key, OperatorType op, 
        int lvalue, int rvalue, KontoQRes& out);
    void queryEntryFloat(const KontoQRes& from, KontoKeyIndex key, OperatorType op, 
        double lvalue, double rvalue, KontoQRes& out);
    void queryEntryString(const KontoQRes& from, KontoKeyIndex key, OperatorType op, 
        const char* lvalue, const char* rvalue, KontoQRes& out);
    void queryEntryDate(const KontoQRes& from, KontoKeyIndex key, OperatorType op, 
        Date lvalue, Date rvalue, KontoQRes& out);

    void queryCompare(const KontoQRes& from, KontoKeyIndex k1, KontoKeyIndex k2, 
        OperatorType op, KontoQRes& out);

    void deletes(const KontoQRes& items);

    static bool checkDeletedFlags(uint flags);
};

#endif