#ifndef KONTOINDEX_H
#define KONTOINDEX_H

#include "KontoConst.h"
#include "KontoRecord.h"
#include <vector>
#include <string>
#include <functional>

struct KontoQueryResult;
typedef KontoQueryResult KontoQRes;

using std::vector;
using std::string;

// 指示某一条索引记录的位置。
struct KontoIPos {
    int page, id;
    KontoIPos(){}
    KontoIPos(int page, int id):page(page),id(id){}
    bool operator ==(const KontoIPos& b){
        return page==b.page && id==b.id;
    }
};

// 索引表，索引文件第一页为元信息，之后的每个页面对应B+树结构的一个节点。
class KontoIndex {
private:
    BufPageManager& pmgr;
    vector<KontoKeyType> keyTypes;
    vector<uint> keyPositions;
    vector<uint> keySizes;
    string filename;
    uint indexSize; // 索引键域的大小，例如由一个int为主关键字，一个double为副关键字的联合索引，索引键总大小为3（个int）
    KontoIndex();
    int fileID;
    int pageCount;
    /** 根据索引键的定义，比较一条数据记录和一个索引键
     * @param record 数据指针。
     * @param index 索引键指针。
     * */
    int compare(char* record, char* index);
    /** 根据索引键的定义，比较两条数据记录
     * @param r1 左参数数据指针。
     * @param r2 右参数数据指针。
     * */
    int compareRecords(char* r1, char* r2);
    /** 设置dest位置上的索引键，数据从record获得，且它在数据表中的位置由pos指定
     * @param dest 索引键指针。
     * @param record 数据记录。
     * @param pos 指定数据记录在数据表中的位置。
     * */
    void setKey(char* dest, char* record, const KontoRPos& pos);
    /** 递归插入。
     * @param record 要插入的数据。
     * @param pos 这条数据在数据表中的位置。
     * @param pageID 递归到的节点（页面编号）。
     * */
    KontoResult insertRecur(char* record, const KontoRPos& pos, uint pageID);
    /** 节点（页）分裂
     * @param pageID 要分裂的页编号。
     * */
    KontoResult split(uint pageID);
    /** 递归查询
     * @param record 要查询的记录数据
     * @param out 查到的结果输出
     * @param pageID 当前递归到的索引表页面
     * @param equal true表示查询不大于record的最后一条，false表示查询小于record的最后一条，不考虑delete标记
     * */
    KontoResult queryIposRecur(char* record, KontoIPos& out, uint pageID, bool equal);
    /** 查询
     * @param record 要查询的记录数据
     * @param out 查到的结果输出
     * @param equal true表示查询不大于record的最后一条，false表示查询小于record的最后一条，不考虑delete标记
     * */
    KontoResult queryIpos(char* record, KontoIPos& out, bool equal);
    /** 递归获取索引表中第一条记录（即键值最小的记录）
     * @param out 查到的结果输出。
     * @param pageID 当前递归到的索引表页面。
     * */
    KontoResult queryIposFirstRecur(KontoIPos& out, uint pageID);
    /** 获取索引表中第一条记录（即键值最小的记录）
     * @param out 查到的结果输出。
     * */
    KontoResult queryIposFirst(KontoIPos& out);
    /** 递归获取索引表中最后一条记录（即键值最大的记录）
     * @param out 查到的结果输出。
     * @param pageID 当前递归到的索引表页面。
     * */
    KontoResult queryIposLastRecur(KontoIPos& out, uint pageID);
    /** 获取索引表中最后一条记录（即键值最大的记录）
     * @param out 查到的结果输出。
     * @param pageID 当前递归到的索引表页面。
     * */
    KontoResult queryIposLast(KontoIPos& out);
    /** 根据索引表中位置获取数据表中位置
     * @param pos 索引表中位置。
     * */
    KontoRPos getRPos(KontoIPos& pos);
    /** 获取索引表中某节点的B+树后继节点，不考虑delete标记。注意pos和out不应当传递同一个引用。
     * @param pos 节点位置。
     * @param out 返回后继节点位置。
     * */
    KontoResult getNext(KontoIPos& pos, KontoIPos& out);
    /** 获取索引表中某节点的B+树前驱节点，不考虑delete标记。注意pos和out不应当传递同一个引用。
     * @param pos 节点位置。
     * @param out 返回后继节点位置。
     * */
    KontoResult getPrevious(KontoIPos& pos, KontoIPos& out);
    /** 获取索引表中某节点的B+树后继节点，不考虑delete标记。
     * @param pos 节点位置，获取到的结果也通过pos返回。
     * */
    KontoResult getNext(KontoIPos& pos);
    /** 获取索引表中某节点的B+树前驱节点，不考虑delete标记。
     * @param pos 节点位置，获取到的结果也通过pos返回。
     * */
    KontoResult getPrevious(KontoIPos& pos);
    /** 判断pos位置的记录是否已经被删除。
     * @param pos 节点位置。
     * */
    bool isDeleted(const KontoIPos& pos);
    /** 判断pos位置的索引键是否为null值。
     * @param pos 节点位置。
     * */
    bool isNull(const KontoIPos& pos);
public:
    /** 比较两个域，通过type指定域的类型，返回值正数表示d1>d2，负数表示d1<d2，0表示两者相等 */
    static int compare(char* d1, char* d2, KontoKeyType type);
    /** 创建索引
     * @param filename 文件名。
     * @param handle 成功创建后结果通过handle指针返回。
     * @param ktypes 索引键各列类型。
     * @param kposs 各列在原表中的存储位置对应数据起始处指针的偏移量。
     * @param ksizes 各列所占空间大小，以字节为单位。
     * */
    static KontoResult createIndex(string filename, KontoIndex** handle, 
        vector<KontoKeyType> ktypes, vector<uint> kposs, vector<uint> ksizes);
    /** 加载索引
     * @param filename 文件名。
     * @param handle 成功读取后结果通过handle返回。
     * */
    static KontoResult loadIndex(string filename, KontoIndex** handle);
    /** 根据键名生成索引文件名
     * @param database 数据库名。
     * @param keyNames 索引键各列名。
     * */
    static string getIndexFilename(const string database, const vector<string> keyNames); 
    /** 插入一条记录
     * @param record 数据。
     * @param pos 数据在数据表中的位置。
     * */
    KontoResult insert(char* record, const KontoRPos& pos);
    /** 删除一条记录
     * @param record 数据。
     * @param pos 数据在数据表中的位置。
     * */
    KontoResult remove(char* record, const KontoRPos& pos);
    /** 查询不大于record的最末一条记录。
     * @param record 用于比较的数据。
     * @param out 返回查询结果。
     * */
    KontoResult queryLE(char* record, KontoRPos& out); 
    /** 查询小于record的最末一条记录。
     * @param record 用于比较的数据。
     * @param out 返回查询结果。
     * */
    KontoResult queryL(char* record, KontoRPos& out);
    /** 区间查询，查询在键值在lower到upper区间上的记录。
     * @param lower 下界，为空指针时表示不限定下界。
     * @param upper 上界，为空指针时表示不限定上界。
     * @param out 返回查询结果。
     * @param lowerIncluded 下界是否闭区间。
     * @param upperIncluded 上界是否闭区间。
     * @param filterNull 是否忽略包含null值的结果。
     * */
    KontoResult queryInterval(char* lower, char* upper, KontoQRes& out, 
        bool lowerIncluded = true, 
        bool upperIncluded = false, 
        bool filterNull = true);
    /** 关闭记录文件 */
    KontoResult close();
    /** 重新创建索引
     * @param original 原索引。
     * @param handle 返回新索引。
     * */
    static KontoResult recreate(KontoIndex* original, KontoIndex** handle);
    // 返回文件名。
    string getFilename();
    // 删除索引。
    KontoResult drop();
    /** 等值查询。
     * @param record 数据。
     * @param out 返回查询结果。
     * */
    KontoResult queryE(char* record, KontoRPos& out);

    void debugPrintKey(char* ptr);
    void debugPrintRecord(char* ptr);
    void debugPrintPage(int pageID, bool recur = true);
    void debugPrint();
    void debugPageOne();

    /** 通知索引表其关联的数据表已重命名。
     * @param newname 新的表名。
     * */
    void renameTable(string newname);
};

#endif