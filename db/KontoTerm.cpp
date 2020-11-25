#include "KontoTerm.h"

#define ASSERTERR(token, type, message) if (token.tokenKind != type) return err(message)

ProcessStatementResult KontoTerminal::err(string message) {
    cout << message << endl;
    return PSR_ERR;
}

ProcessStatementResult KontoTerminal::processStatement() {
    Token cur = lexer.nextToken(), peek = lexer.peek();
    switch (cur.tokenKind) {
        case TK_CREATE: {
            if (peek.tokenKind == TK_DATABASE) {
                lexer.nextToken(); peek = lexer.nextToken();
                ASSERTERR(peek, TK_IDENTIFIER, "Expect identifier.");
                createDatabase(peek.identifier);
                return PSR_OK;
            } else if (peek.tokenKind == TK_TABLE) {
                lexer.nextToken(); cur = lexer.nextToken();
                ASSERTERR(peek, TK_IDENTIFIER, "Expect identifier.");
                string name = cur.identifier;
                cur = lexer.nextToken(); 
                ASSERTERR(cur, TK_LPAREN, "Expect LParen.");
                // analyse keylist
                vector<KontoCDef> defs;
                vector<uint> primaries;
                while (true) {
                    peek = lexer.peek();
                    if (peek.tokenKind == TK_RPAREN) break;
                    if (peek.tokenKind == TK_IDENTIFIER) {
                        lexer.nextToken(); 
                        KontoCDef def("", TK_INT, 4); def.name = peek.identifier;
                        cur = lexer.nextToken(); cur = lexer.nextToken();
                        if (cur.tokenKind == TK_INT) {
                            def.type = KT_INT; def.size = 4;
                            cur = lexer.nextToken(); ASSERTERR(cur, TK_LPAREN, "Expect LParen.");
                            cur = lexer.nextToken(); ASSERTERR(cur, TK_INT_VALUE, "Expect int value.");
                            cur = lexer.nextToken(); ASSERTERR(cur, TK_RPAREN, "Expect RParen.");
                        } else if (cur.tokenKind == TK_FLOAT) {
                            def.type = KT_FLOAT;
                        } else if (cur.tokenKind == TK_VARCHAR) {
                            def.type = KT_STRING; def.size = 8;
                            cur = lexer.nextToken(); ASSERTERR(cur, TK_LPAREN, "Expect LParen.");
                            cur = lexer.nextToken(); ASSERTERR(cur, TK_INT_VALUE, "Expect int value.");
                            def.size = cur.value + 1;
                            cur = lexer.nextToken(); ASSERTERR(cur, TK_RPAREN, "Expect RParen.");
                        }
                        peek = lexer.peek();
                        if (peek.tokenKind == TK_NOT) {
                            lexer.nextToken(); cur = lexer.nextToken();
                            ASSERTERR(cur, TK_NULL, "Expect keyword NULL");
                            def.nullable = false;
                        }
                        defs.push_back(def);
                    } else if (peek.tokenKind == TK_PRIMARY) {
                        lexer.nextToken(); cur = lexer.nextToken();
                        ASSERTERR(cur, TK_KEY, "Expect keyword KEY");
                        cur = lexer.nextToken();
                        ASSERTERR(cur, TK_LPAREN, "Expect LParen."); 
                        while (true) {
                            cur = lexer.nextToken();
                            ASSERTERR(cur, TK_IDENTIFIER, "Expect identifier.");
                            bool flag = false;
                            for (int i=0;i<defs.size();i++) {
                                if (defs[i].name == cur.identifier) {
                                    flag = true; primaries.push_back(i); break;
                                }
                            }
                            if (!flag) return err("Bad primary key: no column called " + cur.identifier);
                            cur = lexer.peek(); 
                            if (cur.tokenKind == TK_COMMA) lexer.nextToken();
                            else if (cur.tokenKind == TK_RPAREN) break;
                            else return err("Expect comma or rparen.");
                        }
                        cur = lexer.nextToken();
                        ASSERTERR(cur, TK_RPAREN, "Expect RParen.");
                    } else if (peek.tokenKind == TK_FOREIGN) {
                        lexer.nextToken(); cur = lexer.nextToken();
                        ASSERTERR(cur, TK_KEY, "Expect keyword KEY");
                        cur = lexer.nextToken();
                        ASSERTERR(cur, TK_LPAREN, "Expect LParen."); 
                        vector<uint> foreigns;
                        vector<string> foreignNames;
                        while (true) {
                            cur = lexer.nextToken();
                            ASSERTERR(cur, TK_IDENTIFIER, "Expect identifier.");
                            bool flag = false;
                            for (int i=0;i<defs.size();i++) {
                                if (defs[i].name == cur.identifier) {
                                    flag = true; foreigns.push_back(i); break;
                                }
                            }
                            if (!flag) return err("Bad primary key: no column called " + cur.identifier);
                            cur = lexer.peek(); 
                            if (cur.tokenKind == TK_COMMA) lexer.nextToken();
                            else if (cur.tokenKind == TK_RPAREN) break;
                            else return err("Expect comma or rparen.");
                        }
                        cur = lexer.nextToken();
                        ASSERTERR(cur, TK_RPAREN, "Expect RParen.");
                        cur = lexer.nextToken();
                        ASSERTERR(cur, TK_REFERENCES, "Expect keyword REFERENCES");
                        cur = lexer.nextToken();
                        ASSERTERR(cur, TK_IDENTIFIER, "Expect identifier");
                        string foreignTable = cur.identifier;
                        cur = lexer.nextToken();
                        ASSERTERR(cur, TK_LPAREN, "Expect LParen."); 
                        while (true) {
                            cur = lexer.nextToken();
                            ASSERTERR(cur, TK_IDENTIFIER, "Expect identifier.");
                            foreignNames.push_back(cur.identifier);
                            cur = lexer.peek(); 
                            if (cur.tokenKind == TK_COMMA) lexer.nextToken();
                            else if (cur.tokenKind == TK_RPAREN) break;
                            else return err("Expect comma or rparen.");
                        }
                        for (int i=0;i<foreigns.size(); i++) {
                            defs[foreigns[i]].foreignTable = foreignTable;
                            defs[foreigns[i]].foreignName = foreignNames[i];
                        }
                    }
                    peek = lexer.peek();
                    if (peek.tokenKind == TK_COMMA) lexer.nextToken();
                    else if (peek.tokenKind == TK_RPAREN) break;
                    else return err("Expect comma or rparen."); 
                }
                // finish analyse keylist
                cur = lexer.nextToken();
                ASSERTERR(cur, TK_RPAREN, "Expect RParen.");
                createTable(name, defs, primaries);
                return PSR_OK;
            } else {
                return err("Expect keyword DATABASE or TABLE.");
            }
        }
        case TK_DROP: {
            if (peek.tokenKind == TK_DATABASE) {
                lexer.nextToken(); peek = lexer.nextToken();
                ASSERTERR(peek, TK_IDENTIFIER, "Expect identifier");
                dropDatabase(peek.identifier);
                return PSR_OK;
            } else if (peek.tokenKind == TK_TABLE) {
                lexer.nextToken(); peek = lexer.nextToken();
                ASSERTERR(peek, TK_IDENTIFIER, "Expect identifier");
                dropTable(peek.identifier);
                return PSR_OK;
            } else {
                return err("Expect keyword DATABASE or TABLE.");
            }
        }
        case TK_SHOW: {
            if (peek.tokenKind == TK_DATABASE) {
                lexer.nextToken(); peek = lexer.nextToken();
                ASSERTERR(peek, TK_IDENTIFIER, "Expect identifier");
                showDatabase(peek.identifier);
                return PSR_OK;
            } else {
                return err("Expect keyword DATABASE.");
            }
        }
        case TK_USE: {
            if (peek.tokenKind == TK_DATABASE) {
                lexer.nextToken(); peek = lexer.nextToken();
                ASSERTERR(peek, TK_IDENTIFIER, "Expect identifier");
                useDatabase(peek.identifier);
                return PSR_OK;
            } else {
                return err("Expect keyword DATABASE.");
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