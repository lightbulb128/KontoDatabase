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

// 指示某一条索引记录的位置
struct KontoIPos {
    int page, id;
    KontoIPos(){}
    KontoIPos(int page, int id):page(page),id(id){}
    bool operator ==(const KontoIPos& b){
        return page==b.page && id==b.id;
    }
};

// 索引表
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
    // 比较两个域，通过type指定域的类型，返回值正数表示d1>d2，负数表示d1<d2，0表示两者相等
    static int compare(char* d1, char* d2, KontoKeyType type);
    // 根据索引键的定义，比较一条数据记录和一个索引键
    int compare(char* record, char* index);
    // 根据索引键的定义，比较两条数据记录
    int compareRecords(char* r1, char* r2);
    // 设置dest位置上的索引键，数据从record获得，且它在数据表中的位置由pos指定
    void setKey(char* dest, char* record, KontoRPos& pos);
    // 递归插入，record为要插入的数据，pos为这条数据在数据表中的位置，pageID为当前索引表的页面
    KontoResult insertRecur(char* record, KontoRPos& pos, uint pageID);
    // 页表分裂
    KontoResult split(uint pageID);
    // 递归查询，record为要查询的记录数据，out为查到的结果输出，pageID为当前索引表页面，equal=true表示查询不大于record的最后一条，
    // equal=false表示查询小于record的最后一条，不考虑delete域
    KontoResult queryIposRecur(char* record, KontoIPos& out, uint pageID, bool equal);
    // 查询，record为要查询的记录数据，out为查到的结果输出，equal=true表示查询不大于record的最后一条，
    // equal=false表示查询小于record的最后一条，不考虑delete域
    KontoResult queryIpos(char* record, KontoIPos& out, bool equal);
    // 递归获取索引表中第一条记录（即键值最小的记录），不考虑delete域
    KontoResult queryIposFirstRecur(KontoIPos& out, uint pageID);
    // 获取索引表中第一条记录（即键值最小的记录），不考虑delete域
    KontoResult queryIposFirst(KontoIPos& out);
    // 递归获取索引表中最后一条记录（即键值最大的记录），不考虑delete域
    KontoResult queryIposLastRecur(KontoIPos& out, uint pageID);
    // 获取索引表中最后一条记录（即键值最大的记录），不考虑delete域
    KontoResult queryIposLast(KontoIPos& out);
    // 根据索引表中位置获取数据表中位置
    KontoRPos getRPos(KontoIPos& pos);
    // 获取索引表中pos的下一条记录输出到out，不考虑delete域
    KontoResult getNext(KontoIPos& pos, KontoIPos& out);
    // 获取索引表中pos的上一条记录输出到out，不考虑delete域
    KontoResult getPrevious(KontoIPos& pos, KontoIPos& out);
    // 获取索引表中pos的上一条记录转存回pos，不考虑delete域
    KontoResult getNext(KontoIPos& pos);
    // 获取索引表中pos的下一条记录转存回pos，不考虑delete域
    KontoResult getPrevious(KontoIPos& pos);
    // 判断pos位置的记录是否已经被删除
    bool isDeleted(KontoIPos& pos);
public:
    // 创建索引
    static KontoResult createIndex(string filename, KontoIndex** handle, 
        vector<KontoKeyType> ktypes, vector<uint> kposs, vector<uint> ksizes);
    // 加载索引
    static KontoResult loadIndex(string filename, KontoIndex** handle);
    // 根据键名生成索引文件名
    static string getIndexFilename(string database, vector<string> keyNames); 
    // 插入一条记录
    KontoResult insert(char* record, KontoRPos& pos);
    // 删除一条记录
    KontoResult remove(char* record, KontoRPos& pos);
    // 查询不大于key的最末一条记录，已被删除的索引节点被跳过
    KontoResult queryLE(char* record, KontoRPos& out); 
    // 查询小于key的最末一条记录，已被删除的索引节点被跳过
    KontoResult queryL(char* record, KontoRPos& out);
    // 查询在键值在lower到upper区间上的记录，included表示是否包含区间端点
    // lower=nullptr时，不限制左端点，upper=nullptr时，不限制右端点
    KontoResult queryInterval(char* lower, char* upper, KontoQRes& out, 
        bool lowerIncluded = true, 
        bool upperIncluded = false);
    // 关闭记录文件
    KontoResult close();
    // 重新创建索引
    static KontoResult recreate(KontoIndex* original, KontoIndex** handle);

    void debugPrintKey(char* ptr);
    void debugPrintRecord(char* ptr);
    void debugPrintPage(int pageID);
    void debugPrint();
};

#endif