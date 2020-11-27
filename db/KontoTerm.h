#ifndef KONTOTERM_H
#define KONTOTERM_H

#include "../KontoConst.h"
#include "../record/KontoRecord.h"
#include "KontoLexer.h"

const string TABLES_FILE = "__tables.txt";

enum ProcessStatementResult {
    PSR_OK, 
    PSR_ERR,
    PSR_QUIT
};

class KontoTerminal {
private:
    string currentDatabase;
    vector<string> tables;
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
    ProcessStatementResult err(string message);
    void main();
    ProcessStatementResult processStatement();
};

#endif