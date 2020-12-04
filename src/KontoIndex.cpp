#include "KontoIndex.h"
#include "KontoConst.h"
#include <assert.h>
#include <memory.h>

/*
第 0 页，
    第0个uint是key数量（单属性索引为1，联合索引大于1）
    第1个uint是页面个数
    从第256个char开始
        每三个uint，是keytype，keypos，keysize
接下来的所有页面：（第 1 页是根节点）
    第0个uint为子结点个数
    第1个uint为该节点类型，1为内部节点，2为叶节点
    第2个uint为上一个兄弟节点页编号（若不存在则为0）
    第3个uint为下一个兄弟节点页编号
    第4个uint为父节点页编号
    若为内部节点，从第256个char开始：每（1+indexsize）个uint，
        第0个uint为子节点页编号
        后indexsize个int为该子节点中最小键值
    若为叶子节点，从第256个char开始，每（3+indexsize）个uint存储
        对应记录的page和id，是否删除（1或0），和该键值
*/

const uint POS_META_KEYCOUNT    = 0x0000;
const uint POS_META_PAGECOUNT   = 0x0004;
const uint POS_META_KEYFIELDS   = 0x0100;

const uint POS_PAGE_CHILDCOUNT  = 0x0000;
const uint POS_PAGE_NODETYPE    = 0x0004;

const uint NODETYPE_INNER       = 1;
const uint NODETYPE_LEAF        = 2;

const uint POS_PAGE_PREV        = 0x0008;
const uint POS_PAGE_NEXT        = 0x000c;
const uint POS_PAGE_PARENT      = 0x0010;
const uint POS_PAGE_DATA        = 0x0014;

const uint FLAGS_DELETED        = 0x00000001;

const uint SPLIT_UPPERBOUND     = 8192;
//const uint SPLIT_UPPERBOUND = 200;

KontoIndex::KontoIndex():
    pmgr(BufPageManager::getInstance()) {}

KontoResult KontoIndex::createIndex(
    string filename, KontoIndex** handle,
    vector<KontoKeyType> ktypes, vector<uint> kposs, vector<uint> ksizes)
{
    if (handle==nullptr) return KR_NULL_PTR;
    if (ktypes.size() == 0) return KR_EMPTY_KEYLIST;
    KontoIndex* ret = new KontoIndex();
    ret->keyTypes = ktypes; 
    ret->keyPositions = kposs;
    ret->keySizes = ksizes;
    string fullFilename = get_filename(filename);
    ret->pmgr.getFileManager().createFile(fullFilename.c_str());
    ret->fileID = ret->pmgr.getFileManager().openFile(fullFilename.c_str());
    //cout << "INDEX FILEID=" << ret->fileID << endl;
    int bufindex;
    ret->filename = filename;
    KontoPage metapage = ret->pmgr.getPage(ret->fileID, 0, bufindex);
    ret->pmgr.markDirty(bufindex);
    int n = ret->keyPositions.size();
    VI(metapage + POS_META_KEYCOUNT) = n;
    ret->pageCount = VI(metapage + POS_META_PAGECOUNT) = 2;
    ret->indexSize = 0;
    for (int i=0;i<n;i++) {
        VI(metapage + POS_META_KEYFIELDS + i * 12    ) = ret->keyTypes[i];
        VI(metapage + POS_META_KEYFIELDS + i * 12 + 4) = ret->keyPositions[i];
        VI(metapage + POS_META_KEYFIELDS + i * 12 + 8) = ret->keySizes[i];
        ret->indexSize += ret->keySizes[i];
    }
    KontoPage rootpage = ret->pmgr.getPage(ret->fileID, 1, bufindex);
    ret->pmgr.markDirty(bufindex);
    VI(rootpage + POS_PAGE_CHILDCOUNT) = 0;
    VI(rootpage + POS_PAGE_NODETYPE) = NODETYPE_LEAF;
    VI(rootpage + POS_PAGE_PREV) = 0;
    VI(rootpage + POS_PAGE_NEXT) = 0;
    VI(rootpage + POS_PAGE_PARENT) = 0;
    if (handle) *handle = ret;
    return KR_OK;
}

KontoResult KontoIndex::loadIndex(
    string filename, KontoIndex** handle)
{
    //cout << "load index, filename = " << filename << endl;
    if (handle==nullptr) return KR_NULL_PTR;
    KontoIndex* ret = new KontoIndex();
    string fullFilename = get_filename(filename);
    ret->fileID = ret->pmgr.getFileManager().openFile(fullFilename.c_str());
    int bufindex;
    ret->filename = filename;
    KontoPage metapage = ret->pmgr.getPage(ret->fileID, 0, bufindex);
    //cout << "got page" << endl;
    ret->pageCount = VI(metapage + POS_META_PAGECOUNT);
    int n = VI(metapage + POS_META_KEYCOUNT);
    //cout << "page count = " << ret->pageCount << endl;
    //cout << "key count = " << n << endl;
    ret->keyPositions = vector<uint>();
    ret->keySizes = vector<uint>();
    ret->keyTypes = vector<KontoKeyType>();
    ret->indexSize = 0;
    for (int i=0;i<n;i++) {
        ret->keyTypes    .push_back(VI(metapage + POS_META_KEYFIELDS + i * 12));
        ret->keyPositions.push_back(VI(metapage + POS_META_KEYFIELDS + i * 12 + 4));
        ret->keySizes    .push_back(VI(metapage + POS_META_KEYFIELDS + i * 12 + 8));
        ret->indexSize += VI(metapage + POS_META_KEYFIELDS + i * 12 + 8);
    }
    *handle = ret;
    return KR_OK;
}


string KontoIndex::getIndexFilename(const string database, const vector<string> keyNames) {
    //cout << "get index filename" << database << endl;
    string ret = database + ".__index";
    for (auto p : keyNames) {
        ret += "." + p;
    }
    return ret;
}

int KontoIndex::compare(char* d1, char* d2, KontoKeyType type) {
    if (type == KT_INT) {
        int p1 = *((int*)d1);
        int p2 = *((int*)d2);
        if (p1 == p2) return 0;
        if (p1<p2 || p1==DEFAULT_INT_VALUE) return -1;
        if (p1>p2 || p2==DEFAULT_INT_VALUE) return 1;
        return 0;
    } else if (type == KT_FLOAT) {
        double p1 = *((double*)d1);
        double p2 = *((double*)d2);
        if (p1 == p2) return 0;
        if (p1<p2 || p1==DEFAULT_FLOAT_VALUE) return -1;
        if (p1>p2 || p2==DEFAULT_FLOAT_VALUE) return 1;
        return 0;
    } else if (type == KT_STRING) {
        char* p1 = (char*)d1;
        char* p2 = (char*)d2;
        int res = strcmp(p1,p2);
        if (res==0) return 0;
        if (res<0 || strcmp(p1,DEFAULT_STRING_VALUE)==0) return -1;
        if (res>0 || strcmp(p2,DEFAULT_STRING_VALUE)==0) return 1;
        return strcmp(p1, p2);
    } else if (type == KT_DATE) {
        Date p1 = *((Date*)d1);
        Date p2 = *((Date*)d2);
        if (p1 == p2) return 0;
        if (p1<p2 || p1==DEFAULT_DATE_VALUE) return -1;
        if (p1>p2 || p2==DEFAULT_DATE_VALUE) return 1;
        return 0;
    }
    return false;
}

int KontoIndex::compare(char* record, char* index) {
    int n = keySizes.size();
    int indexPos = 0;
    for (int i=0;i<n;i++) {
        int c = compare(record + keyPositions[i], index + indexPos, keyTypes[i]);
        if (c!=0) return c;
        indexPos += keySizes[i];
    }
    return 0;
}

int KontoIndex::compareRecords(char* r1, char* r2) {
    int n = keySizes.size();
    for (int i=0;i<n;i++) {
        int c = compare(r1 + keyPositions[i], r2 + keyPositions[i], keyTypes[i]);
        if (c!=0) return c;
    }
    return 0;
}

void KontoIndex::setKey(char* dest, char* record, const KontoRPos& pos) {
    int n = keySizes.size();
    int indexPos = 0;
    for (int i=0;i<n;i++) {
        for (int j=0;j<keySizes[i];j++)
            dest[12+indexPos+j] = record[keyPositions[i]+j];
        indexPos += keySizes[i];
    }
    VI(dest) = pos.page;
    VI(dest + 4) = pos.id;
    VI(dest + 8) = 0;
}

KontoResult KontoIndex::split(uint pageID) {
    //cout << "split: " << pageID << endl;
    // create a new page
    int oldBufIndex;
    KontoPage oldPage = pmgr.getPage(fileID, pageID, oldBufIndex);
    int newBufIndex;
    KontoPage newPage = pmgr.getPage(fileID, pageCount, newBufIndex);
    // FIXME: 可能出现无法同时读取两个页面的情况吗
    int totalCount = VI(oldPage+POS_PAGE_CHILDCOUNT), splitCount = totalCount / 2;
    // update data in oldpage
    VI(oldPage + POS_PAGE_CHILDCOUNT) = splitCount;
    uint nextPageID = VI(oldPage + POS_PAGE_NEXT);
    uint parentPageID = VI(oldPage + POS_PAGE_PARENT);
    VI(oldPage + POS_PAGE_NEXT) = pageCount;
    pmgr.markDirty(oldBufIndex);
    // update data in newpage
    VI(newPage + POS_PAGE_CHILDCOUNT) = totalCount - splitCount;
    VI(newPage + POS_PAGE_NODETYPE) = VI(oldPage + POS_PAGE_NODETYPE);
    VI(newPage + POS_PAGE_PREV) = pageID;
    VI(newPage + POS_PAGE_NEXT) = nextPageID;
    VI(newPage + POS_PAGE_PARENT) = VI(oldPage + POS_PAGE_PARENT);
    if (VI(newPage + POS_PAGE_NODETYPE) == NODETYPE_INNER) 
        memcpy(
            newPage + POS_PAGE_DATA, 
            oldPage + POS_PAGE_DATA + splitCount * (4+indexSize),
            (totalCount - splitCount) * (4 + indexSize));
    else
        memcpy(
            newPage + POS_PAGE_DATA,
            oldPage + POS_PAGE_DATA + splitCount * (12+indexSize),
            (totalCount - splitCount) * (12 + indexSize));
    char* newPageKey = new char[indexSize];
    if (VI(newPage + POS_PAGE_NODETYPE) == NODETYPE_INNER)
        memcpy(newPageKey, newPage + POS_PAGE_DATA + 4, indexSize);
    else 
        memcpy(newPageKey, newPage + POS_PAGE_DATA + 12, indexSize);
    pmgr.markDirty(newBufIndex);
    //cout << "split: create finished" << endl;
    // debugPrintPage(pageCount);
    // update children
    if (VI(newPage + POS_PAGE_NODETYPE) == NODETYPE_INNER) {
        int childrenPageCount = totalCount - splitCount;
        uint* childrenPageID = new uint[childrenPageCount];
        for (int i=0;i<childrenPageCount;i++) 
            childrenPageID[i] = VI(newPage + POS_PAGE_DATA + i*(4+indexSize));
        int oldPageLastChildrenID = VI(oldPage + POS_PAGE_DATA + (splitCount-1) * (4+indexSize));
        for (int i=0;i<childrenPageCount;i++) {
            int childBufIndex;
            KontoPage childPage = pmgr.getPage(fileID, childrenPageID[i], childBufIndex);
            VI(childPage + POS_PAGE_PARENT) = pageCount;
            pmgr.markDirty(childBufIndex);
        }
        int childBufIndex;
        KontoPage childPage = pmgr.getPage(fileID, oldPageLastChildrenID, childBufIndex);
        VI(childPage + POS_PAGE_NEXT) = 0;
        pmgr.markDirty(childBufIndex);
        childPage = pmgr.getPage(fileID, childrenPageID[0], childBufIndex);
        VI(childPage + POS_PAGE_PREV) = 0;
        pmgr.markDirty(childBufIndex);
        delete[] childrenPageID;
    }
    // update data in nextpage
    if (nextPageID != 0) {
        int nextBufIndex;
        KontoPage nextPage = pmgr.getPage(fileID, nextPageID, nextBufIndex);
        VI(nextPage + POS_PAGE_PREV) = pageCount;
        pmgr.markDirty(nextBufIndex);
    }
    // update data in parentpage
    if (parentPageID != 0) {
        int parentBufIndex;
        KontoPage parentPage = pmgr.getPage(fileID, parentPageID, parentBufIndex);
        // find the pageid
        assert(VI(parentPage + POS_PAGE_NODETYPE) == NODETYPE_INNER);
        int i = 0; 
        //cout << "split: update parent, i = " << i << endl;
        while (VI(parentPage + POS_PAGE_DATA + i * (4+indexSize)) != pageID) i++;
        int childCount = VI(parentPage + POS_PAGE_CHILDCOUNT);
        assert(i<childCount);
        // update
        memmove(
            parentPage + POS_PAGE_DATA + (i+2) * (4+indexSize),
            parentPage + POS_PAGE_DATA + (i+1) * (4+indexSize),
            (childCount - i - 1) * (4 + indexSize)
        );
        memcpy(
            parentPage + POS_PAGE_DATA + (i+1) * (4+indexSize) + 4,
            newPageKey,
            indexSize);
        VI(parentPage + POS_PAGE_DATA + (i+1)*(4+indexSize)) = pageCount;
        VI(parentPage + POS_PAGE_CHILDCOUNT) ++;
        pmgr.markDirty(parentBufIndex);
        pageCount ++; 
        if (POS_PAGE_DATA + (VI(parentPage + POS_PAGE_CHILDCOUNT)+1) * (4+indexSize) >= SPLIT_UPPERBOUND) {
            //cout << "before split: page " << parentPageID << " has " << childCount+1 
            //    << " children " << endl;
            split(parentPageID);
        }
    } else {
        // reload old page, transfer to another new page
        oldPage = pmgr.getPage(fileID, pageID, oldBufIndex);
        char* oldPageKey = new char[indexSize];
        if (VI(oldPage + POS_PAGE_NODETYPE) == NODETYPE_INNER)
            memcpy(oldPageKey, oldPage + POS_PAGE_DATA + 4, indexSize);
        else    
            memcpy(oldPageKey, oldPage + POS_PAGE_DATA + 12, indexSize);
        int anotherBufIndex;
        // anotherPageID = pageCount + 1;
        KontoPage anotherPage = pmgr.getPage(fileID, pageCount + 1, anotherBufIndex);
        memcpy(anotherPage, oldPage, PAGE_SIZE);
        VI(anotherPage + POS_PAGE_PARENT) = 1;
        pmgr.markDirty(anotherBufIndex);
        if (VI(anotherPage + POS_PAGE_NODETYPE) == NODETYPE_INNER) {
            int childrenPageCount = VI(anotherPage + POS_PAGE_CHILDCOUNT);
            uint* childrenPageID = new uint[childrenPageCount];
            for (int i=0;i<childrenPageCount;i++) 
                childrenPageID[i] = VI(anotherPage + POS_PAGE_DATA + i*(4+indexSize));
            for (int i=0;i<childrenPageCount;i++) {
                int childBufIndex;
                KontoPage childPage = pmgr.getPage(fileID, childrenPageID[i], childBufIndex);
                VI(childPage + POS_PAGE_PARENT) = pageCount + 1;
                pmgr.markDirty(childBufIndex);
            }
            delete[] childrenPageID;
        }
        //cout << "split: copied to another page." << endl;
        newPage = pmgr.getPage(fileID, pageCount, newBufIndex);
        VI(newPage + POS_PAGE_PREV) = pageCount + 1;
        VI(newPage + POS_PAGE_PARENT) = 1;
        pmgr.markDirty(newBufIndex);
        //cout << "split: connected new page." << endl;
        // refresh root page
        int rootBufIndex;
        KontoPage rootPage = pmgr.getPage(fileID, 1, rootBufIndex);
        VI(rootPage + POS_PAGE_CHILDCOUNT) = 2;
        VI(rootPage + POS_PAGE_NODETYPE) = NODETYPE_INNER;
        VI(rootPage + POS_PAGE_PREV) = VI(rootPage + POS_PAGE_NEXT) = 0;
        VI(rootPage + POS_PAGE_PARENT) = 0;
        VI(rootPage + POS_PAGE_DATA + 0) = pageCount + 1;
        memcpy(rootPage + POS_PAGE_DATA + 4, oldPageKey, indexSize);
        VI(rootPage + POS_PAGE_DATA + 4 + indexSize) = pageCount;
        memcpy(rootPage + POS_PAGE_DATA + 4 + indexSize + 4, newPageKey, indexSize);
        pmgr.markDirty(rootBufIndex);
        delete[] oldPageKey;
        pageCount += 2;
        //cout << "split: connected root page." << endl;
    }
    // refresh meta page for pagecount
    int metaBufIndex;
    KontoPage metaPage = pmgr.getPage(fileID, 0, metaBufIndex);
    VI(metaPage + POS_META_PAGECOUNT) = pageCount;
    pmgr.markDirty(metaBufIndex);
    delete[] newPageKey;
    //cout << "split: finished." << endl;
    //debugPrint();
    return KR_OK;
}

void KontoIndex::debugPageOne() {
    int bufindex;
    auto page = pmgr.getPage(fileID, 1, bufindex);
        printf("%8x, %8x, %8x, %8x, %8x, %8x, %8x, %8x\n", 
            VI(page), VI(page+4), VI(page+8), VI(page+12), VI(page+16), VI(page+20), VI(page+24), VI(page+28));
}

KontoResult KontoIndex::insertRecur(char* record, const KontoRPos& pos, uint pageID) {
    //cout << "insertRecur pos=(" << pos.page << "," << pos.id << ")" << " pageid=" << pageID << endl; 
    int bufindex;
    KontoPage page = pmgr.getPage(fileID, pageID, bufindex);
    //cout << "fileid = " << fileID << " pageid = " << pageID << endl;
    //cout << "got page = " << page << endl;
    //cout << "bef"; debugPageOne();
    uint nodetype = VI(page + POS_PAGE_NODETYPE);
    uint childcount = VI(page + POS_PAGE_CHILDCOUNT);
    //cout << "got nodetype=" << nodetype << ", childcount=" << childcount << endl;
    assert(nodetype == NODETYPE_INNER || nodetype == NODETYPE_LEAF);
    if (nodetype == NODETYPE_LEAF) {
        int iter = 0;
        //cout << "before iteration" << endl;
        while (true) {
            if (iter>=childcount) break;
            char* indexData = page+POS_PAGE_DATA+iter*(12+indexSize)+12;
            if (compare(record, indexData) < 0) break;
            iter++;
            //cout << "iter = " << iter << endl;
        }
        //cout << "iter = " << iter << " before memmove" << endl;
        memmove(
            page + POS_PAGE_DATA + (iter+1) * (12+indexSize), 
            page + POS_PAGE_DATA + iter * (12+indexSize), 
            (childcount - iter) * (12+indexSize));
        //cout << "set key at " << POS_PAGE_DATA + iter * (12+indexSize) << endl;
        setKey(page + POS_PAGE_DATA + iter * (12+indexSize), record, pos);
        //cout << "aft"; debugPageOne();
        //cout << "childcount = " << childcount << endl;
        VI(page + POS_PAGE_CHILDCOUNT) = ++childcount;
        pmgr.markDirty(bufindex);
        if (POS_PAGE_DATA + (childcount+1) * (12+indexSize) >= SPLIT_UPPERBOUND) { 
            //cout << "before split: page " << pageID << " has " << childcount
            //    << " children. indexsize = " << indexSize << endl; 
            split(pageID);
        }
        //cout << "wrt"; debugPageOne();
    } else {
        int iter = 0;
        while (true) {
            if (iter>=childcount) break;
            char* indexData = page + POS_PAGE_DATA + iter*(4+indexSize)+4;
            if (compare(record, indexData) < 0) break;
            iter++;
        }
        iter--; if (iter<0) iter = 0;
        //cout << "iter = " << iter << " before recur" << endl;
        insertRecur(record, pos, VI(page + POS_PAGE_DATA + iter * (4+indexSize)));
    }
    return KR_OK;
}

KontoResult KontoIndex::insert(char* record, const KontoRPos& pos) {
    return insertRecur(record, pos, 1);
}

KontoResult KontoIndex::queryIposRecur(char* record, KontoIPos& out, uint pageID, bool equal) {
    //cout << "queryIposRecur, pageID=" << pageID << " rec=";
    //debugPrintRecord(record); cout << endl;
    int bufindex;
    KontoPage page = pmgr.getPage(fileID, pageID, bufindex);
    uint nodetype = VI(page + POS_PAGE_NODETYPE);
    uint childcount = VI(page + POS_PAGE_CHILDCOUNT);
    if (nodetype == NODETYPE_LEAF) {
        int iter = 0;
        while (true) {
            if (iter>=childcount) break;
            char* indexData = page+POS_PAGE_DATA+iter*(12+indexSize)+12;
            int compareResult = compare(record, indexData);
            if (equal && compareResult < 0) break;
            else if (!equal && compareResult <= 0) break;
            iter++;
        }
        iter--; if (iter<0) return KR_NOT_FOUND;
        out = KontoIPos(pageID, iter);
        return KR_OK;
    } else {
        int iter = 0;
        while (true) {
            if (iter>=childcount) break;
            char* indexData = page + POS_PAGE_DATA + iter*(4+indexSize)+4;
            int compareResult = compare(record, indexData);
            if (equal && compareResult < 0) break;
            else if (!equal && compareResult <= 0) break;
            iter++;
        }
        iter--; if (iter<0) return KR_NOT_FOUND;
        return queryIposRecur(record, out, VI(page + POS_PAGE_DATA + iter * (4+indexSize)), equal);
    }
    return KR_NOT_FOUND;
}

KontoResult KontoIndex::queryIpos(char* record, KontoIPos& out, bool equal) {
    return queryIposRecur(record, out, 1, equal);
}

KontoResult KontoIndex::queryIposFirstRecur(KontoIPos& out, uint pageID) {
    int bufindex;
    KontoPage page = pmgr.getPage(fileID, pageID, bufindex);
    uint nodetype = VI(page + POS_PAGE_NODETYPE);
    uint childcount = VI(page + POS_PAGE_CHILDCOUNT);
    if (nodetype == NODETYPE_LEAF) {
        if (childcount == 0) return KR_NOT_FOUND;
        out = KontoIPos(pageID, 0);
        return KR_OK;
    } else {
        return queryIposFirstRecur(out, VI(page + POS_PAGE_DATA));
    }
    return KR_NOT_FOUND;
}

KontoResult KontoIndex::queryIposFirst(KontoIPos& out) {
    return queryIposFirstRecur(out, 1);
}

KontoResult KontoIndex::queryIposLastRecur(KontoIPos& out, uint pageID) {
    int bufindex;
    KontoPage page = pmgr.getPage(fileID, pageID, bufindex);
    uint nodetype = VI(page + POS_PAGE_NODETYPE);
    uint childcount = VI(page + POS_PAGE_CHILDCOUNT);
    if (nodetype == NODETYPE_LEAF) {
        if (childcount == 0) return KR_NOT_FOUND;
        out = KontoIPos(pageID, childcount-1);
        return KR_OK;
    } else {
        return queryIposLastRecur(out, VI(page + POS_PAGE_DATA+(childcount-1)*(4+indexSize)));
    }
    return KR_NOT_FOUND;
}

KontoResult KontoIndex::queryIposLast(KontoIPos& out) {
    return queryIposLastRecur(out, 1);
}

bool KontoIndex::isDeleted(const KontoIPos& q) {
    int pageBufIndex;
    KontoPage page = pmgr.getPage(fileID, q.page, pageBufIndex);
    return VI(page + POS_PAGE_DATA + (12+indexSize) * q.id + 8) & FLAGS_DELETED;
}

bool KontoIndex::isNull(const KontoIPos& q) {
    assert(keyPositions.size() == 1);
    int pageBufIndex;
    KontoPage page = pmgr.getPage(fileID, q.page, pageBufIndex);
    char* ptr = page + POS_PAGE_DATA + (12+indexSize)*q.id + 12;
    switch (keyTypes[0]) {
        case KT_INT: return *(int*)(ptr) == DEFAULT_INT_VALUE;
        case KT_FLOAT: return *(double*)(ptr) == DEFAULT_FLOAT_VALUE;
        case KT_STRING: return strcmp(DEFAULT_STRING_VALUE, ptr) == 0;
        case KT_DATE: return *(Date*)(ptr) == DEFAULT_DATE_VALUE;
        default: assert(false); return false;
    }
}

KontoResult KontoIndex::remove(char* record, const KontoRPos& pos) {
    KontoIPos query;
    KontoResult qres = queryIpos(record, query, true);
    if (qres == KR_NOT_FOUND) return KR_NOT_FOUND;
    while (true) {
        if (getRPos(query) == pos) 
            if (!isDeleted(query)) break;
        qres = getPrevious(query);
        if (qres == KR_NOT_FOUND) return KR_NOT_FOUND;
    }
    int pageBufIndex;
    KontoPage page = pmgr.getPage(fileID, query.page, pageBufIndex);
    VI(page + POS_PAGE_DATA + (12+indexSize) * query.id + 8) |= FLAGS_DELETED;
    pmgr.markDirty(pageBufIndex);
    return KR_OK;
}

KontoRPos KontoIndex::getRPos(KontoIPos& pos) {
    int pageBufIndex;
    KontoPage page = pmgr.getPage(fileID, pos.page, pageBufIndex);
    int rpage = VI(page + POS_PAGE_DATA + (12+indexSize) * pos.id + 0);
    int rid = VI(page + POS_PAGE_DATA + (12+indexSize) * pos.id + 4);
    return KontoRPos(rpage, rid);
}

KontoResult KontoIndex::queryLE(char* record, KontoRPos& out) {
    KontoIPos query;
    KontoResult qres = queryIpos(record, query, true);
    if (qres == KR_NOT_FOUND) return KR_NOT_FOUND;
    while (true) {
        if (!isDeleted(query)) break;
        qres = getPrevious(query); 
        if (qres == KR_NOT_FOUND) return KR_NOT_FOUND;
    }
    KontoRPos rpos = getRPos(query);
    out = rpos;
    return KR_OK;
}

KontoResult KontoIndex::queryL(char* record, KontoRPos& out) {
    KontoIPos query;
    KontoResult qres = queryIpos(record, query, false);
    if (qres == KR_NOT_FOUND) return KR_NOT_FOUND;
    while (true) {
        if (!isDeleted(query)) break;
        qres = getPrevious(query); 
        if (qres == KR_NOT_FOUND) return KR_NOT_FOUND;
    }
    KontoRPos rpos = getRPos(query);
    out = rpos;
    return KR_OK;
}

KontoResult KontoIndex::queryE(char* record, KontoRPos& out) {
    KontoIPos query;
    //cout << "before qipos" << endl;
    KontoResult qres = queryIpos(record, query, true);
    if (qres == KR_NOT_FOUND) return KR_NOT_FOUND;
    //cout << "before while" << endl;
    while (true) {
        if (!isDeleted(query)) break;
        qres = getPrevious(query); 
        //cout << "qres = " << query.page << " " << query.id << endl;
        if (qres == KR_NOT_FOUND) return KR_NOT_FOUND;
    }
    char buffer[indexSize];
    int pageBufIndex;
    KontoPage page = pmgr.getPage(fileID, query.page, pageBufIndex);
    page += POS_PAGE_DATA + query.id * (12 + indexSize) + 12;
    memcpy(buffer, page, indexSize);
    if (compare(record, page) == 0) {
        KontoRPos rpos = getRPos(query);
        out = rpos;
        return KR_OK;
    } else return KR_NOT_FOUND;
}

KontoResult KontoIndex::getNext(KontoIPos& pos, KontoIPos& out) {
    //cout << "getnext " << pos.page << " " << pos.id << endl; 
    int pageID = pos.page, id = pos.id;
    int pageBufIndex; KontoPage page;
    page = pmgr.getPage(fileID, pageID, pageBufIndex);
    if (id>=VI(page + POS_PAGE_CHILDCOUNT)-1) {
        while (VI(page + POS_PAGE_NEXT) == 0) {
            pageID = VI(page + POS_PAGE_PARENT);
            if (pageID == 0) return KR_NOT_FOUND;
            page = pmgr.getPage(fileID, pageID, pageBufIndex);
            //cout << "upjump " << pageID << endl;
        }
        pageID = VI(page + POS_PAGE_NEXT);
        //cout << "nextjump " << pageID << endl;
        page = pmgr.getPage(fileID, pageID, pageBufIndex);
        while (VI(page + POS_PAGE_NODETYPE) == NODETYPE_INNER) {
            assert(VI(page + POS_PAGE_CHILDCOUNT) > 0);
            pageID = VI(page + POS_PAGE_DATA);
            page = pmgr.getPage(fileID, pageID, pageBufIndex);
            //cout << "downjump " << pageID << endl;
        }
        out = KontoIPos(pageID, 0);
        return KR_OK;
    } else {
        out = KontoIPos(pageID, id+1);
        return KR_OK;
    }
}

KontoResult KontoIndex::getPrevious(KontoIPos& pos, KontoIPos& out) {
    //cout << "getnext " << pos.page << " " << pos.id << endl; 
    int pageID = pos.page, id = pos.id;
    int pageBufIndex; KontoPage page;
    page = pmgr.getPage(fileID, pageID, pageBufIndex);
    if (id==0) {
        while (VI(page + POS_PAGE_PREV) == 0) {
            pageID = VI(page + POS_PAGE_PARENT);
            if (pageID == 0) return KR_NOT_FOUND;
            page = pmgr.getPage(fileID, pageID, pageBufIndex);
            //cout << "upjump " << pageID << endl;
        }
        pageID = VI(page + POS_PAGE_PREV);
        //cout << "nextjump " << pageID << endl;
        page = pmgr.getPage(fileID, pageID, pageBufIndex);
        while (VI(page + POS_PAGE_NODETYPE) == NODETYPE_INNER) {
            assert(VI(page + POS_PAGE_CHILDCOUNT) > 0);
            pageID = VI(page + POS_PAGE_DATA);
            page = pmgr.getPage(fileID, pageID, pageBufIndex);
            //cout << "downjump " << pageID << endl;
        }
        out = KontoIPos(pageID, VI(page + POS_PAGE_CHILDCOUNT)-1);
        return KR_OK;
    } else {
        out = KontoIPos(pageID, id-1);
        return KR_OK;
    }
}

KontoResult KontoIndex::getNext(KontoIPos& pos) {
    KontoIPos temp; 
    KontoResult r = getNext(pos, temp);
    if (r==KR_OK) pos=temp; 
    return r;
}

KontoResult KontoIndex::getPrevious(KontoIPos& pos) {
    KontoIPos temp; 
    KontoResult r = getPrevious(pos, temp);
    if (r==KR_OK) pos=temp; 
    return r;
}

KontoResult KontoIndex::close() {
    pmgr.closeFile(fileID);
    pmgr.getFileManager().closeFile(fileID);
    return KR_OK;
}

void KontoIndex::debugPrintKey(char* ptr) {
    printf("(");
    int kc = keyPositions.size();
    int indexpos = 0;
    for (int i=0;i<kc;i++) {
        if (i!=0) printf(", ");
        switch (keyTypes[i]) {
            case KT_INT:
                printf("%d", *((int*)(ptr+indexpos))); break;
            case KT_FLOAT:
                printf("%lf", *((double*)(ptr+indexpos))); break;
            case KT_STRING:
                printf("%s", (char*)(ptr+indexpos)); break;
            case KT_DATE:
                cout << value_to_string(ptr+indexpos, KT_DATE); break;
            default:
                printf("BADTYPE");
        }
        indexpos += keySizes[i];
    }
    printf(")");
}

void KontoIndex::debugPrintRecord(char* ptr) {
    printf("(");
    int kc = keyPositions.size();
    for (int i=0;i<kc;i++) {
        if (i!=0) printf(", ");
        switch (keyTypes[i]) {
            case KT_INT:
                printf("%d", *((int*)(ptr+keyPositions[i]))); break;
            case KT_FLOAT:
                printf("%lf", *((double*)(ptr+keyPositions[i]))); break;
            case KT_STRING:
                printf("%s", (char*)(ptr+keyPositions[i])); break;
            case KT_DATE:
                cout << value_to_string(ptr+keyPositions[i], KT_DATE); break;
            default:
                printf("BADTYPE");
        }
    }
    printf(")");
}

void KontoIndex::debugPrint() {
    int metaBufIndex;
    KontoPage metaPage = pmgr.getPage(fileID, 0, metaBufIndex);
    printf("\n========================================================\n");
    printf("=============[(%d) Filename: ", fileID); cout << filename << "]=============" << endl;
    cout << "PageCount = " << pageCount << endl;
    assert(pageCount == VI(metaPage + POS_META_PAGECOUNT));
    printf("KeyCount = %d\n", keyPositions.size());
    assert(keyPositions.size() == VI(metaPage + POS_META_KEYCOUNT));
    printf("IndexSize = %d\n", indexSize);
    printf("Keyfields: \n");
    for (int i=0;i<keyPositions.size(); i++) {
        printf("    [%d] type=%d, pos=%d, size=%d\n",
            i, keyTypes[i], keyPositions[i], keySizes[i]);
        assert(keyTypes[i] == VI(metaPage + POS_META_KEYFIELDS + i * 12 + 0));
        assert(keyPositions[i] == VI(metaPage + POS_META_KEYFIELDS + i * 12 + 4));
        assert(keySizes[i] == VI(metaPage + POS_META_KEYFIELDS + i * 12 + 8));
    }
    printf("\n");
    debugPrintPage(1);
    printf("========== Finished ==========\n");
    printf("==============================\n\n");
}

void KontoIndex::debugPrintPage(int pageID, bool recur) {
    int pageBufIndex;
    KontoPage page = pmgr.getPage(fileID, pageID, pageBufIndex);
    printf("-----[PageId: %d]-----\n", pageID);
    printf("NodeType = %d\n", VI(page + POS_PAGE_NODETYPE));
    printf("ChildCount = %d\n", VI(page + POS_PAGE_CHILDCOUNT));
    printf("PrevBroPage = %d, NextBroPage = %d\n", VI(page + POS_PAGE_PREV), VI(page + POS_PAGE_NEXT));
    printf("ParentPage = %d\n", VI(page + POS_PAGE_PARENT));
    int cnt = VI(page + POS_PAGE_CHILDCOUNT);
    assert(cnt<PAGE_SIZE);
    int type = VI(page + POS_PAGE_NODETYPE);
    for (int i=0;i<cnt;i++) {
        if (type==NODETYPE_INNER) {
            printf("    [%d @ %d] page=%d, key=", i,
                POS_PAGE_DATA + i * (4+indexSize), 
                VI(page + POS_PAGE_DATA + i * (4+indexSize))); 
            debugPrintKey(page + POS_PAGE_DATA + i * (4+indexSize) + 4);
            printf("\n");
        } else {
            printf("    [%d @ %d] rpos=(%d,%d), del=%d, key=", 
                i, 
                POS_PAGE_DATA + i * (12+indexSize), 
                VI(page + POS_PAGE_DATA + i * (12+indexSize)), 
                VI(page + POS_PAGE_DATA + i * (12+indexSize) + 4),
                VI(page + POS_PAGE_DATA + i * (12+indexSize) + 8));
            debugPrintKey(page + POS_PAGE_DATA + i*(12+indexSize) + 12);
            printf("\n");
        }
    }
    printf("\n");
    if (recur && type==NODETYPE_INNER) {
        for (int i=0;i<cnt;i++) 
            debugPrintPage(VI(page + POS_PAGE_DATA + i * (4+indexSize)));
    }
} 

KontoResult KontoIndex::recreate(KontoIndex* original, KontoIndex** handle) {
    original->close();
    string filename = original->filename;
    string fullFilename = get_filename(original->filename);
    remove_file(fullFilename);
    createIndex(filename, handle, original->keyTypes, 
        original->keyPositions, original->keySizes);
    return KR_OK;
}

KontoResult KontoIndex::drop() {
    string fullFilename = get_filename(filename);
    remove_file(fullFilename);
    return KR_OK;
}

KontoResult KontoIndex::queryInterval(char* lower, char* upper, KontoQRes& out,
    bool lowerIncluded, bool upperIncluded, bool filterNull)
{
    if (lower && upper) {
        int comp = compareRecords(lower, upper);
        if (comp > 0 || (comp == 0 && (!lowerIncluded || !upperIncluded)))
            {out = KontoQRes();return KR_OK;} 
    }
    KontoIPos lowerIpos;
    KontoResult result;
    if (lower) {
        result = queryIpos(lower, lowerIpos, !lowerIncluded);
        if (result == KR_NOT_FOUND) 
            result = queryIposFirst(lowerIpos);
        else {
            KontoIPos lowerNext; 
            result = getNext(lowerIpos, lowerNext);
            if (result == KR_NOT_FOUND) {out = KontoQRes();return KR_OK;}
            lowerIpos = lowerNext;
        }
    } else {
        result = queryIposFirst(lowerIpos);
        if (result == KR_NOT_FOUND) {out = KontoQRes();return KR_OK;}
    }
    KontoIPos upperIpos;
    if (upper) {
        result = queryIpos(upper, upperIpos, upperIncluded);
        if (result == KR_NOT_FOUND) {out = KontoQRes();return KR_OK;}
    } else {
        result = queryIposLast(upperIpos);
        if (result == KR_NOT_FOUND) {out = KontoQRes();return KR_OK;}
    }
    KontoQRes ret;
    KontoIPos iterator = lowerIpos;
    while (true) {
        if (!isDeleted(iterator) && (!filterNull || !isNull(iterator)))
            ret.push(getRPos(iterator));
        if (iterator == upperIpos) break;
        KontoIPos temp; getNext(iterator, temp);
        iterator = temp;
    }
    //ret.sort();
    out = ret;
    return KR_OK;
}

string KontoIndex::getFilename() {return filename;}
