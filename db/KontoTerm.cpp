#include "KontoTerm.h"
#include <fstream>

using std::to_string;

#define ASSERTERR(token, type, message) if (token.tokenKind != type) return err(message)

string bool_to_string(bool b){
    return b ? "Yes" : "No";
}

string type_to_string(KontoKeyType type, int size) {
    switch (type) {
        case KT_INT: return "INT";
        case KT_FLOAT: return "FLOAT";
        case KT_STRING: return "VCHAR(" + to_string(size - 1) + ")";
        default: return "UNKNOWN";
    }
}

string value_to_string(char* value, KontoKeyType type) {
    if (value == nullptr) return "";
    switch (type) {
        case KT_INT: 
            if (*(int*)value == DEFAULT_INT_VALUE) return "NULL";
            return to_string(*(int*)value);
        case KT_FLOAT: 
            if (*(double*)value == DEFAULT_FLOAT_VALUE) return "NULL";
            return to_string(*(double*)value);
        case KT_STRING: return string(value);
        default: return "BAD";
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

        case TK_ALTER: {
            cur = lexer.nextToken();
            ASSERTERR(cur, TK_TABLE, "alter: Expect keyword TABLE.");
            cur = lexer.nextToken();
            ASSERTERR(cur, TK_IDENTIFIER, "alter table: Expect identifier.");
            string table = cur.identifier;
            cur = lexer.nextToken();
            if (cur.tokenKind == TK_ADD) {
                cur = lexer.nextToken();
                if (cur.tokenKind == TK_PRIMARY) {
                    cur = lexer.nextToken();
                    ASSERTERR(cur, TK_KEY, "alter table add primary: Expect keyword key.");
                    cur = lexer.nextToken();
                    ASSERTERR(cur, TK_LPAREN, "create table - primary: Expect LParen."); 
                    vector<string> primaries; primaries.clear();
                    while (true) {
                        cur = lexer.nextToken();
                        ASSERTERR(cur, TK_IDENTIFIER, "create table - primary: Expect identifier.");
                        primaries.push_back(cur.identifier);
                        cur = lexer.peek(); 
                        if (cur.tokenKind == TK_COMMA) lexer.nextToken();
                        else if (cur.tokenKind == TK_RPAREN) break;
                        else return err("create table - primary: Expect comma or rparen.");
                    }
                    cur = lexer.nextToken();
                    ASSERTERR(cur, TK_RPAREN, "create table - primary: Expect RParen.");
                    alterAddPrimaryKey(table, primaries);
                    return PSR_OK;
                } else if (cur.tokenKind == TK_CONSTRAINT) {
                    cur = lexer.nextToken();
                    ASSERTERR(cur, TK_IDENTIFIER, "alter table add constraint: Expect identifier.");
                    string fkname = cur.identifier;
                    vector<string> foreigns; foreigns.clear();
                    string foreignTable;
                    vector<string> foreignName; foreignName.clear();
                    cur = lexer.nextToken();
                    ASSERTERR(cur, TK_FOREIGN, "alter table add constraint: Expect keyword foreign.");
                    cur = lexer.nextToken();
                    ASSERTERR(cur, TK_KEY, "alter table add constraint: Expect keyword key.");
                    cur = lexer.nextToken();
                    ASSERTERR(cur, TK_LPAREN, "alter table add constraint - foreign: Expect LParen."); 
                    int cnt = 0;
                    while (true) {
                        cur = lexer.nextToken();
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
                    cur = lexer.nextToken();
                    ASSERTERR(cur, TK_IDENTIFIER, "alter table add constraint - references: Expect identifier");
                    foreignTable = cur.identifier;
                    cur = lexer.nextToken();
                    ASSERTERR(cur, TK_LPAREN, "alter table add constraint - references: Expect LParen."); 
                    int ncnt = 0;
                    while (true) {
                        cur = lexer.nextToken();
                        ASSERTERR(cur, TK_IDENTIFIER, "alter table add constraint - references: Expect identifier.");
                        foreignName.push_back(cur.identifier); ncnt++;
                        cur = lexer.peek(); 
                        if (cur.tokenKind == TK_COMMA) lexer.nextToken();
                        else if (cur.tokenKind == TK_RPAREN) break;
                        else return err("alter table add constraint - references: Expect comma or rparen.");
                    }
                    if (ncnt!=cnt) return err("alter table add constraint - foreign: Reference count does not match.");
                    alterAddForeignKey(table, fkname, foreigns, foreignTable, foreignName);
                    return PSR_OK;
                } else if (cur.tokenKind == TK_INDEX) {
                    cur = lexer.nextToken(); 
                    ASSERTERR(cur, TK_IDENTIFIER, "alter table add index : Expect identifier");
                    string idname = cur.identifier;
                    cur = lexer.nextToken();
                    ASSERTERR(cur, TK_LPAREN, "alter table add index: Expect LParen."); 
                    vector<string> cols; cols.clear();
                    while (true) {
                        cur = lexer.nextToken();
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
                        cur = lexer.nextToken(); ASSERTERR(cur, TK_LPAREN, "alter table add col - int: Expect LParen.");
                        cur = lexer.nextToken(); ASSERTERR(cur, TK_INT_VALUE, "alter table add col - int: Expect int value.");
                        cur = lexer.nextToken(); ASSERTERR(cur, TK_RPAREN, "alter table add col - int: Expect RParen.");
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
                    }
                    peek = lexer.peek();
                    if (peek.tokenKind == TK_NOT) {
                        lexer.nextToken(); cur = lexer.nextToken();
                        ASSERTERR(cur, TK_NULL, "alter table add col - not: Expect keyword NULL");
                        def.nullable = false;
                    }
                    peek = lexer.peek();
                    if (peek.tokenKind == TK_DEFAULT) {
                        lexer.nextToken(); cur = lexer.nextToken();
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
                        }
                    }
                    alterAddColumn(table, def);
                    return PSR_OK;
                } else {
                    return err("alter table add: Expect PRIMARY or CONSTRAINT or INDEX or identifier.");
                }
            } else if (cur.tokenKind == TK_DROP) {
                cur = lexer.nextToken();
                if (cur.tokenKind == TK_PRIMARY) {
                    cur = lexer.nextToken();
                    ASSERTERR(cur, TK_KEY, "alter table drop primary: Expect keyword key.");
                    alterDropPrimaryKey(table);
                    return PSR_OK;
                } else if (cur.tokenKind == TK_FOREIGN) {
                    cur = lexer.nextToken();
                    ASSERTERR(cur, TK_KEY, "alter table drop foreign: Expect keyword key.");
                    cur = lexer.nextToken();
                    ASSERTERR(cur, TK_IDENTIFIER, "alter table drop foreign: Expect identifier.");
                    alterDropForeignKey(table, cur.identifier);
                    return PSR_OK;
                } else if (cur.tokenKind == TK_INDEX) {
                    cur = lexer.nextToken();
                    ASSERTERR(cur, TK_IDENTIFIER, "alter table drop index: Expect identifier.");
                    string idname = cur.identifier;
                    dropIndex(idname);
                } else if (cur.tokenKind == TK_IDENTIFIER) {
                    alterDropColumn(table, cur.identifier);
                    return PSR_OK;
                } else {
                    return err("alter table drop: Expect PRIMARY or FOREIGN or identifier.");
                }
            } else if (cur.tokenKind == TK_CHANGE) {
                cur = lexer.nextToken(); 
                ASSERTERR(cur, TK_IDENTIFIER, "alter table change: Expect identifier");
                string old = cur.identifier;
                cur = lexer.nextToken();
                ASSERTERR(cur, TK_IDENTIFIER, "alter table change: Expect identifier");
                KontoCDef def("", TK_INT, 4); def.name = cur.identifier;
                cur = lexer.nextToken();
                if (cur.tokenKind == TK_INT) {
                    def.type = KT_INT; def.size = 4;
                    cur = lexer.nextToken(); ASSERTERR(cur, TK_LPAREN, "alter table change - int: Expect LParen.");
                    cur = lexer.nextToken(); ASSERTERR(cur, TK_INT_VALUE, "alter table change - int: Expect int value.");
                    cur = lexer.nextToken(); ASSERTERR(cur, TK_RPAREN, "alter table change - int: Expect RParen.");
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
                }
                peek = lexer.peek();
                if (peek.tokenKind == TK_NOT) {
                    lexer.nextToken(); cur = lexer.nextToken();
                    ASSERTERR(cur, TK_NULL, "alter table change - not: Expect keyword NULL");
                    def.nullable = false;
                }
                peek = lexer.peek();
                if (peek.tokenKind == TK_DEFAULT) {
                    lexer.nextToken(); cur = lexer.nextToken();
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
                    }
                }
                alterChangeColumn(table, old, def);
                return PSR_OK;
            } else {
                return err("alter table: Expect keyword ADD, DROP or CHANGE.");
            }
        }

        case TK_CREATE: {
            Token peek = lexer.peek();
            if (peek.tokenKind == TK_DATABASE) {
                lexer.nextToken(); cur = lexer.nextToken();
                ASSERTERR(cur, TK_IDENTIFIER, "create database: Expect identifier.");
                createDatabase(cur.identifier);
                return PSR_OK;
            } else if (peek.tokenKind == TK_TABLE) {
                lexer.nextToken(); cur = lexer.nextToken();
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
                    if (peek.tokenKind == TK_IDENTIFIER) {
                        lexer.nextToken(); 
                        KontoCDef def("", TK_INT, 4); def.name = peek.identifier;
                        cur = lexer.nextToken();
                        if (cur.tokenKind == TK_INT) {
                            def.type = KT_INT; def.size = 4;
                            cur = lexer.nextToken(); ASSERTERR(cur, TK_LPAREN, "create table - int: Expect LParen.");
                            cur = lexer.nextToken(); ASSERTERR(cur, TK_INT_VALUE, "create table - int: Expect int value.");
                            cur = lexer.nextToken(); ASSERTERR(cur, TK_RPAREN, "create table - int: Expect RParen.");
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
                        }
                        peek = lexer.peek();
                        if (peek.tokenKind == TK_NOT) {
                            lexer.nextToken(); cur = lexer.nextToken();
                            ASSERTERR(cur, TK_NULL, "create table - not: Expect keyword NULL");
                            def.nullable = false;
                        }
                        peek = lexer.peek();
                        if (peek.tokenKind == TK_DEFAULT) {
                            lexer.nextToken(); cur = lexer.nextToken();
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
                            }
                        }
                        defs.push_back(def);
                    } else if (peek.tokenKind == TK_PRIMARY) {
                        lexer.nextToken(); cur = lexer.nextToken();
                        ASSERTERR(cur, TK_KEY, "create table - primary: Expect keyword KEY");
                        cur = lexer.nextToken();
                        ASSERTERR(cur, TK_LPAREN, "create table - primary: Expect LParen."); 
                        while (true) {
                            cur = lexer.nextToken();
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
                            cur = lexer.nextToken();
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
                        cur = lexer.nextToken();
                        ASSERTERR(cur, TK_IDENTIFIER, "create table - references: Expect identifier");
                        foreignTable.push_back(cur.identifier);
                        cur = lexer.nextToken();
                        ASSERTERR(cur, TK_LPAREN, "create table - references: Expect LParen."); 
                        int ncnt = 0;
                        while (true) {
                            cur = lexer.nextToken();
                            ASSERTERR(cur, TK_IDENTIFIER, "create table - references: Expect identifier.");
                            refs.push_back(cur.identifier); ncnt++;
                            cur = lexer.peek(); 
                            if (cur.tokenKind == TK_COMMA) lexer.nextToken();
                            else if (cur.tokenKind == TK_RPAREN) break;
                            else return err("create table - references: Expect comma or rparen.");
                        }
                        if (ncnt!=cnt) return err("create table - foreign: Reference count does not match.");
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
                for (int i=0;i<foreigns.size();i++)
                    alterAddForeignKey(name, "", foreigns[i], foreignTable[i], foreignName[i]);
                return PSR_OK;
            } else {
                return err("create: Expect keyword DATABASE or TABLE.");
            }
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
                cur = lexer.nextToken();
                ASSERTERR(cur, TK_IDENTIFIER, "debug table: expect identifier.");
                debugTable(cur.identifier);
                return PSR_OK;
            } else if (cur.tokenKind == TK_PRIMARY) {
                cur = lexer.nextToken();
                ASSERTERR(cur, TK_IDENTIFIER, "debug primary: expect identifier.");
                debugPrimary(cur.identifier);
                return PSR_OK;
            }
        }

        case TK_DROP: {
            Token peek = lexer.peek();
            if (peek.tokenKind == TK_DATABASE) {
                lexer.nextToken(); cur = lexer.nextToken();
                ASSERTERR(cur, TK_IDENTIFIER, "drop database: Expect identifier");
                dropDatabase(cur.identifier);
                return PSR_OK;
            } else if (peek.tokenKind == TK_TABLE) {
                lexer.nextToken(); cur = lexer.nextToken();
                ASSERTERR(cur, TK_IDENTIFIER, "drop table: Expect identifier");
                dropTable(cur.identifier);
                return PSR_OK;
            } else {
                return err("drop: Expect keyword DATABASE or TABLE.");
            }
        }

        case TK_INSERT: {
            cur = lexer.nextToken();
            ASSERTERR(cur, TK_INTO, "insert: Expect keyword INTO.");
            cur = lexer.nextToken();
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

        case TK_SHOW: {
            Token peek = lexer.peek();
            if (peek.tokenKind == TK_DATABASE) {
                lexer.nextToken(); cur = lexer.nextToken();
                ASSERTERR(cur, TK_IDENTIFIER, "show database: Expect identifier");
                showDatabase(cur.identifier);
                return PSR_OK;
            } else if (peek.tokenKind == TK_DATABASES) {
                lexer.nextToken(); 
                showDatabases();
                return PSR_OK;
            } else if (peek.tokenKind == TK_TABLE) {
                lexer.nextToken(); cur = lexer.nextToken();
                ASSERTERR(cur, TK_IDENTIFIER, "show table: Expect identifier");
                showTable(cur.identifier);
                return PSR_OK;
            } else {
                return err("show: Expect keyword DATABASE, DATABASES or TABLE.");
            }
        }

        case TK_USE: {
            Token peek = lexer.peek();
            if (peek.tokenKind == TK_DATABASE) {
                lexer.nextToken(); cur = lexer.nextToken();
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

void KontoTerminal::main() {
    
    lexer.setStream(&std::cin);
    while (true) {
        cout << ">>> ";
        ProcessStatementResult psr = processStatement();
        if (psr==PSR_QUIT) break;
        std::cin.clear(); std::cin.ignore(1024, '\n'); lexer.clearBuffer();
    }
}

KontoTerminal::KontoTerminal() : lexer(true) {currentDatabase = "";}

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
    cout << TABS[2] << "|" << SS(10, "NAME") << "|" << SS(10, "TYPE") << "|" <<
        SS(10, "NULL") << "|" <<
        SS(30, "DEFAULT") << "|" << endl;
    for (auto& item: handle->keys) {
        cout << TABS[2] << "|" << SS(10, item.name) << "|"
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
                if (i!=0) cout << ", ";
                cout << handle->keys[cols[i][j]].name; 
            } 
            cout << ") " << "REFERENCES " << foreignTable[i] << "(";
            for (int j=0;j<foreignName[i].size();j++) {
                if (i!=0) cout << ", ";
                cout << foreignName[i][j];
            }
            cout << ")" << endl;
        }
    } else PT(2, "No foreign keys defined.");
    handle->close();
}

void KontoTerminal::alterAddPrimaryKey(string table, const vector<string>& cols) {
    if (currentDatabase == "") {PT(1, "Error: Not using a database!");return;}
    if (!hasTable(table)) {PT(1, "Error: No such table!"); return;}
    KontoTableFile* handle; 
    KontoTableFile::loadFile(currentDatabase + "/" + table, &handle);
    vector<uint> id; id.clear();
    for (auto& item : cols) {
        uint p;
        KontoResult res = handle->getKeyIndex(item.c_str(), p);
        if (res != KR_OK) {
            PT(1, "Error: No such key named " + item + ".");
            return; 
        }
        id.push_back(p);
    }
    handle->alterAddPrimaryKey(id);
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
    handle->alterDropColumn(col);
    dropTableIndices(table); saveIndices();
    handle->close();
}

void KontoTerminal::alterChangeColumn(string table, string original, const KontoCDef& newdef) {
    if (currentDatabase == "") {PT(1, "Error: Not using a database!");return;}
    if (!hasTable(table)) {PT(1, "Error: No such table!"); return;}
    KontoTableFile* handle; 
    KontoTableFile::loadFile(currentDatabase + "/" + table, &handle);
    handle->alterChangeColumn(original, newdef);
    dropTableIndices(table); saveIndices();
    handle->close();
}

void KontoTerminal::alterAddForeignKey(string table, string fkname, 
    const vector<string>& cols, string foreignTable, 
    const vector<string>& foreignNames) 
{
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
    handle->alterDropForeignKey(fkname);
    handle->close();
}

ProcessStatementResult KontoTerminal::processInsert(string tbname) {
    if (currentDatabase == "") {PT(1, "Error: Not using a database!");return PSR_ERR;}
    if (!hasTable(tbname)) return err("Error: No such table!");
    KontoTableFile* handle; 
    KontoTableFile::loadFile(currentDatabase + "/" + tbname, &handle);
    char* buffer = new char[handle->getRecordSize()];
    while (true) {
        Token cur = lexer.nextToken();
        ASSERTERR(cur, TK_LPAREN, "insert values: Expect Lparen.");
        int n = handle->keys.size();
        for (int i=0;i<n;i++) {
            Token cur = lexer.nextToken();
            const auto& key = handle->keys[i];
            switch (key.type) {
                case KT_INT:
                    ASSERTERR(cur, TK_INT_VALUE, "insert - type error: Expect int value.");
                    handle->setEntryInt(buffer, i, cur.value);
                    break;
                case KT_FLOAT:
                    ASSERTERR(cur, TK_FLOAT_VALUE, "insert - type error: Expect float value.");
                    handle->setEntryFloat(buffer, i, cur.doubleValue);
                    break;
                case KT_STRING:
                    ASSERTERR(cur, TK_STRING_VALUE, "insert - type error: Expect string value.");
                    if (cur.identifier.length() > key.size - 1) return err("insert - value error: String too long.");
                    handle->setEntryString(buffer, i, cur.identifier.c_str());
                    break;
                default:
                    return err("insert - table type error: Unidentified type.");
            }
            if (i!=n-1) {
                cur=lexer.nextToken();
                ASSERTERR(cur, TK_COMMA, "insert value: Expect comma.");
            }
        }
        cur = lexer.nextToken();
        ASSERTERR(cur, TK_RPAREN, "insert values: Expect Rparen.");
        handle->insert(buffer);
        if (lexer.peek().tokenKind == TK_SEMICOLON) break;
        cur = lexer.nextToken();
        ASSERTERR(cur, TK_COMMA, "insert values: Expect comma.");
    }
    delete[] buffer;
    handle->close();
    return PSR_OK;
}

void KontoTerminal::loadIndices() {
    if (currentDatabase == "") {PT(1, "Error: Not using a database!");return;}
    indices.clear();
    if (!file_exist(currentDatabase, INDICES_FILE)) return;
    std::ifstream fin(get_filename(currentDatabase + "/" + INDICES_FILE));
    while (!fin.eof()) {
        KontoIndexDesc desc;
        fin >> desc.name; fin >> desc.table;
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
    for (auto& item: cols) {
        uint p;
        KontoResult res = handle->getKeyIndex(item.c_str(), p);
        if (res!=KR_OK) {PT(1, "Error: No such column called " + item); return;}
        colids.push_back(p);
    }
    if (handle->createIndex(colids, nullptr)!=KR_OK) {
        PT(1, "Error: Create index failed.");
        return;
    };
    KontoIndexDesc desc{
        .name = idname,
        .table = table,
        .cols = colids
    }; 
    indices.push_back(desc);
    saveIndices();
    handle->close();
}

void KontoTerminal::dropIndex(string idname) {
    if (currentDatabase == "") {PT(1, "Error: Not using a database!");return;}
    string table;
    int n = indices.size();
    KontoIndexDesc* ptr = nullptr; int position;
    for (int i=0; i<n; i++) if (indices[i].name == idname) {ptr = &indices[i]; position = i; break;}
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
    saveIndices();
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