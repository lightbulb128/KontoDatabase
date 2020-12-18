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
TODO ALTER TABLE [TBNAME] RENAME TO [NEWTBNAME]

create database [dbname]
TODO CREATE INDEX [IDNAME] ON [TBNAME] (COLS...)
create table [tbname] (coldefs...)

debug echo [message]
debug from [tbname] where [wheres...]
debug index [idname]
debug primary [tbname]
debug table [tbname]
debug [message]

delete from [tbname] where [wheres...]

desc [tbname]

drop database [dbname]
TODO DROP INDEX [IDNAME]
drop table [tbname]

echo [message]

insert into [tbname] values [vals]
insert into [tbname] values file [csv filename]

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

enum ProcessStatementResult {
    PSR_OK, 
    PSR_ERR,
    PSR_QUIT
};

struct KontoIndexDesc {
    string name;
    string table;
    vector<uint> cols;
};

enum WhereType {
    WT_CONST,
    WT_INNER,
    WT_CROSS
};

struct KontoWhere {
    WhereType type;
    KontoKeyType keytype;
    OperatorType op;
    Token rvalue;
    Token lvalue;
    string ltable, rtable;
    uint lid, rid;
};

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
    static KontoTerminal* getInstance();
    bool hasTable(string table);
    void createDatabase(string dbname);
    void useDatabase(string dbname);
    void dropDatabase(string dbname);
    void showDatabase(string dbname);
    void showDatabases();
    void createTable(string name, const vector<KontoCDef>& defs);
    void dropTable(string name);
    void showTable(string name);
    void alterAddPrimaryKey(string table, const vector<string>& cols);
    void alterDropPrimaryKey(string table);
    void alterAddColumn(string table, const KontoCDef& def);
    void alterDropColumn(string table, string col);
    void alterChangeColumn(string table, string original, const KontoCDef& newdef);
    void alterAddForeignKey(string table, string fkname, 
        const vector<string>& col, string foreignTable, 
        const vector<string>& foreignNames);
    void alterRenameColumn(string table, string origname, string newname);
    void alterRenameTable(string table, string newname);
    void alterDropForeignKey(string table, string fkname);
    void loadIndices();
    void saveIndices();
    void dropTableIndices(string tb);
    void createIndex(string idname, string table, const vector<string>& cols);
    void dropIndex(string idname, string table="");
    void debugIndex(string idname);
    void debugIndex();
    void debugTable(string tbname);
    void debugPrimary(string tbname);
    void deletes(string tbname, const vector<KontoWhere>& wheres);
    KontoQRes queryWhere(const KontoWhere& where);
    KontoQRes queryWhereWithin(const KontoQRes& prev, const KontoWhere& where);
    void queryWheres(const vector<KontoWhere>& wheres, KontoQRes& out); // 约定wheres中只有一个表
    void queryWheres(const vector<KontoWhere>& wheres, vector<string>& tables, vector<KontoQRes>& results); // 忽略跨表查询
    void queryWheresFrom(const vector<KontoWhere>& wheres, const vector<string>& givenTables, vector<KontoQRes>& results);
    void debugFrom(string tbname, const vector<KontoWhere>& wheres);
    void printWhere(const KontoWhere& where);
    void printWheres(const vector<KontoWhere>& wheres);
    void printQRes(const KontoQRes& qres);
    void debugEcho(string str);
    TokenExpectation valueTypeToExpectation(KontoKeyType type);

    ProcessStatementResult err(string message);
    void main(bool prompt = true);
    ProcessStatementResult processStatement();
    uint getColumnIndex(string table, string col, KontoKeyType& type);
    ProcessStatementResult processWhereTerm(const vector<string>& tables, KontoWhere& out);
    ProcessStatementResult processWheres(const string& table, vector<KontoWhere>& out);
    ProcessStatementResult processWheres(const vector<string>& tables, vector<KontoWhere>& out);
    ProcessStatementResult processInsert(string tbname);
    ProcessStatementResult processSelect();
    ProcessStatementResult processUpdate(string tbname);
    ProcessStatementResult processAlterAdd(string tbname);
    ProcessStatementResult processAlterDrop(string tbname);
    ProcessStatementResult processCreate();
    ProcessStatementResult processAlterChange(string tbname);
    ProcessStatementResult processAlterRename(string tbname);
};

#endif