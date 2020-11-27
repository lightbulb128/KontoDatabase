#include <iostream>
#include <fstream>
#include <string.h>
#include "KontoLexer.h"

typedef LexAutomatonNode LNode;
typedef LNode* LNodePtr;

using std::cout;
using std::endl;
using std::istream;

const bool SHOW_ROW_LINE = true;

std::ostream& operator <<(std::ostream& stream, const Token& t){
    stream << "[";
    if (SHOW_ROW_LINE) stream<<t.row<<":"<<t.column<<" ";
    switch (t.tokenKind) {
        case TK_IDENTIFIER: stream << "Identifier: " << t.identifier; break;
        case TK_INT_VALUE: stream << "Integer Value: " << t.value; break;
        case TK_FLOAT_VALUE: 
            stream << "Float Value: " << t.doubleValue; break;
        case TK_STRING_VALUE: stream << "String Value: " << t.identifier; break;
        case TK_LPAREN: stream << "LParen ("; break;
        case TK_RPAREN: stream << "RParen )"; break;
        case TK_LBRACE: stream << "LBrace {"; break;
        case TK_RBRACE: stream << "RBrace }"; break;
        case TK_SEMICOLON: stream << "Semicolon ;"; break;
        case TK_COMMA: stream << "Comma ,"; break;
        case TK_GREATER: stream << "Greater >"; break; 
        case TK_LESS: stream << "Less <"; break;
        case TK_GREATER_EQUAL: stream << "Greater or equal >="; break;
        case TK_LESS_EQUAL: stream << "Less or equal <="; break;
        case TK_RIGHT_SHIFT: stream << "Right shift >>"; break;
        case TK_LEFT_SHIFT: stream << "Left shift <<"; break;
        case TK_EQUAL: stream << "Equal =="; break;
        case TK_ASSIGN: stream << "Assign ="; break;
        case TK_EOF: stream << "EOF"; break;
        case TK_UNDEFINED: stream << "Undefined"; break;
        case TK_ERROR: stream << "Error"; break;
        case TK_EXCLAMATION: stream << "Exclamation !"; break;
        case TK_TILDE: stream << "Tilde ~"; break;
        case TK_NOT_EQUAL: stream << "Not equal !="; break;
        case TK_MINUS: stream << "Minus -"; break;
        case TK_PLUS: stream << "Plus +"; break;
        case TK_ASTERISK: stream << "Asterisk *"; break;
        case TK_LSLASH: stream << "LSlash /"; break;
        case TK_PERCENT: stream << "Percentage %"; break;
        case TK_QUESTION: stream << "Question ?"; break;
        case TK_COLON: stream << "Colon :"; break;
        case TK_LBRACKET: stream << "LBracket '['"; break;
        case TK_RBRACKET: stream << "RBracket ']'"; break;
        case TK_DATABASE: stream << "Database"; break;
        case TK_DATABASES: stream << "Databases"; break;
        case TK_TABLE: stream << "Table"; break;
        case TK_SHOW: stream << "Show"; break;
        case TK_CREATE: stream << "Create"; break;
        case TK_DROP: stream << "Drop"; break;
        case TK_USE: stream << "Use"; break;
        case TK_PRIMARY: stream << "Primary"; break;
        case TK_KEY: stream << "Key"; break;
        case TK_NOT: stream << "Not"; break;
        case TK_NULL: stream << "Null"; break;
        case TK_INSERT: stream << "Insert"; break;
        case TK_INTO: stream << "Into"; break;
        case TK_VALUES: stream << "Values"; break;
        case TK_DELETE: stream << "Delete"; break;
        case TK_FROM: stream << "From"; break;
        case TK_WHERE: stream << "Where"; break;
        case TK_UPDATE: stream << "Update"; break;
        case TK_SET: stream << "Set"; break;
        case TK_SELECT: stream << "Select"; break;
        case TK_IS: stream << "Is"; break;
        case TK_INT: stream << "Int"; break;
        case TK_VARCHAR: stream << "Varchar"; break;
        case TK_DEFAULT: stream << "Default"; break;
        case TK_CONSTRAINT: stream << "Constraint"; break;
        case TK_CHANGE: stream << "Change"; break;
        case TK_ALTER: stream << "Alter"; break;
        case TK_ADD: stream << "Add"; break;
        case TK_RENAME: stream << "Rename"; break;
        case TK_DESC: stream << "Desc"; break;
        case TK_INDEX: stream << "Index"; break;
        case TK_AND: stream << "And"; break;
        case TK_DATE: stream << "Date"; break;
        case TK_FLOAT: stream << "Float"; break;
        case TK_FOREIGN: stream << "Foreign"; break;
        case TK_REFERENCES: stream << "References"; break;
        case TK_QUIT: stream << "Quit"; break;
        default: stream << "Unknown token type"; break;
    }
    stream << "]";
    return stream;
}

LNode::LexAutomatonNode() {
    for (int i=0;i<VALID_CHAR_RANGE;i++) children[i]=nullptr; 
    tokenKind = TK_UNDEFINED;
}

LNodePtr LNode::getNode(const char* path) {
    if (path[0]==0) return this;
    int c = path[0] - VALID_CHAR_OFFSET;
    if (children[c] == nullptr) 
        children[c] = new LexAutomatonNode(); 
    return children[c]->getNode(path+1);
}

LNodePtr LNode::createNode(const char* path, const TokenKind token) {
    LNodePtr ptr = getNode(path);
    ptr->tokenKind = token;
    return ptr;
}

LNode::~LexAutomatonNode() {
    for (int i=0;i<VALID_CHAR_RANGE;i++) 
        if (children[i]!=nullptr) {
            delete children[i];
            children[i] = nullptr;
        }
}

LNode* LNode::transfer(char c){
    return children[c-VALID_CHAR_OFFSET];
}

KontoLexer::KontoLexer(bool addDefaultKeyword) {
    root = new LNode();
    currentPtr = root;
    if (addDefaultKeyword) addDefaultKeywords();
}

KontoLexer::~KontoLexer(){
    delete root;
}

bool KontoLexer::isAlphabet(char c) { 
    return (c>='a' && c<='z') || (c>='A' && c<='Z') || (c=='_');
}

bool KontoLexer::isLowercase(char c) { 
    return (c>='a' && c<='z');
}

bool KontoLexer::isNumber(char c) {
    return (c>='0' && c<='9');
}

bool KontoLexer::isSymbol(char c){
    int k = strlen(VALID_SYMBOLS);
    for (int i=0;i<k;i++) if (c==VALID_SYMBOLS[i]) return true;
    return false;
}

bool KontoLexer::isSpace(char c){
    return (c=='\n') || (c=='\r') || (c=='\t') || (c==' ') || (c==EOF);
}

void KontoLexer::setStream(istream* input) {
    stream = input;
    currentRow = 1; currentColumn = 0; lastChar = ' ';
    charBuffer = stack<char>();
}

void KontoLexer::reset(){
    currentToken = Token();
    flagIdentifier = true;
    flagSymbol = true;
    flagKeyword = true;
    flagValue = true;
    currentPtr = root;
    currentIdentifier = "";
    currentValue = 0;
    currentLength = 0;
}

// If function transfer are run the first time after reset(),
// the stream must have skipped all the spaces.
KontoLexer::TransferResult KontoLexer::transfer(char c, char peek){
    //cout << "c=" << c << " peek=" << peek << " " << "skiv=" << flagSymbol << flagKeyword << flagIdentifier << flagValue << endl;
    if (!flagSymbol && !flagKeyword && !flagIdentifier && !flagValue) {
        appendErrorInfo("Token: Unknown token."); return TR_ERROR;
    }
    if (flagSymbol) { 
        switch (c) {
            case '(': 
                currentToken = Token(TK_LPAREN); return TR_FINISHED; break; 
            case ')':
                currentToken = Token(TK_RPAREN); return TR_FINISHED; break; 
            case '{':
                currentToken = Token(TK_LBRACE); return TR_FINISHED; break;
            case '}':
                currentToken = Token(TK_RBRACE); return TR_FINISHED; break;
            case ';':
                currentToken = Token(TK_SEMICOLON); return TR_FINISHED; break; 
            case ',':
                currentToken = Token(TK_COMMA); return TR_FINISHED; break; 
            case '=':
                if (peek=='=') {currentToken = Token(TK_EQUAL); return TR_PEEKUSED_FINISHED;} 
                else {currentToken = Token(TK_ASSIGN); return TR_FINISHED;}
                break;
            case '>':
                if (peek=='=') {currentToken = Token(TK_GREATER_EQUAL); return TR_PEEKUSED_FINISHED;}
                else if (peek=='>') {currentToken = Token(TK_RIGHT_SHIFT); return TR_PEEKUSED_FINISHED;}
                else {currentToken = Token(TK_GREATER); return TR_FINISHED;}
                break;
            case '<':
                if (peek=='=') {currentToken = Token(TK_LESS_EQUAL); return TR_PEEKUSED_FINISHED;}
                else if (peek=='>') {currentToken = Token(TK_LEFT_SHIFT); return TR_PEEKUSED_FINISHED;}
                else {currentToken = Token(TK_LESS); return TR_FINISHED;}
                break;
            case '-':
                currentToken = Token(TK_MINUS); return TR_FINISHED; break;
            case '!':
                if (peek=='=') {currentToken = Token(TK_NOT_EQUAL); return TR_PEEKUSED_FINISHED;}
                else {currentToken = Token(TK_EXCLAMATION); return TR_FINISHED;}
                break;
            case '~':
                currentToken = Token(TK_TILDE); return TR_FINISHED; break;
            case '+':
                currentToken = Token(TK_PLUS); return TR_FINISHED; break;
            case '*':
                currentToken = Token(TK_ASTERISK); return TR_FINISHED; break;
            case '/':
                currentToken = Token(TK_LSLASH); return TR_FINISHED; break;
            case '%':
                currentToken = Token(TK_PERCENT); return TR_FINISHED; break;
            case '?':
                currentToken = Token(TK_QUESTION); return TR_FINISHED; break;
            case ':':
                currentToken = Token(TK_COLON); return TR_FINISHED; break;
            case '[':
                currentToken = Token(TK_LBRACKET); return TR_FINISHED; break;
            case ']':
                currentToken = Token(TK_RBRACKET); return TR_FINISHED; break;
            default:
                flagSymbol = false;
        }
    }
    if (flagKeyword) {
        if (isSymbol(c) || isSpace(c)) {
            if (currentPtr->getTokenKind() != TK_UNDEFINED) {
                currentToken = Token(currentPtr->getTokenKind());
                return TR_PUTBACK;
            } else {
                flagKeyword = false;
            }
        } else if (isLowercase(c) || isLowercase(c-'A'+'a')) {
            currentPtr = currentPtr->transfer(c >= 'a' ? c : (c-'A'+'a'));
            if (currentPtr == nullptr) flagKeyword = false; 
        } else {
            flagKeyword = false;
        }
    } 
    if (flagIdentifier) {
        if (isAlphabet(c) || (currentLength>0 && isNumber(c))) {
            currentIdentifier += c;
        } else if (currentLength>0) {
            currentToken = Token(currentIdentifier);
            return TR_PUTBACK;
        } else {
            flagIdentifier = false;
        }
    }
    if (flagValue) {
        if (currentLength == 0) {
            if (!isNumber(c) && c!='\"' && c!='.') {flagValue = false;}
            else {
                isFloat = false; isString = false;
                if (isNumber(c)) {currentValue = c-48; return TR_CONTINUE;}
                else if (c=='\"') {isString = true, currentIdentifier = ""; return TR_CONTINUE;}
                else {
                    isFloat = true, currentDecimal = true, currentFloatValue = 0, currentFloatUnit = 0.1;
                    return TR_CONTINUE;
                }
            }
        } else {
            if (isString) {
                if (c=='\"') {currentToken = Token(TK_STRING_VALUE, currentIdentifier); return TR_FINISHED;}
                else {currentIdentifier += c; return TR_CONTINUE;}
            } else if (isFloat) {
                if (isNumber(c)) {currentFloatValue += currentFloatUnit * (c-48), currentFloatUnit /= 10; return TR_CONTINUE;}
                else {currentToken = Token(TK_FLOAT_VALUE, currentFloatValue); return TR_PUTBACK;}
            } else {
                if (isNumber(c)) {currentValue = currentValue * 10 + c - 48; return TR_CONTINUE;}
                else if (c=='.') {isFloat = true; currentDecimal = true; currentFloatValue = currentValue; currentFloatUnit = 0.1; return TR_CONTINUE;}
                else {currentToken = Token(TK_INT_VALUE, currentValue); return TR_PUTBACK;}
            }
        }
    }
    return TR_CONTINUE;
}

void KontoLexer::killSpaces(){ 
    char c;
    while (true) { // get rid of spaces
        if (stream->eof()) return;
        c = getChar();
        if (c=='/' && peekChar()=='/') { // process // comment
            while (c!='\n') c = getChar();
        } else if (c=='/' && peekChar()=='*') { // process /* */ comment
            while (c!='*' || peekChar()!='/') c=getChar();
            c=getChar(); c=getChar();
        }
        if (!isSpace(c)) break;
    }
    putbackChar(c);
}

Token KontoLexer::nextToken(){
    char peek = peekChar();
    if (!buffer.empty()) {Token ret = buffer.top(); buffer.pop(); return ret;}
    reset(); killSpaces();
    char c = getChar(); peek = peekChar();
    if (c==EOF){
        Token ret = Token(TK_EOF);
        ret.setRC(currentRow+1, currentColumn);
        return ret;
    }
    int cr = currentRow, cc = currentColumn;
    peek = peekChar();
    while (true) {
        TransferResult result = transfer(c, peek);
        switch (result) {
            case TR_FINISHED: 
                currentLength++; currentToken.setRC(cr,cc);
                return currentToken; break;
            case TR_PEEKUSED_FINISHED: 
                currentLength+=2; getChar(); currentToken.setRC(cr,cc); 
                return currentToken; break;
            case TR_PEEKUSED_CONTINUE:
                currentLength+=2; 
                getChar(); break;
            case TR_PUTBACK: 
                putbackChar(c); currentToken.setRC(cr,cc);
                return currentToken; break;
            case TR_CONTINUE:
                currentLength++; break;
            case TR_ERROR:
                Token ret = Token(TK_ERROR); ret.setRC(cr, cc);
                return ret; break;
        }
        c = getChar(); peek = peekChar();
    }
}

void KontoLexer::addKeyword(const char* keyword, TokenKind tk) {
    root->createNode(keyword, tk);
}

void KontoLexer::addDefaultKeywords(){
    addKeyword("database", TK_DATABASE);
    addKeyword("databases", TK_DATABASES);
    addKeyword("table", TK_TABLE);
    addKeyword("show", TK_SHOW);
    addKeyword("create", TK_CREATE);
    addKeyword("drop", TK_DROP);
    addKeyword("use", TK_USE);
    addKeyword("primary", TK_PRIMARY);
    addKeyword("key", TK_KEY);
    addKeyword("not", TK_NOT);
    addKeyword("null", TK_NULL);
    addKeyword("insert", TK_INSERT);
    addKeyword("into", TK_INTO);
    addKeyword("values", TK_VALUES);
    addKeyword("delete", TK_DELETE);
    addKeyword("from", TK_FROM);
    addKeyword("where", TK_WHERE);
    addKeyword("update", TK_UPDATE);
    addKeyword("set", TK_SET);
    addKeyword("select", TK_SELECT);
    addKeyword("is", TK_IS);
    addKeyword("int", TK_INT);
    addKeyword("varchar", TK_VARCHAR);
    addKeyword("default", TK_DEFAULT);
    addKeyword("constraint", TK_CONSTRAINT);
    addKeyword("change", TK_CHANGE);
    addKeyword("alter", TK_ALTER);
    addKeyword("add", TK_ADD);
    addKeyword("rename", TK_RENAME);
    addKeyword("desc", TK_DESC);
    addKeyword("index", TK_INDEX);
    addKeyword("and", TK_AND);
    addKeyword("date", TK_DATE);
    addKeyword("float", TK_FLOAT);
    addKeyword("foreign", TK_FOREIGN);
    addKeyword("references", TK_REFERENCES);
    addKeyword("quit", TK_QUIT);
}

void KontoLexer::putback(Token token) {
    buffer.push(token);
}

Token KontoLexer::peek(){
    Token next = nextToken();
    putback(next);
    return next;
}

void KontoLexer::appendErrorInfo(string str) {
    cout << str << endl;
}