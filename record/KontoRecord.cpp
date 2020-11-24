#include "KontoRecord.h"
#include <string.h>
/*
### 记录文件的存储方式
* 每页的大小位8192（个char）
* 第一页为文件信息
  * 0到63位置为文件名
  * 64到2048为域的声明：域类型，域长度，域名称，域flags，默认值，若为外键还包括外键表名和列名
    * 域flags从最低位开始：可空、是否外键
  * 2048开始，依次存储：
    文件中已有的页数（包括第一页），
    文件中已有的记录总数（包括已删除的），
    文件中已有的记录总数（不包括已删除的），
    每条记录的长度
    域的个数
    主键域的个数（若无主键置零）
    各个主键的编号
  * 3072位开始为每页的信息：当前页中已有的记录数（包括已删除的）
* 之后每页存储信息，按照每条记录长度存储
  * 每条记录，0为记录编号rid，1为控制位（二进制最低位表示是否已删除），之后开始为数据
*/

const uint POS_FILENAME          = 0x00000000;
const uint POS_FIELDS            = 0x00000040;
const uint POS_META_PAGECOUNT    = 0x00000800;
const uint POS_META_RECORDCOUNT  = 0x00000804;
const uint POS_META_EXISTCOUNT   = 0x00000808;
const uint POS_META_RECORDSIZE   = 0x0000080c;
const uint POS_META_FIELDCOUNT   = 0x00000810;
const uint POS_PAGES             = 0x00000c00;

const uint FLAGS_DELETED         = 0x00000001;
const uint FIELD_FLAGS_NULLABLE  = 0x00000001;
const uint FIELD_FLAGS_FOREIGN   = 0x00000002;

KontoTableFile::KontoTableFile() : pmgr(BufPageManager::getInstance()) {
    fieldDefined = false;
    keys = vector<KontoColumnDefinition>();
}

KontoTableFile::~KontoTableFile() {}

KontoResult KontoTableFile::createFile(
        string filename, 
        KontoTableFile** handle) {
    if (handle==nullptr) return KR_NULL_PTR;
    string fullFilename = get_filename(filename);
    KontoTableFile* ret = new KontoTableFile();
    ret->pmgr.getFileManager().createFile(fullFilename.c_str());
    ret->fileID = ret->pmgr.getFileManager().openFile(fullFilename.c_str());
    int bufindex;
    ret->filename = filename;
    KontoPage metapage = ret->pmgr.getPage(ret->fileID, 0, bufindex);
    strcpy(metapage+POS_FILENAME, filename.c_str());
    ret->pageCount = VI(metapage + POS_META_PAGECOUNT) = 1;
    ret->recordCount = VI(metapage + POS_META_RECORDCOUNT) = 0;
    VI(metapage + POS_META_EXISTCOUNT) = 0;
    ret->recordSize = VI(metapage + POS_META_RECORDSIZE) = 8; // 仅包括rid和控制位两个uint 
    VI(metapage + POS_META_FIELDCOUNT) = 0;
    ret->pmgr.markDirty(bufindex);
    ret->removeIndices();
    *handle = ret;
    return KR_OK;
}

KontoResult KontoTableFile::loadFile(
        string filename, 
        KontoTableFile** handle) {
    if (handle==nullptr) return KR_NULL_PTR;
    KontoTableFile* ret = new KontoTableFile();
    ret->filename = filename;
    string fullFilename = get_filename(filename);
    ret->fileID = ret->pmgr.getFileManager().openFile(fullFilename.c_str());
    int bufindex;
    ret->filename = filename;
    KontoPage metapage = ret->pmgr.getPage(ret->fileID, 0, bufindex);
    cout << (char*)metapage << endl;
    ret->fieldDefined = true;
    ret->recordCount = VI(metapage + POS_META_RECORDCOUNT);
    ret->pageCount = VI(metapage + POS_META_PAGECOUNT);
    ret->recordSize = VI(metapage + POS_META_RECORDSIZE);
    char* ptr = metapage + POS_FIELDS;
    int fc = VI(metapage + POS_META_FIELDCOUNT);
    int pos = 8;
    while (fc--) {
        KontoCDef col;
        //域类型，域长度，域名称，域flags，默认值，若为外键还包括外键表名和列名
        //域flags从最低位开始：可空、是否外键
        col.type = VIP(ptr);
        col.size = VIP(ptr);
        CS(col.name, ptr);
        uint flags = VIP(ptr); 
        col.nullable = flags & FIELD_FLAGS_NULLABLE;
        col.isForeign = flags & FIELD_FLAGS_FOREIGN;
        NCS(col.defaultValue, ptr, col.size);
        if (col.isForeign) {
            CS(col.foreignTable, ptr);
            CS(col.foreignName, ptr);
        }
        col.position = pos; 
        pos += col.size;
    }
    ret->recordSize = pos;
    ret->loadIndices();
    *handle = ret;
    return KR_OK;
}

KontoResult KontoTableFile::defineField(KontoCDef& def) {
    if (fieldDefined) return KR_FIELD_ALREARY_DEFINED;
    // 添加到元数据页
    int bufindex;
    KontoPage metapage = pmgr.getPage(fileID, 0, bufindex);
    char* ptr = metapage + POS_FIELDS;
    int fc = VI(metapage + POS_META_FIELDCOUNT);
    int pos = 8;
    while (fc--) {
        KontoCDef col;
        //域类型，域长度，域名称，域flags，默认值，若为外键还包括外键表名和列名
        //域flags从最低位开始：可空、是否外键
        col.type = VIP(ptr);
        col.size = VIP(ptr);
        CS(col.name, ptr);
        uint flags = VIP(ptr); 
        col.nullable = flags & FIELD_FLAGS_NULLABLE;
        col.isForeign = flags & FIELD_FLAGS_FOREIGN;
        NCS(col.defaultValue, ptr, col.size);
        if (col.isForeign) {
            CS(col.foreignTable, ptr);
            CS(col.foreignName, ptr);
        }
        col.position = pos; 
        pos += col.size;
    }
    VIP(ptr) = def.type;
    VIP(ptr) = def.size;
    PS(ptr, def.name);
    VIP(ptr) = def.nullable * FIELD_FLAGS_NULLABLE + def.isForeign * FIELD_FLAGS_FOREIGN;
    if (def.isForeign) {
        PS(ptr, def.foreignTable);
        PS(ptr, def.foreignName);
    }
    def.position = recordSize;
    recordSize += def.size;
    VI(metapage + POS_META_RECORDSIZE) = recordSize;
    VI(metapage + POS_META_FIELDCOUNT)++;
    keys.push_back(def);
    pmgr.markDirty(bufindex);
    return KR_OK;
}

KontoResult KontoTableFile::finishDefineField(){
    if (fieldDefined) return KR_FIELD_ALREARY_DEFINED;
    fieldDefined = true;
    return KR_OK;
}

KontoResult KontoTableFile::close() {
    pmgr.closeFile(fileID);
    for (auto indexPtr : indices) {
        indexPtr->close();
    }
    return KR_OK;
}

KontoResult KontoTableFile::insertEntry(KontoRPos* pos) {
    int metapid;
    KontoPage meta = pmgr.getPage(fileID, 0, metapid);
    KontoRPos rec;
    bool found = false;
    for (int i=pageCount-1;i>=1;i--) {
        int rc = VI(meta + POS_PAGES + 4*(i-1));
        if ((rc+1) * recordSize >= PAGE_SIZE) continue;
        rec = KontoRPos(i, rc);
        VI(meta + POS_PAGES + 4*(i-1)) = rc+1;
        found = true; break;
    }
    if (!found) {
        meta[POS_META_PAGECOUNT] = ++pageCount;
        rec = KontoRPos(pageCount-1, 0);
        VI(meta + POS_PAGES + 4*(pageCount-2)) = 1;
    }
    VI(meta + POS_META_RECORDCOUNT) = ++recordCount;
    VI(meta + POS_META_EXISTCOUNT)++;
    pmgr.markDirty(metapid);
    int wrpid; 
    KontoPage wr = pmgr.getPage(fileID, rec.page, wrpid);
    char* ptr = wr + rec.id * recordSize;
    VIP(ptr) = recordCount;
    VIP(ptr) = 0;
    pmgr.markDirty(wrpid);
    if (pos) *pos=rec;
    return KR_OK;
}

KontoResult KontoTableFile::deleteEntry(KontoRPos& pos) {
    int metapid;
    KontoPage meta = pmgr.getPage(fileID, 0, metapid);
    if (pos.page>=VI(meta + POS_META_PAGECOUNT)) return KR_PAGE_TOO_GREAT;
    if (pos.id>=VI(meta + POS_PAGES+4*(pos.page-1))) return KR_RECORD_INDEX_TOO_GREAT;
    int wrpid;
    KontoPage wr = pmgr.getPage(fileID, pos.page, wrpid);
    char* ptr = wr + pos.id * recordSize;
    VI(ptr + 4) |= FLAGS_DELETED;
    pmgr.markDirty(wrpid);
    return KR_OK;
}

KontoResult KontoTableFile::checkPosition(KontoRPos& pos) {
    int metapid;
    KontoPage meta = pmgr.getPage(fileID, 0, metapid);
    if (pos.page>=VI(meta + POS_META_PAGECOUNT)) return KR_PAGE_TOO_GREAT;
    if (pos.id>=VI(meta + POS_PAGES+4*(pos.page-1))) return KR_RECORD_INDEX_TOO_GREAT;
    return KR_OK;
}

char* KontoTableFile::getDataPointer(KontoRPos& pos, KontoKeyIndex key, bool write = false){
    int wrpid;
    KontoPage wr = pmgr.getPage(fileID, pos.page, wrpid);
    char* ptr = wr + pos.id * recordSize;
    ptr += keys[key].position;
    if (write) pmgr.markDirty(wrpid);
    return ptr;
}

uint KontoTableFile::getRecordSize() {return recordSize;}

KontoResult KontoTableFile::getDataCopied(KontoRPos& pos, char* dest) {
    int wrpid;
    KontoPage wr = pmgr.getPage(fileID, pos.page, wrpid);
    char* ptr = wr + pos.id * recordSize;
    memcpy(dest, ptr, recordSize);
    return KR_OK;
}

KontoResult KontoTableFile::editEntryInt(KontoRPos& pos, KontoKeyIndex key, int datum) {
    KontoResult result = checkPosition(pos); if (result!=KR_OK) return result;
    if (key<0 || key>=keys.size()) return KR_UNDEFINED_FIELD;
    if (keys[key].type!=KT_INT) return KR_TYPE_NOT_MATCHING;
    char* ptr = getDataPointer(pos, key, true);
    *((int*)ptr) = datum;
    return KR_OK;
}

KontoResult KontoTableFile::readEntryInt(KontoRPos& pos, KontoKeyIndex key, int& out) {
    KontoResult result = checkPosition(pos); if (result!=KR_OK) return result;
    if (key<0 || key>=keys.size()) return KR_UNDEFINED_FIELD;
    if (keys[key].type!=KT_INT) return KR_TYPE_NOT_MATCHING;
    char* ptr = getDataPointer(pos, key, true);
    out = *((int*)ptr);
    return KR_OK;
}

KontoResult KontoTableFile::editEntryFloat(KontoRPos& pos, KontoKeyIndex key, double datum) {
    KontoResult result = checkPosition(pos); if (result!=KR_OK) return result;
    if (key<0 || key>=keys.size()) return KR_UNDEFINED_FIELD;
    if (keys[key].type!=KT_FLOAT) return KR_TYPE_NOT_MATCHING;
    char* ptr = getDataPointer(pos, key, true);
    *((double*)ptr) = datum;
    return KR_OK;
}

KontoResult KontoTableFile::readEntryFloat(KontoRPos& pos, KontoKeyIndex key, double& out) {
    KontoResult result = checkPosition(pos); if (result!=KR_OK) return result;
    if (key<0 || key>=keys.size()) return KR_UNDEFINED_FIELD;
    if (keys[key].type!=KT_FLOAT) return KR_TYPE_NOT_MATCHING;
    char* ptr = getDataPointer(pos, key, true);
    out = *((double*)ptr);
    return KR_OK;
}

KontoResult KontoTableFile::editEntryString(KontoRPos& pos, KontoKeyIndex key, char* data) {
    KontoResult result = checkPosition(pos); if (result!=KR_OK) return result;
    if (key<0 || key>=keys.size()) return KR_UNDEFINED_FIELD;
    if (keys[key].type!=KT_STRING) return KR_TYPE_NOT_MATCHING;
    char* ptr = getDataPointer(pos, key, true);
    strcpy((char*)ptr, data);
    return KR_OK;
}

KontoResult KontoTableFile::readEntryString(KontoRPos& pos, KontoKeyIndex key, char* out) {
    KontoResult result = checkPosition(pos); if (result!=KR_OK) return result;
    if (key<0 || key>=keys.size()) return KR_UNDEFINED_FIELD;
    if (keys[key].type!=KT_STRING) return KR_TYPE_NOT_MATCHING;
    char* ptr = getDataPointer(pos, key, true);
    strcpy(out, (char*)ptr);
    return KR_OK;
}

KontoResult KontoTableFile::allEntries(KontoQRes& out) {
    out = KontoQueryResult();
    int metapid;
    KontoPage meta = pmgr.getPage(fileID, 0, metapid);
    KontoRPos rec;
    for (int i=1;i<pageCount;i++) {
        int rc = meta[POS_PAGES+i-1];
        for (int j=0;j<rc;j++) 
            out.push(KontoRPos(i, j));
    }
    return KR_OK;
}

KontoResult KontoTableFile::queryEntryInt(KontoQRes& from, KontoKeyIndex key, function<bool(int)> cond, KontoQRes& out) {
    if (keys[key].type!=KT_INT) return KR_TYPE_NOT_MATCHING; 
    KontoQRes result;
    for (auto item : from.items) {
        char* ptr = getDataPointer(item, key, false);
        if (cond(*((int*)ptr))) result.push(item);
    }
    out = result;
    return KR_OK;
}

KontoResult KontoTableFile::queryEntryFloat(KontoQRes& from, KontoKeyIndex key, function<bool(double)> cond, KontoQRes& out) {
    if (keys[key].type!=KT_FLOAT) return KR_TYPE_NOT_MATCHING; 
    KontoQRes result;
    for (auto item : from.items) {
        char* ptr = getDataPointer(item, key, false);
        if (cond(*((double*)ptr))) result.push(item);
    }
    out = result;
    return KR_OK;
}

KontoResult KontoTableFile::queryEntryString(KontoQRes& from, KontoKeyIndex key, function<bool(char*)> cond, KontoQRes& out) {
    if (keys[key].type!=KT_STRING) return KR_TYPE_NOT_MATCHING; 
    KontoQRes result;
    for (auto item : from.items) {
        char* ptr = getDataPointer(item, key, false);
        if (cond((char*)ptr)) result.push(item);
    }
    out = result;
    return KR_OK;
}

KontoResult KontoTableFile::getKeyIndex(const char* key, KontoKeyIndex& out) {
    int ks = keys.size();
    for (int i=0;i<ks;i++)
        if (key==keys[i].name) {
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

KontoResult KontoTableFile::createIndex(vector<KontoKeyIndex>& keyIndices, KontoIndex** handle) {
    vector<string> opt = vector<string>();
    vector<uint> kpos = vector<uint>();
    vector<uint> ktype = vector<KontoKeyType>();
    vector<uint> ksize = vector<uint>();
    for (auto key: keyIndices) {
        opt.push_back(keys[key].name);
        kpos.push_back(keys[key].position);
        ktype.push_back(keys[key].type);
        ksize.push_back(keys[key].size);
    }
    string indexFilename = KontoIndex::getIndexFilename(filename, opt);
    KontoIndex* ptr;
    KontoResult result = KontoIndex::createIndex(
        indexFilename, &ptr, ktype, kpos, ksize);
    indices.push_back(ptr);
    if (handle) *handle = ptr;
    return result;
}

void KontoTableFile::loadIndices() {
    indices = vector<KontoIndex*>();
    auto indexFilenames = get_files(filename + ".index.");
    for (auto indexFilename : indexFilenames) {
        KontoIndex* ptr; KontoIndex::loadIndex(
            strip_filename(indexFilename), &ptr);
        indices.push_back(ptr);
    }
}

void KontoTableFile::removeIndices() {
    indices = vector<KontoIndex*>();
    auto indexFilenames = get_files(filename + ".index.");
    for (auto indexFilename : indexFilenames) 
        remove_file(indexFilename);
}

KontoResult KontoTableFile::insertIndex(KontoRPos& pos) {
    char* data = new char[recordSize];
    getDataCopied(pos, data);
    for (auto index : indices)
        index->insert(data, pos);
    delete[] data;
    return KR_OK;
}

KontoResult KontoTableFile::deleteIndex(KontoRPos& pos) {
    char* data = new char[recordSize];
    getDataCopied(pos, data);
    for (auto index : indices)
        index->remove(data, pos);
    delete[] data;
    return KR_OK;
}

KontoResult KontoTableFile::recreateIndices() {
    KontoQRes q;
    allEntries(q);
    int n = indices.size();
    vector<KontoIndex*> newIndices;
    for (int i=0;i<n;i++) {
        KontoIndex* ptr;
        KontoIndex::recreate(indices[i], &ptr);
        newIndices.push_back(ptr);
    }
    indices = newIndices;
    for (auto item : q.items) 
        insertIndex(item);
    return KR_OK;
}

KontoIndex* KontoTableFile::getIndex(uint id){
    return indices[id];
}

void KontoTableFile::printRecord(char* record) {
    cout << "["; 
    for (int i=0;i<keys.size();i++) {
        if (i!=0) cout <<"; ";
        cout<<keys[i].name<<"=";
        switch (keys[i].type) {
            case KT_INT: {
                int vi = *((int*)(record + keys[i].position));
                cout << vi; break;
            }
            case KT_FLOAT: {
                double vd = *((double*)(record + keys[i].position));
                cout << vd; break;
            }
            case KT_STRING: {
                char* vs = (char*)(record+keys[i].position);
                cout << vs; break;
            }
        }
    }
    cout << "]";
}

void KontoTableFile::printRecord(KontoRPos& pos) {
    char* data = new char[getRecordSize()];
    getDataCopied(pos, data);
    printRecord(data);
    delete[] data;
}

KontoResult KontoTableFile::setEntryInt(char* record, KontoKeyIndex key, int datum) {
    if (key<0 || key>=keys.size()) return KR_UNDEFINED_FIELD;
    if (keys[key].type!=KT_INT) return KR_TYPE_NOT_MATCHING;
    char* ptr = record + keys[key].position;
    *((int*)ptr) = datum;
    return KR_OK;
}

KontoResult KontoTableFile::setEntryFloat(char* record, KontoKeyIndex key, double datum) {
    if (key<0 || key>=keys.size()) return KR_UNDEFINED_FIELD;
    if (keys[key].type!=KT_FLOAT) return KR_TYPE_NOT_MATCHING;
    char* ptr = record + keys[key].position;
    *((double*)ptr) = datum;
    return KR_OK;
}

KontoResult KontoTableFile::setEntryString(char* record, KontoKeyIndex key, const char* data) {
    if (key<0 || key>=keys.size()) return KR_UNDEFINED_FIELD;
    if (keys[key].type!=KT_STRING) return KR_TYPE_NOT_MATCHING;
    char* ptr = record + keys[key].position;
    strcpy((char*)ptr, data);
    return KR_OK;
}