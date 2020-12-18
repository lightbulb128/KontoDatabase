#include "KontoRecord.h"
#include <string.h>
#include <math.h>
#include "KontoTerm.h"
/*
### 记录文件的存储方式
* 每页的大小位8192（个char）
* 第一页为文件信息
  * 0到63位置为文件名
  * 64到2048为域的声明：域类型，域长度，域名称，域flags，默认值
    * 域flags从最低位开始：可空
  * 2048开始，依次存储：
    文件中已有的页数（包括第一页），
    文件中已有的记录总数（包括已删除的），
    文件中已有的记录总数（不包括已删除的），
    每条记录的长度
    域的个数
    当前最后一页已有的条目数
  * 2560开始，
    主键域的个数（若无主键置零）
    各个主键的编号
  * 3072开始，存储外键：
    * 外键个数
      * 每个外键的列数，以及对应哪些列，fkname, foreigntablename, foreignnames
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
const uint POS_META_LASTPAGE     = 0x00000814;
const uint POS_META_PRIMARYCOUNT = 0x00000a00;
const uint POS_META_PRIMARIES    = 0x00000a04;
const uint POS_META_FOREIGNS     = 0x00000c00;

const uint FLAGS_DELETED         = 0x00000001;
const uint FIELD_FLAGS_NULLABLE  = 0x00000001;

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
    ret->pageCount = VI(metapage + POS_META_PAGECOUNT) = 2;
    ret->recordCount = VI(metapage + POS_META_RECORDCOUNT) = 0;
    VI(metapage + POS_META_EXISTCOUNT) = 0;
    ret->recordSize = VI(metapage + POS_META_RECORDSIZE) = 8; // 仅包括rid和控制位两个uint 
    VI(metapage + POS_META_FIELDCOUNT) = 0;
    VI(metapage + POS_META_PRIMARYCOUNT) = 0;
    VI(metapage + POS_META_FOREIGNS) = 0;
    VI(metapage + POS_META_LASTPAGE) = 0;
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
    ret->fieldDefined = true;
    ret->recordCount = VI(metapage + POS_META_RECORDCOUNT);
    ret->pageCount = VI(metapage + POS_META_PAGECOUNT);
    ret->recordSize = VI(metapage + POS_META_RECORDSIZE);
    char* ptr = metapage + POS_FIELDS;
    int fc = VI(metapage + POS_META_FIELDCOUNT);
    int pos = 8;
    while (fc--) {
        KontoCDef col;
        //域类型，域长度，域名称，域flags，默认值
        //域flags从最低位开始：可空、是否外键
        col.type = VIP(ptr);
        col.size = VIP(ptr);
        CS(col.name, ptr);
        uint flags = VIP(ptr); 
        col.nullable = flags & FIELD_FLAGS_NULLABLE;
        NCS(col.defaultValue, ptr, col.size);
        col.position = pos; 
        ret->keys.push_back(col);
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
    //char* originalptr = ptr;
    int fc = VI(metapage + POS_META_FIELDCOUNT);
    int pos = 8;
    while (fc--) {
        KontoCDef col;
        //域类型，域长度，域名称，域flags，默认值
        //域flags从最低位开始：可空
        col.type = VIP(ptr);
        //cout << "type=" << col.type << endl;
        col.size = VIP(ptr);
        //cout << "size=" << col.size << endl;
        CS(col.name, ptr);
        //cout << "name=" << col.name << endl;
        uint flags = VIP(ptr); 
        //cout << "flags=" << flags << endl;
        col.nullable = flags & FIELD_FLAGS_NULLABLE;
        ptr += col.size;
        col.position = pos; 
        pos += col.size;
        //cout << "add: " << (int)(ptr - originalptr) << endl;
    }
    VIP(ptr) = def.type;
    VIP(ptr) = def.size;
    PS(ptr, def.name);
    VIP(ptr) = def.nullable * FIELD_FLAGS_NULLABLE;
    assert(def.defaultValue!=nullptr);
    PD(ptr, def.defaultValue, def.size);
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
    pmgr.getFileManager().closeFile(fileID);
    for (auto indexPtr : indices) {
        indexPtr->close();
    }
    return KR_OK;
}

KontoResult KontoTableFile::insertEntry(KontoRPos* pos) {
    int metapid;
    KontoPage meta = pmgr.getPage(fileID, 0, metapid);
    KontoRPos rec;
    bool found = (1 + VI(meta + POS_META_LASTPAGE)) * recordSize <= PAGE_SIZE;
    if (!found) {
        meta[POS_META_PAGECOUNT] = ++pageCount;
        rec = KontoRPos(pageCount-1, 0);
        VI(meta + POS_META_LASTPAGE) = 1;
    } else {
        rec = KontoRPos(pageCount-1, VI(meta+POS_META_LASTPAGE));
        VI(meta+POS_META_LASTPAGE)++;
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

KontoResult KontoTableFile::deleteEntry(const KontoRPos& pos) {
    int metapid;
    KontoPage meta = pmgr.getPage(fileID, 0, metapid);
    int wrpid;
    KontoPage wr = pmgr.getPage(fileID, pos.page, wrpid);
    char* ptr = wr + pos.id * recordSize;
    VI(ptr + 4) |= FLAGS_DELETED;
    pmgr.markDirty(wrpid);
    return KR_OK;
}

char* KontoTableFile::getDataPointer(const KontoRPos& pos, KontoKeyIndex key, bool write = false){
    int wrpid;
    KontoPage wr = pmgr.getPage(fileID, pos.page, wrpid);
    char* ptr = wr + pos.id * recordSize;
    ptr += keys[key].position;
    if (write) pmgr.markDirty(wrpid);
    return ptr;
}

char* KontoTableFile::getRecordPointer(const KontoRPos& pos, bool write) {
    int wrpid;
    KontoPage wr = pmgr.getPage(fileID, pos.page, wrpid);
    char* ptr = wr + pos.id * recordSize;
    if (write) pmgr.markDirty(wrpid);
    return ptr;
}

uint KontoTableFile::getRecordSize() {return recordSize;}

KontoResult KontoTableFile::getDataCopied(const KontoRPos& pos, char* dest) {
    int wrpid;
    KontoPage wr = pmgr.getPage(fileID, pos.page, wrpid);
    char* ptr = wr + pos.id * recordSize;
    memcpy(dest, ptr, recordSize);
    return KR_OK;
}

KontoResult KontoTableFile::editEntryInt(const KontoRPos& pos, KontoKeyIndex key, int datum) {
    if (key<0 || key>=keys.size()) return KR_NO_SUCH_COLUMN;
    if (keys[key].type!=KT_INT) return KR_TYPE_NOT_MATCHING;
    char* ptr = getDataPointer(pos, key, true);
    *((int*)ptr) = datum;
    return KR_OK;
}

KontoResult KontoTableFile::readEntryInt(const KontoRPos& pos, KontoKeyIndex key, int& out) {
    if (key<0 || key>=keys.size()) return KR_NO_SUCH_COLUMN;
    if (keys[key].type!=KT_INT) return KR_TYPE_NOT_MATCHING;
    char* ptr = getDataPointer(pos, key, true);
    out = *((int*)ptr);
    return KR_OK;
}

KontoResult KontoTableFile::editEntryFloat(const KontoRPos& pos, KontoKeyIndex key, double datum) {
    if (key<0 || key>=keys.size()) return KR_NO_SUCH_COLUMN;
    if (keys[key].type!=KT_FLOAT) return KR_TYPE_NOT_MATCHING;
    char* ptr = getDataPointer(pos, key, true);
    *((double*)ptr) = datum;
    return KR_OK;
}

KontoResult KontoTableFile::readEntryFloat(const KontoRPos& pos, KontoKeyIndex key, double& out) {
    if (key<0 || key>=keys.size()) return KR_NO_SUCH_COLUMN;
    if (keys[key].type!=KT_FLOAT) return KR_TYPE_NOT_MATCHING;
    char* ptr = getDataPointer(pos, key, true);
    out = *((double*)ptr);
    return KR_OK;
}

KontoResult KontoTableFile::editEntryString(const KontoRPos& pos, KontoKeyIndex key, const char* data) {
    if (key<0 || key>=keys.size()) return KR_NO_SUCH_COLUMN;
    if (keys[key].type!=KT_STRING) return KR_TYPE_NOT_MATCHING;
    char* ptr = getDataPointer(pos, key, true);
    strcpy((char*)ptr, data);
    return KR_OK;
}

KontoResult KontoTableFile::readEntryString(const KontoRPos& pos, KontoKeyIndex key, char* out) {
    if (key<0 || key>=keys.size()) return KR_NO_SUCH_COLUMN;
    if (keys[key].type!=KT_STRING) return KR_TYPE_NOT_MATCHING;
    char* ptr = getDataPointer(pos, key, true);
    strcpy(out, (char*)ptr);
    return KR_OK;
}

KontoResult KontoTableFile::editEntryDate(const KontoRPos& pos, KontoKeyIndex key, Date datum) {
    if (key<0 || key>=keys.size()) return KR_NO_SUCH_COLUMN;
    if (keys[key].type!=KT_DATE) return KR_TYPE_NOT_MATCHING;
    char* ptr = getDataPointer(pos, key, true);
    *((Date*)ptr) = datum;
    return KR_OK;
}

KontoResult KontoTableFile::readEntryDate(const KontoRPos& pos, KontoKeyIndex key, Date& out) {
    if (key<0 || key>=keys.size()) return KR_NO_SUCH_COLUMN;
    if (keys[key].type!=KT_DATE) return KR_TYPE_NOT_MATCHING;
    char* ptr = getDataPointer(pos, key, true);
    out = *((Date*)ptr);
    return KR_OK;
}

KontoResult KontoTableFile::allEntries(KontoQRes& out) {
    out = KontoQueryResult();
    int metapid;
    KontoPage meta = pmgr.getPage(fileID, 0, metapid);
    KontoRPos rec;
    for (int i=1;i<pageCount-1;i++) {
        int cnt = PAGE_SIZE / recordSize;
        for (int j=0;j<cnt;j++) 
            out.push(KontoRPos(i, j));
    }
    int last = pageCount - 1;
    int cnt = VI(meta + POS_META_LASTPAGE);
    for (int j=0;j<cnt;j++) out.push(KontoRPos(last, j));
    return KR_OK;
}

KontoResult KontoTableFile::queryEntryInt(const KontoQRes& from, KontoKeyIndex key, function<bool(int)> cond, KontoQRes& out) {
    if (keys[key].type!=KT_INT) return KR_TYPE_NOT_MATCHING; 
    KontoQRes result; 
    for (auto& item : from.items) {
        char* ptr = getRecordPointer(item, false);
        if (VI(ptr + 4) & FLAGS_DELETED) continue;
        ptr += keys[key].position;
        if (cond(*((int*)ptr))) result.push(item);
    }
    result.sorted = true;
    out = result;
    return KR_OK;
}

KontoResult KontoTableFile::queryEntryFloat(const KontoQRes& from, KontoKeyIndex key, function<bool(double)> cond, KontoQRes& out) {
    if (keys[key].type!=KT_FLOAT) return KR_TYPE_NOT_MATCHING; 
    KontoQRes result;
    for (auto& item : from.items) {
        char* ptr = getRecordPointer(item, false);
        if (VI(ptr + 4) & FLAGS_DELETED) continue;
        ptr += keys[key].position;
        if (cond(*((double*)ptr))) result.push(item);
    }
    result.sorted = true;
    out = result;
    return KR_OK;
}

KontoResult KontoTableFile::queryEntryString(const KontoQRes& from, KontoKeyIndex key, function<bool(const char*)> cond, KontoQRes& out) {
    if (keys[key].type!=KT_STRING) return KR_TYPE_NOT_MATCHING; 
    KontoQRes result;
    for (auto& item : from.items) {
        char* ptr = getRecordPointer(item, false);
        if (VI(ptr + 4) & FLAGS_DELETED) continue;
        ptr += keys[key].position;
        if (cond((char*)ptr)) result.push(item);
    }
    result.sorted = true;
    out = result;
    return KR_OK;
}

KontoResult KontoTableFile::queryEntryDate(const KontoQRes& from, KontoKeyIndex key, function<bool(Date)> cond, KontoQRes& out) {
    if (keys[key].type!=KT_DATE) return KR_TYPE_NOT_MATCHING; 
    KontoQRes result; 
    for (auto& item : from.items) {
        char* ptr = getRecordPointer(item, false);
        if (VI(ptr + 4) & FLAGS_DELETED) continue;
        ptr += keys[key].position;
        if (cond(*((Date*)ptr))) result.push(item);
    }
    result.sorted = true;
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
    return KR_NO_SUCH_COLUMN;
}

KontoQueryResult::KontoQueryResult(const KontoQRes& r) {
    clear();
    sorted = r.sorted;
    for (auto item : r.items) items.push_back(item);
}

KontoQueryResult KontoQueryResult::join(KontoQueryResult& b) {
    KontoQRes ret;
    int sa = size(), sb = b.size();
    if (!sorted) sort();
    if (!b.sorted) b.sort();
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
    ret.sorted = true;
    return ret;
}

KontoQueryResult KontoQueryResult::meet(KontoQueryResult& b) {
    KontoQRes ret;
    if (!sorted) sort();
    if (!b.sorted) b.sort();
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
    ret.sorted = true;
    return ret;
}

KontoQueryResult KontoQueryResult::substract(KontoQueryResult& b) {
    KontoQRes ret;
    int sa = size(), sb = b.size();
    if (!sorted) sort();
    if (!b.sorted) b.sort();
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
    ret.sorted = true;
    return ret;
}

KontoResult KontoTableFile::createIndex(const vector<KontoKeyIndex>& keyIndices, KontoIndex** handle) {
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
    for (auto& item : indices) {if (item->getFilename() == indexFilename) return KR_INDEX_ALREADY_EXISTS;}
    KontoIndex* ptr;
    KontoResult result = KontoIndex::createIndex(
        indexFilename, &ptr, ktype, kpos, ksize);
    indices.push_back(ptr);
    KontoQRes q;
    allEntries(q);
    for (auto item : q.items) 
        insertIndex(item, ptr);
    if (handle) *handle = ptr;
    return result;
}

void KontoTableFile::loadIndices() {
    indices = vector<KontoIndex*>();
    //cout << "load indices" << endl;
    auto indexFilenames = get_files(filename + ".__index.");
    for (auto indexFilename : indexFilenames) {
        KontoIndex* ptr; KontoIndex::loadIndex(
            strip_filename(indexFilename), &ptr);
        //cout << "loaded : " << indexFilename << endl;
        indices.push_back(ptr);
    }
    if (hasPrimaryKey()) {
        vector<uint> primaryKeyIndices;
        getPrimaryKeys(primaryKeyIndices);
        vector<string> cols;
        for (auto i : primaryKeyIndices) cols.push_back(keys[i].name);
        string fn = KontoIndex::getIndexFilename(filename, cols);
        for (auto& id : indices) if (id->getFilename() == fn) {
            primaryIndex = id; break;
        }
    }
}

void KontoTableFile::removeIndices() {
    indices = vector<KontoIndex*>();
    //cout << "remove indices" << endl;
    auto indexFilenames = get_files(filename + ".__index.");
    for (auto indexFilename : indexFilenames) 
        remove_file(indexFilename);
}

KontoResult KontoTableFile::insertIndex(const KontoRPos& pos) {
    char* data = new char[recordSize];
    getDataCopied(pos, data);
    for (auto& index : indices) {
        //cout << "before debug print" << endl;
        //index->debugPrint();
        //cout << "insert into: " << index->getFilename() << endl;
        index->insert(data, pos);
        //cout << "after debug print" << endl;
        //index->debugPrint();
    }
    delete[] data;
    return KR_OK;
}

KontoResult KontoTableFile::insertIndex(const KontoRPos& pos, KontoIndex* dest) {
    char* data = new char[recordSize];
    getDataCopied(pos, data);
    dest->insert(data, pos);
    delete[] data;
    return KR_OK;
}

KontoResult KontoTableFile::deleteIndex(const KontoRPos& pos) {
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

KontoIndex* KontoTableFile::getIndex(const vector<KontoKeyIndex>& keyIndices) {
    vector<string> opt = vector<string>();
    //cout << keys.size() << endl;
    for (auto key : keyIndices) {
        //cout << key << endl;
        opt.push_back(keys[key].name);
    }
    string indexFilename = KontoIndex::getIndexFilename(filename, opt);
    //cout << indexFilename << endl;
    for (auto index : indices) 
        if (index->getFilename() == indexFilename) return index;
    return nullptr;
}

KontoResult KontoTableFile::setEntryInt(char* record, KontoKeyIndex key, int datum) {
    if (key<0 || key>=keys.size()) return KR_NO_SUCH_COLUMN;
    if (keys[key].type!=KT_INT) return KR_TYPE_NOT_MATCHING;
    char* ptr = record + keys[key].position;
    *((int*)ptr) = datum;
    return KR_OK;
}

KontoResult KontoTableFile::setEntryFloat(char* record, KontoKeyIndex key, double datum) {
    if (key<0 || key>=keys.size()) return KR_NO_SUCH_COLUMN;
    if (keys[key].type!=KT_FLOAT) return KR_TYPE_NOT_MATCHING;
    char* ptr = record + keys[key].position;
    *((double*)ptr) = datum;
    return KR_OK;
}

KontoResult KontoTableFile::setEntryString(char* record, KontoKeyIndex key, const char* data) {
    if (key<0 || key>=keys.size()) return KR_NO_SUCH_COLUMN;
    if (keys[key].type!=KT_STRING) return KR_TYPE_NOT_MATCHING;
    char* ptr = record + keys[key].position;
    strcpy((char*)ptr, data);
    return KR_OK;
}

KontoResult KontoTableFile::setEntryDate(char* record, KontoKeyIndex key, Date datum) {
    if (key<0 || key>=keys.size()) return KR_NO_SUCH_COLUMN;
    if (keys[key].type!=KT_DATE) return KR_TYPE_NOT_MATCHING;
    char* ptr = record + keys[key].position;
    *((Date*)ptr) = datum;
    return KR_OK;
}

bool KontoTableFile::hasPrimaryKey() {
    int bufindex;
    KontoPage metapage = pmgr.getPage(fileID, 0, bufindex);
    return VI(metapage + POS_META_PRIMARYCOUNT) != 0;
}

KontoResult KontoTableFile::insertEntry(char* record, KontoRPos* out) {
    KontoRPos pos;  KontoResult res = insertEntry(&pos);
    if (res != KR_OK) return res;
    char* ptr = getRecordPointer(pos, true);
    memcpy(ptr+8, record+8, recordSize-8);
    if (out) *out = pos;
    return KR_OK;
}

KontoResult KontoTableFile::getKeyNames(const vector<uint>& keyIndices, vector<string>& out) {
    out = vector<string>();
    for (auto key : keyIndices) {
        if (key<0 || key>=keys.size()) return KR_NO_SUCH_COLUMN;
        out.push_back(keys[key].name);
    }
    return KR_OK;
}

KontoResult KontoTableFile::alterAddPrimaryKey(const vector<uint>& primaryKeys) {
    if (hasPrimaryKey()) return KR_PRIMARY_REDECLARATION;
    int bufindex;
    KontoPage metapage = pmgr.getPage(fileID, 0, bufindex);
    KontoPage ptr = metapage + POS_META_PRIMARYCOUNT;
    VIP(ptr) = primaryKeys.size();
    for (auto id : primaryKeys) VIP(ptr) = id;
    pmgr.markDirty(bufindex);
    recreatePrimaryIndex();
    return KR_OK;
}

KontoResult KontoTableFile::alterDropPrimaryKey() {
    if (!hasPrimaryKey()) return KR_NO_PRIMARY;
    int n = indices.size();
    cout << primaryIndex->getFilename() << endl;
    for (int i=0;i<n;i++) 
        if (indices[i]->getFilename() == primaryIndex->getFilename()) {
            indices.erase(indices.begin() + i);
            primaryIndex->drop();
            break;
        }
    int bufindex;
    KontoPage metapage = pmgr.getPage(fileID, 0, bufindex);
    KontoPage ptr = metapage + POS_META_PRIMARYCOUNT;
    VIP(ptr) = 0;
    pmgr.markDirty(bufindex);
    return KR_OK;
}

void KontoTableFile::recreatePrimaryIndex() {
    int bufindex;
    KontoPage metapage = pmgr.getPage(fileID, 0, bufindex);
    KontoPage ptr = metapage + POS_META_PRIMARYCOUNT;
    int n = VIP(ptr);
    if (n==0) return;
    vector<uint> primaryKeys; 
    for (int i=0;i<n;i++) primaryKeys.push_back(VIP(ptr));
    vector<string> primaryKeyNames;
    KontoResult res = getKeyNames(primaryKeys, primaryKeyNames);
    string indexFilename = KontoIndex::getIndexFilename(filename, primaryKeyNames);
    n = indices.size();
    for (int i=0;i<n;i++) {
        if (indices[i]->getFilename() == indexFilename) {
            indices.erase(indices.begin() + i);
            break;
        }
    }
    res = createIndex(primaryKeys, &primaryIndex);
}

KontoResult KontoTableFile::alterAddColumn(const KontoCDef& def) {
    removeIndices();
    KontoTableFile* ret;
    createFile(filename + ".__altertemp", &ret);
    for (auto item : keys) ret->defineField(item);
    KontoCDef newdef(def);
    ret->defineField(newdef);
    uint newKeyId;
    ret->getKeyIndex(newdef.name.c_str(), newKeyId);
    uint pos = ret->keys[newKeyId].position;
    ret->finishDefineField();
    KontoQRes q; allEntries(q);
    char* buffer = new char[ret->getRecordSize()];
    //cout << "query entries " << q.size() << endl;
    for (auto item : q.items) {
        getDataCopied(item, buffer);
        if (VI(buffer + 4) & FLAGS_DELETED) continue;
        if (newdef.defaultValue) memcpy(buffer + pos, newdef.defaultValue, newdef.size);
        ret->insertEntry(buffer, nullptr);
    }
    int bufindex;
    KontoPage metapage = pmgr.getPage(fileID, 0, bufindex);
    KontoPage ptr = metapage + POS_META_PRIMARYCOUNT;
    int n = VIP(ptr);
    vector<uint> primaryKeys; 
    for (int i=0;i<n;i++) primaryKeys.push_back(VIP(ptr));
    close(); remove_file(get_filename(filename));
    ret->close();
    rename_file(get_filename(filename + ".__altertemp"), get_filename(filename));
    // copy
    keys = ret->keys;
    indices.clear();
    recordCount = ret->recordCount;
    recordSize = ret->recordSize;
    pageCount = ret->pageCount;
    fileID = pmgr.getFileManager().openFile(get_filename(filename).c_str());
    metapage = pmgr.getPage(fileID, 0, bufindex);
    ptr = metapage + POS_META_PRIMARYCOUNT;
    VIP(ptr) = primaryKeys.size();
    for (auto id : primaryKeys) VIP(ptr) = id;
    pmgr.markDirty(bufindex);
    recreatePrimaryIndex();
    return KR_OK;
}

KontoResult KontoTableFile::alterDropColumn(string name) {
    removeIndices();
    uint keyId;
    KontoResult res = getKeyIndex(name.c_str(), keyId);
    if (res != KR_OK) return res;
    KontoTableFile* ret;
    createFile(filename + ".__altertemp", &ret);
    for (int i=0;i<keys.size();i++) if (i!=keyId) ret->defineField(keys[i]);
    ret->finishDefineField();
    char* buffer = new char[ret->getRecordSize()];
    char* origin = new char[getRecordSize()];
    KontoQRes q; allEntries(q);
    int pos = keys[keyId].position, size = keys[keyId].size, tot = recordSize;
    assert(tot - size == ret->getRecordSize());
    //cout << "drop : to copy " << q.size() << endl;
    //int c = 0;
    for (auto item : q.items) {
        getDataCopied(item, origin);
        if (VI(origin + 4) & FLAGS_DELETED) continue;
        memcpy(buffer, origin, pos);
        memcpy(buffer + pos, origin + pos + size, recordSize - pos - size);
        ret->insertEntry(buffer, nullptr);
    }
    //cout << "drop: copied " << c << endl;
    int bufindex;
    KontoPage metapage = pmgr.getPage(fileID, 0, bufindex);
    KontoPage ptr = metapage + POS_META_PRIMARYCOUNT;
    bool savePrimaryIndex = true;
    int n = VIP(ptr);
    vector<uint> primaryKeys; 
    for (int i=0;i<n;i++) {
        int k = VIP(ptr); if (k == keyId) {savePrimaryIndex = false; break;}
        primaryKeys.push_back(k > keyId ? (k-1) : k);
    }
    close(); remove_file(get_filename(filename));
    ret->close();
    rename_file(get_filename(filename + ".__altertemp"), get_filename(filename));
    // copy
    keys = ret->keys;
    indices.clear();
    recordCount = ret->recordCount;
    cout << "drop: recordcount " << recordCount << endl;
    recordSize = ret->recordSize;
    pageCount = ret->pageCount;
    fileID = pmgr.getFileManager().openFile(get_filename(filename).c_str());
    metapage = pmgr.getPage(fileID, 0, bufindex);
    ptr = metapage + POS_META_PRIMARYCOUNT;
    VIP(ptr) = primaryKeys.size();
    for (auto id : primaryKeys) VIP(ptr) = id;
    pmgr.markDirty(bufindex);
    recreatePrimaryIndex();
    return KR_OK;
}

KontoResult KontoTableFile::alterChangeColumn(string original, const KontoCDef& newdef) {
    KontoResult res = alterDropColumn(original);
    if (res!=KR_OK) return res;
    res = alterAddColumn(newdef);
    if (res!=KR_OK) return res;
    return KR_OK;
}

void KontoTableFile::rewriteKeyDefinitions() {
    int bufindex;
    KontoPage metapage = pmgr.getPage(fileID, 0, bufindex);
    char* ptr = metapage + POS_FIELDS;
    int fc = VI(metapage + POS_META_FIELDCOUNT);
    for (int i=0;i<fc;i++) {
        KontoCDef& col = keys[i];
        VIP(ptr) = col.type;
        VIP(ptr) = col.size;
        PS(ptr, col.name);
        uint flags = col.nullable * FIELD_FLAGS_NULLABLE;
        VIP(ptr) = flags;
        if (col.defaultValue) PD(ptr, col.defaultValue, col.size);
        else PE(ptr, col.size);
    }
    pmgr.markDirty(bufindex);
}

KontoResult KontoTableFile::alterRenameColumn(string old, string newname) {
    removeIndices();
    uint keyId; 
    KontoResult res = getKeyIndex(old.c_str(), keyId);
    if (res!=KR_OK) return KR_NO_SUCH_COLUMN;
    keys[keyId].name = newname;
    rewriteKeyDefinitions();
    recreatePrimaryIndex();
    return KR_OK;
}

uint KontoTableFile::getIndexCount() {return indices.size();}

KontoIndex* KontoTableFile::getPrimaryIndex() {
    if (hasPrimaryKey()) return primaryIndex;
    else return nullptr;
}

KontoResult KontoTableFile::alterAddForeignKey(string name, const vector<uint>& foreignKeys, string foreignTable, const vector<string>& foreignName) {
    int bufindex;
    KontoPage metapage = pmgr.getPage(fileID, 0, bufindex);
    char* ptr = metapage + POS_META_FOREIGNS;
    int n = VI(ptr); VIP(ptr) = n+1; 
    char buffer[PAGE_SIZE];
    while (n--) {
        int c = VIP(ptr); 
        for (int i=0;i<c;i++) VIP(ptr); 
        CS(buffer, ptr);
        CS(buffer, ptr);
        for (int i=0;i<c;i++) CS(buffer, ptr);
    }
    VIP(ptr) = foreignKeys.size(); 
    for (int i=0;i<foreignKeys.size();i++) {
        int p = foreignKeys[i];
        VIP(ptr) = p;
    }
    PS(ptr, name);
    PS(ptr, foreignTable);
    for (auto& fname: foreignName) {
        //cout << "fname" << fname << endl;
        PS(ptr, fname);
    }
    pmgr.markDirty(bufindex);
    rewriteKeyDefinitions();
    return KR_OK;
}

KontoResult KontoTableFile::alterDropForeignKey(string name) {
    int bufindex;
    KontoPage metapage = pmgr.getPage(fileID, 0, bufindex);
    char* ptr = metapage + POS_META_FOREIGNS;
    int n = VI(ptr); VIP(ptr) = n-1; 
    char buffer[PAGE_SIZE], namebuffer[PAGE_SIZE];
    char* target, *tail; bool flag = false;
    while (n--) {
        char* bef = ptr;
        int c = VIP(ptr); 
        for (int i=0;i<c;i++) VIP(ptr); 
        CS(namebuffer, ptr);
        CS(buffer, ptr);
        for (int i=0;i<c;i++) CS(buffer, ptr);
        if (namebuffer == name) target = bef, tail = ptr, flag = true;
    }
    if (!flag) return KR_NO_SUCH_FOREIGN;
    memmove(target, tail, ptr-tail);
    pmgr.markDirty(bufindex);
    return KR_OK;
}

void KontoTableFile::getPrimaryKeys(vector<uint>& cols) {
    cols.clear();
    int bufindex;
    KontoPage metapage = pmgr.getPage(fileID, 0, bufindex);
    char* ptr = metapage + POS_META_PRIMARYCOUNT;
    int n = VIP(ptr);
    while (n--) cols.push_back(VIP(ptr));
}

void KontoTableFile::getForeignKeys(
    vector<string>& fknames,
    vector<vector<uint>>& cols, 
    vector<string>& foreignTable, 
    vector<vector<string>>& foreignName) 
{
    cols.clear(); foreignTable.clear(); foreignName.clear();
    fknames.clear();
    int bufindex;
    KontoPage metapage = pmgr.getPage(fileID, 0, bufindex);
    char* ptr = metapage + POS_META_FOREIGNS;
    int n = VIP(ptr);
    while (n--) {
        int c = VIP(ptr);
        vector<uint> curcols; curcols.clear();
        for (int i=0;i<c;i++) curcols.push_back(VIP(ptr));
        string fkname; 
        cols.push_back(curcols);
        CS(fkname, ptr); 
        fknames.push_back(fkname);
        string table; 
        CS(table, ptr);
        foreignTable.push_back(table);
        vector<string> foreigncols; foreigncols.clear();
        for (int i=0;i<c;i++) {
            string buf; CS(buf, ptr);
            foreigncols.push_back(buf);
        }
        foreignName.push_back(foreigncols);
    }
}

void KontoTableFile::drop() {
    close();
    remove_file(get_filename(filename));
    for (auto& i : indices) {
        remove_file(get_filename(i->getFilename()));
    }
}

KontoResult KontoTableFile::insert(char* record) {
    KontoResult legal = checkLegal(record);
    if (legal!=KR_OK) return legal;
    KontoRPos pos; 
    insertEntry(record, &pos);
    //cout << "inserted entry, pos=" << pos.page << " " << pos.id << endl;
    insertIndex(pos);
    //cout << "inserted index" << endl;
    //indices[0]->debugPrint();
    return KR_OK;
}

KontoResult KontoTableFile::dropIndex(const vector<uint>& cols) {
    vector<string> opt = vector<string>();
    for (auto key: cols)
        opt.push_back(keys[key].name);
    string indexFilename = KontoIndex::getIndexFilename(filename, opt);
    KontoIndex* ptr = nullptr;
    for (int i=0;i<indices.size();i++) {
        if (indices[i]->getFilename() == indexFilename) {
            ptr = indices[i];
            indices.erase(indices.begin() + i); 
            break;
        }
    }
    if (ptr==nullptr) return KR_NOT_FOUND;
    ptr->drop(); return KR_OK;
}

void KontoTableFile::debugIndex(const vector<uint>& cols) {
    KontoIndex* index = getIndex(cols);
    index->debugPrint();
}

void KontoTableFile::printTableHeader(bool pos) {
    if (pos) {
        cout << "|" << SS(2, "P", true); 
        cout << "|" << SS(4, "I", true);
    }
    for (auto key: keys) {
        int s;
        if (key.type == KT_INT) s = clamp(MIN_INT_WIDTH, MAX_INT_WIDTH, key.name.length());
        else if (key.type == KT_FLOAT) s = clamp(MIN_FLOAT_WIDTH, MAX_FLOAT_WIDTH, key.name.length());
        else if (key.type == KT_STRING) s = clamp(MIN_VARCHAR_WIDTH, MAX_VARCHAR_WIDTH, std::max((uint)key.name.length(), key.size-1));
        else if (key.type == KT_DATE) s = clamp(MIN_DATE_WIDTH, MAX_DATE_WIDTH, key.name.length());
        else assert(false);
        cout << "|" << SS(s, key.name, true);
    }
    cout << "|" << endl;
}

bool KontoTableFile::printTableEntry(const KontoRPos& item, bool pos) {
    char* data = new char[getRecordSize()];
    getDataCopied(item, data);
    if (VI(data+4) & FLAGS_DELETED) return false;
    if (pos) {
        cout << "|" << SS(2, std::to_string(item.page), true);
        cout << "|" << SS(4, std::to_string(item.id), true);
    }
    for (int i=0;i<keys.size();i++) {
        cout << "|";
        int s;
        const KontoCDef& key = keys[i];
        if (key.type == KT_INT) s = clamp(MIN_INT_WIDTH, MAX_INT_WIDTH, key.name.length());
        else if (key.type == KT_FLOAT) s = clamp(MIN_FLOAT_WIDTH, MAX_FLOAT_WIDTH, key.name.length());
        else if (key.type == KT_STRING) s = clamp(MIN_VARCHAR_WIDTH, MAX_VARCHAR_WIDTH, std::max((uint)key.name.length(), key.size-1));
        else if (key.type == KT_DATE) s = clamp(MIN_DATE_WIDTH, MAX_DATE_WIDTH, key.name.length());
        else assert(false);
        cout << SS(s, value_to_string(data + keys[i].position, keys[i].type), true);
    }
    cout << "|" << endl;
    delete[] data;
    return true;
}

void KontoTableFile::printTable(bool meta, bool pos) {
    if (meta) {
        cout << "[TABLE " << filename << "]\n";
        cout << "    recordsize=" << recordSize << endl;
        cout << "    recordcount=" << recordCount << endl;
        cout << "    pagecount=" << pageCount << endl;
    } else cout << "[TABLE]" << endl;
    printTableHeader(pos);
    KontoQRes q; allEntries(q); 
    int total = 0;
    for (auto& item : q.items) 
        if (printTableEntry(item, pos)) total++;
    cout << "Total: " << total << endl;
}

void KontoTableFile::printTable(const KontoQRes& list, bool pos) {
    if (list.size()==0) {cout << TABS[1] << "The result is empty table."; return;}
    //cout << "[TABLE " << filename << "]\n";
    //cout << "     querycount=" << list.size() << endl;
    printTableHeader(pos);
    int total = 0;
    for (auto& item: list.items) 
        if (printTableEntry(item, pos)) total++;
    cout << "Total: " << total << endl;
}

bool _kontoRPosComp(const KontoRPos& a, const KontoRPos& b) {
    return (a.page < b.page || (a.page == b.page && a.id < b.id));
}

void KontoQueryResult::sort() {
    std::sort(items.begin(), items.end(), _kontoRPosComp);
    sorted = true;
}

KontoQRes KontoQueryResult::append(const KontoQRes& b) {
    KontoQRes ret = KontoQRes(*this);
    for (auto& item : b.items) 
        ret.push(item);
    ret.sorted = false;
    return ret;
}

void KontoTableFile::queryEntryInt(const KontoQRes& q, KontoKeyIndex key, OperatorType op, int vi, KontoQRes& ret) {
    switch (op) {
        case OP_EQUAL:        queryEntryInt(q, key, [vi](int p){return p==vi;}, ret); break;
        case OP_NOT_EQUAL:    queryEntryInt(q, key, [vi](int p){return p!=vi && p!=DEFAULT_INT_VALUE;}, ret); break;
        case OP_LESS:         queryEntryInt(q, key, [vi](int p){return p< vi && p!=DEFAULT_INT_VALUE;}, ret); break;
        case OP_LESS_EQUAL   :queryEntryInt(q, key, [vi](int p){return p<=vi && p!=DEFAULT_INT_VALUE;}, ret); break;
        case OP_GREATER      :queryEntryInt(q, key, [vi](int p){return p> vi && p!=DEFAULT_INT_VALUE;}, ret); break;
        case OP_GREATER_EQUAL:queryEntryInt(q, key, [vi](int p){return p>=vi && p!=DEFAULT_INT_VALUE;}, ret); break;
    } 
}

void KontoTableFile::queryEntryInt(const KontoQRes& q, KontoKeyIndex key, OperatorType op, int vl, int vr, KontoQRes& ret) {
    switch (op) {
        case OP_LCRC: queryEntryInt(q, key, [vl, vr](int p){return p>=vl && p<=vr && p!=DEFAULT_INT_VALUE;}, ret); break;
        case OP_LCRO: queryEntryInt(q, key, [vl, vr](int p){return p>=vl && p< vr && p!=DEFAULT_INT_VALUE;}, ret); break;
        case OP_LORC: queryEntryInt(q, key, [vl, vr](int p){return p> vl && p<=vr && p!=DEFAULT_INT_VALUE;}, ret); break;
        case OP_LORO: queryEntryInt(q, key, [vl, vr](int p){return p> vl && p< vr && p!=DEFAULT_INT_VALUE;}, ret); break;
    } 
}

void KontoTableFile::queryEntryFloat(const KontoQRes& q, KontoKeyIndex key, OperatorType op, double vd, KontoQRes& ret) {
    switch (op) {
        case OP_EQUAL:        queryEntryFloat(q, key, [vd](double p){return p==vd && p!=DEFAULT_FLOAT_VALUE;}, ret); break;
        case OP_NOT_EQUAL:    queryEntryFloat(q, key, [vd](double p){return p!=vd && p!=DEFAULT_FLOAT_VALUE;}, ret); break;
        case OP_LESS:         queryEntryFloat(q, key, [vd](double p){return p< vd && p!=DEFAULT_FLOAT_VALUE;}, ret); break;
        case OP_LESS_EQUAL   :queryEntryFloat(q, key, [vd](double p){return p<=vd && p!=DEFAULT_FLOAT_VALUE;}, ret); break;
        case OP_GREATER      :queryEntryFloat(q, key, [vd](double p){return p> vd && p!=DEFAULT_FLOAT_VALUE;}, ret); break;
        case OP_GREATER_EQUAL:queryEntryFloat(q, key, [vd](double p){return p>=vd && p!=DEFAULT_FLOAT_VALUE;}, ret); break;
    } 
}

void KontoTableFile::queryEntryFloat(const KontoQRes& q, KontoKeyIndex key, OperatorType op, double vl, double vr, KontoQRes& ret) {
    switch (op) {
        case OP_LCRC: queryEntryFloat(q, key, [vl, vr](double p){return p>=vl && p<=vr && p!=DEFAULT_FLOAT_VALUE;}, ret); break;
        case OP_LCRO: queryEntryFloat(q, key, [vl, vr](double p){return p>=vl && p< vr && p!=DEFAULT_FLOAT_VALUE;}, ret); break;
        case OP_LORC: queryEntryFloat(q, key, [vl, vr](double p){return p> vl && p<=vr && p!=DEFAULT_FLOAT_VALUE;}, ret); break;
        case OP_LORO: queryEntryFloat(q, key, [vl, vr](double p){return p> vl && p< vr && p!=DEFAULT_FLOAT_VALUE;}, ret); break;
    } 
}

void KontoTableFile::queryEntryString(const KontoQRes& q, KontoKeyIndex key, OperatorType op, const char* vs, KontoQRes& ret) {
    switch (op) {
        case OP_EQUAL:        queryEntryString(q, key, [vs](const char* p){return strcmp(p, vs)==0;}, ret); break;
        case OP_NOT_EQUAL:    queryEntryString(q, key, [vs](const char* p){return strcmp(p, vs)!=0 && strcmp(p, DEFAULT_STRING_VALUE)!=0;}, ret); break;
        case OP_LESS:         queryEntryString(q, key, [vs](const char* p){return strcmp(p, vs)< 0 && strcmp(p, DEFAULT_STRING_VALUE)!=0;}, ret); break;
        case OP_LESS_EQUAL   :queryEntryString(q, key, [vs](const char* p){return strcmp(p, vs)<=0 && strcmp(p, DEFAULT_STRING_VALUE)!=0;}, ret); break;
        case OP_GREATER      :queryEntryString(q, key, [vs](const char* p){return strcmp(p, vs)> 0 && strcmp(p, DEFAULT_STRING_VALUE)!=0;}, ret); break;
        case OP_GREATER_EQUAL:queryEntryString(q, key, [vs](const char* p){return strcmp(p, vs)>=0 && strcmp(p, DEFAULT_STRING_VALUE)!=0;}, ret); break;
    } 
}

void KontoTableFile::queryEntryString(const KontoQRes& q, KontoKeyIndex key, OperatorType op, const char* vl, const char* vr, KontoQRes& ret) {
    switch (op) {
        case OP_LCRC: queryEntryString(q, key, [vl, vr](const char* p)
            {return strcmp(p, vl)>=0 && strcmp(p, vr)<=0 && strcmp(p, DEFAULT_STRING_VALUE)!=0;}, ret); break;
        case OP_LCRO: queryEntryString(q, key, [vl, vr](const char* p)
            {return strcmp(p, vl)>=0 && strcmp(p, vr)< 0 && strcmp(p, DEFAULT_STRING_VALUE)!=0;}, ret); break;
        case OP_LORC: queryEntryString(q, key, [vl, vr](const char* p)
            {return strcmp(p, vl)> 0 && strcmp(p, vr)<=0 && strcmp(p, DEFAULT_STRING_VALUE)!=0;}, ret); break;
        case OP_LORO: queryEntryString(q, key, [vl, vr](const char* p)
            {return strcmp(p, vl)> 0 && strcmp(p, vr)< 0 && strcmp(p, DEFAULT_STRING_VALUE)!=0;}, ret); break;
    }
}

void KontoTableFile::queryEntryDate(const KontoQRes& q, KontoKeyIndex key, OperatorType op, Date vi, KontoQRes& ret) {
    switch (op) {
        case OP_EQUAL:        queryEntryDate(q, key, [vi](Date p){return p==vi;}, ret); break;
        case OP_NOT_EQUAL:    queryEntryDate(q, key, [vi](Date p){return p!=vi && p!=DEFAULT_DATE_VALUE;}, ret); break;
        case OP_LESS:         queryEntryDate(q, key, [vi](Date p){return p< vi && p!=DEFAULT_DATE_VALUE;}, ret); break;
        case OP_LESS_EQUAL   :queryEntryDate(q, key, [vi](Date p){return p<=vi && p!=DEFAULT_DATE_VALUE;}, ret); break;
        case OP_GREATER      :queryEntryDate(q, key, [vi](Date p){return p> vi && p!=DEFAULT_DATE_VALUE;}, ret); break;
        case OP_GREATER_EQUAL:queryEntryDate(q, key, [vi](Date p){return p>=vi && p!=DEFAULT_DATE_VALUE;}, ret); break;
    } 
}

void KontoTableFile::queryEntryDate(const KontoQRes& q, KontoKeyIndex key, OperatorType op, Date vl, Date vr, KontoQRes& ret) {
    switch (op) {
        case OP_LCRC: queryEntryDate(q, key, [vl, vr](Date p){return p>=vl && p<=vr && p!=DEFAULT_DATE_VALUE;}, ret); break;
        case OP_LCRO: queryEntryDate(q, key, [vl, vr](Date p){return p>=vl && p< vr && p!=DEFAULT_DATE_VALUE;}, ret); break;
        case OP_LORC: queryEntryDate(q, key, [vl, vr](Date p){return p> vl && p<=vr && p!=DEFAULT_DATE_VALUE;}, ret); break;
        case OP_LORO: queryEntryDate(q, key, [vl, vr](Date p){return p> vl && p< vr && p!=DEFAULT_DATE_VALUE;}, ret); break;
    } 
}

void KontoTableFile::queryCompare(const KontoQRes& from, 
        KontoKeyIndex k1, KontoKeyIndex k2, 
        OperatorType op, KontoQRes& out)
{
    KontoQRes result; 
    KontoKeyType type = keys[k1].type;
    uint pos1 = keys[k1].position, pos2 = keys[k2].position;
    for (auto& item : from.items) {
        char* ptr = getRecordPointer(item, false);
        if (VI(ptr + 4) & FLAGS_DELETED) continue;
        switch (type) {
            case KT_INT: {
                int v1 = *(int*)(ptr+pos1), v2 = *(int*)(ptr+pos2);
                switch (op) {
                    case OP_EQUAL:        if (v1==v2) result.push(item); break;
                    case OP_NOT_EQUAL:    if (v1!=v2) result.push(item); break;
                    case OP_LESS:         if (v1< v2) result.push(item); break;
                    case OP_LESS_EQUAL   :if (v1<=v2) result.push(item); break;
                    case OP_GREATER      :if (v1> v2) result.push(item); break;
                    case OP_GREATER_EQUAL:if (v1>=v2) result.push(item); break;
                }
                break;
            }
            case KT_FLOAT: {
                double v1 = *(double*)(ptr+pos1), v2 = *(double*)(ptr+pos2);
                switch (op) {
                    case OP_EQUAL:        if (v1==v2) result.push(item); break;
                    case OP_NOT_EQUAL:    if (v1!=v2) result.push(item); break;
                    case OP_LESS:         if (v1< v2) result.push(item); break;
                    case OP_LESS_EQUAL   :if (v1<=v2) result.push(item); break;
                    case OP_GREATER      :if (v1> v2) result.push(item); break;
                    case OP_GREATER_EQUAL:if (v1>=v2) result.push(item); break;
                }
                break;
            }
            case KT_STRING: {
                const char* v1=ptr+pos1, *v2 = ptr+pos2;
                switch (op) {
                    case OP_EQUAL:        if (strcmp(v1,v2)==0) result.push(item); break;
                    case OP_NOT_EQUAL:    if (strcmp(v1,v2)!=0) result.push(item); break;
                    case OP_LESS:         if (strcmp(v1,v2)< 0) result.push(item); break;
                    case OP_LESS_EQUAL   :if (strcmp(v1,v2)<=0) result.push(item); break;
                    case OP_GREATER      :if (strcmp(v1,v2)> 0) result.push(item); break;
                    case OP_GREATER_EQUAL:if (strcmp(v1,v2)>=0) result.push(item); break;
                }
                break;
            }
            case KT_DATE: {
                Date v1 = *(Date*)(ptr+pos1), v2 = *(Date*)(ptr+pos2);
                switch (op) {
                    case OP_EQUAL:        if (v1==v2) result.push(item); break;
                    case OP_NOT_EQUAL:    if (v1!=v2) result.push(item); break;
                    case OP_LESS:         if (v1< v2) result.push(item); break;
                    case OP_LESS_EQUAL   :if (v1<=v2) result.push(item); break;
                    case OP_GREATER      :if (v1> v2) result.push(item); break;
                    case OP_GREATER_EQUAL:if (v1>=v2) result.push(item); break;
                }
                break;
            }
        }
    }
    result.sorted = from.sorted;
    out = result;
}

void KontoTableFile::deletes(const KontoQRes& items) {
    for (auto& item: items.items) {
        deleteEntry(item);
        deleteIndex(item);
    }
}

bool KontoTableFile::checkDeletedFlags(uint flags){
    return flags & FLAGS_DELETED;
}

KontoResult KontoTableFile::checkLegal(char* record, uint checkSingle) {
    KontoRPos pos;
    // check primary key repeat
    if (hasPrimaryKey()) {
        //cout << "chekc primary key" << endl;
        assert(primaryIndex != nullptr);
        if (primaryIndex->queryE(record, pos) == KR_OK) return KR_PRIMARY_REPETITION;
    }
    //cout << "checked primary key" << endl;
    // check nullability
    for (int i=0;i<keys.size();i++) {
        if (checkSingle!=-1 && checkSingle!=i) continue;
        if (keys[i].nullable) continue;
        char* ptr = record + keys[i].position;
        bool flag = false;
        switch (keys[i].type) {
            case KT_INT: flag = *(int*)(ptr) == DEFAULT_INT_VALUE; break;
            case KT_FLOAT: flag = *(double*)(ptr) == DEFAULT_FLOAT_VALUE; break;
            case KT_STRING: flag = strcmp(DEFAULT_STRING_VALUE, ptr) == 0; break;
            case KT_DATE: flag = *(Date*)(ptr) == DEFAULT_DATE_VALUE; break;
            default: assert(false); return KR_OK;
        }
        if (flag) return KR_NULLABLE_FAIL;
    }
    //cout << "checked nullability" << endl;
    // check foreign key
    int bufindex;
    KontoPage metapage = pmgr.getPage(fileID, 0, bufindex);
    char* ptr = metapage + POS_META_FOREIGNS;
    int n = VIP(ptr); 
    //cout << "n = " << n << endl;
    string tableName, constraintName;
    vector<uint> cols; vector<string> foreignNames;
    vector<string> toDrop; toDrop.clear();
    char* target, *tail; bool flag = false;
    while (n--) {
        char* bef = ptr;
        flag = checkSingle == -1;
        int c = VIP(ptr); cols.clear();
        for (int i=0;i<c;i++) {
            cols.push_back(VIP(ptr)); 
            if (cols[cols.size()-1]==checkSingle) flag=true;
        }
        CS(constraintName, ptr);
        CS(tableName, ptr);
        for (int i=0;i<c;i++) {
            string buffer;
            CS(buffer, ptr);
            foreignNames.push_back(buffer);
        }
        //cout << "flag = " << flag << endl;
        if (!flag) continue;
        KontoResult foreignCheck = checkForeignKey(record, cols, tableName, foreignNames);
        if (foreignCheck == KR_FOREIGN_KEY_FAIL) return foreignCheck;
        if (foreignCheck == KR_FOREIGN_TABLE_NONEXIST || foreignCheck == KR_FOREIGN_COLUMN_UNMATCH) 
            toDrop.push_back(constraintName);
    }
    //cout << "checked foreign key" << endl;
    for (auto& item : toDrop) alterDropForeignKey(item);
    return KR_OK;
}

KontoResult KontoTableFile::checkForeignKey(char* record, const vector<uint> cols, 
    const string& tableName, const vector<string>& foreignNames) 
{
    string tableDir = filename.substr(0, filename.find("/"));
    KontoTableFile* handle; 
    if (!file_exist(tableDir, get_filename(tableName))) return KR_FOREIGN_TABLE_NONEXIST;
    //if (filename == tableDir + "/" + tableName) return KR_OK; // do not check self foreign-key
    loadFile(tableDir + "/" + tableName, &handle);
    vector<uint> foreignCols; foreignCols.clear();
    int nCols = cols.size();
    for (int i=0;i<nCols;i++) {
        uint kid;
        KontoResult res = handle->getKeyIndex(foreignNames[i].c_str(), kid);
        if (res != KR_OK) {handle->close(); return KR_FOREIGN_COLUMN_UNMATCH;}
        if (handle->keys[kid].type != keys[cols[i]].type) {handle->close(); return KR_FOREIGN_COLUMN_UNMATCH;}
        foreignCols.push_back(kid);
    }
    handle->close();
    KontoTerminal* term = KontoTerminal::getInstance();
    vector<KontoWhere> wheres; wheres.clear();
    for (int i=0;i<nCols;i++) {
        Token rvalue = Token();
        switch (keys[cols[i]].type) {
            case KT_INT: 
                rvalue.tokenKind = TK_INT_VALUE; 
                rvalue.value = *(int*)(record+keys[cols[i]].position);
                if (rvalue.value == DEFAULT_INT_VALUE) return KR_OK;
                break;
            case KT_FLOAT:
                rvalue.tokenKind = TK_FLOAT_VALUE; 
                rvalue.doubleValue = *(double*)(record+keys[cols[i]].position);
                if (rvalue.doubleValue == DEFAULT_FLOAT_VALUE) return KR_OK;
                break;
            case KT_STRING:
                rvalue.tokenKind = TK_STRING_VALUE; 
                rvalue.identifier = record+keys[cols[i]].position;
                if (rvalue.identifier == DEFAULT_STRING_VALUE) return KR_OK;
                break;
            case KT_DATE:
                rvalue.tokenKind = TK_DATE_VALUE; 
                rvalue.value = *(Date*)(record+keys[cols[i]].position);
                if (rvalue.value == DEFAULT_DATE_VALUE) return KR_OK;
                break;
            default:
                assert(false);
        }
        wheres.push_back(KontoWhere{
            .type = WT_CONST,
            .keytype = keys[cols[i]].type,
            .op = OP_EQUAL,
            .rvalue = rvalue,
            .lvalue = Token(),
            .ltable = tableName,
            .rtable = "",
            .lid = foreignCols[i],
            .rid = 0,
        });
    }
    KontoQRes q;
    //term->printWheres(wheres);
    term->queryWheres(wheres, q);
    if (q.size() == 0) return KR_FOREIGN_KEY_FAIL;
    return KR_OK;
}

KontoResult KontoTableFile::alterRename(string newname) { 
    int bufindex;
    KontoPage metapage = pmgr.getPage(fileID, 0, bufindex);
    char* ptr = metapage + POS_FILENAME;
    PS(ptr, newname);
    pmgr.markDirty(bufindex);
    pmgr.closeFile(fileID);
    rename_file(get_filename(filename), get_filename(newname));
    string fullFilename = get_filename(newname);
    fileID = pmgr.getFileManager().openFile(fullFilename.c_str());
    for (auto& id : indices) {id->renameTable(newname);}
    filename = newname;
}