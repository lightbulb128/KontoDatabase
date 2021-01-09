#ifndef KONTORECORD_H
#define KONTORECORD_H

#include "KontoConst.h"
#include "KontoIndex.h"
#include <vector>
#include <string>
#include <functional>

using std::vector;
using std::string;
using std::function;

// 数据表的列定义
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

// 数据表的列定义
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

// 表查询结果，用向量实现，每个条目为KontoRPos。
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
    /** 将本结果与另一个结果求并。
     * @param b 另一个结果。
     * @return 并。
     * */
    KontoQueryResult join(KontoQueryResult& b);
    /** 将本结果与另一个结果求交。
     * @param b 另一个结果。
     * @return 交。
     * */
    KontoQueryResult meet(KontoQueryResult& b);
    /** 将本结果与另一个结果求集合差。
     * @param b 被减去的集合。
     * @return 差。
     * */
    KontoQueryResult substract(KontoQueryResult& b);
    /** 将另一个结果列表添加到本列表之后。
     * @param b 另一个结果。
     * @return 两结果列表的拼接。
     * */
    KontoQueryResult append(const KontoQueryResult& b);
    // 查询结果中的记录数目。
    uint size() const {return items.size();}
    // 清空查询结果向量。
    void clear(){items.clear();}
    /** 获取查询结果的某一项。
     * @param id 编号。
     * @return 结果。
     * */
    const KontoRPos& get(int id) const {return items[id];}
    /** 将查询结果排序，主关键字为所在页面page，次关键字为页面中的编号id。
     * */
    void sort();
};

// 表查询结果，用向量实现，每个条目为KontoRPos。
typedef KontoQueryResult KontoQRes;

// 数据表。
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
    /** 重新创建主索引。例如当删除某非主索引列，应当重新创建主索引。
     * */
    KontoResult recreatePrimaryIndex();
    
public:
    ~KontoTableFile();
    /** 创建新的表。创建后应该调用defineField声明各个属性，finishDefineField结束声明。
     * @param filename 文件名。
     * @param handle 成功创建后返回指针。
     * */
    static KontoResult createFile(string filename, KontoTableFile** handle);
    /** 载入已有的表文件。
     * @param filename 文件名。
     * @param handle 成功读取后返回指针。
     * */
    static KontoResult loadFile(string filename, KontoTableFile** handle);
    /** 获取指向记录位置数据的指针，并指出接下来是读取还是写入。
     * @param pos 数据行的位置。
     * @param key 列编号。
     * @param write 是否接下来要写入。
     * @return 指向数据记录对应列的指针。
     * */
    char* getDataPointer(const KontoRPos& pos, KontoKeyIndex key, bool write);
    /** 定义表的一个列。
     * @param def 定义。
     * */
    KontoResult defineField(KontoCDef& def); 
    /** 结束属性域的定义，之后若再尝试定义将会出错。 */
    KontoResult finishDefineField();
    // 关闭文件。
    KontoResult close();
    /** 插入数据记录。
     * @param pos 插入后通过pos返回其位置。
     * */
    KontoResult insertEntry(KontoRPos* pos);
    /** 删除指定位置的记录。
     * @param pos 记录位置。
     * */
    KontoResult deleteEntry(const KontoRPos& pos);
    /** 修改指定位置的记录的int域。
     * @param pos 记录位置。
     * @param key 列编号。
     * @param datum 新值。
     * */
    KontoResult editEntryInt(const KontoRPos& pos, KontoKeyIndex key, int datum);
    /** 修改指定位置的记录的string域。
     * @param pos 记录位置。
     * @param key 列编号。
     * @param datum 新值。
     * */
    KontoResult editEntryString(const KontoRPos& pos, KontoKeyIndex key, const char* data);
    /** 修改指定位置的记录的float域。
     * @param pos 记录位置。
     * @param key 列编号。
     * @param datum 新值。
     * */
    KontoResult editEntryFloat(const KontoRPos& pos, KontoKeyIndex key, double datum);
    /** 修改指定位置的记录的date域。
     * @param pos 记录位置。
     * @param key 列编号。
     * @param datum 新值。
     * */
    KontoResult editEntryDate(const KontoRPos& pos, KontoKeyIndex key, Date datum);
    /** 读取指定位置记录的int列值。
     * @param pos 记录位置。
     * @param key 列编号。
     * @param out 返回值存储在out中。
     * */
    KontoResult readEntryInt(const KontoRPos& pos, KontoKeyIndex key, int& out);
    /** 读取指定位置记录的string列值。
     * @param pos 记录位置。
     * @param key 列编号。
     * @param out 返回值存储在out中。
     * */
    KontoResult readEntryString(const KontoRPos& pos, KontoKeyIndex key, char* out);
    /** 读取指定位置记录的float列值。
     * @param pos 记录位置。
     * @param key 列编号。
     * @param out 返回值存储在out中。
     * */
    KontoResult readEntryFloat(const KontoRPos& pos, KontoKeyIndex key, double& out);
    /** 读取指定位置记录的date列值。
     * @param pos 记录位置。
     * @param key 列编号。
     * @param out 返回值存储在out中。
     * */
    KontoResult readEntryDate(const KontoRPos& pos, KontoKeyIndex key, Date& out);
    /** 查询表中某个int列满足某条件的结果列表。
     * @param from 在该指定的范围内查询。
     * @param key 列编号。
     * @param cond 条件。
     * @param out 返回列表。
     * */
    KontoResult queryEntryInt(const KontoQRes& from, KontoKeyIndex key, function<bool(int)> cond, KontoQRes& out);
    /** 查询表中某个float列满足某条件的结果列表。
     * @param from 在该指定的范围内查询。
     * @param key 列编号。
     * @param cond 条件。
     * @param out 返回列表。
     * */
    KontoResult queryEntryString(const KontoQRes& from, KontoKeyIndex key, function<bool(const char*)> cond, KontoQRes& out);
    /** 查询表中某个string列满足某条件的结果列表。
     * @param from 在该指定的范围内查询。
     * @param key 列编号。
     * @param cond 条件。
     * @param out 返回列表。
     * */
    KontoResult queryEntryFloat(const KontoQRes& from, KontoKeyIndex key, function<bool(double)> cond, KontoQRes& out);
    /** 查询表中某个date列满足某条件的结果列表。
     * @param from 在该指定的范围内查询。
     * @param key 列编号。
     * @param cond 条件。
     * @param out 返回列表。
     * */
    KontoResult queryEntryDate(const KontoQRes& from, KontoKeyIndex key, function<bool(Date)> cond, KontoQRes& out);
    /** 获取所有记录位置组成的列表，用于新的查询。
     * @param out 返回列表。
     * */
    KontoResult allEntries(KontoQRes& out);
    /** 根据列名获取列编号。
     * @param key 列名。
     * @param out 返回结果。
     * @return 仅当key指定的列名存在的时候，返回KR_OK。
     * */
    KontoResult getKeyIndex(const char* key, KontoKeyIndex& out);
    // 获取一条记录的大小，以字节为单位。
    uint getRecordSize();
    /** 获取某一行数据。
     * @param pos 数据行位置。
     * @param dest 将获取到的数据拷贝到dest中。
     * */
    KontoResult getDataCopied(const KontoRPos& pos, char* dest);
    /** 创建索引表并与该数据表绑定.
     * @param keyIndices 列编号的列表。对于单列索引，keyIndices仅一个元素，对于联合索引则有多个元素。
     * @param handle 非空指针时，返回创建索引的指针。
     * @param noRepeat 该索引是否允许重复值。
     * */
    KontoResult createIndex(const vector<KontoKeyIndex>& keyIndices, KontoIndex** handle, bool noRepeat);
    // 删除所有索引表
    void removeIndices();
    /** 向所有已经关联的索引表中添加记录
     * @param pos 记录在数据表文件中的位置。
     * */
    KontoResult insertIndex(const KontoRPos& pos);
    /** 从已经关联的索引表中删除记录
     * @param pos 记录在数据表文件中的位置。
     * */
    KontoResult deleteIndex(const KontoRPos& pos);
    // 重新生成索引表
    KontoResult recreateIndices();
    /** 获取索引表的指针
     * @param id 索引表的编号。
     * @return 指向获取到的索引表的指针。
     * */
    KontoIndex* getIndex(uint id);
    // 获取已经关联的索引数量。
    uint getIndexCount();
    // 是否定义了主索引。
    bool hasPrimaryKey();
    /** 根据列编号获取对应索引。
     * @param keyIndices 列编号。
     * @return 当对应索引存在，返回其指针，否则返回空指针。
     * */
    KontoIndex* getIndex(const vector<KontoKeyIndex>& keyIndices);
    /** 获取主索引指针。当主索引不存在返回空指针。*/
    KontoIndex* getPrimaryIndex();
    /** 修改指定记录的int域。
     * @param pos 指向记录起始位置的指针。
     * @param key 列编号。
     * @param datum 新值。
     * */
    KontoResult setEntryInt(char* record, KontoKeyIndex key, int datum);
    /** 修改指定记录的string域。
     * @param pos 指向记录起始位置的指针。
     * @param key 列编号。
     * @param datum 新值。
     * */
    KontoResult setEntryString(char* record, KontoKeyIndex key, const char* data);
    /** 修改指定记录的float域。
     * @param pos 指向记录起始位置的指针。
     * @param key 列编号。
     * @param datum 新值。
     * */
    KontoResult setEntryFloat(char* record, KontoKeyIndex key, double datum);
    /** 修改指定记录的date域。
     * @param pos 指向记录起始位置的指针。
     * @param key 列编号。
     * @param datum 新值。
     * */
    KontoResult setEntryDate(char* record, KontoKeyIndex key, Date datum);
    /** 插入一条数据。
     * @param record 指向数据起始位置的指针。注意，实际数据应当从record+8位置开始，因为一条数据记录的前2个字节分别为行编号和删除标记。
     * */
    KontoResult insertEntry(char* record, KontoRPos* pos);
    /** 获取指向记录位置数据的指针，并指出接下来是读取还是写入。
     * @param pos 数据行的位置。
     * @param write 是否接下来要写入。
     * @return 指向数据记录对应列的指针。
     * */
    char* getRecordPointer(const KontoRPos& pos, bool write);
    /** 从列编号获取列名。
     * @param keyIndices 列编号。
     * @param out 返回的列名。
     * */
    KontoResult getKeyNames(const vector<uint>& keyIndices, vector<string>& out); 
    /** 向索引中插入数据。
     * @param pos 数据在表中的位置。
     * @param dest 索引表指针。
     * @param noRepeat 当此参数置真，插入前将在索引表中查询是否已有重复项，若存在重复项，则终止插入并返回错误。
     * */
    KontoResult insertIndex(const KontoRPos& pos, KontoIndex* dest, bool noRepeat);
    /** 将各列定义重新写入文件。例如修改某列定义时需要调用此函数。*/
    void rewriteKeyDefinitions();
    /** 添加主键。
     * @param primaryKeys 主键列编号。
     * */
    KontoResult alterAddPrimaryKey(const vector<uint>& primaryKeys);
    /** 删除主键。 */
    KontoResult alterDropPrimaryKey();
    /** 添加外键。
     * @param name 外键名。
     * @param foreignKeys 外键列编号。
     * @param foreignTable 外键指向的表名。
     * @param foreignName 外键指向的表中的对应列名。
     * */
    KontoResult alterAddForeignKey(string name, const vector<uint>& foreignKeys, string foreignTable, const vector<string>& foreignName);
    /** 删除外键。
     * @param name 外键名。
     * */
    KontoResult alterDropForeignKey(string name);
    /** 添加列。
     * @param def 列定义。
     * */
    KontoResult alterAddColumn(const KontoCDef& def);
    /** 删除列。
     * @param def 要删除的列名。
     * */
    KontoResult alterDropColumn(string name);
    /** 重命名列。
     * @param old 旧名。
     * @param newname 新名。
     * */
    KontoResult alterRenameColumn(string old, string newname);
    /** 修改列定义。
     * @param original 列名。
     * @param newdef 新定义。
     * */
    KontoResult alterChangeColumn(string original, const KontoCDef& newdef);
    /** 重命名表。
     * @param newname 新表名。
     * */
    KontoResult alterRename(string newname);
    /** 获取主键定义。
     * @param cols 返回主键列编号。*/
    void getPrimaryKeys(vector<uint>& cols);
    /** 获取所有已经定义的外键。
     * @param fknames 外键名列表。
     * @param cols 外键列编号列表。
     * @param foreignTable 外键指向的表名列表。
     * @param foreignName 外键指向的表中的对应列名列表。
     * */
    void getForeignKeys(
        vector<string>& fknames, 
        vector<vector<uint>>& cols, 
        vector<string>& foreignTable, 
        vector<vector<string>>& foreignName);
    /** 删除表。*/
    void drop();
    /** 插入记录。若不满足主键条件、外键条件、非空条件将终止插入。
     * @param record 指向记录数据起始位置的指针。注意，实际数据应当从record+8位置开始，因为一条数据记录的前2个字节分别为行编号和删除标记。*/
    KontoResult insert(char* record);
    /** 删除指定列构成的索引。
     * @param cols 列编号。
     * */
    KontoResult dropIndex(const vector<uint>& cols);

    void debugIndex(const vector<uint>& cols);
    
    /** 输出表头。
     * @param pos 是否输出记录位置。
     * */
    void printTableHeader(bool pos = false);
    /** 输出表行。
     * @param item 记录位置。
     * @param pos 是否输出记录位置。
     * */
    bool printTableEntry(const KontoRPos& item, bool pos = false);
    /** 输出全表。
     * @param meta 是否输出表元信息。
     * @param pos 是否输出记录位置。
     * */
    void printTable(bool meta = false, bool pos = false);
    /** 输出指定部分表。
     * @param list 要输出的行。
     * @param pos 是否输出记录位置。
     * */
    void printTable(const KontoQRes& list, bool pos);
    /** 查询满足条件的记录项，条件形式为某列与常值的比较。
     * @param from 从指定列表查询。
     * @param key 列编号。
     * @param op 比较条件。
     * @param rvalue 要比较的常值。
     * @param out 查询结果。
     * */
    void queryEntryInt(const KontoQRes& from, KontoKeyIndex key, OperatorType op, int rvalue, KontoQRes& out);
    /** 查询满足条件的记录项，条件形式为某列与常值的比较。
     * @param from 从指定列表查询。
     * @param key 列编号。
     * @param op 比较条件。
     * @param rvalue 要比较的常值。
     * @param out 查询结果。
     * */
    void queryEntryFloat(const KontoQRes& from, KontoKeyIndex key, OperatorType op, double rvalue, KontoQRes& out);
    /** 查询满足条件的记录项，条件形式为某列与常值的比较。
     * @param from 从指定列表查询。
     * @param key 列编号。
     * @param op 比较条件。
     * @param rvalue 要比较的常值。
     * @param out 查询结果。
     * */
    void queryEntryString(const KontoQRes& from, KontoKeyIndex key, OperatorType op, const char* rvalue, KontoQRes& out);
    /** 查询满足条件的记录项，条件形式为某列与常值的比较。
     * @param from 从指定列表查询。
     * @param key 列编号。
     * @param op 比较条件。
     * @param rvalue 要比较的常值。
     * @param out 查询结果。
     * */
    void queryEntryDate(const KontoQRes& from, KontoKeyIndex key, OperatorType op, Date rvalue, KontoQRes& out);

    /** 查询满足条件的记录项，条件形式为某列与两个常值的比较，即 a </<= value </<= b 的形式。
     * @param from 从指定列表查询。
     * @param key 列编号。
     * @param op 比较条件。
     * @param lvalue 要比较的较小常值。
     * @param rvalue 要比较的较大常值。
     * @param out 查询结果。
     * */
    void queryEntryInt(const KontoQRes& from, KontoKeyIndex key, OperatorType op, 
        int lvalue, int rvalue, KontoQRes& out);
    /** 查询满足条件的记录项，条件形式为某列与两个常值的比较，即 a </<= value </<= b 的形式。
     * @param from 从指定列表查询。
     * @param key 列编号。
     * @param op 比较条件。
     * @param lvalue 要比较的较小常值。
     * @param rvalue 要比较的较大常值。
     * @param out 查询结果。
     * */
    void queryEntryFloat(const KontoQRes& from, KontoKeyIndex key, OperatorType op, 
        double lvalue, double rvalue, KontoQRes& out);
    /** 查询满足条件的记录项，条件形式为某列与两个常值的比较，即 a </<= value </<= b 的形式。
     * @param from 从指定列表查询。
     * @param key 列编号。
     * @param op 比较条件。
     * @param lvalue 要比较的较小常值。
     * @param rvalue 要比较的较大常值。
     * @param out 查询结果。
     * */
    void queryEntryString(const KontoQRes& from, KontoKeyIndex key, OperatorType op, 
        const char* lvalue, const char* rvalue, KontoQRes& out);
    /** 查询满足条件的记录项，条件形式为某列与两个常值的比较，即 a </<= value </<= b 的形式。
     * @param from 从指定列表查询。
     * @param key 列编号。
     * @param op 比较条件。
     * @param lvalue 要比较的较小常值。
     * @param rvalue 要比较的较大常值。
     * @param out 查询结果。
     * */
    void queryEntryDate(const KontoQRes& from, KontoKeyIndex key, OperatorType op, 
        Date lvalue, Date rvalue, KontoQRes& out);
    /** 查询满足条件的记录项，条件形式为某两同类型列的比较。
     * @param from 从指定列表查询。
     * @param k1 左比较数列编号。
     * @param k2 右比较数列编号。
     * @param op 比较条件。
     * @param out 查询结果。
     * */
    void queryCompare(const KontoQRes& from, KontoKeyIndex k1, KontoKeyIndex k2, 
        OperatorType op, KontoQRes& out);
    /** 删除记录。
     * @param items 要删除的记录位置。
     * */
    void deletes(const KontoQRes& items);
    /** 判断数据是否合法。包括主键约束、外键约束、非空约束。
     * @param record 数据指针。
     * @param checkSingle 当此参数非-1时，指定一个列编号，仅检查与该列有关的合法性。
     * */
    KontoResult checkLegal(char* record, uint checkSingle = -1);
    /** 检查数据是否符合外键约束。
     * @param record 数据指针。
     * @param cols 外键列编号。
     * @param tableName 外键指向的表。
     * @param foreignNames 外键指向的表中的对应列名。
     * */
    KontoResult checkForeignKey(char* record, const vector<uint> cols, const string& tableName, const vector<string>& foreignNames);
    /** 检查删除标记。
     * @param flags 标记。
     * */
    static bool checkDeletedFlags(uint flags);
};

#endif