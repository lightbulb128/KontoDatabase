#include "KontoRecord.h"
#include <string.h>
/*
### 记录文件的存储方式
* 每页的大小位8192（个整数）
* 第一页为文件信息
  * 0到63位置为文件名
  * 64到1023为每32位为域的声明：域类型，域长度，域名称，
  * 1024开始，依次存储：
    文件中已有的页数（包括第一页），
    文件中已有的记录总数（包括已删除的），
    文件中已有的记录总数（不包括已删除的），
    每条记录的长度
    域的个数
  * 1536位开始为每页的信息：当前页中已有的记录数（包括已删除的）
* 之后每页存储信息，按照每条记录长度存储
  * 每条记录，0为记录编号rid，1为控制位（二进制最低位表示是否已删除），之后开始为数据
*/

const uint POS_FILENAME          = 0x00000000;
const uint POS_FIELDS            = 0x00000040;
const uint SIZE_FIELDS_LENGTH    = 32;
const uint POS_META_PAGECOUNT    = 0x00000400;
const uint POS_META_RECORDCOUNT  = 0x00000401;
const uint POS_META_EXISTCOUNT   = 0x00000402;
const uint POS_META_RECORDSIZE   = 0x00000403;
const uint POS_META_FIELDCOUNT   = 0x00000404;
const uint POS_PAGES             = 0x00000600;

const uint FLAGS_DELETED         = 0x00000001;


KontoTableFile::KontoTableFile(BufPageManager* pManager, FileManager* fManager) : pmgr(pManager), fmgr(fManager) {
    fieldDefined = false;
    keyPosition = vector<uint>();
    keySize = vector<uint>();
    keyType = vector<KontoKeyType>();
}

KontoTableFile::~KontoTableFile() {}

KontoResult KontoTableFile::createFile(
        string filename, 
        KontoTableFile** handle, 
        BufPageManager* pManager, 
        FileManager* fManager) {
    if (handle==nullptr) return KR_NULL_PTR;
    string fullFilename = get_filename(filename);
    KontoTableFile* ret = new KontoTableFile(pManager, fManager);
    if (!ret->fmgr->createFile(fullFilename.c_str())) return KR_FILE_CREATE_FAIL;
    if (!ret->fmgr->openFile(fullFilename.c_str(), ret->fileID)) return KR_FILE_OPEN_FAIL;
    int bufindex; 
    cout << "TABLE FILEID=" << ret->fileID << endl;
    ret->filename = filename;
    KontoPage metapage = ret->pmgr->getPage(ret->fileID, 0, bufindex);
    strcpy((char*)(metapage+POS_FILENAME), filename.c_str());
    ret->pageCount = metapage[POS_META_PAGECOUNT] = 1;
    ret->recordCount = metapage[POS_META_RECORDCOUNT] = 0;
    metapage[POS_META_EXISTCOUNT] = 0;
    ret->recordSize = metapage[POS_META_RECORDSIZE] = 2; // 仅包括rid和控制位两个uint 
    metapage[POS_META_FIELDCOUNT] = 0;
    ret->pmgr->markDirty(bufindex);
    *handle = ret;
    return KR_OK;
}

KontoResult KontoTableFile::loadFile(
        string filename, 
        KontoTableFile** handle, 
        BufPageManager* pManager, 
        FileManager* fManager) {
    if (handle==nullptr) return KR_NULL_PTR;
    KontoTableFile* ret = new KontoTableFile(pManager, fManager);
    ret->filename = filename;
    string fullFilename = get_filename(filename);
    if (!ret->fmgr->openFile(fullFilename.c_str(), ret->fileID)) return KR_FILE_OPEN_FAIL;
    int bufindex;
    ret->filename = filename;
    KontoPage metapage = ret->pmgr->getPage(ret->fileID, 0, bufindex);
    cout << (char*)metapage << endl;
    ret->fieldDefined = true;
    ret->recordCount = metapage[POS_META_RECORDCOUNT];
    ret->pageCount = metapage[POS_META_PAGECOUNT];
    ret->recordSize = metapage[POS_META_RECORDSIZE];
    uint* ptr = metapage+POS_FIELDS;
    int fc = metapage[POS_META_FIELDCOUNT];
    int pos = 2;
    while (fc--) {
        ret->keyType.push_back(ptr[0]);
        ret->keySize.push_back(ptr[1]);
        ret->keyPosition.push_back(pos);
        pos+=ptr[1];
        char* charptr = (char*)(ptr+2);
        string str(charptr);
        ret->keyNames.push_back(str);
        ptr+=SIZE_FIELDS_LENGTH;
    }
    ret->recordSize = pos;
    *handle = ret;
    return KR_OK;
}

KontoResult KontoTableFile::defineField(int size, KontoKeyType type, const char* key) {
    if (fieldDefined) return KR_FIELD_ALREARY_DEFINED;
    // 添加到元数据页
    int bufindex;
    KontoPage metapage = pmgr->getPage(fileID, 0, bufindex);
    uint* ptr = metapage + POS_FIELDS + keyNames.size() * SIZE_FIELDS_LENGTH;
    ptr[0] = type;
    ptr[1] = size;
    char* charptr = (char*)(ptr+2);
    strcpy(charptr, key);
    keyPosition.push_back(recordSize);
    keyType.push_back(type);
    keySize.push_back(size);
    keyNames.push_back(string(key));
    recordSize += size;
    metapage[POS_META_RECORDSIZE] = recordSize;
    metapage[POS_META_FIELDCOUNT]++;
    pmgr->markDirty(bufindex);
    return KR_OK;
}

KontoResult KontoTableFile::finishDefineField(){
    if (fieldDefined) return KR_FIELD_ALREARY_DEFINED;
    fieldDefined = true;
    return KR_OK;
}

KontoResult KontoTableFile::close() {
    pmgr->closeFile(fileID);
    return KR_OK;
}

KontoResult KontoTableFile::insertEntry(KontoRPos* pos) {
    int metapid;
    KontoPage meta = pmgr->getPage(fileID, 0, metapid);
    KontoRPos rec;
    bool found = false;
    for (int i=pageCount-1;i>=1;i--) {
        int rc = meta[POS_PAGES+i-1];
        if ((rc+1) * recordSize >= PAGE_INT_NUM) continue;
        rec = KontoRPos(i, rc);
        meta[POS_PAGES+i-1] = rc+1;
        found = true; break;
    }
    if (!found) {
        meta[POS_META_PAGECOUNT] = ++pageCount;
        rec = KontoRPos(pageCount-1, 0);
        meta[POS_PAGES+pageCount-2] = 1;
    }
    meta[POS_META_RECORDCOUNT] = ++recordCount;
    meta[POS_META_EXISTCOUNT]++;
    pmgr->markDirty(metapid);
    int wrpid; 
    KontoPage wr = pmgr->getPage(fileID, rec.page, wrpid);
    uint* ptr = wr + rec.id * recordSize;
    ptr[0] = recordCount;
    ptr[1] = 0;
    pmgr->markDirty(wrpid);
    if (pos) *pos=rec;
    return KR_OK;
}

KontoResult KontoTableFile::deleteEntry(KontoRPos& pos) {
    int metapid;
    KontoPage meta = pmgr->getPage(fileID, 0, metapid);
    if (pos.page>=meta[POS_META_PAGECOUNT]) return KR_PAGE_TOO_GREAT;
    if (pos.id>=meta[POS_PAGES+pos.page-1]) return KR_RECORD_INDEX_TOO_GREAT;
    int wrpid;
    KontoPage wr = pmgr->getPage(fileID, pos.page, wrpid);
    uint* ptr = wr + pos.id * recordSize;
    ptr[1] |= FLAGS_DELETED;
    pmgr->markDirty(wrpid);
    return KR_OK;
}

KontoResult KontoTableFile::checkPosition(KontoRPos& pos) {
    int metapid;
    KontoPage meta = pmgr->getPage(fileID, 0, metapid);
    if (pos.page>=meta[POS_META_PAGECOUNT]) return KR_PAGE_TOO_GREAT;
    if (pos.id>=meta[POS_PAGES+pos.page-1]) return KR_RECORD_INDEX_TOO_GREAT;
    return KR_OK;
}

uint* KontoTableFile::getDataPointer(KontoRPos& pos, KontoKeyIndex key, bool write = false){
    int wrpid;
    KontoPage wr = pmgr->getPage(fileID, pos.page, wrpid);
    uint* ptr = wr + pos.id * recordSize;
    ptr += keyPosition[key];
    if (write) pmgr->markDirty(wrpid);
    return ptr;
}

uint KontoTableFile::getRecordSize() {return recordSize;}

KontoResult KontoTableFile::getDataCopied(KontoRPos& pos, uint* dest) {
    int wrpid;
    KontoPage wr = pmgr->getPage(fileID, pos.page, wrpid);
    uint* ptr = wr + pos.id * recordSize;
    memcpy(dest, ptr, 4*recordSize);
    return KR_OK;
}

KontoResult KontoTableFile::editEntryInt(KontoRPos& pos, KontoKeyIndex key, int datum) {
    KontoResult result = checkPosition(pos); if (result!=KR_OK) return result;
    if (key<0 || key>=keyNames.size()) return KR_UNDEFINED_FIELD;
    if (keyType[key]!=KT_INT) return KR_TYPE_NOT_MATCHING;
    uint* ptr = getDataPointer(pos, key, true);
    *((int*)ptr) = datum;
    return KR_OK;
}

KontoResult KontoTableFile::readEntryInt(KontoRPos& pos, KontoKeyIndex key, int& out) {
    KontoResult result = checkPosition(pos); if (result!=KR_OK) return result;
    if (key<0 || key>=keyNames.size()) return KR_UNDEFINED_FIELD;
    if (keyType[key]!=KT_INT) return KR_TYPE_NOT_MATCHING;
    uint* ptr = getDataPointer(pos, key, false);
    out = *((int*)ptr);
    return KR_OK;
}

KontoResult KontoTableFile::editEntryFloat(KontoRPos& pos, KontoKeyIndex key, double datum) {
    KontoResult result = checkPosition(pos); if (result!=KR_OK) return result;
    if (key<0 || key>=keyNames.size()) return KR_UNDEFINED_FIELD;
    if (keyType[key]!=KT_FLOAT) return KR_TYPE_NOT_MATCHING;
    uint* ptr = getDataPointer(pos, key, true);
    *((double*)ptr) = datum;
    return KR_OK;
}

KontoResult KontoTableFile::readEntryFloat(KontoRPos& pos, KontoKeyIndex key, double& out) {
    KontoResult result = checkPosition(pos); if (result!=KR_OK) return result;
    if (key<0 || key>=keyNames.size()) return KR_UNDEFINED_FIELD;
    if (keyType[key]!=KT_FLOAT) return KR_TYPE_NOT_MATCHING;
    uint* ptr = getDataPointer(pos, key, false);
    out = *((double*)ptr);
    return KR_OK;
}

KontoResult KontoTableFile::editEntryString(KontoRPos& pos, KontoKeyIndex key, char* data) {
    KontoResult result = checkPosition(pos); if (result!=KR_OK) return result;
    if (key<0 || key>=keyNames.size()) return KR_UNDEFINED_FIELD;
    if (keyType[key]!=KT_STRING) return KR_TYPE_NOT_MATCHING;
    uint* ptr = getDataPointer(pos, key, true);
    strcpy((char*)ptr, data);
    return KR_OK;
}

KontoResult KontoTableFile::readEntryString(KontoRPos& pos, KontoKeyIndex key, char* out) {
    KontoResult result = checkPosition(pos); if (result!=KR_OK) return result;
    if (key<0 || key>=keyNames.size()) return KR_UNDEFINED_FIELD;
    if (keyType[key]!=KT_STRING) return KR_TYPE_NOT_MATCHING;
    uint* ptr = getDataPointer(pos, key, true);
    strcpy(out, (char*)ptr);
    return KR_OK;
}

KontoResult KontoTableFile::allEntries(KontoQRes& out) {
    out = KontoQueryResult();
    int metapid;
    KontoPage meta = pmgr->getPage(fileID, 0, metapid);
    KontoRPos rec;
    for (int i=1;i<pageCount;i++) {
        int rc = meta[POS_PAGES+i-1];
        for (int j=0;j<rc;j++) 
            out.push(KontoRPos(i, j));
    }
    return KR_OK;
}

KontoResult KontoTableFile::queryEntryInt(KontoQRes& from, KontoKeyIndex key, function<bool(int)> cond, KontoQRes& out) {
    if (keyType[key]!=KT_INT) return KR_TYPE_NOT_MATCHING; 
    KontoQRes result;
    for (auto item : from.items) {
        uint* ptr = getDataPointer(item, key, false);
        if (cond(*((int*)ptr))) result.push(item);
    }
    out = result;
    return KR_OK;
}

KontoResult KontoTableFile::queryEntryFloat(KontoQRes& from, KontoKeyIndex key, function<bool(double)> cond, KontoQRes& out) {
    if (keyType[key]!=KT_FLOAT) return KR_TYPE_NOT_MATCHING; 
    KontoQRes result;
    for (auto item : from.items) {
        uint* ptr = getDataPointer(item, key, false);
        if (cond(*((double*)ptr))) result.push(item);
    }
    out = result;
    return KR_OK;
}

KontoResult KontoTableFile::queryEntryString(KontoQRes& from, KontoKeyIndex key, function<bool(char*)> cond, KontoQRes& out) {
    if (keyType[key]!=KT_STRING) return KR_TYPE_NOT_MATCHING; 
    KontoQRes result;
    for (auto item : from.items) {
        uint* ptr = getDataPointer(item, key, false);
        if (cond((char*)ptr)) result.push(item);
    }
    out = result;
    return KR_OK;
}

KontoResult KontoTableFile::getKeyIndex(const char* key, KontoKeyIndex& out) {
    int ks = keyNames.size();
    for (int i=0;i<ks;i++)
        if (key==keyNames[i]) {
            out = i;
            return KR_OK;
        }
    return KR_UNDEFINED_FIELD;
}

KontoQueryResult::KontoQueryResult(const KontoQRes& r) {
    clear();
    for (auto item : r.items) items.push_back(item);
}

KontoQueryResult KontoQueryResult::join(const KontoQueryResult& b) {
    KontoQRes ret;
    int sa = size(), sb = b.size();
    int i = 0, j = 0;
    while (i<sa || j<sb) {
        if (i<sa && j<sb && items[i]==b.items[j]) {
            ret.push(items[i]); i++; j++;
        } else if ((i<sa && j<sb && items[i]<b.items[j]) || (j==sb)) {
            ret.push(items[i]); i++;
        } else {
            ret.push(b.items[j]); j++;
        }
    }
    return ret;
}

KontoQueryResult KontoQueryResult::meet(const KontoQueryResult& b) {
    KontoQRes ret;
    int sa = size(), sb = b.size();
    int i = 0, j = 0;
    while (i<sa || j<sb) {
        if (i<sa && j<sb && items[i]==b.items[j]) {
            ret.push(items[i]); i++; j++;
        } else if ((i<sa && j<sb && items[i]<b.items[j]) || (j==sb)) {
            i++;
        } else {
            j++;
        }
    }
    return ret;
}

KontoQueryResult KontoQueryResult::substract(const KontoQueryResult& b) {
    KontoQRes ret;
    int sa = size(), sb = b.size();
    int i = 0, j = 0;
    while (i<sa || j<sb) {
        if (i<sa && j<sb && items[i]==b.items[j]) {
            i++; j++;
        } else if ((i<sa && j<sb && items[i]<b.items[j]) || (j==sb)) {
            ret.push(items[i]); i++;
        } else {
            j++;
        }
    }
    return ret;
}

void KontoTableFile::debugtest(){
    cout << "recordsize=" << recordSize << endl;
    cout << "recordcount=" << recordCount << endl;
    cout << "pagecount=" << pageCount << endl;
}

KontoResult KontoTableFile::createIndex(vector<KontoKeyIndex>& keys, KontoIndex** handle) {
    vector<string> opt = vector<string>();
    vector<uint> kpos = vector<uint>();
    vector<uint> ktype = vector<KontoKeyType>();
    vector<uint> ksize = vector<uint>();
    for (auto key: keys) {
        opt.push_back(keyNames[key]);
        kpos.push_back(keyPosition[key]);
        ktype.push_back(keyType[key]);
        ksize.push_back(keySize[key]);
    }
    string indexFilename = KontoIndex::getIndexFilename(filename, opt);
    KontoResult result = KontoIndex::createIndex(
        indexFilename, handle, pmgr, fmgr,
        ktype, kpos, ksize);
    return result;
}
