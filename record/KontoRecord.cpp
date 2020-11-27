#include "KontoRecord.h"
#include <string.h>
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
  * 2560开始，
    主键域的个数（若无主键置零）
    各个主键的编号
  * 3072开始，存储外键：
    * 外键个数
      * 每个外键的列数，以及对应哪些列，fkname, foreigntablename, foreignnames
  * 4096位开始为每页的信息：当前页中已有的记录数（包括已删除的）
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
const uint POS_META_PRIMARYCOUNT = 0x00000a00;
const uint POS_META_PRIMARIES    = 0x00000a04;
const uint POS_META_FOREIGNS     = 0x00000c00;
const uint POS_PAGES             = 0x00001000;

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
    ret->pageCount = VI(metapage + POS_META_PAGECOUNT) = 1;
    ret->recordCount = VI(metapage + POS_META_RECORDCOUNT) = 0;
    VI(metapage + POS_META_EXISTCOUNT) = 0;
    ret->recordSize = VI(metapage + POS_META_RECORDSIZE) = 8; // 仅包括rid和控制位两个uint 
    VI(metapage + POS_META_FIELDCOUNT) = 0;
    VI(metapage + POS_META_PRIMARYCOUNT) = 0;
    VI(metapage + POS_META_FOREIGNS) = 0;
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
        //域类型，域长度，域名称，域flags，默认值，若为外键还包括外键表名和列名
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
        //域类型，域长度，域名称，域flags，默认值，若为外键还包括外键表名和列名
        //域flags从最低位开始：可空、是否外键
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

char* KontoTableFile::getRecordPointer(KontoRPos& pos, bool write) {
    int wrpid;
    KontoPage wr = pmgr.getPage(fileID, pos.page, wrpid);
    char* ptr = wr + pos.id * recordSize;
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
        int rc = VI(meta + POS_PAGES + 4*(i-1));
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
    cout << "[TABLE NAME = " << filename << "]\n";
    cout << "    recordsize=" << recordSize << endl;
    cout << "    recordcount=" << recordCount << endl;
    cout << "    pagecount=" << pageCount << endl;
    cout << "    FIELDS:" << endl;
    for (auto key : keys) {
        cout << "        [" << key.name << "]" << " type=" << key.type << " size=" << key.size
        << " pos=" << key.position << endl;
    }
    KontoQRes q; allEntries(q); 
    int i = 0;
    for (auto item : q.items) {
        cout << "   (" << i << ") "; printRecord(item, true); cout << endl;
        i++;
    }
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
        cout << "loaded : " << indexFilename << endl;
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

KontoResult KontoTableFile::insertIndex(KontoRPos& pos) {
    char* data = new char[recordSize];
    getDataCopied(pos, data);
    for (auto& index : indices) {
        cout << "insert into: " << index->getFilename() << endl;
        index->insert(data, pos);
        //index->debugPrint();
    }
    delete[] data;
    return KR_OK;
}

KontoResult KontoTableFile::insertIndex(KontoRPos& pos, KontoIndex* dest) {
    char* data = new char[recordSize];
    getDataCopied(pos, data);
    dest->insert(data, pos);
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

void KontoTableFile::printRecord(KontoRPos& pos, bool printPos) {
    char* data = new char[getRecordSize()];
    getDataCopied(pos, data);
    if (printPos) {
        cout << "[POS " << pos.page << ":" << pos.id << "] ";
    }
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

bool KontoTableFile::hasPrimaryKey() {
    int bufindex;
    KontoPage metapage = pmgr.getPage(fileID, 0, bufindex);
    return VI(metapage + POS_META_PRIMARYCOUNT) != 0;
}

KontoResult KontoTableFile::insertEntry(char* record, KontoRPos* out) {
    KontoRPos pos;  KontoResult res = insertEntry(&pos);
    if (res != KR_OK) return res;
    char* ptr = getRecordPointer(pos, true);
    memcpy(ptr+8, record+8, recordSize);
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
        cout << "fname" << fname << endl;
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
    KontoRPos pos; 
    if (hasPrimaryKey()) {
        
    }
    insertEntry(record, &pos);
    insertIndex(pos);
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

void KontoTableFile::printTable(bool meta, bool pos) {
    cout << "[TABLE " << filename << "]\n";
    if (meta) {
        cout << "    recordsize=" << recordSize << endl;
        cout << "    recordcount=" << recordCount << endl;
        cout << "    pagecount=" << pageCount << endl;
    }
    if (pos) {
        cout << "|" << SS(2, "P", true); 
        cout << "|" << SS(4, "I", true);
    }
    vector<uint> sz; sz.clear();
    for (auto key: keys) {
        if (key.type == KT_INT) sz.push_back(7);
        else if (key.type == KT_FLOAT) sz.push_back(9);
        else if (key.type == KT_STRING) sz.push_back(key.size > 20 ? 20 : (key.size-1));
        else sz.push_back(8);
        cout << "|" << SS(sz[sz.size()-1], key.name, true);
    }
    cout << "|" << endl;
    KontoQRes q; allEntries(q); 
    char* data = new char[getRecordSize()];
    for (auto item : q.items) {
        if (pos) {
            cout << "|" << SS(2, std::to_string(item.page), true);
            cout << "|" << SS(4, std::to_string(item.id), true);
        }
        getDataCopied(item, data);
        for (int i=0;i<keys.size();i++) {
            cout << "|";
            switch (keys[i].type) {
                case KT_INT: {
                    int vi = *((int*)(data + keys[i].position));
                    cout << SS(sz[i], std::to_string(vi), true); break;
                }
                case KT_FLOAT: {
                    double vd = *((double*)(data + keys[i].position));
                    cout << SS(sz[i], std::to_string(vd), true); break;
                }
                case KT_STRING: {
                    char* vs = (char*)(data+keys[i].position);
                    cout << SS(sz[i], vs, true); break;
                }
                default: {
                    cout << SS(sz[i], "unknown"); break;
                }
            }
        }
        cout << "|" << endl;
    }
    delete[] data;
}