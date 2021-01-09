#ifndef KONTOTERM_H
#define KONTOTERM_H

#include "KontoConst.h"
#include "KontoRecord.h"
#include "KontoLexer.h"

/* SUPPORTED COMMANDS 

alter table [tbname] add constraint [fkname] foreign key (cols...) references [ftable] (fcols...)
alter table [tbname] add constraint [pkname] primary key (cols...);
alter table [tbname] add index [idname] (cols...)
alter table [tbname] add primary key (cols...)
alter table [tbname] add [colname] [typedef]
alter table [tbname] drop foreign key [fkname]
alter table [tbname] drop index [idname]
alter table [tbname] drop primary key
alter table [tbname] drop [colname]
alter table [tbname] change [colname] [typedef]
alter table [tbname] rename [col] to [col]
alter table [tbname] rename to [newtbname]

create database [dbname]
create index [idname] on [tbname] (cols...)
create table [tbname] (coldefs...)

debug echo [message]
debug echo 
debug from [tbname] where [wheres...]
debug index [idname]
debug primary [tbname]
debug table [tbname]
debug [message]

delete from [tbname] where [wheres...]

desc [tbname]

drop database [dbname]
drop index [idname]
drop table [tbname]

echo [message]

insert into [tbname] values [vals]
insert into [tbname] values from [tbl filename]

quit

select [* or cols] from [tables...] where [wheres...]

show database [dbname]
show databases
show table [tbname]
show tables

update [tbname] set [setstmt] where [wheres...]

use [dbname]
use database [dbname]

*/

const string TABLES_FILE = "__tables";
const string INDICES_FILE = "__indices";

// 语句语法分析结果。
enum ProcessStatementResult {
    PSR_OK, 
    PSR_ERR,
    PSR_QUIT
};

// 索引表描述。
struct KontoIndexDesc {
    string name;
    string table;
    vector<uint> cols;
};

// where子句项的类型。WT_CONST 表示单列与常值比较；WT_INNER 表示单表内两列的比较；WT_CROSS 表示两表之间两列的比较。
enum WhereType {
    WT_CONST,
    WT_INNER,
    WT_CROSS
};

// where 的数据结构。
struct KontoWhere {
    WhereType type;
    KontoKeyType keytype;
    OperatorType op;
    Token rvalue;
    Token lvalue;
    string ltable, rtable;
    uint lid, rid;
};

// 用户终端，采用 Singleton 模式。
class KontoTerminal {
private:
    string currentDatabase;
    vector<string> tables;
    vector<KontoIndexDesc> indices;
    KontoTerminal();
    static KontoTerminal* instancePtr;
    bool commandLine;
public:
    KontoLexer lexer;
    // Singleton 模式获取单例。
    static KontoTerminal* getInstance();
    // 判断某表是否存在。
    bool hasTable(string table);
    // 创建数据库。
    void createDatabase(string dbname);
    // 使用数据库。
    void useDatabase(string dbname);
    // 删除数据库。
    void dropDatabase(string dbname);
    // 显示某数据库的可用表。
    void showDatabase(string dbname);
    // 显示所有可用数据库。
    void showDatabases();
    /** 创建数据表。
     * @param name 表名
     * @param defs 列定义。
     * */
    void createTable(string name, const vector<KontoCDef>& defs);
    // 删除表。
    void dropTable(string name);
    // 显示表元数据。
    void showTable(string name);
    /** 添加主键。
     * @param table 表名。
     * @param cols 主键各列名。
     * */
    void alterAddPrimaryKey(string table, const vector<string>& cols);
    // 删除主键。
    void alterDropPrimaryKey(string table);
    /** 在数据表中新增列。
     * @param table 表名。
     * @param def 列定义。
     * */
    void alterAddColumn(string table, const KontoCDef& def);
    /** 删除数据表中的列。
     * @param table 表名。
     * @param col 列名。
     * */
    void alterDropColumn(string table, string col);
    /** 修改数据表中的列。
     * @param table 表名。
     * @param original 原列名。
     * @param newdef 新列定义。
     * */
    void alterChangeColumn(string table, string original, const KontoCDef& newdef);
    /** 为数据表添加外键。
     * @param table 要添加外键的表。
     * @param fkname 外键名。
     * @param col 外键各列。
     * @param foreignTable 外键指向的表名。
     * @param foreignNames 外键指向的表中对应列名。
     * */
    void alterAddForeignKey(string table, string fkname, 
        const vector<string>& col, string foreignTable, 
        const vector<string>& foreignNames);
    /** 重命名表的某列。
     * @param table 表名。
     * @param origname 原列名。
     * @param newname 新列名。
     * */
    void alterRenameColumn(string table, string origname, string newname);
    /** 重命名某表。
     * @param table 原表名。
     * @param newname 新表名。
     * */
    void alterRenameTable(string table, string newname);
    /** 从表中删除外键。
     * @param table 表名。
     * @param fkname 外键名。
     * */
    void alterDropForeignKey(string table, string fkname);
    // 获取已定义的索引表。
    void loadIndices();
    // 保存已定义的索引表。
    void saveIndices();
    // 删除某数据表的所有索引。
    void dropTableIndices(string tb);
    /** 为某表创建索引
     * @param idname 索引名。
     * @param table 表名。
     * @param cols 要创建索引的各列名。
     * */
    void createIndex(string idname, string table, const vector<string>& cols);
    /** 删除索引。
     * @param idname 索引名。
     * @param table 表名。指定表名时，仅查找该表关联的索引。
     * */
    void dropIndex(string idname, string table="");
    void debugIndex(string idname);
    void debugIndex();
    void debugTable(string tbname);
    void debugPrimary(string tbname);
    /** 删除表行。
     * @param tbname 表名。
     * @param wheres where子句。*/
    void deletes(string tbname, const vector<KontoWhere>& wheres);
    /** 对一个where项作单表查询。
     * @param where where子句项，不能是WT_CROSS类型（即不能是跨表比较）。
     * */
    KontoQRes queryWhere(const KontoWhere& where);
    /** 在已有结果中进一步作单表查询。
     * @param prev 已有结果。
     * @param where where子句项，不能是WT_CROSS类型（即不能是跨表比较）。
     * */
    KontoQRes queryWhereWithin(const KontoQRes& prev, const KontoWhere& where);
    /** 单表查询。
     * @param wheres 多个where子句项的列表，且它们都是对同一个表的单表查询。
     * @param out 返回查询结果
     * */
    void queryWheres(const vector<KontoWhere>& wheres, KontoQRes& out); 
    /** 对多个表的单表查询。
     * @param wheres 多个where子句项。跨表比较查询项将被忽略。
     * @param tables 返回所有被查询的表名。
     * @param results 对tables的每个表，返回一个查询结果。*/
    void queryWheres(const vector<KontoWhere>& wheres, vector<string>& tables, vector<KontoQRes>& results); 
    /** 对多个表的单表查询，可供查询的表已指定。
     * @param wheres 多个where子句项。跨表比较查询项将被忽略。
     * @param givenTables 可供查询的表。
     * @param results 对givenTables的每个表，返回一个查询结果。
     * */
    void queryWheresFrom(const vector<KontoWhere>& wheres, const vector<string>& givenTables, vector<KontoQRes>& results);
    void debugFrom(string tbname, const vector<KontoWhere>& wheres);
    void printWhere(const KontoWhere& where);
    void printWheres(const vector<KontoWhere>& wheres);
    void printQRes(const KontoQRes& qres);
    void debugEcho(string str);
    /** 将一个类型转化为它的Token期待。*/
    TokenExpectation valueTypeToExpectation(KontoKeyType type);
    // 输出错误信息。
    ProcessStatementResult err(string message);
    /** 处理用户输入的主函数。
     * @param prompt 表示是否通过控制台直接输入，否表示通过文件输入。
     * */
    void main(bool prompt = true);
    /** 解析用户命令。*/
    ProcessStatementResult processStatement();
    /** 获取某个表中对应列名的列编号。
     * @param table 表名。
     * @param col 列名。
     * @param type 同时返回列类型。
     * */
    uint getColumnIndex(string table, string col, KontoKeyType& type);
    /** 解析单个 where 子句项。
     * @param tables 可供使用的表名。
     * @param out 返回子句项。
     * */
    ProcessStatementResult processWhereTerm(const vector<string>& tables, KontoWhere& out);
    /** 解析单个表的多个 where 子句项，各项用 and 连接。
     * @param table 可供使用的表名。
     * @param out 返回子句项列表。
     * */
    ProcessStatementResult processWheres(const string& table, vector<KontoWhere>& out);
    /** 解析多个 where 子句项，各项用 and 连接。
     * @param tables 可供使用的表名。
     * @param out 返回子句项列表。
     * */
    ProcessStatementResult processWheres(const vector<string>& tables, vector<KontoWhere>& out);
    /** 解析 insert 指令 */
    ProcessStatementResult processInsert(string tbname);
    /** 解析 select 指令 */
    ProcessStatementResult processSelect();
    /** 解析 update 指令 */
    ProcessStatementResult processUpdate(string tbname);
    /** 解析 alter add 指令 */
    ProcessStatementResult processAlterAdd(string tbname);
    /** 解析 alter drop 指令 */
    ProcessStatementResult processAlterDrop(string tbname);
    /** 解析 create 指令 */
    ProcessStatementResult processCreate();
    /** 解析 alter change 指令 */
    ProcessStatementResult processAlterChange(string tbname);
    /** 解析 alter rename 指令 */
    ProcessStatementResult processAlterRename(string tbname);
};

#endif