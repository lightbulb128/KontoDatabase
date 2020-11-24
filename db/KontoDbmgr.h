#include "../KontoConst.h"

class KontoDbmgr {
private:
    string currentDatabase;
public:
    KontoResult listDatabases(vector<string>& out);
    KontoResult listTables(vector<string>& out);
    KontoResult createDatabase(string dbname);
    KontoResult useDatabase(string dbname);
    KontoResult dropDatabase(string dbname);
    KontoResult createTable(string name, const vector<KontoCDef>& defs, const vector<uint>& primaryKeys);
    KontoResult dropTable(string name);
    KontoResult alterAddPrimaryKey(string table, const vector<uint>& primaryKeys);
    KontoResult alterDropPrimaryKey(string table);
    KontoResult alterAddColumn(string table, const KontoCDef& def);
    KontoResult alterDropColumn(string table, const KontoCDef& def);
    KontoResult alterChangeColumn(string table, string original, const KontoCDef& newdef);
    KontoResult alterAddForeignKey(string table, const KontoCDef& def);
    KontoResult alterDropForeignKey(string table, string name);
};
