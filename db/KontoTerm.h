#ifndef KONTOTERM_H
#define KONTOTERM_H

#include "../KontoConst.h"
#include "../record/KontoRecord.h"
#include "KontoLexer.h"

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
public:
    KontoLexer lexer;
    KontoTerminal();
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
    void alterDropForeignKey(string table, string fkname);
    void loadIndices();
    void saveIndices();
    void dropTableIndices(string tb);
    void createIndex(string idname, string table, const vector<string>& cols);
    void dropIndex(string idname);
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
};

#endif