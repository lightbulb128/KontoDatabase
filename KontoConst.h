#ifndef KONTOCONST_H
#define KONTOCONST_H

typedef unsigned int uint;

typedef unsigned int* KontoPage;
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
    KR_LAST_IPOS                = 0x00000202
};

const KontoKeyType KT_INT        = 0x0;
const KontoKeyType KT_STRING     = 0x1;
const KontoKeyType KT_FLOAT      = 0x2;

string get_filename(string filename);

void debug_assert(bool assertion, std::string message);

#endif