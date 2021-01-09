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
const char * const VALID_SYMBOLS = "(){};><=,~!+*%/|&?:[]."; // quote should not be treated as a symbol
const int MAX_INT = 0x7fffffff;

// 预期的Token类型。
enum TokenExpectation {
    TE_NONE,
    TE_IDENTIFIER,
    TE_INT_VALUE,
    TE_FLOAT_VALUE,
    TE_STRING_VALUE,
    TE_DATE_VALUE
};

// Token类型
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
    TK_REFERENCES, TK_QUIT, TK_DEBUG, TK_ECHO, TK_TABLES, TK_TO,
    TK_OFF,
    TK_ON,
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
    TK_DOT,
    // eof
    TK_EOF, 
    // error
    TK_UNDEFINED, TK_ERROR
};

const auto TK_KEYWORDS_BEGIN = TK_DATABASE;
const auto TK_KEYWORDS_END = TK_LPAREN;

// Token数据结构
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

// Trie树节点
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

/**
 * 词法分析器。需要设置输入流才能使用。
 * */
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
    bool isNegative;
    char quoteType;
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
    /** 
     * 在Trie树上作转移。
     * @param c 转移字符。
     * @param peek 待使用的下一个字符。
     * @return 返回转移结果，指示当前转移是否消耗了c和peek，并指示是否已经在Trie树上找到一个匹配项。
     * */
    TransferResult transfer(char c, char peek);
    void reset();
public:
    /** 构造Lexer词法分析器。
     * @param addDefaultKeyword 是否添加默认关键字。
     * 默认关键字包括 select, where, from, insert 等 SQL 语法关键字。
     * */
    KontoLexer(bool addDefaultKeyword);
    ~KontoLexer();
    /** 指示c是否是字母或下划线。
     * @param c 字符。
     * */
    static bool isAlphabet(char c); 
    /** 指示c是否是数字。
     * @param c 字符。
     * */
    static bool isNumber(char c);
    /** 指示c是否是符号。其中符号集合由VALID_SYMBOLS指定。
     * @param c 字符。
     * */
    static bool isSymbol(char c);
    /** 指示字母c是否是小写字母。
     * @param c 字母字符。
     * */
    static bool isLowercase(char c);
    /** 指示字符c是否是空白符。包括换行符、制表符、回车符和空格。
     * @param c 字符。
     * */
    static bool isSpace(char c);
    /** 为词法分析器设置输入流。词法分析器将从此流读入进行分析。
     * @param input 输入流。当读取文件时可用某 ifstream，当从标准输入流读入可用 cin。
     * */
    void setStream(std::istream* input);
    /** 在Trie树中添加匹配关键词。
     * @param keyword 关键词，使用小写。
     * @param tk 该关键词的Token类型。
     * */
    void addKeyword(const char* keyword, TokenKind tk);
    // 添加默认关键字。默认关键字包括 select, where, from, insert 等 SQL 语法关键字。
    void addDefaultKeywords();
    /** 输出词法分析错误信息。
     * @param str 错误信息。
     * */
    void appendErrorInfo(string str);
    /** 消耗输入流获取下一个Token。
     * @param exp 希望获取的Token类型。例如，若希望获取字符串而实际得到一个关键字，则将其转换为字符串。
     * 转换是尽力而为的，当无法转换为目标类型，则返回原始Token。
     * @return 获取到的Token。
     * */
    Token nextToken(TokenExpectation exp = TE_NONE);
    /** 消耗输入流获取下一个Token。
     * @return 获取到的Token。
     * */
    Token rawNextToken();
    /** 将Token转换为期待的类型。转换是尽力而为的，当无法转换为目标类型，则返回原始Token。
     * @param t 原始token。
     * @param expect 期待的类型。
     * @return 转换后的Token。
     * */
    Token toExpected(Token t, TokenExpectation expect);
    /** 将Token放回输入流供下次获取。实际实现上并不是放回输入流，而是存储到缓冲区中留待使用。
     * @param token 要放回的Token。
     * */
    void putback(Token token);
    /** 从输入流中消耗所有空白符。
     * */
    void killSpaces();
    /** 清空输入流buffer。
     * */
    void clearBuffer(){
        while (!charBuffer.empty()) charBuffer.pop();
        while (!buffer.empty()) buffer.pop();
    }
    /** 从输入流中获取一个Token但并不消耗输入流。
     * @param exp 期待获取的Token类型。转换是尽力而为的，当无法转换为目标类型，则返回原始Token。
     * @return 获取到的Token。
     * */
    Token peek(TokenExpectation exp = TE_NONE);
    /** 从输入流或缓冲区读入一个字符。
     * @return 获取到的字符。
     * */
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
    /** 查看输入流或缓冲区的首字符。
     * @return 查看到的字符。
     * */
    char peekChar(){
        if (charBuffer.size()>0) return charBuffer.top();
        else return stream->peek();
    }
    /** 将一个字符放回缓冲区。
     * @param c 要放回的字符。
     * */
    void putbackChar(char c){
        char peek = peekChar();
        currentColumn--;
        lastChar = ' ';
        charBuffer.push(c);
    }
    /** 获取当前读入位置的行号。
     * @return 获取到的行号。
     * */
    int getCurrentRow(){
        if (!buffer.empty()) return buffer.top().row;
        return currentRow;
    }
    /** 获取当前读入位置的列号。
     * @return 获取到的列号。
     * */
    int getCurrentColumn(){
        if (!buffer.empty()) return buffer.top().column;
        return currentColumn;
    }
};

#endif