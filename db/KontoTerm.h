#ifndef KONTOTERM_H
#define KONTOTERM_H

#include "../KontoConst.h"
#include "../record/KontoRecord.h"
#include "KontoLexer.h"

enum ProcessStatementResult {
    PSR_OK, 
    PSR_ERR,
    PSR_QUIT
};

class KontoTerminal {
private:
    string currentDatabase;
public:
    KontoLexer lexer;
    void listDatabases();
    void listTables();
    void createDatabase(string dbname);
    void useDatabase(string dbname);
    void dropDatabase(string dbname);
    void showDatabase(string dbname);
    void createTable(string name, const vector<KontoCDef>& defs, const vector<uint>& primaryKeys);
    void dropTable(string name);
    void alterAddPrimaryKey(string table, const vector<uint>& primaryKeys);
    void alterDropPrimaryKey(string table);
    void alterAddColumn(string table, const KontoCDef& def);
    void alterDropColumn(string table, const KontoCDef& def);
    void alterChangeColumn(string table, string original, const KontoCDef& newdef);
    void alterAddForeignKey(string table, const KontoCDef& def);
    void alterDropForeignKey(string table, string name);
    ProcessStatementResult err(string message);
    void main();
    ProcessStatementResult processStatement();
};

#endif