#include "KontoTerm.h"

#define ASSERTERR(token, type, message) if (token.tokenKind != type) return err(message)

ProcessStatementResult KontoTerminal::err(string message) {
    cout << message << endl;
    return PSR_ERR;
}

ProcessStatementResult KontoTerminal::processStatement() {
    Token cur = lexer.nextToken(), peek = lexer.peek();
    switch (cur.tokenKind) {

        case TK_ALTER: {
            ASSERTERR(peek, TK_TABLE, "alter: Expect keyword TABLE.");
            lexer.nextToken(); cur = lexer.nextToken();
            ASSERTERR(cur, TK_IDENTIFIER, "alter table: Expect identifier.");
            string name = cur.identifier;
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
                    alterAddPrimaryKey(name, primaries);
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
                    alterAddForeignKey(name, fkname, foreigns, foreignTable, foreignName);
                    return PSR_OK;
                } else if (cur.tokenKind == TK_IDENTIFIER) {
                    KontoCDef def("", TK_INT, 4); def.name = cur.identifier;
                    cur = lexer.nextToken(); cur = lexer.nextToken();
                    if (cur.tokenKind == TK_INT) {
                        def.type = KT_INT; def.size = 4;
                        cur = lexer.nextToken(); ASSERTERR(cur, TK_LPAREN, "alter table add col - int: Expect LParen.");
                        cur = lexer.nextToken(); ASSERTERR(cur, TK_INT_VALUE, "alter table add col - int: Expect int value.");
                        cur = lexer.nextToken(); ASSERTERR(cur, TK_RPAREN, "alter table add col - int: Expect RParen.");
                    } else if (cur.tokenKind == TK_FLOAT) {
                        def.type = KT_FLOAT;
                    } else if (cur.tokenKind == TK_VARCHAR) {
                        def.type = KT_STRING; def.size = 8;
                        cur = lexer.nextToken(); ASSERTERR(cur, TK_LPAREN, "alter table add col - varchar: Expect LParen.");
                        cur = lexer.nextToken(); ASSERTERR(cur, TK_INT_VALUE, "alter table add col - varchar: Expect int value.");
                        def.size = cur.value + 1;
                        cur = lexer.nextToken(); ASSERTERR(cur, TK_RPAREN, "alter table add col - varchar: Expect RParen.");
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
                    alterAddColumn(name, def);
                    return PSR_OK;
                } else {
                    return err("alter table add: Expect PRIMARY or CONSTRAINT or identifier.");
                }
            } else if (cur.tokenKind == TK_DROP) {
                cur = lexer.nextToken();
                if (cur.tokenKind == TK_PRIMARY) {
                    cur = lexer.nextToken();
                    ASSERTERR(cur, TK_KEY, "alter table drop primary: Expect keyword key.");
                    alterDropPrimaryKey(name);
                    return PSR_OK;
                } else if (cur.tokenKind == TK_FOREIGN) {
                    cur = lexer.nextToken();
                    ASSERTERR(cur, TK_KEY, "alter table drop foreign: Expect keyword key.");
                    cur = lexer.nextToken();
                    ASSERTERR(cur, TK_IDENTIFIER, "alter table drop foreign: Expect identifier.");
                    alterDropForeignKey(name, cur.identifier);
                    return PSR_OK;
                } else if (cur.tokenKind == TK_IDENTIFIER) {
                    alterDropColumn(name, cur.identifier);
                    return PSR_OK;
                } else {
                    return err("alter table drop: Expect PRIMARY or FOREIGN or identifier.");
                }
            } else if (cur.tokenKind == TK_CHANGE) {
                cur = lexer.nextToken(); 
                ASSERTERR(cur, TK_IDENTIFIER, "alter table change: Expect identifier");
                string old = cur.identifier;
                cur = lexer.nextToken();
                KontoCDef def("", TK_INT, 4); def.name = cur.identifier;
                cur = lexer.nextToken(); cur = lexer.nextToken();
                if (cur.tokenKind == TK_INT) {
                    def.type = KT_INT; def.size = 4;
                    cur = lexer.nextToken(); ASSERTERR(cur, TK_LPAREN, "alter table change - int: Expect LParen.");
                    cur = lexer.nextToken(); ASSERTERR(cur, TK_INT_VALUE, "alter table change - int: Expect int value.");
                    cur = lexer.nextToken(); ASSERTERR(cur, TK_RPAREN, "alter table change - int: Expect RParen.");
                } else if (cur.tokenKind == TK_FLOAT) {
                    def.type = KT_FLOAT;
                } else if (cur.tokenKind == TK_VARCHAR) {
                    def.type = KT_STRING; def.size = 8;
                    cur = lexer.nextToken(); ASSERTERR(cur, TK_LPAREN, "alter table change - varchar: Expect LParen.");
                    cur = lexer.nextToken(); ASSERTERR(cur, TK_INT_VALUE, "alter table change - varchar: Expect int value.");
                    def.size = cur.value + 1;
                    cur = lexer.nextToken(); ASSERTERR(cur, TK_RPAREN, "alter table change - varchar: Expect RParen.");
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
                alterChangeColumn(name, old, def);
                return PSR_OK;
            } else {
                return err("alter table: Expect keyword ADD, DROP or CHANGE.");
            }
        }

        case TK_CREATE: {
            if (peek.tokenKind == TK_DATABASE) {
                lexer.nextToken(); peek = lexer.nextToken();
                ASSERTERR(peek, TK_IDENTIFIER, "create database: Expect identifier.");
                createDatabase(peek.identifier);
                return PSR_OK;
            } else if (peek.tokenKind == TK_TABLE) {
                lexer.nextToken(); cur = lexer.nextToken();
                ASSERTERR(peek, TK_IDENTIFIER, "create table: Expect identifier.");
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
                        cur = lexer.nextToken(); cur = lexer.nextToken();
                        if (cur.tokenKind == TK_INT) {
                            def.type = KT_INT; def.size = 4;
                            cur = lexer.nextToken(); ASSERTERR(cur, TK_LPAREN, "create table - int: Expect LParen.");
                            cur = lexer.nextToken(); ASSERTERR(cur, TK_INT_VALUE, "create table - int: Expect int value.");
                            cur = lexer.nextToken(); ASSERTERR(cur, TK_RPAREN, "create table - int: Expect RParen.");
                        } else if (cur.tokenKind == TK_FLOAT) {
                            def.type = KT_FLOAT;
                        } else if (cur.tokenKind == TK_VARCHAR) {
                            def.type = KT_STRING; def.size = 8;
                            cur = lexer.nextToken(); ASSERTERR(cur, TK_LPAREN, "create table - varchar: Expect LParen.");
                            cur = lexer.nextToken(); ASSERTERR(cur, TK_INT_VALUE, "create table - varchar: Expect int value.");
                            def.size = cur.value + 1;
                            cur = lexer.nextToken(); ASSERTERR(cur, TK_RPAREN, "create table - varchar: Expect RParen.");
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

        case TK_DROP: {
            if (peek.tokenKind == TK_DATABASE) {
                lexer.nextToken(); peek = lexer.nextToken();
                ASSERTERR(peek, TK_IDENTIFIER, "drop database: Expect identifier");
                dropDatabase(peek.identifier);
                return PSR_OK;
            } else if (peek.tokenKind == TK_TABLE) {
                lexer.nextToken(); peek = lexer.nextToken();
                ASSERTERR(peek, TK_IDENTIFIER, "drop table: Expect identifier");
                dropTable(peek.identifier);
                return PSR_OK;
            } else {
                return err("drop: Expect keyword DATABASE or TABLE.");
            }
        }

        case TK_SHOW: {
            if (peek.tokenKind == TK_DATABASE) {
                lexer.nextToken(); peek = lexer.nextToken();
                ASSERTERR(peek, TK_IDENTIFIER, "show database: Expect identifier");
                showDatabase(peek.identifier);
                return PSR_OK;
            } else if (peek.tokenKind == TK_TABLE) {
                lexer.nextToken(); peek = lexer.nextToken();
                ASSERTERR(peek, TK_IDENTIFIER, "show table: Expect identifier");
                showTable(peek.identifier);
                return PSR_OK;
            } else {
                return err("show: Expect keyword DATABASE or TABLE.");
            }
        }

        case TK_USE: {
            if (peek.tokenKind == TK_DATABASE) {
                lexer.nextToken(); peek = lexer.nextToken();
                ASSERTERR(peek, TK_IDENTIFIER, "use database: Expect identifier");
                useDatabase(peek.identifier);
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
        processStatement();
    }
}