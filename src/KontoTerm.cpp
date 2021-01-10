#include "KontoTerm.h"
#include <fstream>

using std::to_string;

#define ASSERTERR(token, type, message) if (token.tokenKind != type) return err(message)
#define ASSERTERR_CLOSE(token, type, message) if (token.tokenKind != type) {handle->close(); return err(message);}

string bool_to_string(bool b){
    return b ? "Yes" : "No";
}

string type_to_string(KontoKeyType type, int size) {
    switch (type) {
        case KT_INT: return "INT";
        case KT_FLOAT: return "FLOAT";
        case KT_STRING: return "VCHAR(" + to_string(size - 1) + ")";
        case KT_DATE: return "DATE";
        default: return "UNKNOWN";
    }
}

ProcessStatementResult KontoTerminal::err(string message) {
    cout << TABS[1] << message << endl;
    return PSR_ERR;
}

ProcessStatementResult KontoTerminal::processStatement() {
    Token cur = lexer.nextToken(), peek;
    while (cur.tokenKind == TK_SEMICOLON) cur = lexer.nextToken();
    switch (cur.tokenKind) {

        case TK_EOF:
            return PSR_QUIT;

        case TK_ALTER: {
            cur = lexer.nextToken();
            ASSERTERR(cur, TK_TABLE, "alter: Expect keyword TABLE.");
            cur = lexer.nextToken(TE_IDENTIFIER);
            ASSERTERR(cur, TK_IDENTIFIER, "alter table: Expect identifier.");
            string table = cur.identifier;
            cur = lexer.nextToken();
            if (cur.tokenKind == TK_ADD) {
                return processAlterAdd(table);
            } else if (cur.tokenKind == TK_DROP) {
                return processAlterDrop(table);
            } else if (cur.tokenKind == TK_CHANGE) {
                return processAlterChange(table);
            } else if (cur.tokenKind == TK_RENAME) {
                return processAlterRename(table);
            } else {
                return err("alter table: Expect keyword ADD, DROP or CHANGE.");
            } 
        }

        case TK_CREATE: {
            return processCreate();
        }

        case TK_DEBUG: {
            cur = lexer.nextToken();
            if (cur.tokenKind == TK_INDEX) {
                peek = lexer.peek();
                if (peek.tokenKind == TK_IDENTIFIER) {
                    lexer.nextToken(); debugIndex(peek.identifier);
                } else debugIndex();
                return PSR_OK;
            } else if (cur.tokenKind == TK_TABLE) {
                cur = lexer.nextToken(TE_IDENTIFIER);
                ASSERTERR(cur, TK_IDENTIFIER, "debug table: expect identifier.");
                debugTable(cur.identifier);
                return PSR_OK;
            } else if (cur.tokenKind == TK_PRIMARY) {
                cur = lexer.nextToken(TE_IDENTIFIER);
                ASSERTERR(cur, TK_IDENTIFIER, "debug primary: expect identifier.");
                debugPrimary(cur.identifier);
                return PSR_OK;
            } else if (cur.tokenKind == TK_FROM) {
                if (currentDatabase == "") {PT(1, "Error: Not using a database!");return PSR_ERR;}
                cur = lexer.nextToken(TE_IDENTIFIER);
                ASSERTERR(cur, TK_IDENTIFIER, "debug from: Expect identifier");
                string table = cur.identifier;
                cur = lexer.nextToken();
                ASSERTERR(cur, TK_WHERE, "debug from: Expect keyword WHERE.");
                vector<KontoWhere> wheres; 
                ProcessStatementResult psr = processWheres(table, wheres);
                if (psr == PSR_OK) debugFrom(table, wheres);
                return psr;
            } else if (cur.tokenKind == TK_STRING_VALUE) {
                debugEcho(cur.identifier);
                return PSR_OK;
            } else if (cur.tokenKind == TK_ECHO) {
                cur = lexer.nextToken(TE_STRING_VALUE);
                debugEcho(cur.identifier);
                return PSR_OK;
            } else {return PSR_ERR;}
        }

        case TK_DELETE: {
            if (currentDatabase == "") {PT(1, "Error: Not using a database!");return PSR_ERR;}
            cur = lexer.nextToken();
            ASSERTERR(cur, TK_FROM, "delete: Expect keyword FROM.");
            cur = lexer.nextToken(TE_IDENTIFIER);
            ASSERTERR(cur, TK_IDENTIFIER, "delete: Expect identifier");
            string table = cur.identifier;
            cur = lexer.nextToken();
            ASSERTERR(cur, TK_WHERE, "delete: Expect keyword WHERE.");
            vector<KontoWhere> wheres; 
            ProcessStatementResult psr = processWheres(table, wheres);
            if (psr == PSR_OK) deletes(table, wheres);
            return psr;
        }

        case TK_DESC: {
            if (currentDatabase == "") {PT(1, "Error: Not using a database!");return PSR_ERR;}
            cur = lexer.nextToken();
            ASSERTERR(cur, TK_IDENTIFIER, "desc: Expect identifier");
            showTable(cur.identifier);
            return PSR_OK;
        }

        case TK_DROP: {
            Token peek = lexer.peek();
            if (peek.tokenKind == TK_DATABASE) {
                lexer.nextToken(); cur = lexer.nextToken(TE_IDENTIFIER);
                ASSERTERR(cur, TK_IDENTIFIER, "drop database: Expect identifier");
                dropDatabase(cur.identifier);
                return PSR_OK;
            } else if (peek.tokenKind == TK_TABLE) {
                lexer.nextToken(); cur = lexer.nextToken(TE_IDENTIFIER);
                ASSERTERR(cur, TK_IDENTIFIER, "drop table: Expect identifier");
                dropTable(cur.identifier);
                return PSR_OK;
            } else if (peek.tokenKind == TK_INDEX) {
                lexer.nextToken(); cur = lexer.nextToken(TE_IDENTIFIER);
                ASSERTERR(cur, TK_IDENTIFIER, "drop index: Expect identifier");
                dropIndex(cur.identifier);
                return PSR_OK;
            } else {
                return err("drop: Expect keyword DATABASE or TABLE.");
            }
        }

        case TK_ECHO: {
            cur = lexer.nextToken();
            if (cur.tokenKind == TK_OFF) commandLine = false;
            else if (cur.tokenKind == TK_ON) commandLine = true;
            else {
                debugEcho(cur.identifier);
            }
            return PSR_OK;
        }

        case TK_INSERT: {
            cur = lexer.nextToken();
            ASSERTERR(cur, TK_INTO, "insert: Expect keyword INTO.");
            cur = lexer.nextToken(TE_IDENTIFIER);
            ASSERTERR(cur, TK_IDENTIFIER, "insert: Expect identifier.");
            string tbname = cur.identifier;
            cur = lexer.nextToken();
            ASSERTERR(cur, TK_VALUES, "insert: Expect keyword VALUES.");
            return processInsert(tbname);
        }

        case TK_QUIT: {
            Token peek = lexer.peek();
            return PSR_QUIT;
        }

        case TK_SELECT: {
            return processSelect();
        }

        case TK_SHOW: {
            Token peek = lexer.peek();
            if (peek.tokenKind == TK_DATABASE) {
                lexer.nextToken(); cur = lexer.nextToken(TE_IDENTIFIER);
                ASSERTERR(cur, TK_IDENTIFIER, "show database: Expect identifier");
                showDatabase(cur.identifier);
                return PSR_OK;
            } else if (peek.tokenKind == TK_DATABASES) {
                lexer.nextToken(); 
                showDatabases();
                return PSR_OK;
            } else if (peek.tokenKind == TK_TABLE) {
                lexer.nextToken(); cur = lexer.nextToken(TE_IDENTIFIER);
                ASSERTERR(cur, TK_IDENTIFIER, "show table: Expect identifier");
                showTable(cur.identifier);
                return PSR_OK;
            } else if (peek.tokenKind == TK_TABLES) {
                lexer.nextToken(); 
                for (auto& table : tables) {
                    showTable(table); 
                    cout << endl;
                }
                return PSR_OK;
            } else {
                return err("show: Expect keyword DATABASE, DATABASES or TABLE.");
            }
        }

        case TK_UPDATE: {
            cur = lexer.nextToken(TE_IDENTIFIER);
            ASSERTERR(cur, TK_IDENTIFIER, "update: Expect identifier.");
            return processUpdate(cur.identifier);
        }

        case TK_USE: {
            Token peek = lexer.peek();
            if (peek.tokenKind == TK_DATABASE || peek.tokenKind == TK_IDENTIFIER) {
                if (peek.tokenKind == TK_DATABASE) {lexer.nextToken(); cur = lexer.nextToken(TE_IDENTIFIER);}
                else {cur = lexer.nextToken(TE_IDENTIFIER);}
                ASSERTERR(cur, TK_IDENTIFIER, "use database: Expect identifier");
                useDatabase(cur.identifier);
                return PSR_OK;
            } else {
                return err("use: Expect keyword DATABASE.");
            }
        }

        default: {
            return err("Invalid command.");
        }
    }
}

void KontoTerminal::main(bool prompt) {
    commandLine = prompt;
    lexer.setStream(&std::cin);
    while (true) {
        if (commandLine) cout << ">>> ";
        ProcessStatementResult psr = processStatement();
        if (psr==PSR_QUIT) break;
        std::cin.clear(); std::cin.ignore(1024, '\n'); lexer.clearBuffer();
    }
}

KontoTerminal::KontoTerminal() : lexer(true) {
    currentDatabase = "";
    commandLine = true;
}

void KontoTerminal::createDatabase(string dbname) {
    if (directory_exist(dbname)) {
        PT(1, "Error: Directory already exists!");
    } else 
        create_directory(dbname);
}

void KontoTerminal::useDatabase(string dbname) {
    if (directory_exist(dbname)) {
        currentDatabase = dbname;
        if (file_exist(dbname, "__tables.txt"))
            tables = get_lines(dbname, "__tables.txt");
        else tables.clear();
        loadIndices();
    } else {
        PT(1, "Error: No such database!");
    }
}

void KontoTerminal::dropDatabase(string dbname) {
    if (directory_exist(dbname)) {
        remove_directory(dbname);
    } else {
        PT(1, "Error: No such database!");
    }
}

void KontoTerminal::showDatabase(string dbname) {
    if (directory_exist(dbname)) {
        vector<string> t; t.clear();
        if (file_exist(dbname, get_filename(TABLES_FILE)))
            t = get_lines(dbname, get_filename(TABLES_FILE));
        PT(1, "[DATABASE " + dbname + "]");
        if (t.size() > 0) {
            PT(2, "Available tables: ");
            for (auto item : t) 
                PT(3, item);
        } else PT(2, "No available tables.");
    } else {
        PT(1, "Error: No such database!");
    }
}

void KontoTerminal::showDatabases() {
    auto dirs = get_directories();
    for (auto str : dirs) {
        if (str == currentDatabase) cout << " -> "; else cout << "    ";
        cout << str << endl;
    }
    if (dirs.size()==0) PT(1, "No available databases.");
}

void KontoTerminal::createTable(string name, const vector<KontoCDef>& defs) {
    if (currentDatabase == "") {PT(1, "Error: Not using a database!");return;}
    if (hasTable(name)) {PT(1, "Error: Table already exists."); return;}
    KontoTableFile* handle; 
    KontoTableFile::createFile(currentDatabase + "/" + name, &handle);
    for (auto& def : defs) {
        KontoCDef copy = def;
        handle->defineField(copy);
    }
    handle->finishDefineField();
    handle->close();
    tables.push_back(name);
    save_lines(currentDatabase, get_filename(TABLES_FILE), tables);
}

void KontoTerminal::dropTable(string name) {
    if (currentDatabase == "") {PT(1, "Error: Not using a database!");return;}
    bool flag = false;
    for (int i=0;i<tables.size();i++) if (tables[i] == name) {
        tables.erase(tables.begin() + i);
        flag = true; break;
    }
    if (!flag) {PT(1, "Error: No such table!"); return;}
    KontoTableFile* handle; 
    KontoTableFile::loadFile(currentDatabase + "/" + name, &handle);
    handle->drop();
    save_lines(currentDatabase, get_filename(TABLES_FILE), tables);
}

bool KontoTerminal::hasTable(string table) {
    for (int i=0;i<tables.size();i++) if (tables[i]==table) return true;
    return false;
}

void KontoTerminal::showTable(string name) {
    if (currentDatabase == "") {PT(1, "Error: Not using a database!");return;}
    if (!hasTable(name)) {PT(1, "Error: No such table!"); return;}
    KontoTableFile* handle; 
    KontoTableFile::loadFile(currentDatabase + "/" + name, &handle);
    PT(1, "[TABLE " + name + "]");
    cout << TABS[2] << "Records count: " << handle->recordCount << endl;
    PT(1, "[COLUMNS]");
    cout << TABS[2] << "|" << SS(20, "NAME") << "|" << SS(10, "TYPE") << "|" <<
        SS(10, "NULLABLE") << "|" <<
        SS(30, "DEFAULT") << "|" << endl;
    for (auto& item: handle->keys) {
        cout << TABS[2] << "|" << SS(20, item.name) << "|"
        << SS(10, type_to_string(item.type, item.size)) << "|"
        << SS(10, bool_to_string(item.nullable)) << "|"
        << SS(30, value_to_string(item.defaultValue, item.type)) << "|" << endl;
    }
    PT(1, "[PRIMARY KEY]");
    if (handle->hasPrimaryKey()) {
        cout << TABS[2] << "(";
        vector<uint> primaryKeys;
        handle->getPrimaryKeys(primaryKeys);
        for (int i=0;i<primaryKeys.size();i++) {
            if (i!=0) cout << ", ";
            cout << handle->keys[primaryKeys[i]].name;
        }
        cout << ")" << endl;
    } else PT(2, "No primary key defined.");
    vector<string> fknames;
    vector<vector<uint>> cols;
    vector<string> foreignTable;
    vector<vector<string>> foreignName;
    handle->getForeignKeys(fknames, cols, foreignTable, foreignName);
    PT(1, "[FOREIGN KEY]");
    if (fknames.size() > 0) {
        for (int i=0;i<fknames.size();i++) {
            cout << TABS[2] << "FOREIGN KEY " << fknames[i] << " (";
            for (int j=0;j<cols[i].size();j++) {
                if (j!=0) cout << ", ";
                cout << handle->keys[cols[i][j]].name; 
            } 
            cout << ") " << "REFERENCES " << foreignTable[i] << "(";
            for (int j=0;j<foreignName[i].size();j++) {
                if (j!=0) cout << ", ";
                cout << foreignName[i][j];
            }
            cout << ")" << endl;
        }
    } else PT(2, "No foreign keys defined.");
    PT(1, "[INDICES]");
    bool hasIndex = false;
    for (const auto& id: indices) {
        if (id.table != name) continue;
        hasIndex = true;
        cout << TABS[2] << id.name << " (";
        bool first = true;
        for (const auto& col: id.cols) {
            if (!first) cout << ", "; first = false;
            cout << handle->keys[col].name;
        }
        cout << ")" << endl;
    }
    if (!hasIndex) PT(2, "No indices created."); 
    handle->close();
}

void KontoTerminal::alterAddPrimaryKey(string table, const vector<string>& cols) {
    if (currentDatabase == "") {PT(1, "Error: Not using a database!");return;}
    if (!hasTable(table)) {PT(1, "Error: No such table!"); return;}
    KontoTableFile* handle; 
    KontoTableFile::loadFile(currentDatabase + "/" + table, &handle);
    vector<uint> id; id.clear();
    KontoResult res;
    for (auto& item : cols) {
        uint p;
        res = handle->getKeyIndex(item.c_str(), p);
        if (res != KR_OK) {
            PT(1, "Error: No such key named " + item + ".");
            return; 
        }
        id.push_back(p);
    }
    res = handle->alterAddPrimaryKey(id);
    if (res==KR_REPETITION) {err("primary key: Cannot create primary key for there are repetitions.");}
    handle->close();
}

void KontoTerminal::alterDropPrimaryKey(string table) {
    if (currentDatabase == "") {PT(1, "Error: Not using a database!");return;}
    if (!hasTable(table)) {PT(1, "Error: No such table!"); return;}
    KontoTableFile* handle; 
    KontoTableFile::loadFile(currentDatabase + "/" + table, &handle);
    handle->alterDropPrimaryKey();
    handle->close();
}

void KontoTerminal::alterAddColumn(string table, const KontoCDef& def) {
    if (currentDatabase == "") {PT(1, "Error: Not using a database!");return;}
    if (!hasTable(table)) {PT(1, "Error: No such table!"); return;}
    KontoTableFile* handle; 
    KontoTableFile::loadFile(currentDatabase + "/" + table, &handle);
    handle->alterAddColumn(def);
    dropTableIndices(table); saveIndices();
    handle->close();
}

void KontoTerminal::alterDropColumn(string table, string col) {
    if (currentDatabase == "") {PT(1, "Error: Not using a database!");return;}
    if (!hasTable(table)) {PT(1, "Error: No such table!"); return;}
    KontoTableFile* handle; 
    KontoTableFile::loadFile(currentDatabase + "/" + table, &handle);
    auto res = handle->alterDropColumn(col);
    if (res==KR_NO_SUCH_COLUMN) {
        err("alter drop: No such column!");
        handle->close();
        return;
    }
    dropTableIndices(table); saveIndices();
    handle->close();
}

void KontoTerminal::alterChangeColumn(string table, string original, const KontoCDef& newdef) {
    if (currentDatabase == "") {PT(1, "Error: Not using a database!");return;}
    if (!hasTable(table)) {PT(1, "Error: No such table!"); return;}
    KontoTableFile* handle; 
    KontoTableFile::loadFile(currentDatabase + "/" + table, &handle);
    auto res = handle->alterChangeColumn(original, newdef);
    if (res==KR_NO_SUCH_COLUMN) {
        err("alter change: No such column!");
        handle->close();
        return;
    }
    dropTableIndices(table); saveIndices();
    handle->close();
}

void KontoTerminal::alterAddForeignKey(string table, string fkname, 
    const vector<string>& cols, string foreignTable, 
    const vector<string>& foreignNames) 
{
    //cout << "alteraddforeign" << endl;
    if (currentDatabase == "") {PT(1, "Error: Not using a database!");return;}
    if (!hasTable(table)) {PT(1, "Error: No such table!"); return;}
    KontoTableFile* handle; 
    KontoTableFile::loadFile(currentDatabase + "/" + table, &handle);
    vector<uint> id; id.clear();
    if (fkname == "") {
        fkname = "__fkey." + foreignTable;
        for (auto& item:foreignNames) fkname+="."+item;
    }
    for (auto& item : cols) {
        uint p;
        KontoResult res = handle->getKeyIndex(item.c_str(), p);
        if (res != KR_OK) {
            PT(1, "Error: No such key named " + item + ".");
            return; 
        }
        id.push_back(p);
    }
    handle->alterAddForeignKey(fkname, id, foreignTable, foreignNames);
    handle->close();
}

void KontoTerminal::alterDropForeignKey(string table, string fkname) {
    if (currentDatabase == "") {PT(1, "Error: Not using a database!");return;}
    if (!hasTable(table)) {PT(1, "Error: No such table!"); return;}
    KontoTableFile* handle; 
    KontoTableFile::loadFile(currentDatabase + "/" + table, &handle);
    auto res = handle->alterDropForeignKey(fkname);
    if (res==KR_NO_SUCH_FOREIGN) err("Drop foreign: No such foreign key!");
    handle->close();
}

bool _checkInt(string str) {
    int n = str.length();
    int st = 0; if (n > 0 && str[0]=='-') st++;
    for (int i=st;i<n;i++) if (str[i]>'9' || str[i]<'0') return false;
    return true;
}

bool _checkFloat(string str) {
    int n = str.length();
    int st = 0; if (n > 0 && str[0]=='-') st++;
    for (int i=st;i<n;i++) 
        if (!((str[i]<='9' && str[i]>='0') || (str[i]=='.'))) 
            return false;
    return true;
}

ProcessStatementResult KontoTerminal::processInsert(string tbname) {
    if (currentDatabase == "") {PT(1, "Error: Not using a database!");return PSR_ERR;}
    if (!hasTable(tbname)) return err("Error: No such table!");
    KontoTableFile* handle; 
    KontoTableFile::loadFile(currentDatabase + "/" + tbname, &handle);
    char* buffer = new char[handle->getRecordSize()];
    Token cur = lexer.peek();
    int line = 0;
    if (cur.tokenKind != TK_FROM) {
        int insertedCount = 0;
        while (true) {
            //cout << "while true " << endl;
            //handle->indices[0]->debugPrint();
            if (lexer.peek().tokenKind == TK_SEMICOLON) break;
            line++;
            bool error = false;
            Token cur = lexer.nextToken();
            ASSERTERR_CLOSE(cur, TK_LPAREN, "insert values: Expect Lparen.");
            int n = handle->keys.size();
            for (int i=0;i<n;i++) {
                const auto& key = handle->keys[i];
                Token cur = lexer.nextToken(valueTypeToExpectation(key.type));
                switch (key.type) {
                    case KT_INT:
                        if (cur.tokenKind != TK_INT_VALUE) {err("insert - type error: Expect int value." + string(" Line ") + to_string(line)); error=true; break;}
                        handle->setEntryInt(buffer, i, cur.value);
                        break;
                    case KT_FLOAT:
                        if (cur.tokenKind != TK_FLOAT_VALUE) {err("insert - type error: Expect float value." + string(" Line ") + to_string(line)); error=true; break;}
                        handle->setEntryFloat(buffer, i, cur.doubleValue);
                        break;
                    case KT_STRING:
                        if (cur.tokenKind != TK_STRING_VALUE) {err("insert - type error: Expect string value." + string(" Line ") + to_string(line)); error=true; break;}
                        if (cur.identifier.length() > key.size - 1) {err("insert - value error: String too long." + string(" Line ") + to_string(line)); error=true; break;}
                        handle->setEntryString(buffer, i, cur.identifier.c_str());
                        //cout << "cur.identifier = " << cur.identifier << endl;
                        break;
                    case KT_DATE:
                        if (cur.tokenKind != TK_STRING_VALUE) {err("insert - type error: Expect date string value." + string(" Line ") + to_string(line)); error=true; break;}
                        Date date; if (!parse_date(cur.identifier, date)) {err("insert - value error: Invalid date." + string(" Line ") + to_string(line)); error=true; break;}
                        handle->setEntryDate(buffer, i, date);
                        break;
                    default:
                        error = true; err("insert - table type error: Unidentified type." + string(" Line ") + to_string(line)); break;
                }
                if (i!=n-1) {
                    cur=lexer.nextToken();
                    if (cur.tokenKind != TK_COMMA) {error=true; err("insert values: Expect comma." + string(" Line ") + to_string(line)); break;}
                }
            }
            while (true) {
                cur = lexer.peek();
                if (cur.tokenKind == TK_SEMICOLON || cur.tokenKind == TK_RPAREN) break;
                cur = lexer.nextToken();
            }
            if (cur.tokenKind == TK_RPAREN) cur = lexer.nextToken();
            if (lexer.peek().tokenKind == TK_COMMA) cur = lexer.nextToken(); 
            if (error) continue;
            //cout << "before insert" << endl;
            auto result = handle->insert(buffer);
            if (result == KR_REPETITION) {
                err("insert values: Repetition on primary key when inserting #" + to_string(line) + " value");
                continue;
            } else if (result == KR_FOREIGN_KEY_FAIL) {
                err("insert values: Foreign key check failed for #" + to_string(line) + " value");
                continue;
            } else if (result == KR_NULLABLE_FAIL) {
                err("insert values: Nullability check failed for #" + to_string(line) + " value");
                continue;
            }
            insertedCount ++;
            //cout << "inserted " << insertedCount << endl;
            //if (insertedCount%10==0) handle->printTable(true, true);
        }
    } else {
        cur = lexer.nextToken(); cur = lexer.nextToken(TE_STRING_VALUE);
        ASSERTERR_CLOSE(cur, TK_STRING_VALUE, "insert from file: Expect filename string.");
        int insertedCount = 0;
        std::ifstream fin(cur.identifier);
        int line = 0;
        const char delim = '|'; const string delims = "|";
        while (true) {
            //cout << "while true " << endl;
            //handle->indices[0]->debugPrint();
            while (fin.peek()=='\n' || fin.peek()==' ' || fin.peek()=='\r')
                fin.get();
            if (fin.eof()) break;
            line++;
            //if (line>10) break;
            bool error = false;
            int n = handle->keys.size();
            for (int i=0;i<n;i++) {
                const auto& key = handle->keys[i];
                string value;
                char b[2048]; 
                if (i!=n-1) {
                    if (fin.peek()==delim) value=""; else {
                        fin.get(b, 2048, delim); 
                        value=b;
                    }
                } else {
                    fin.get(b, 2048, '\n'); 
                    value=b; 
                    if (value[value.length()-1]==delim) {
                        value=value.substr(0, value.length()-1);
                    }
                }
                //cout << "value: " << i << " - " << value << endl;
                switch (key.type) {
                    case KT_INT: {
                        int pos = value.find('#');
                        if (pos != value.npos) value = value.substr(pos+1, value.length() - pos - 1);
                        if (!_checkInt(value)) {error=true; err("insert - value error: Invalid int." + string(" Line ") + to_string(line)); break;}
                        handle->setEntryInt(buffer, i, atoi(value.c_str()));
                        break;
                    }
                    case KT_FLOAT:
                        if (!_checkFloat(value)) {error=true; err("insert - value error: Invalid float." + string(" Line ") + to_string(line)); break;}
                        handle->setEntryFloat(buffer, i, atof(value.c_str()));
                        break;
                    case KT_STRING:
                        if (value.length() > key.size - 1) {error=true; err("insert - value error: String too long." + string(" Line ") + to_string(line)); break;}
                        handle->setEntryString(buffer, i, value.c_str());
                        //cout << "cur.identifier = " << cur.identifier << endl;
                        break;
                    case KT_DATE:
                        Date date; if (!parse_date(value, date)) {error=true; err("insert - value error: Invalid date." + string(" Line ") + to_string(line)); break;}
                        handle->setEntryDate(buffer, i, date);
                        break;
                    default:
                        error=true; err("insert from file - type error: Unidentified type." + string(" Line ") + to_string(line)); break;
                }
                if (error) break;
                if (i!=n-1) {
                    value = fin.get();
                    //cout << i << " value : " << int(value[0]);
                    if (value != delims) {error=true; err("insert from file: Expect comma." + string(" Line ") + to_string(line)); break;}
                } else {if (fin.peek() == delim) fin.get();}
            }
            if (!error) {
                auto result = handle->insert(buffer);
                if (result == KR_REPETITION) {
                    err("insert from file: Repetition on primary key when inserting #" + to_string(line) + " value");
                } else if (result == KR_FOREIGN_KEY_FAIL) {
                    err("insert from file: Foreign key check failed for #" + to_string(line) + " value");
                } else if (result == KR_NULLABLE_FAIL) {
                    err("insert values: Nullability check failed for #" + to_string(line) + " value");
                } else insertedCount ++;
            }
            while (fin.peek()!=EOF && fin.peek()!='\n') fin.get();
        }
        //cout << "close" << endl;
        fin.close();
    }
    delete[] buffer;
    handle->close();
    return PSR_OK;
}

void KontoTerminal::loadIndices() {
    if (currentDatabase == "") {PT(1, "Error: Not using a database!");return;}
    indices.clear();
    if (!file_exist(currentDatabase, get_filename(INDICES_FILE))) return;
    std::ifstream fin(get_filename(currentDatabase + "/" + INDICES_FILE));
    while (!fin.eof()) {
        KontoIndexDesc desc;
        fin >> desc.name; 
        if (fin.eof()) break;
        fin >> desc.table;
        int n; fin >> n;
        desc.cols.clear();
        while (n--) {int p; fin>>p; desc.cols.push_back(p);}
        indices.push_back(desc);
    }
    fin.close();
}

void KontoTerminal::saveIndices() {
    if (currentDatabase == "") {PT(1, "Error: Not using a database!");return;}
    std::ofstream fout(get_filename(currentDatabase + "/" + INDICES_FILE));
    for (auto& item: indices) {
        fout << item.name << " " << item.table << " " << item.cols.size() << endl;
        for (auto& p: item.cols) fout << p << " "; 
        fout << endl;
    }
    fout.close();
}

void KontoTerminal::dropTableIndices(string tb) {
    int n = indices.size();
    for (int i=n-1;i>=0;i--) if (indices[i].table == tb) indices.erase(indices.begin() + i);
}

void KontoTerminal::createIndex(string idname, string table, const vector<string>& cols) {
    if (currentDatabase == "") {PT(1, "Error: Not using a database!");return;}
    if (!hasTable(table)) {PT(1,"Error: No such table!"); return;}
    KontoTableFile* handle; 
    KontoTableFile::loadFile(currentDatabase + "/" + table, &handle);
    vector<uint> colids; colids.clear();
    KontoResult res;
    for (auto& item: cols) {
        uint p;
        res = handle->getKeyIndex(item.c_str(), p);
        if (res!=KR_OK) {PT(1, "Error: No such column called " + item); return;}
        colids.push_back(p);
    }
    res = handle->createIndex(colids, nullptr, false);
    if (res == KR_INDEX_ALREADY_EXISTS) {
        PT(1, "Error: Index already exists.");
        handle->close();
        return;
    }
    KontoIndexDesc desc{
        .name = idname,
        .table = table,
        .cols = colids
    }; 
    indices.push_back(desc);
    saveIndices();
    handle->close();
}

void KontoTerminal::dropIndex(string idname, string table) {
    if (currentDatabase == "") {PT(1, "Error: Not using a database!");return;}
    int n = indices.size();
    KontoIndexDesc* ptr = nullptr; int position;
    for (int i=0; i<n; i++) 
        if (indices[i].name == idname && (table=="" || table==indices[i].table)) {ptr = &indices[i]; position = i; break;}
    if (ptr==nullptr) {PT(1, "Error: No such index!"); return;}
    KontoTableFile* handle; 
    KontoTableFile::loadFile(currentDatabase + "/" + ptr->table, &handle);
    handle->dropIndex(ptr->cols);
    handle->close();
    indices.erase(indices.begin() + position);
    saveIndices();
}

void KontoTerminal::debugIndex(string idname) {
    if (currentDatabase == "") {PT(1, "Error: Not using a database!");return;}
    string table;
    int n = indices.size();
    KontoIndexDesc* ptr = nullptr; int position;
    for (int i=0; i<n; i++) if (indices[i].name == idname) {ptr = &indices[i]; position = i; break;}
    if (ptr==nullptr) {PT(1, "Error: No such index!"); return;}
    KontoTableFile* handle; 
    KontoTableFile::loadFile(currentDatabase + "/" + ptr->table, &handle);
    handle->debugIndex(ptr->cols);
    handle->close();
}

void KontoTerminal::debugIndex() {
    if (currentDatabase == "") {PT(1, "Error: Not using a database!");return;}
    for (auto& item : indices) {
        cout << TABS[1] << "[" << item.name << " on " << item.table << "] ";
        cout << "columns (";
        for (int i=0;i<item.cols.size();i++) {
            if (i!=0) cout << ", ";
            cout << item.cols[i];
        }
        cout << ")" << endl;
    }
    if (indices.size()==0) cout << TABS[1] << "No explicitly defined index!" << endl;
}

void KontoTerminal::debugTable(string tbname) {
    if (currentDatabase == "") {PT(1, "Error: Not using a database!");return;}
    if (!hasTable(tbname)) {PT(1,"Error: No such table!"); return;}
    KontoTableFile* handle; 
    KontoTableFile::loadFile(currentDatabase + "/" + tbname, &handle);
    handle->printTable(true, true);
    handle->close();
}

void KontoTerminal::debugPrimary(string tbname) {
    if (currentDatabase == "") {PT(1, "Error: Not using a database!");return;}
    if (!hasTable(tbname)) {PT(1,"Error: No such table!"); return;}
    KontoTableFile* handle; 
    KontoTableFile::loadFile(currentDatabase + "/" + tbname, &handle);
    KontoIndex* index = handle->getPrimaryIndex();
    if (index==nullptr) {PT(1,"Error: This table has no primary index."); return;};
    index->debugPrint();
    handle->close();
}

uint KontoTerminal::getColumnIndex(string table, string col, KontoKeyType& type) {
    if (!hasTable(table)) return -1;
    KontoTableFile* handle; 
    KontoTableFile::loadFile(currentDatabase + "/" + table, &handle);
    uint ret;
    KontoResult res = handle->getKeyIndex(col.c_str(), ret);
    if (res == KR_OK) type = handle->keys[ret].type;
    handle->close();
    if (res == KR_OK) return ret;
    else return -1;
} 

Token getDefaultValueToken(KontoKeyType type) {
    switch (type) {
        case KT_INT: return Token(TK_INT_VALUE, DEFAULT_INT_VALUE);
        case KT_FLOAT: return Token(TK_FLOAT_VALUE, DEFAULT_FLOAT_VALUE);
        case KT_STRING: return Token(TK_STRING_VALUE, DEFAULT_STRING_VALUE);
        case KT_DATE: return Token(TK_STRING_VALUE, DEFAULT_DATE_VALUE_STRING);
    }
    return Token();
}

ProcessStatementResult KontoTerminal::processWhereTerm(const vector<string>& tables, KontoWhere& out) {
    Token cur = lexer.nextToken(TE_IDENTIFIER);
    ASSERTERR(cur, TK_IDENTIFIER, "where: Expect identifier");
    Token peek = lexer.peek();
    out = KontoWhere();
    KontoKeyType keytype;
    if (peek.tokenKind != TK_DOT) {
        bool flag = false;
        for (auto& table : tables) {
            int res = getColumnIndex(table, cur.identifier, keytype);
            if (res != -1) {
                out.ltable = table; 
                out.lid = res;
                out.keytype = keytype;
                flag = true;
                break;
            }
        }
        if (!flag) return err("where: No column called " + cur.identifier);
    } else {
        lexer.nextToken(); peek = lexer.nextToken(TE_IDENTIFIER);
        ASSERTERR(peek, TK_IDENTIFIER, "where: Expect identifier after dot.");
        int res = getColumnIndex(cur.identifier, peek.identifier, keytype);
        if (res==-1) return err("where: No column called " + peek.identifier + " in " + cur.identifier);
        out.ltable = cur.identifier;
        out.lid = res;
        out.keytype = keytype;
    }
    cur = lexer.nextToken();
    if (cur.tokenKind == TK_IS) {
        cur = lexer.nextToken();
        if (cur.tokenKind == TK_NULL) {
            out.type = WT_CONST; 
            out.rvalue = getDefaultValueToken(out.keytype);
            out.op = OP_EQUAL;
            return PSR_OK;
        } else if (cur.tokenKind == TK_NOT) {
            cur = lexer.nextToken();
            ASSERTERR(cur, TK_NULL, "where - not: Expect keyword NULL");
            out.type = WT_CONST; 
            out.rvalue = getDefaultValueToken(out.keytype);
            out.op = OP_NOT_EQUAL;
            return PSR_OK;
        } else {
            return err("where - is: expect keyword NOT or NULL");
        }
    };
    // cur is operator
    switch (cur.tokenKind) {
        case TK_EQUAL: out.op = OP_EQUAL; break;
        case TK_NOT_EQUAL: out.op = OP_NOT_EQUAL; break;
        case TK_LESS: out.op = OP_LESS; break;
        case TK_LESS_EQUAL: out.op = OP_LESS_EQUAL; break;
        case TK_GREATER: out.op = OP_GREATER; break;
        case TK_GREATER_EQUAL: out.op = OP_GREATER_EQUAL; break;
        default: return err("where: operator not recognized.");
    }
    cur = lexer.nextToken(valueTypeToExpectation(out.keytype));
    if (cur.tokenKind == TK_IDENTIFIER) {
        peek = lexer.peek();
        if (peek.tokenKind != TK_DOT) {
            bool flag = false;
            for (auto& table : tables) {
                int res = getColumnIndex(table, cur.identifier, keytype);
                if (res != -1) {
                    out.rtable = table; 
                    out.rid = res;
                    if (keytype != out.keytype) return err("where: lhs and rhs type does not match.");
                    flag = true;
                    break;
                }
            }
            if (!flag) return err("where: No column called " + cur.identifier);
        } else {
            lexer.nextToken(); peek = lexer.nextToken(TE_IDENTIFIER);
            int res = getColumnIndex(cur.identifier, peek.identifier, keytype);
            if (res==-1) return err("where: No column called " + peek.identifier + " in " + cur.identifier);
            out.rtable = cur.identifier;
            out.rid = res;
            if (keytype != out.keytype) return err("where: lhs and rhs type does not match.");
        }
        if (out.ltable == out.rtable) out.type = WT_INNER;
        else out.type = WT_CROSS;
        return PSR_OK;
    } else {
        if (out.keytype == KT_INT) {
            ASSERTERR(cur, TK_INT_VALUE, "where: rvalue should be int.");
        } else if (out.keytype == KT_FLOAT) {
            ASSERTERR(cur, TK_FLOAT_VALUE, "where: rvalue should be float.");
        } else if (out.keytype == KT_STRING) {
            ASSERTERR(cur, TK_STRING_VALUE, "where: rvalue should be string.");
        } else if (out.keytype == KT_DATE) {
            ASSERTERR(cur, TK_STRING_VALUE, "where: rvalue should be date string.");
            cur.tokenKind = TK_DATE_VALUE;
            Date parsed; if (!parse_date(cur.identifier, parsed)) return err("where: rvalue is not valid date.");
            cur.value = parsed;
        }
        out.rvalue = cur;
        out.type = WT_CONST;
        return PSR_OK;
    }
}

ProcessStatementResult KontoTerminal::processWheres(const string& table, vector<KontoWhere>& out) {
    vector<string> str; str.clear(); str.push_back(table);
    return processWheres(str, out);
}

/*bool _kontoWhereComp(const KontoWhere& a, const KontoWhere& b) {
    if (a.type != WT_CROSS && b.type == WT_CROSS) return true;
    if (a.type == WT_CROSS) return false;
    if (a.ltable < b.ltable) return true;

}*/

ProcessStatementResult KontoTerminal::processWheres(const vector<string>& tables, vector<KontoWhere>& out) {
    out.clear();
    Token peek = lexer.peek();
    if (peek.tokenKind == TK_SEMICOLON) return PSR_OK;
    while (true) {
        KontoWhere term;
        ProcessStatementResult psr = processWhereTerm(tables, term);
        if (psr != PSR_OK) return psr;
        out.push_back(term);
        if (lexer.peek().tokenKind == TK_AND) {
            lexer.nextToken(); continue;
        }
        else break;
    }
    for (int i=0;i<out.size();i++) {
        if (out[i].type != WT_CONST) continue;
        if (out[i].op == OP_EQUAL || out[i].op == OP_NOT_EQUAL) continue;
        for (int j=i+1;j<out.size();j++) {
            if (out[j].type != WT_CONST) continue;
            if (out[j].ltable != out[i].ltable) continue;
            if (out[j].lid != out[i].lid) continue;
            if (out[j].op == OP_EQUAL || out[j].op == OP_NOT_EQUAL) continue;
            if (out[i].op == OP_GREATER && out[j].op == OP_LESS) {
                out[i].op = OP_LORO; out[i].lvalue = out[i].rvalue; out[i].rvalue = out[j].rvalue; out.erase(out.begin() + j);
            }
            if (out[i].op == OP_GREATER && out[j].op == OP_LESS_EQUAL) {
                out[i].op = OP_LORC; out[i].lvalue = out[i].rvalue; out[i].rvalue = out[j].rvalue; out.erase(out.begin() + j);
            }
            if (out[i].op == OP_GREATER_EQUAL && out[j].op == OP_LESS) {
                out[i].op = OP_LCRO; out[i].lvalue = out[i].rvalue; out[i].rvalue = out[j].rvalue; out.erase(out.begin() + j);
            }
            if (out[i].op == OP_GREATER_EQUAL && out[j].op == OP_LESS_EQUAL) {
                out[i].op = OP_LCRC; out[i].lvalue = out[i].rvalue; out[i].rvalue = out[j].rvalue; out.erase(out.begin() + j);
            }
            if (out[i].op == OP_LESS && out[j].op == OP_GREATER) {
                out[i].op = OP_LORO; out[i].lvalue = out[j].rvalue; out.erase(out.begin() + j);
            }
            if (out[i].op == OP_LESS_EQUAL && out[j].op == OP_GREATER) {
                out[i].op = OP_LORC; out[i].lvalue = out[j].rvalue; out.erase(out.begin() + j);
            }
            if (out[i].op == OP_LESS && out[j].op == OP_GREATER_EQUAL) {
                out[i].op = OP_LCRO; out[i].lvalue = out[j].rvalue; out.erase(out.begin() + j);
            }
            if (out[i].op == OP_LESS_EQUAL && out[j].op == OP_GREATER_EQUAL) {
                out[i].op = OP_LCRC; out[i].lvalue = out[j].rvalue; out.erase(out.begin() + j);
            }
        }
    }
    return PSR_OK;
}

KontoQRes KontoTerminal::queryWhere(const KontoWhere& where) {
    assert(where.type != WT_CROSS);
    KontoTableFile* handle; 
    KontoQRes ret, tmp;
    KontoTableFile::loadFile(currentDatabase + "/" + where.ltable, &handle);
    if (where.type != WT_INNER) {
        vector<uint> list = single_uint_vector(where.lid);
        KontoIndex* index = handle->getIndex(list);
        if (index != nullptr) {
            //cout << "using index to query" << endl;
            char* buffer = new char[handle->getRecordSize()];
            char* lbuffer = new char[handle->getRecordSize()];
            switch (where.keytype) {
                case KT_INT: 
                    handle->setEntryInt(buffer, where.lid, where.rvalue.value); 
                    handle->setEntryInt(lbuffer, where.lid, where.lvalue.value);
                    break;
                case KT_FLOAT: 
                    handle->setEntryFloat(buffer, where.lid, where.rvalue.value);
                    handle->setEntryFloat(lbuffer, where.lid, where.lvalue.doubleValue);
                    break;
                case KT_STRING:
                    handle->setEntryString(buffer, where.lid, where.rvalue.identifier.c_str());
                    handle->setEntryString(lbuffer, where.lid, where.lvalue.identifier.c_str());
                    break;
                case KT_DATE:
                    handle->setEntryDate(buffer, where.lid, where.rvalue.value);
                    handle->setEntryDate(lbuffer, where.lid, where.lvalue.value);
                    break;
                default:
                    assert(false);
                    break;
            }
            switch (where.op) {
                case OP_EQUAL: 
                    index->queryInterval(buffer, buffer, ret, true, true, false); break;
                case OP_NOT_EQUAL:
                    index->queryInterval(buffer, nullptr, ret, false, true); 
                    index->queryInterval(nullptr, buffer, tmp, true, false);
                    ret = tmp.append(ret); break;
                case OP_LESS:
                    index->queryInterval(nullptr, buffer, ret, true, false); break;
                case OP_LESS_EQUAL:
                    index->queryInterval(nullptr, buffer, ret, true, true); break;
                case OP_GREATER:
                    index->queryInterval(buffer, nullptr, ret, false, true); break;
                case OP_GREATER_EQUAL:
                    index->queryInterval(buffer, nullptr, ret, true, true); break;
                case OP_LCRC:
                    index->queryInterval(lbuffer, buffer, ret, true, true); break;
                case OP_LCRO:
                    index->queryInterval(lbuffer, buffer, ret, true, false); break;
                case OP_LORC:
                    index->queryInterval(lbuffer, buffer, ret, false, true); break;
                case OP_LORO:
                    index->queryInterval(lbuffer, buffer, ret, false, false); break;
            }
            delete[] buffer;
            delete[] lbuffer;
        } else {
            //cout << "using iterator to query" << endl;
            KontoQRes q; handle->allEntries(q);
            if (where.op < OP_DOUBLE) {
                switch (where.keytype) {
                    case KT_INT: {
                        int vi = where.rvalue.value;
                        handle->queryEntryInt(q, where.lid, where.op, vi, ret);
                        break;
                    }
                    case KT_FLOAT: {
                        double vd = where.rvalue.doubleValue;
                        handle->queryEntryFloat(q, where.lid, where.op, vd, ret);
                        break;
                    }
                    case KT_STRING: {
                        const char* vs = where.rvalue.identifier.c_str();
                        handle->queryEntryString(q, where.lid, where.op, vs, ret);
                        break;
                    }
                    case KT_DATE: {
                        Date vd = where.rvalue.value;
                        handle->queryEntryDate(q, where.lid, where.op, vd, ret);
                        break;
                    }
                    default:
                        assert(false);
                }
            } else {
                switch (where.keytype) {
                    case KT_INT: {
                        int vir = where.rvalue.value, vil = where.lvalue.value;
                        handle->queryEntryInt(q, where.lid, where.op, vil, vir, ret);
                        break;
                    }
                    case KT_FLOAT: {
                        double vdr = where.rvalue.doubleValue, vdl = where.lvalue.doubleValue;
                        handle->queryEntryFloat(q, where.lid, where.op, vdl, vdr, ret);
                        break;
                    }
                    case KT_STRING: {
                        const char* vsr = where.rvalue.identifier.c_str(), *vsl = where.lvalue.identifier.c_str();
                        handle->queryEntryString(q, where.lid, where.op, vsl, vsr, ret);
                        break;
                    }
                    case KT_DATE: {
                        Date vdr = where.rvalue.value, vdl = where.lvalue.value;
                        handle->queryEntryDate(q, where.lid, where.op, vdl, vdr, ret);
                        break;
                    }
                    default:
                        assert(false);
                }
            }
        }
    } else {
        KontoQRes q; handle->allEntries(q);
        handle->queryCompare(q, where.lid, where.rid, where.op, ret);
    }
    handle->close();
    return ret;
}

KontoQRes KontoTerminal::queryWhereWithin(const KontoQRes& prev, const KontoWhere& where) {
    assert(where.type != WT_CROSS);
    KontoTableFile* handle; 
    KontoQRes ret, tmp;
    KontoTableFile::loadFile(currentDatabase + "/" + where.ltable, &handle);
    if (where.type != WT_INNER) {
        const KontoQRes& q = prev;
        if (where.op < OP_DOUBLE) {
            switch (where.keytype) {
                case KT_INT: {
                    int vi = where.rvalue.value;
                    handle->queryEntryInt(q, where.lid, where.op, vi, ret);
                    break;
                }
                case KT_FLOAT: {
                    double vd = where.rvalue.doubleValue;
                    handle->queryEntryFloat(q, where.lid, where.op, vd, ret);
                    break;
                }
                case KT_STRING: {
                    const char* vs = where.rvalue.identifier.c_str();
                    handle->queryEntryString(q, where.lid, where.op, vs, ret);
                    break;
                }
                case KT_DATE: {
                    Date vd = where.rvalue.value;
                    handle->queryEntryDate(q, where.lid, where.op, vd, ret);
                    break;
                }
                default:
                    assert(false);
            }
        } else {
            switch (where.keytype) {
                case KT_INT: {
                    int vir = where.rvalue.value, vil = where.lvalue.value;
                    handle->queryEntryInt(q, where.lid, where.op, vil, vir, ret);
                    break;
                }
                case KT_FLOAT: {
                    double vdr = where.rvalue.doubleValue, vdl = where.lvalue.doubleValue;
                    handle->queryEntryFloat(q, where.lid, where.op, vdl, vdr, ret);
                    break;
                }
                case KT_STRING: {
                    const char* vsr = where.rvalue.identifier.c_str(), *vsl = where.lvalue.identifier.c_str();
                    handle->queryEntryString(q, where.lid, where.op, vsl, vsr, ret);
                    break;
                }
                case KT_DATE: {
                    Date vdr = where.rvalue.value, vdl = where.lvalue.value;
                    handle->queryEntryDate(q, where.lid, where.op, vdl, vdr, ret);
                    break;
                }
                default:
                    assert(false);
            }
        }
    } else {
        handle->queryCompare(prev, where.lid, where.rid, where.op, ret);
    }
    handle->close();
    return ret;
}

// should guarantee that wheres are from the same table.
void KontoTerminal::queryWheres(const vector<KontoWhere>& wheres, KontoQRes& out) {
    assert(wheres.size() > 0);
    string table = wheres[0].ltable;
    for (int i=1;i<wheres.size();i++) assert(wheres[i].ltable == table);
    KontoTableFile* handle;
    KontoTableFile::loadFile(currentDatabase + "/" + table, &handle);
    int first = 0; bool found = false;
    for (int i=0;i<wheres.size();i++) {
        if (wheres[i].type == WT_CONST && wheres[i].op >= OP_DOUBLE) {
            KontoIndex* index = handle->getIndex(single_uint_vector(wheres[i].lid));
            if (index!=nullptr) {found=true; first=i; break;}
        } 
    }
    if (!found) {
        for (int i=0;i<wheres.size();i++) {
            if (wheres[i].type == WT_CONST && wheres[i].op < OP_DOUBLE) {
                KontoIndex* index = handle->getIndex(single_uint_vector(wheres[i].lid));
                if (index!=nullptr) {first=i; break;}
            }
        }
    }
    handle->close();
    out = queryWhere(wheres[first]);
    for (int i=0;i<wheres.size();i++) {
        if (i!=first) out = queryWhereWithin(out, wheres[i]);
    }
}

void KontoTerminal::queryWheres(const vector<KontoWhere>& wheres, vector<string>& tables, vector<KontoQRes>& results) {
    vector<vector<KontoWhere>> whereSplited; whereSplited.clear();
    tables.clear();
    for (auto& where: wheres) {
        if (where.type == WT_CROSS) continue;
        string tb = where.ltable;
        bool found = false; 
        for (int i=0;i<tables.size();i++) 
            if (tables[i]==tb) {found=true; whereSplited[i].push_back(where);}
        if (!found) {
            tables.push_back(tb); 
            whereSplited.push_back(vector<KontoWhere>()); 
            whereSplited[whereSplited.size()-1].push_back(where);
        }
    }
    results.clear();
    for (auto& vec: whereSplited) {
        KontoQRes tbResult;
        queryWheres(vec, tbResult);
        results.push_back(tbResult);
    }
}

void KontoTerminal::queryWheresFrom(const vector<KontoWhere>& wheres, const vector<string>& givenTables, vector<KontoQRes>& results) {
    vector<string> tables; 
    vector<KontoQRes> temp;
    queryWheres(wheres, tables, temp);
    results.clear();
    for (int i=0;i<givenTables.size();i++) {
        bool flag = false;
        for (int j=0;j<tables.size();j++) {if (tables[j]==givenTables[i]) {results.push_back(temp[j]); flag = true; break;}}
        if (!flag) {
            KontoTableFile* handle;
            KontoTableFile::loadFile(currentDatabase + "/" + givenTables[i], &handle);
            KontoQRes q;
            handle->allEntries(q);
            results.push_back(q);
            handle->close();
        }
    }
}

void KontoTerminal::deletes(string tbname, const vector<KontoWhere>& wheres) {
    if (currentDatabase == "") {PT(1, "Error: Not using a database!");return;}
    if (!hasTable(tbname)) {PT(1,"Error: No such table!"); return;}
    for (auto& item: wheres) assert(item.type != WT_CROSS);
    KontoQRes q; queryWheres(wheres, q);
    KontoTableFile* handle;
    KontoTableFile::loadFile(currentDatabase + "/" + tbname, &handle);
    handle->deletes(q);
    handle->close();
}

void KontoTerminal::debugFrom(string tbname, const vector<KontoWhere>& wheres) {
    printWheres(wheres);
    for (auto& item: wheres) assert(item.type != WT_CROSS);
    KontoQRes q; queryWheres(wheres, q);
    KontoTableFile* handle;
    KontoTableFile::loadFile(currentDatabase + "/" + tbname, &handle);
    handle->printTable(q, true);
    handle->close();
}

ProcessStatementResult KontoTerminal::processSelect() {
    if (currentDatabase == "") {PT(1, "Error: Not using a database!");return PSR_ERR;}
    Token cur, peek = lexer.peek();
    vector<string> selectedColumns; selectedColumns.clear();
    vector<string> selectedColumnTables; selectedColumnTables.clear();
    bool asterisk = false;
    if (peek.tokenKind == TK_ASTERISK) {
        asterisk = true;
        cur = lexer.nextToken();
        peek = lexer.peek();
        ASSERTERR(peek, TK_FROM, "select asterisk: Expect keyword from after asterisk.");
    } else {
        while (peek.tokenKind != TK_FROM) {
            cur = lexer.nextToken(TE_IDENTIFIER); peek = lexer.peek();
            ASSERTERR(cur, TK_IDENTIFIER, "select cols: Expect identifier.");
            if (peek.tokenKind == TK_DOT) {
                selectedColumnTables.push_back(cur.identifier);
                cur = lexer.nextToken(); cur = lexer.nextToken(TE_IDENTIFIER);
                ASSERTERR(cur, TK_IDENTIFIER, "select cols: Expect identifier after dot.");
                selectedColumns.push_back(cur.identifier);
            } else {
                selectedColumns.push_back(cur.identifier);
                selectedColumnTables.push_back("");
            } 
            peek = lexer.peek(); 
            if (peek.tokenKind == TK_COMMA) lexer.nextToken();
        }
    }
    lexer.nextToken(); 
    vector<string> fromTables; fromTables.clear();
    while (true) {
        cur = lexer.nextToken(TE_IDENTIFIER);
        ASSERTERR(cur, TK_IDENTIFIER, "select from: Expect identifier.");
        if (!hasTable(cur.identifier)) {return err("No such a table named " + cur.identifier);}
        fromTables.push_back(cur.identifier);
        cur = lexer.nextToken();
        if (cur.tokenKind == TK_WHERE || cur.tokenKind == TK_SEMICOLON) break;
    }
    if (cur.tokenKind == TK_SEMICOLON) lexer.putback(cur);
    vector<KontoWhere> wheres;
    ProcessStatementResult psr = processWheres(fromTables, wheres);
    if (psr != PSR_OK) return psr;
    vector<KontoQRes> lists;
    //printWheres(wheres);
    uint nTables = fromTables.size();
    queryWheresFrom(wheres, fromTables, lists);
    for (int i=0;i<nTables;i++) if (lists[i].size()==0) {
        cout << TABS[1] << "The result is empty table." << endl;
        return PSR_OK;
    }
    typedef KontoTableFile* KontoTableFilePtr;
    KontoTableFilePtr tables[nTables];
    for (int i=0;i<nTables;i++) 
        KontoTableFile::loadFile(currentDatabase + "/" + fromTables[i], &tables[i]);
    vector<uint> selectedTables; // the selected columns' table, indicated by index
    vector<uint> selectedKids;   // the selected columns, indicated by index
    KontoTableFile* tempTable;
    KontoTableFile::createFile(currentDatabase + "/" + TEMP_FILE, &tempTable);
    selectedTables.clear();
    for (int i=0;i<selectedColumns.size();i++) {
        if (selectedColumnTables[i]=="") { 
            string target = selectedColumns[i];
            bool flag = false;
            for (int j=0;j<nTables;j++) {
                uint kid;
                KontoResult result = tables[j]->getKeyIndex(target.c_str(), kid);
                if (result == KR_OK) {
                    selectedTables.push_back(j); selectedKids.push_back(kid); flag=true; break;
                }
            }
            if (!flag) {
                for (auto& ptr : tables) ptr->close();
                return err("select: no such column called " + target);
            }
        } else {
            for (int j=0;j<nTables;j++) {
                if (fromTables[j] != selectedColumnTables[i]) continue;
                uint kid; 
                KontoResult result = tables[j]->getKeyIndex(selectedColumns[i].c_str(), kid);
                if (result != KR_OK) {
                    for (auto& ptr : tables) ptr->close();
                    return err("select: no such column called " + selectedColumns[i] + " in " + selectedColumnTables[i]);
                }
                selectedTables.push_back(j); selectedKids.push_back(kid);
                break;
            }
        }
    }
    if (asterisk) {
        for (int i=0;i<nTables;i++) 
            for (int j=0;j<tables[i]->keys.size();j++) {
                selectedTables.push_back(i); selectedKids.push_back(j);
                selectedColumns.push_back(tables[i]->keys[j].name);
                selectedColumnTables.push_back(fromTables[i]);
            }
    }
    //for (int i=0;i<selectedTables.size();i++) cout << selectedTables[i] << " " << selectedKids[i] << endl;
    int nSelected = selectedKids.size();
    for (int i=0;i<nSelected;i++) {
        KontoCDef defcloned = tables[selectedTables[i]]->keys[selectedKids[i]];
        bool repeat = false;
        for (int j=0;j<nSelected;j++) {
            if (i==j) continue;
            if (selectedColumns[j]==selectedColumns[i]) {repeat=true; break;}
        }
        if (repeat) {
            string tableName = tables[selectedTables[i]]->filename;
            int plc = tableName.find('/');
            tableName = tableName.substr(plc+1, tableName.length() - plc-1);
            defcloned.name = tableName + "_" + tables[selectedTables[i]]->keys[selectedKids[i]].name;
        }
        tempTable->defineField(defcloned);
    }
    tempTable->finishDefineField();
    charptr buffers[nTables]; for (int i=0;i<nTables;i++) buffers[i] = new char[tables[i]->getRecordSize()];
    char insertBuffer[tempTable->getRecordSize()];
    // get WT_CROSS wheres
    vector<KontoWhere> whereTemp = wheres; wheres.clear(); 
    for (auto& where: whereTemp) if (where.type == WT_CROSS) wheres.push_back(where);
    uint nWheres = wheres.size();
    vector<uint> ltables, rtables; ltables.clear(); rtables.clear();
    for (auto& where: wheres) {
        for (int i=0;i<nTables;i++) if (where.ltable == fromTables[i]) {ltables.push_back(i); break;}
        for (int i=0;i<nTables;i++) if (where.rtable == fromTables[i]) {rtables.push_back(i); break;}
    }
    // iterate from iterators[0]
    uint iterators[nTables]; for (int i=0;i<nTables;i++) iterators[i] = 0;
    for (int i=0;i<nTables;i++) tables[i]->getDataCopied(lists[i].get(iterators[i]), buffers[i]);
    //printWheres(wheres);
    while (true) {
        bool flag = true;
        // check all wheres of WT_CROSS, whereas other types of wheres are already filtered in the lists
        //cout << "consuling wheres" << endl;
        for (int i=0;i<nTables;i++) if (KontoTableFile::checkDeletedFlags(VI(buffers[i] + 4))) {
            flag = false; break;
        }
        for (int i=0;i<nWheres;i++) {
            char* lptr = buffers[ltables[i]] + tables[ltables[i]]->keys[wheres[i].lid].position;
            char* rptr = buffers[rtables[i]] + tables[rtables[i]]->keys[wheres[i].rid].position;
            int comp = KontoIndex::compare(lptr, rptr, wheres[i].keytype);
            bool consistent = false;
            switch (wheres[i].op) {
                case OP_LESS: consistent = comp < 0; break;
                case OP_LESS_EQUAL: consistent = comp <= 0; break;
                case OP_GREATER: consistent = comp > 0; break;
                case OP_GREATER_EQUAL: consistent = comp >= 0; break;
                case OP_EQUAL: consistent = comp == 0; break;
                case OP_NOT_EQUAL: consistent = comp != 0; break;
            }
            if (!consistent) {flag=false; break;}
        }
        if (flag) {
            //cout << "adding the permutation" << endl;
            // add the permutation to the results
            for (int i=0;i<nSelected;i++) 
                memcpy(insertBuffer + tempTable->keys[i].position, 
                    buffers[selectedTables[i]] + tables[selectedTables[i]]->keys[selectedKids[i]].position, 
                    tables[selectedTables[i]]->keys[selectedKids[i]].size);
            tempTable->insert(insertBuffer);
        }
        // iterate
        int t = 0; iterators[t]++;
        while (t<nTables && iterators[t]>=lists[t].size()) {
            iterators[t]=0; 
            tables[t]->getDataCopied(lists[t].get(iterators[t]), buffers[t]);
            t++;
            iterators[t]++;
        }
        if (t==nTables) break;
        tables[t]->getDataCopied(lists[t].get(iterators[t]), buffers[t]);
        //cout << "iterators: "; for (int i=0;i<nTables;i++) cout << iterators[i] << " "; cout << endl;
    }
    for (int i=0;i<nTables;i++) {
        tables[i]->close(); delete[] buffers[i];
    }
    tempTable->printTable(false, false);
    tempTable->drop();
    return PSR_OK;
}

void KontoTerminal::printWhere(const KontoWhere& where) {
    cout << "[where ";
    if (where.type == WT_CONST) {
        cout << "wt_const" << " " << "type=" << _key_type_strs[where.keytype] << " ";
        cout << "op=" << _operator_type_strs[where.op];
        cout << " l=" << where.ltable << "(" << where.lid << ")";
        if (where.op < OP_DOUBLE) {
            cout << " const=";
            if (where.keytype == KT_INT) cout << where.rvalue.value;
            else if (where.keytype == KT_FLOAT) cout << where.rvalue.doubleValue;
            else if (where.keytype == KT_STRING) cout << where.rvalue.identifier;
            else if (where.keytype == KT_DATE) cout << date_to_string(where.rvalue.value);
        } else {
            cout << " lconst=";
            if (where.keytype == KT_INT) cout << where.lvalue.value;
            else if (where.keytype == KT_FLOAT) cout << where.lvalue.doubleValue;
            else if (where.keytype == KT_STRING) cout << where.lvalue.identifier;
            else if (where.keytype == KT_DATE) cout << date_to_string(where.lvalue.value);
            cout << " rconst=";
            if (where.keytype == KT_INT) cout << where.rvalue.value;
            else if (where.keytype == KT_FLOAT) cout << where.rvalue.doubleValue;
            else if (where.keytype == KT_STRING) cout << where.rvalue.identifier;
            else if (where.keytype == KT_DATE) cout << date_to_string(where.rvalue.value);
        }
    }
    else if (where.type == WT_CROSS) {
        cout << "wt_cross" << " " << "type=" << _key_type_strs[where.keytype] << " ";
        cout << "op=" << _operator_type_strs[where.op];
        cout << " l=" << where.ltable << "(" << where.lid << ")";
        cout << " r=" << where.rtable << "(" << where.rid << ")";
    }
    else if (where.type == WT_INNER) {
        cout << "wt_inner" << " " << "type=" << _key_type_strs[where.keytype] << " ";
        cout << "op=" << _operator_type_strs[where.op];
        cout << " l=" << where.ltable << "(" << where.lid << ")";
        cout << " r=" << where.ltable << "(" << where.rid << ")";
    }
    cout << "]";
}

void KontoTerminal::printWheres(const vector<KontoWhere>& wheres) {
    cout << "[wheres] cnt = " << wheres.size() << endl;
    for (const auto& where: wheres) {cout << TABS[1]; printWhere(where); cout << endl;}
}

void KontoTerminal::printQRes(const KontoQRes& qres) {
    cout << "[qres] count = " << qres.size() << endl;
    int n = qres.size();
    for (int i=0;i<n;i++) 
        cout << TABS[1] << "[" << i << "] @ (" << qres.get(i).page << "," << qres.get(i).id << ")" << endl;
}

TokenExpectation KontoTerminal::valueTypeToExpectation(KontoKeyType type) {
    switch (type) {
        case KT_INT: return TE_INT_VALUE;
        case KT_FLOAT: return TE_FLOAT_VALUE;
        case KT_STRING: return TE_STRING_VALUE;
        case KT_DATE: return TE_DATE_VALUE;
        default: assert(false); return TE_NONE;
    }
}

ProcessStatementResult KontoTerminal::processUpdate(string tbname) {
    if (currentDatabase == "") {PT(1, "Error: Not using a database!");return PSR_ERR;}
    if (!hasTable(tbname)) {PT(1,"Error: No such table!"); return PSR_ERR;}
    vector<uint> kids; kids.clear();
    vector<Token> values; values.clear();
    Token cur = lexer.nextToken(), peek;
    ASSERTERR(cur, TK_SET, "update: Expect keyword SET.");
    KontoTableFile* handle; 
    KontoTableFile::loadFile(currentDatabase + "/" + tbname, &handle);
    while (true) {
        cur = lexer.nextToken(TE_IDENTIFIER);
        ASSERTERR_CLOSE(cur, TK_IDENTIFIER, "update set: Expect identifier.");
        peek = lexer.peek();
        if (peek.tokenKind == TK_DOT) {
            if (cur.identifier != tbname) {handle->close(); return err("update set: column table name is not " + tbname);}
            lexer.nextToken(); cur = lexer.nextToken(TE_IDENTIFIER);
            ASSERTERR_CLOSE(cur, TK_IDENTIFIER, "update set: Expect identifier after dot.");
        }
        uint kid;
        KontoResult res = handle->getKeyIndex(cur.identifier.c_str(), kid);
        if (res != KR_OK) {handle->close(); return err("update set: no such column called " + cur.identifier);}
        kids.push_back(kid);
        cur = lexer.nextToken();
        ASSERTERR_CLOSE(cur, TK_EQUAL, "update set: Expect equal symbol.");
        cur = lexer.nextToken(valueTypeToExpectation(handle->keys[kid].type));
        switch (handle->keys[kid].type) {
            case KT_INT: ASSERTERR_CLOSE(cur, TK_INT_VALUE, "update set: Expect int value."); break;
            case KT_FLOAT: ASSERTERR_CLOSE(cur, TK_FLOAT_VALUE, "update set: Expect float value."); break;
            case KT_STRING: ASSERTERR_CLOSE(cur, TK_STRING_VALUE, "update set: Expect string value."); break;
            case KT_DATE: {
                ASSERTERR_CLOSE(cur, TK_STRING_VALUE, "update set: Expect string value."); 
                cur.tokenKind = TK_DATE_VALUE;
                Date parsed; if (!parse_date(cur.identifier, parsed)) {handle->close(); return err("update set: not valid date string.");}
                cur.value = parsed;
                break;
            }
            default: assert(false); break;
        }
        values.push_back(cur);
        cur = lexer.nextToken();
        if (cur.tokenKind == TK_WHERE) break;
        ASSERTERR_CLOSE(cur, TK_COMMA, "update set: Expect comma.");
    }
    handle->close();
    vector<KontoWhere> wheres; 
    ProcessStatementResult psr = processWheres(tbname, wheres);
    if (psr != PSR_OK) return psr;
    KontoQRes qres;
    //cout << "query wheres" << endl;
    queryWheres(wheres, qres);
    //cout << "updating " << endl;
    KontoTableFile::loadFile(currentDatabase + "/" + tbname, &handle);
    int nQuery = qres.size();
    int nSet = values.size();
    char buffer[handle->getRecordSize()];
    for (int i=0;i<nQuery;i++) {
        const auto& pos = qres.get(i);
        handle->deleteIndex(pos);
        KontoResult res;
        for (int j=0;j<nSet;j++) {
            //cout << "edit" << endl;
            switch (handle->keys[kids[j]].type) {
                case KT_INT: 
                    handle->getDataCopied(pos, buffer);
                    handle->setEntryInt(buffer, kids[j], values[j].value);
                    res = handle->checkLegal(buffer, kids[j]);
                    if (res==KR_OK) handle->editEntryInt(pos, kids[j], values[j].value);
                    break;
                case KT_FLOAT:
                    handle->getDataCopied(pos, buffer);
                    handle->setEntryFloat(buffer, kids[j], values[j].doubleValue);
                    res = handle->checkLegal(buffer, kids[j]);
                    if (res==KR_OK) handle->editEntryFloat(pos, kids[j], values[j].doubleValue);
                    break;
                case KT_STRING: 
                    handle->getDataCopied(pos, buffer);
                    handle->setEntryString(buffer, kids[j], values[j].identifier.c_str());
                    res = handle->checkLegal(buffer, kids[j]);
                    if (res==KR_OK) handle->editEntryString(pos, kids[j], values[j].identifier.c_str());
                    break;
                case KT_DATE: 
                    handle->getDataCopied(pos, buffer);
                    handle->setEntryDate(buffer, kids[j], values[j].value);
                    res = handle->checkLegal(buffer, kids[j]);
                    if (res==KR_OK) handle->editEntryDate(pos, kids[j], values[j].value);
                    break;
            }
            //cout << "edit fin" << endl;
            if (res == KR_REPETITION) {
                handle->close(); 
                return err("Error: Primary key unique constraint failed.");
            } else if (res == KR_FOREIGN_KEY_FAIL) {
                handle->close();
                return err("Error: Foreign key check failed.");
            }
        }
        handle->insertIndex(pos);
    }
    handle->close();
    return PSR_OK;
}

KontoTerminal* KontoTerminal::instancePtr = nullptr;

KontoTerminal* KontoTerminal::getInstance(){
    if (instancePtr == nullptr) return instancePtr = new KontoTerminal();
    else return instancePtr;
}

ProcessStatementResult KontoTerminal::processAlterAdd(string table){
    Token cur = lexer.nextToken(), peek;
    if (cur.tokenKind == TK_PRIMARY) {
        cur = lexer.nextToken();
        ASSERTERR(cur, TK_KEY, "alter table add primary: Expect keyword key.");
        cur = lexer.nextToken();
        ASSERTERR(cur, TK_LPAREN, "alter table - primary: Expect LParen."); 
        vector<string> primaries; primaries.clear();
        while (true) {
            cur = lexer.nextToken(TE_IDENTIFIER);
            ASSERTERR(cur, TK_IDENTIFIER, "alter table - primary: Expect identifier.");
            primaries.push_back(cur.identifier);
            cur = lexer.peek(); 
            if (cur.tokenKind == TK_COMMA) lexer.nextToken();
            else if (cur.tokenKind == TK_RPAREN) break;
            else return err("alter table - primary: Expect comma or rparen.");
        }
        cur = lexer.nextToken();
        ASSERTERR(cur, TK_RPAREN, "alter table - primary: Expect RParen.");
        alterAddPrimaryKey(table, primaries);
        return PSR_OK;
    } else if (cur.tokenKind == TK_CONSTRAINT) {
        string kname = "";
        if (lexer.peek().tokenKind != TK_PRIMARY) {
            cur = lexer.nextToken(TE_IDENTIFIER);
            ASSERTERR(cur, TK_IDENTIFIER, "alter table add constraint: Expect identifier.");
            kname = cur.identifier;
            cur = lexer.nextToken();
        }
        if (cur.tokenKind == TK_FOREIGN) {
            //cout << "xxx foreign xx" << endl;
            vector<string> foreigns; foreigns.clear();
            string foreignTable;
            vector<string> foreignName; foreignName.clear();
            ASSERTERR(cur, TK_FOREIGN, "alter table add constraint: Expect keyword foreign.");
            cur = lexer.nextToken();
            ASSERTERR(cur, TK_KEY, "alter table add constraint: Expect keyword key.");
            cur = lexer.nextToken();
            ASSERTERR(cur, TK_LPAREN, "alter table add constraint - foreign: Expect LParen."); 
            int cnt = 0;
            while (true) {
                cur = lexer.nextToken(TE_IDENTIFIER);
                ASSERTERR(cur, TK_IDENTIFIER, "alter table add constraint - foreign: Expect identifier.");
                foreigns.push_back(cur.identifier); cnt++;
                cur = lexer.peek(); 
                if (cur.tokenKind == TK_COMMA) lexer.nextToken();
                else if (cur.tokenKind == TK_RPAREN) break;
                else return err("alter table add constraint - foreign: Expect comma or rparen.");
            }
            cur = lexer.nextToken();
            ASSERTERR(cur, TK_RPAREN, "alter table add constraint - foreign: Expect RParen.");
            cur = lexer.nextToken();
            ASSERTERR(cur, TK_REFERENCES, "alter table add constraint - foreign: Expect keyword REFERENCES");
            cur = lexer.nextToken(TE_IDENTIFIER);
            ASSERTERR(cur, TK_IDENTIFIER, "alter table add constraint - references: Expect identifier");
            foreignTable = cur.identifier;
            cur = lexer.nextToken();
            ASSERTERR(cur, TK_LPAREN, "alter table add constraint - references: Expect LParen."); 
            int ncnt = 0;
            while (true) {
                cur = lexer.nextToken(TE_IDENTIFIER);
                ASSERTERR(cur, TK_IDENTIFIER, "alter table add constraint - references: Expect identifier.");
                foreignName.push_back(cur.identifier); ncnt++;
                cur = lexer.peek(); 
                if (cur.tokenKind == TK_COMMA) lexer.nextToken();
                else if (cur.tokenKind == TK_RPAREN) break;
                else return err("alter table add constraint - references: Expect comma or rparen.");
            }
            lexer.nextToken();
            if (ncnt!=cnt) return err("alter table add constraint - foreign: Reference count does not match.");
            alterAddForeignKey(table, kname, foreigns, foreignTable, foreignName);
            return PSR_OK;
        } else if (cur.tokenKind == TK_PRIMARY) {
            vector<string> cols; cols.clear();
            ASSERTERR(cur, TK_PRIMARY, "alter table add constraint: Expect keyword primary.");
            cur = lexer.nextToken();
            ASSERTERR(cur, TK_KEY, "alter table add constraint: Expect keyword key.");
            cur = lexer.nextToken();
            ASSERTERR(cur, TK_LPAREN, "alter table add constraint - primary: Expect LParen."); 
            int cnt = 0;
            while (true) {
                cur = lexer.nextToken(TE_IDENTIFIER);
                ASSERTERR(cur, TK_IDENTIFIER, "alter table add constraint - primary: Expect identifier.");
                cols.push_back(cur.identifier); cnt++;
                cur = lexer.peek(); 
                if (cur.tokenKind == TK_COMMA) lexer.nextToken();
                else if (cur.tokenKind == TK_RPAREN) break;
                else return err("alter table add constraint - primary: Expect comma or rparen.");
            }
            cur = lexer.nextToken();
            ASSERTERR(cur, TK_RPAREN, "alter table add constraint - primary: Expect RParen.");
            alterAddPrimaryKey(table, cols);
            return PSR_OK;
        }
    } else if (cur.tokenKind == TK_INDEX) {
        cur = lexer.nextToken(TE_IDENTIFIER);
        ASSERTERR(cur, TK_IDENTIFIER, "alter table add index : Expect identifier");
        string idname = cur.identifier;
        cur = lexer.nextToken();
        ASSERTERR(cur, TK_LPAREN, "alter table add index: Expect LParen."); 
        vector<string> cols; cols.clear();
        while (true) {
            cur = lexer.nextToken(TE_IDENTIFIER);
            ASSERTERR(cur, TK_IDENTIFIER, "alter table add index: Expect identifier.");
            cols.push_back(cur.identifier);
            cur = lexer.peek(); 
            if (cur.tokenKind == TK_COMMA) lexer.nextToken();
            else if (cur.tokenKind == TK_RPAREN) break;
            else return err("alter table add index: Expect comma or rparen.");
        }
        cur = lexer.nextToken();
        ASSERTERR(cur, TK_RPAREN, "alter table add index: Expect RParen.");
        createIndex(idname, table, cols);
        return PSR_OK;
    } else if (cur.tokenKind == TK_IDENTIFIER) {
        KontoCDef def("", TK_INT, 4); def.name = cur.identifier;
        cur = lexer.nextToken();
        if (cur.tokenKind == TK_INT) {
            def.type = KT_INT; def.size = 4;
            if (lexer.peek().tokenKind == TK_LPAREN) {
                cur = lexer.nextToken(); ASSERTERR(cur, TK_LPAREN, "alter table add col - int: Expect LParen.");
                cur = lexer.nextToken(); ASSERTERR(cur, TK_INT_VALUE, "alter table add col - int: Expect int value.");
                cur = lexer.nextToken(); ASSERTERR(cur, TK_RPAREN, "alter table add col - int: Expect RParen.");
            }
            def.defaultValue = new char[4];
            *(int*)(def.defaultValue) = DEFAULT_INT_VALUE;
        } else if (cur.tokenKind == TK_FLOAT) {
            def.type = KT_FLOAT; def.size = 8;
            def.defaultValue = new char[8];
            *(double*)(def.defaultValue) = DEFAULT_FLOAT_VALUE;
        } else if (cur.tokenKind == TK_VARCHAR) {
            def.type = KT_STRING; 
            cur = lexer.nextToken(); ASSERTERR(cur, TK_LPAREN, "alter table add col - varchar: Expect LParen.");
            cur = lexer.nextToken(); ASSERTERR(cur, TK_INT_VALUE, "alter table add col - varchar: Expect int value.");
            def.size = cur.value + 1;
            cur = lexer.nextToken(); ASSERTERR(cur, TK_RPAREN, "alter table add col - varchar: Expect RParen.");
            def.defaultValue = new char[def.size];
            memset(def.defaultValue, 0, def.size);
        } else if (cur.tokenKind == TK_DATE) {
            def.type = KT_DATE; def.size = 4;
            def.defaultValue = new char[4];
            *(Date*)(def.defaultValue) = DEFAULT_DATE_VALUE;
        } else return err("alter table add col: expect a type definition.");
        peek = lexer.peek();
        if (peek.tokenKind == TK_NOT) {
            lexer.nextToken(); cur = lexer.nextToken();
            ASSERTERR(cur, TK_NULL, "alter table add col - not: Expect keyword NULL");
            def.nullable = false;
        }
        peek = lexer.peek();
        if (peek.tokenKind == TK_DEFAULT) {
            lexer.nextToken(); cur = lexer.nextToken(valueTypeToExpectation(def.type));
            if (def.type == KT_INT) {
                ASSERTERR(cur, TK_INT_VALUE, "alter table add col - default: Expect int value.");
                def.defaultValue = new char[4]; 
                *(int*)(def.defaultValue) = cur.value;
            } else if (def.type == KT_FLOAT) {
                ASSERTERR(cur, TK_FLOAT_VALUE, "alter table add col - default: Expect double value.");
                def.defaultValue = new char[8];
                *(double*)(def.defaultValue) = cur.doubleValue;
            } else if (def.type == KT_STRING) {
                ASSERTERR(cur, TK_STRING_VALUE, "alter table add col - default: Expect string value.");
                def.defaultValue = new char[def.size];
                strcpy(def.defaultValue, cur.identifier.c_str());
            } else if (def.type == KT_DATE) {
                ASSERTERR(cur, TK_STRING_VALUE, "alter table add col - default: Expect date string.");
                def.defaultValue = new char[4];
                Date parsed; if (!parse_date(cur.identifier, parsed)) return err("value error: Not a valid date.");
                *(Date*)(def.defaultValue) = parsed;
            }
        }
        alterAddColumn(table, def);
        return PSR_OK;
    } else {
        return err("alter table add: Expect PRIMARY or CONSTRAINT or INDEX or identifier.");
    }
    return PSR_OK;
}

ProcessStatementResult KontoTerminal::processAlterDrop(string table) {
    Token cur = lexer.nextToken();
    if (cur.tokenKind == TK_PRIMARY) {
        cur = lexer.nextToken();
        ASSERTERR(cur, TK_KEY, "alter table drop primary: Expect keyword key.");
        alterDropPrimaryKey(table);
        return PSR_OK;
    } else if (cur.tokenKind == TK_FOREIGN) {
        cur = lexer.nextToken();
        ASSERTERR(cur, TK_KEY, "alter table drop foreign: Expect keyword key.");
        cur = lexer.nextToken(TE_IDENTIFIER);
        ASSERTERR(cur, TK_IDENTIFIER, "alter table drop foreign: Expect identifier.");
        alterDropForeignKey(table, cur.identifier);
        return PSR_OK;
    } else if (cur.tokenKind == TK_INDEX) {
        cur = lexer.nextToken(TE_IDENTIFIER);
        ASSERTERR(cur, TK_IDENTIFIER, "alter table drop index: Expect identifier.");
        string idname = cur.identifier;
        dropIndex(idname);
    } else if (cur.tokenKind == TK_IDENTIFIER) {
        alterDropColumn(table, cur.identifier);
        return PSR_OK;
    } else {
        return err("alter table drop: Expect PRIMARY or FOREIGN or identifier.");
    }
    return PSR_OK;
}

ProcessStatementResult KontoTerminal::processCreate(){
    Token peek = lexer.peek(), cur;
    if (peek.tokenKind == TK_DATABASE) {
        lexer.nextToken(); cur = lexer.nextToken(TE_IDENTIFIER);
        ASSERTERR(cur, TK_IDENTIFIER, "create database: Expect identifier.");
        createDatabase(cur.identifier);
        return PSR_OK;
    } else if (peek.tokenKind == TK_INDEX) {
        if (currentDatabase == "") return err("create table: Not using a database!");
        lexer.nextToken(); cur = lexer.nextToken(TE_IDENTIFIER);
        ASSERTERR(cur, TK_IDENTIFIER, "create index: Expect identifier.");
        string idname = cur.identifier;
        cur = lexer.nextToken();
        ASSERTERR(cur, TK_ON, "create index: Expect keyword on.");
        cur = lexer.nextToken(TE_IDENTIFIER);
        string table = cur.identifier;
        cur = lexer.nextToken();
        ASSERTERR(cur, TK_LPAREN, "create index: Expect LParen."); 
        vector<string> cols; cols.clear();
        while (true) {
            cur = lexer.nextToken(TE_IDENTIFIER);
            ASSERTERR(cur, TK_IDENTIFIER, "create index: Expect identifier.");
            cols.push_back(cur.identifier);
            cur = lexer.peek(); 
            if (cur.tokenKind == TK_COMMA) lexer.nextToken();
            else if (cur.tokenKind == TK_RPAREN) break;
            else return err("create index: Expect comma or rparen.");
        }
        cur = lexer.nextToken();
        ASSERTERR(cur, TK_RPAREN, "alter table add index: Expect RParen.");
        createIndex(idname, table, cols);
        return PSR_OK;
    } else if (peek.tokenKind == TK_TABLE) {
        //cout << "create table" << endl;
        if (currentDatabase == "") return err("create table: Not using a database!");
        lexer.nextToken(); cur = lexer.nextToken(TE_IDENTIFIER);
        ASSERTERR(cur, TK_IDENTIFIER, "create table: Expect identifier.");
        string name = cur.identifier;
        cur = lexer.nextToken(); 
        ASSERTERR(cur, TK_LPAREN, "create table: Expect LParen.");
        // analyse keylist
        vector<KontoCDef> defs;
        vector<string> primaries; primaries.clear();
        vector<vector<string>> foreigns; foreigns.clear();
        vector<string> foreignTable; foreignTable.clear();
        vector<vector<string>> foreignName; foreignName.clear();
        while (true) {
            peek = lexer.peek();
            if (peek.tokenKind == TK_RPAREN) break;
            if (peek.tokenKind == TK_PRIMARY) {
                lexer.nextToken(); cur = lexer.nextToken();
                ASSERTERR(cur, TK_KEY, "create table - primary: Expect keyword KEY");
                cur = lexer.nextToken();
                ASSERTERR(cur, TK_LPAREN, "create table - primary: Expect LParen."); 
                while (true) {
                    cur = lexer.nextToken(TE_IDENTIFIER);
                    ASSERTERR(cur, TK_IDENTIFIER, "create table - primary: Expect identifier.");
                    primaries.push_back(cur.identifier);
                    cur = lexer.peek(); 
                    if (cur.tokenKind == TK_COMMA) lexer.nextToken();
                    else if (cur.tokenKind == TK_RPAREN) break;
                    else return err("create table - primary: Expect comma or rparen.");
                }
                cur = lexer.nextToken();
                ASSERTERR(cur, TK_RPAREN, "create table - primary: Expect RParen.");
            } else if (peek.tokenKind == TK_FOREIGN) {
                vector<string> cols; cols.clear();
                vector<string> refs; refs.clear();
                lexer.nextToken(); cur = lexer.nextToken();
                ASSERTERR(cur, TK_KEY, "create table - foreign: Expect keyword KEY");
                cur = lexer.nextToken();
                ASSERTERR(cur, TK_LPAREN, "create table - foreign: Expect LParen."); 
                int cnt = 0;
                while (true) {
                    cur = lexer.nextToken(TE_IDENTIFIER);
                    ASSERTERR(cur, TK_IDENTIFIER, "create table - foreign: Expect identifier.");
                    cols.push_back(cur.identifier); cnt++;
                    cur = lexer.peek(); 
                    if (cur.tokenKind == TK_COMMA) lexer.nextToken();
                    else if (cur.tokenKind == TK_RPAREN) break;
                    else return err("create table - foreign: Expect comma or rparen.");
                }
                cur = lexer.nextToken();
                ASSERTERR(cur, TK_RPAREN, "create table - foreign: Expect RParen.");
                cur = lexer.nextToken();
                ASSERTERR(cur, TK_REFERENCES, "create table - foreign: Expect keyword REFERENCES");
                cur = lexer.nextToken(TE_IDENTIFIER);
                ASSERTERR(cur, TK_IDENTIFIER, "create table - references: Expect identifier");
                foreignTable.push_back(cur.identifier);
                cur = lexer.nextToken();
                ASSERTERR(cur, TK_LPAREN, "create table - references: Expect LParen."); 
                int ncnt = 0;
                while (true) {
                    cur = lexer.nextToken(TE_IDENTIFIER);
                    ASSERTERR(cur, TK_IDENTIFIER, "create table - references: Expect identifier.");
                    refs.push_back(cur.identifier); ncnt++;
                    cur = lexer.peek(); 
                    if (cur.tokenKind == TK_COMMA) lexer.nextToken();
                    else if (cur.tokenKind == TK_RPAREN) break;
                    else return err("create table - references: Expect comma or rparen.");
                }
                lexer.nextToken();
                if (ncnt!=cnt) return err("create table - foreign: Reference count does not match.");
                foreigns.push_back(cols);
                foreignName.push_back(refs);
            } else if (lexer.toExpected(peek, TE_IDENTIFIER).tokenKind == TK_IDENTIFIER) {
                lexer.nextToken(); 
                KontoCDef def("", TK_INT, 4); def.name = peek.identifier;
                cur = lexer.nextToken();
                if (cur.tokenKind == TK_INT) {
                    def.type = KT_INT; def.size = 4;
                    if (lexer.peek().tokenKind == TK_LPAREN) {
                        cur = lexer.nextToken(); ASSERTERR(cur, TK_LPAREN, "create table - int: Expect LParen.");
                        cur = lexer.nextToken(); ASSERTERR(cur, TK_INT_VALUE, "create table - int: Expect int value.");
                        cur = lexer.nextToken(); ASSERTERR(cur, TK_RPAREN, "create table - int: Expect RParen.");
                    }
                    def.defaultValue = new char[4];
                    *(int*)(def.defaultValue) = DEFAULT_INT_VALUE;
                } else if (cur.tokenKind == TK_FLOAT) {
                    def.type = KT_FLOAT; def.size = 8;
                    def.defaultValue = new char[8];
                    *(double*)(def.defaultValue) = DEFAULT_FLOAT_VALUE;
                } else if (cur.tokenKind == TK_VARCHAR) {
                    def.type = KT_STRING; 
                    cur = lexer.nextToken(); ASSERTERR(cur, TK_LPAREN, "create table - varchar: Expect LParen.");
                    cur = lexer.nextToken(); ASSERTERR(cur, TK_INT_VALUE, "create table - varchar: Expect int value.");
                    def.size = cur.value + 1;
                    cur = lexer.nextToken(); ASSERTERR(cur, TK_RPAREN, "create table - varchar: Expect RParen.");
                    def.defaultValue = new char[def.size];
                    memset(def.defaultValue, 0, def.size);
                } else if (cur.tokenKind == TK_DATE) {
                    def.type = KT_DATE; def.size = 4;
                    def.defaultValue = new char[4];
                    *(Date*)(def.defaultValue) = DEFAULT_DATE_VALUE;
                } else return err("create table: expect a type definition.");
                peek = lexer.peek();
                if (peek.tokenKind == TK_NOT) {
                    lexer.nextToken(); cur = lexer.nextToken();
                    ASSERTERR(cur, TK_NULL, "create table - not: Expect keyword NULL");
                    def.nullable = false;
                }
                peek = lexer.peek();
                if (peek.tokenKind == TK_DEFAULT) {
                    lexer.nextToken(); cur = lexer.nextToken(valueTypeToExpectation(def.type));
                    if (def.type == KT_INT) {
                        ASSERTERR(cur, TK_INT_VALUE, "create table - default: Expect int value.");
                        def.defaultValue = new char[4]; 
                        *(int*)(def.defaultValue) = cur.value;
                    } else if (def.type == KT_FLOAT) {
                        ASSERTERR(cur, TK_FLOAT_VALUE, "create table - default: Expect double value.");
                        def.defaultValue = new char[8];
                        *(double*)(def.defaultValue) = cur.doubleValue;
                    } else if (def.type == KT_STRING) {
                        ASSERTERR(cur, TK_STRING_VALUE, "create table - default: Expect string value.");
                        def.defaultValue = new char[def.size];
                        strcpy(def.defaultValue, cur.identifier.c_str());
                    } else if (def.type == KT_DATE) {
                        ASSERTERR(cur, TK_STRING_VALUE, "create table - default: Expect date string.");
                        def.defaultValue = new char[4];
                        Date parsed; if (!parse_date(cur.identifier, parsed)) return err("value error: Not a valid date.");
                        *(Date*)(def.defaultValue) = parsed;
                    }
                }
                defs.push_back(def);
            } 
            peek = lexer.peek();
            if (peek.tokenKind == TK_COMMA) lexer.nextToken();
            else if (peek.tokenKind == TK_RPAREN) break;
            else return err("create table: Expect comma or rparen."); 
        }
        // finish analyse keylist
        cur = lexer.nextToken();
        ASSERTERR(cur, TK_RPAREN, "create table: Expect RParen.");
        createTable(name, defs);
        alterAddPrimaryKey(name, primaries);
        //cout << "foreigns = " << foreigns.size() << endl;
        for (int i=0;i<foreigns.size();i++)
            alterAddForeignKey(name, "", foreigns[i], foreignTable[i], foreignName[i]);
        return PSR_OK;
    } else {
        return err("create: Expect keyword DATABASE or TABLE.");
    }
}

ProcessStatementResult KontoTerminal::processAlterChange(string table) {
    Token cur = lexer.nextToken(TE_IDENTIFIER), peek; 
    ASSERTERR(cur, TK_IDENTIFIER, "alter table change: Expect identifier");
    string old = cur.identifier;
    KontoCDef def("", TK_INT, 4); def.name = cur.identifier;
    cur = lexer.nextToken();
    if (cur.tokenKind == TK_INT) {
        def.type = KT_INT; def.size = 4;
        if (lexer.peek().tokenKind == TK_LPAREN) {
            cur = lexer.nextToken(); ASSERTERR(cur, TK_LPAREN, "alter table change - int: Expect LParen.");
            cur = lexer.nextToken(); ASSERTERR(cur, TK_INT_VALUE, "alter table change - int: Expect int value.");
            cur = lexer.nextToken(); ASSERTERR(cur, TK_RPAREN, "alter table change - int: Expect RParen.");
        }
        def.defaultValue = new char[4];
        *(int*)(def.defaultValue) = DEFAULT_INT_VALUE;
    } else if (cur.tokenKind == TK_FLOAT) {
        def.type = KT_FLOAT; def.size = 8;
        def.defaultValue = new char[8];
        *(double*)(def.defaultValue) = DEFAULT_FLOAT_VALUE;
    } else if (cur.tokenKind == TK_VARCHAR) {
        def.type = KT_STRING; 
        cur = lexer.nextToken(); ASSERTERR(cur, TK_LPAREN, "alter table change - varchar: Expect LParen.");
        cur = lexer.nextToken(); ASSERTERR(cur, TK_INT_VALUE, "alter table change - varchar: Expect int value.");
        def.size = cur.value + 1;
        cur = lexer.nextToken(); ASSERTERR(cur, TK_RPAREN, "alter table change - varchar: Expect RParen.");
        def.defaultValue = new char[def.size];
        memset(def.defaultValue, 0, def.size);
    } else if (cur.tokenKind == TK_DATE) {
        def.type = KT_DATE; def.size = 4;
        def.defaultValue = new char[4];
        *(Date*)(def.defaultValue) = DEFAULT_DATE_VALUE;
    } else {
        return err("alter table change: expect a type definition.");
    }
    peek = lexer.peek();
    if (peek.tokenKind == TK_NOT) {
        lexer.nextToken(); cur = lexer.nextToken();
        ASSERTERR(cur, TK_NULL, "alter table change - not: Expect keyword NULL");
        def.nullable = false;
    }
    peek = lexer.peek();
    if (peek.tokenKind == TK_DEFAULT) {
        lexer.nextToken(); cur = lexer.nextToken(valueTypeToExpectation(def.type));
        if (def.type == KT_INT) {
            ASSERTERR(cur, TK_INT_VALUE, "alter table change - default: Expect int value.");
            def.defaultValue = new char[4]; 
            *(int*)(def.defaultValue) = cur.value;
        } else if (def.type == KT_FLOAT) {
            ASSERTERR(cur, TK_FLOAT_VALUE, "alter table change - default: Expect double value.");
            def.defaultValue = new char[8];
            *(double*)(def.defaultValue) = cur.doubleValue;
        } else if (def.type == KT_STRING) {
            ASSERTERR(cur, TK_STRING_VALUE, "alter table change - default: Expect string value.");
            def.defaultValue = new char[def.size];
            strcpy(def.defaultValue, cur.identifier.c_str());
        } else if (def.type == KT_DATE) {
            ASSERTERR(cur, TK_STRING_VALUE, "alter table add col - default: Expect date string.");
            def.defaultValue = new char[4];
            Date parsed; if (!parse_date(cur.identifier, parsed)) return err("value error: Not a valid date.");
            *(Date*)(def.defaultValue) = parsed;
        }
    }
    alterChangeColumn(table, old, def);
    return PSR_OK;
}

ProcessStatementResult KontoTerminal::processAlterRename(string table) {
    Token cur = lexer.nextToken();
    if (cur.tokenKind == TK_TO) {
        Token cur = lexer.nextToken(TE_IDENTIFIER);
        ASSERTERR(cur, TK_IDENTIFIER, "alter rename: Expect identifier.");
        string newname = cur.identifier;
        alterRenameTable(table, newname);
        return PSR_OK;
    } else {
        ASSERTERR(cur, TK_IDENTIFIER, "alter rename: Expect identifier.");
        string origname = cur.identifier;
        cur = lexer.nextToken();
        ASSERTERR(cur, TK_TO, "alter rename: Expect keyword To.");
        cur = lexer.nextToken(TE_IDENTIFIER);
        ASSERTERR(cur, TK_IDENTIFIER, "alter rename: Expect identifier.");
        string newname = cur.identifier;
        alterRenameColumn(table, origname, newname);
        return PSR_OK;
    }
}

void KontoTerminal::alterRenameColumn(string table, string origname, string newname) {
    if (currentDatabase == "") {PT(1, "Error: Not using a database!");return;}
    if (!hasTable(table)) {PT(1, "Error: No such table!"); return;}
    KontoTableFile* handle; 
    KontoTableFile::loadFile(currentDatabase + "/" + table, &handle);
    auto res = handle->alterRenameColumn(origname, newname);
    if (res == KR_NO_SUCH_COLUMN) {
        err("alter rename: No such column!");
        handle->close();
        return;
    }
    dropTableIndices(table); saveIndices();
    handle->close();
}

void KontoTerminal::alterRenameTable(string table, string newname) {
    if (currentDatabase == "") {PT(1, "Error: Not using a database!");return;}
    if (hasTable(newname)) {err("alter rename: already got a table named " + newname + "!"); return;}
    if (!hasTable(table)) {err("alter rename: no such table called " + table + "!");}
    for (int i=0;i<tables.size();i++) if (tables[i]==table) tables[i]=newname;
    for (auto& id: indices) if (id.table == table) id.table = newname;
    KontoTableFile* handle;
    KontoTableFile::loadFile(currentDatabase + "/" + table, &handle);
    handle->alterRename(currentDatabase + "/" + newname);
    save_lines(currentDatabase, get_filename(TABLES_FILE), tables);
    saveIndices();
}

void KontoTerminal::debugEcho(string str) {
    cout << "====================" << str << "====================" << endl;
}