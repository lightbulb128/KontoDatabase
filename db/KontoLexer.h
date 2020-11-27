#ifndef KONTOLEXER_H
#define KONTOLEXER_H

#include <iostream>
#include <fstream>
#include <string>
#include <stack>

using std::string;
using std::stack;

const int   VALID_CHAR_RANGE  = 26;
const int   VALID_CHAR_OFFSET = 97;
// const char* VALID_SYMBOLS    = "~!%^&*()-=+[]{}\\|;:<>,./?";
const char * const VALID_SYMBOLS = "(){};><=,-~!+*%/|&?:[]"; // quote should not be treated as a symbol
const int MAX_INT = 0x7fffffff;

enum TokenKind {
    // identifier
    TK_IDENTIFIER,
    // values
    TK_INT_VALUE, TK_FLOAT_VALUE, TK_DATE_VALUE, TK_STRING_VALUE, 
    // keywords
    TK_DATABASE, TK_DATABASES, TK_TABLE, TK_SHOW, TK_CREATE,
    TK_DROP, TK_USE, TK_PRIMARY, TK_KEY, TK_NOT, TK_NULL,
    TK_INSERT, TK_INTO, TK_VALUES, TK_DELETE, TK_FROM, TK_WHERE,
    TK_UPDATE, TK_SET, TK_SELECT, TK_IS, TK_INT, TK_VARCHAR,
    TK_DEFAULT, TK_CONSTRAINT, TK_CHANGE, TK_ALTER, TK_ADD, TK_RENAME,
    TK_DESC, TK_INDEX, TK_AND, TK_DATE, TK_FLOAT, TK_FOREIGN,
    TK_REFERENCES, TK_QUIT, TK_DEBUG,
    // symbols
    TK_LPAREN, TK_RPAREN, TK_LBRACE, TK_RBRACE, TK_SEMICOLON, 
    TK_COMMA, 
    TK_GREATER, TK_LESS, TK_GREATER_EQUAL, TK_LESS_EQUAL, 
    TK_RIGHT_SHIFT, TK_LEFT_SHIFT, 
    TK_EQUAL,
    TK_MINUS, TK_TILDE, TK_EXCLAMATION, TK_NOT_EQUAL, 
    TK_ASSIGN,
    TK_PLUS, TK_ASTERISK, TK_LSLASH, TK_PERCENT, 
    TK_QUESTION, TK_COLON, 
    TK_LBRACKET, TK_RBRACKET,
    // eof
    TK_EOF, 
    // error
    TK_UNDEFINED, TK_ERROR
};

struct Token{
    TokenKind tokenKind;
    int value;
    double doubleValue;
    string identifier;
    int row, column;
    Token(TokenKind tokenKind, int value=0):tokenKind(tokenKind), value(value){identifier="";}
    Token(TokenKind tokenKind, double value):tokenKind(tokenKind), doubleValue(value){identifier="";}
    Token(TokenKind tokenKind, string str):tokenKind(tokenKind), identifier(str){}
    Token(){tokenKind=TK_UNDEFINED; value=0; identifier="";}
    Token(string identifier):identifier(identifier), tokenKind(TK_IDENTIFIER), value(0){}
    void setRC(int r, int c){row=r;column=c;}
    friend std::ostream& operator <<(std::ostream& stream, const Token& t);
};

class LexAutomatonNode {
private:
    LexAutomatonNode* children[VALID_CHAR_RANGE];
    TokenKind tokenKind;
public:
    LexAutomatonNode();
    ~LexAutomatonNode();
    LexAutomatonNode* getNode(const char* path);
    LexAutomatonNode* createNode(const char* path, const TokenKind token);
    LexAutomatonNode* transfer(char c);
    TokenKind getTokenKind(){return tokenKind;}
};

/*
    Use a trie structure to detect keywords
    Constant value and identifier detection are done manually (brute-force)
*/
class KontoLexer {
private:
    LexAutomatonNode* root;
    LexAutomatonNode* currentPtr;
    Token currentToken;
    bool flagIdentifier;
    bool flagSymbol;
    bool flagKeyword;
    bool flagValue;
    int currentValue;
    double currentFloatValue;
    double currentFloatUnit;
    bool currentDecimal;
    bool isFloat;
    bool isString;
    string currentIdentifier;
    int currentLength;
    std::istream* stream;
    //string errorInfo;
    stack<Token> buffer;
    stack<char> charBuffer;
    int currentRow, currentColumn;
    char lastChar;
    enum TransferResult{
        TR_CONTINUE,
        TR_FINISHED,
        TR_PUTBACK,
        TR_ERROR,
        TR_PEEKUSED_CONTINUE,
        TR_PEEKUSED_FINISHED
    };
    TransferResult transfer(char c, char peek);
    void reset();
public:
    KontoLexer(bool addDefaultKeyword);
    ~KontoLexer();
    static bool isAlphabet(char c); // underline is considered as an alphabet
    static bool isNumber(char c);
    static bool isSymbol(char c);
    static bool isLowercase(char c);
    static bool isSpace(char c);
    void setStream(std::istream* input);
    void addKeyword(const char* keyword, TokenKind tk);
    void addDefaultKeywords();
    void appendErrorInfo(string str);
    Token nextToken();
    void putback(Token token);
    void killSpaces();
    void clearBuffer(){
        while (!charBuffer.empty()) charBuffer.pop();
        while (!buffer.empty()) buffer.pop();
    }
    Token peek();
    char getChar(){
        if (lastChar == '\n' || lastChar == EOF) {currentRow++; currentColumn=1;}
        else {currentColumn++;}
        // std::cout<<"getchar: "<<currentRow<<":"<<currentColumn<<std::endl;
        if (charBuffer.size()>0) {
            lastChar = charBuffer.top();
            charBuffer.pop();
        } else lastChar = stream->get();
        return lastChar;
    }
    char peekChar(){
        if (charBuffer.size()>0) return charBuffer.top();
        else return stream->peek();
    }
    void putbackChar(char c){
        char peek = peekChar();
        currentColumn--;
        lastChar = ' ';
        charBuffer.push(c);
    }
    int getCurrentRow(){
        if (!buffer.empty()) return buffer.top().row;
        return currentRow;
    }
    int getCurrentColumn(){
        if (!buffer.empty()) return buffer.top().column;
        return currentColumn;
    }
};

#endif