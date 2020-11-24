#ifndef KONTOCONST_H
#define KONTOCONST_H

#include <iostream>
#include <string>
#include <vector>

using std::cout;
using std::endl;
using std::string;
using std::vector;

#include "io/BufPageManager.h"

typedef unsigned int uint;

typedef char* KontoPage;
typedef unsigned int KontoKeyIndex; 
typedef unsigned int KontoKeyType;

struct KontoRPos;

class KontoIndex;

enum KontoResult {
    // META
    KR_ERROR                    = 0x00000000,
    KR_OK                       = 0x00000001,
    KR_NULL_PTR                 = 0x00000002,
    // FILE
    KR_FILE_CREATE_FAIL         = 0x00000200,
    KR_FILE_OPEN_FAIL           = 0x00000201,
    // ERROR IN KONTORECORD
    KR_FIELD_ALREARY_DEFINED    = 0x00000100,
    KR_PAGE_TOO_GREAT           = 0x00000101,
    KR_RECORD_INDEX_TOO_GREAT   = 0x00000102,
    KR_UNDEFINED_FIELD          = 0x00000103,
    KR_DATA_TOO_LONG            = 0x00000104,
    KR_TYPE_NOT_MATCHING        = 0x00000105,
    // ERROR IN KONTOINDEX
    KR_EMPTY_KEYLIST            = 0x00000200,
    KR_NOT_FOUND                = 0x00000201,
    KR_LAST_IPOS                = 0x00000202,
    // ERROR IN KONTODBMGR
    KR_NOT_USING_DATABASE       = 0x00000300,
    KR_NO_SUCH_DATABASE         = 0x00000301,
    KR_DATABASE_NAME_EXISTS     = 0x00000302
};

const KontoKeyType KT_INT        = 0x0;
const KontoKeyType KT_STRING     = 0x1;
const KontoKeyType KT_FLOAT      = 0x2;

string get_filename(string filename);

string strip_filename(string filename);

void debug_assert(bool assertion, std::string message);

std::vector<string> get_files(string prefix);

std::vector<string> get_index_key_names(string fullFilename);

void remove_file(string filename);

// VALUE INTEGER
inline uint& VI(char* ptr){return *(uint*)(ptr);}
// VALUE INTEGER PLUS
inline uint& VIP(char* ptr){ptr+=4; return *(uint*)(ptr-4);}
// COPY STRING
inline void CS(char* dest, KontoPage& ptr){
    strcpy(dest, ptr); int len = strlen(dest); ptr+=len+1;
}
// COPY STRING
inline void CS(string& dest, KontoPage& ptr){
    dest = ptr; int len = dest.length(); ptr+=len+1;
}
// NEW COPY STRING
inline void NCS(KontoPage& dest, KontoPage& ptr, uint len){
    dest = new char[len];
    strcpy(dest, ptr); ptr+=len;
}
// PASTE STRING
inline void PS(KontoPage& dest, char* src){
    strcpy(dest, src); dest += strlen(src) + 1;
} 
// PASTE STRING
inline void PS(KontoPage& dest, const string& src) {
    strcpy(dest, src.c_str()); dest+=src.length()+1;
}

#endif