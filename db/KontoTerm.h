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

    ProcessStatementResult err(string message);
    void main();
    ProcessStatementResult processStatement();
    ProcessStatementResult processInsert(string tbname);
};

#endif