#include "KontoIndex.h"
#include "../KontoConst.h"
#include <assert.h>
#include <memory.h>

/*
第 0 页，
    第0个uint是key数量（单属性索引为1，联合索引大于1）
    第1个uint是页面个数
    从第64个uint开始
        每三个uint，是keytype，keypos，keysize
接下来的所有页面：（第 1 页是根节点）
    第0个uint为子结点个数
    第1个uint为该节点类型，1为内部节点，2为叶节点
    第2个uint为上一个兄弟节点页编号（若不存在则为0）
    第3个uint为下一个兄弟节点页编号
    第4个uint为父节点页编号
    若为内部节点，从第64个uint开始：每（1+indexsize）个uint，
        第0个uint为子节点页编号
        后indexsize个int为该子节点中最小键值
    若为叶子节点，从第64个uint开始，每（3+indexsize）个uint存储
        对应记录的page和id，是否删除（1或0），和该键值
*/

const uint POS_META_KEYCOUNT    = 0x0000;
const uint POS_META_PAGECOUNT   = 0x0001;
const uint POS_META_KEYFIELDS   = 0x0040;

const uint POS_PAGE_CHILDCOUNT  = 0x0000;
const uint POS_PAGE_NODETYPE    = 0x0001;

const uint NODETYPE_INNER       = 1;
const uint NODETYPE_LEAF        = 2;

const uint POS_PAGE_PREV        = 0x0002;
const uint POS_PAGE_NEXT        = 0x0003;
const uint POS_PAGE_PARENT      = 0x0004;
const uint POS_PAGE_DATA        = 0x0040;

const uint FLAGS_DELETED        = 0x00000001;

KontoIndex::KontoIndex(BufPageManager* Pmgr, FileManager* Fmgr):
    pmgr(Pmgr), fmgr(Fmgr) {}

KontoResult KontoIndex::createIndex(
    string filename, KontoIndex** handle, 
    BufPageManager* pManager, 
    FileManager* fManager, 
    vector<KontoKeyType> ktypes, vector<uint> kposs, vector<uint> ksizes)
{
    if (handle==nullptr) return KR_NULL_PTR;
    if (ktypes.size() == 0) return KR_EMPTY_KEYLIST;
    KontoIndex* ret = new KontoIndex(pManager, fManager);
    ret->keyTypes = ktypes; 
    ret->keyPositions = kposs;
    ret->keySizes = ksizes;
    string fullFilename = get_filename(filename);
    if (!ret->fmgr->createFile(fullFilename.c_str())) 
        return KR_FILE_CREATE_FAIL;
    if (!ret->fmgr->openFile(fullFilename.c_str(), ret->fileID)) 
        return KR_FILE_OPEN_FAIL;
    cout << "INDEX FILEID=" << ret->fileID << endl;
    int bufindex;
    ret->filename = filename;
    KontoPage metapage = ret->pmgr->getPage(ret->fileID, 0, bufindex);
    ret->pmgr->markDirty(bufindex);
    int n = ret->keyPositions.size();
    metapage[POS_META_KEYCOUNT] = n;
    ret->pageCount = metapage[POS_META_PAGECOUNT] = 2;
    ret->indexSize = 0;
    for (int i=0;i<n;i++) {
        metapage[POS_META_KEYFIELDS + i * 3    ] = ret->keyTypes[i];
        metapage[POS_META_KEYFIELDS + i * 3 + 1] = ret->keyPositions[i];
        metapage[POS_META_KEYFIELDS + i * 3 + 2] = ret->keySizes[i];
        ret->indexSize += ret->keySizes[i];
    }
    KontoPage rootpage = ret->pmgr->getPage(ret->fileID, 1, bufindex);
    ret->pmgr->markDirty(bufindex);
    rootpage[POS_PAGE_CHILDCOUNT] = 0;
    rootpage[POS_PAGE_NODETYPE] = NODETYPE_LEAF;
    rootpage[POS_PAGE_PREV] = 0;
    rootpage[POS_PAGE_NEXT] = 0;
    rootpage[POS_PAGE_PARENT] = 0;
    *handle = ret;
    return KR_OK;
}

KontoResult KontoIndex::loadIndex(
    string filename, KontoIndex** handle, 
    BufPageManager* pManager, FileManager* fManager)
{
    if (handle==nullptr) return KR_NULL_PTR;
    KontoIndex* ret = new KontoIndex(pManager, fManager);
    string fullFilename = get_filename(filename);
    if (!ret->fmgr->openFile(fullFilename.c_str(), ret->fileID)) return KR_FILE_OPEN_FAIL;
    int bufindex;
    ret->filename = filename;
    KontoPage metapage = ret->pmgr->getPage(ret->fileID, 0, bufindex);
    ret->pageCount = metapage[POS_META_PAGECOUNT];
    int n = metapage[POS_META_KEYCOUNT];
    ret->keyPositions = vector<uint>();
    ret->keySizes = vector<uint>();
    ret->keyTypes = vector<KontoKeyType>();
    ret->indexSize = 0;
    for (int i=0;i<n;i++) {
        ret->keyTypes    .push_back(metapage[POS_META_KEYFIELDS + i * 3]);
        ret->keyPositions.push_back(metapage[POS_META_KEYFIELDS + i * 3 + 1]);
        ret->keySizes    .push_back(metapage[POS_META_KEYFIELDS + i * 3 + 2]);
        ret->indexSize += metapage[POS_META_KEYFIELDS + i * 3 + 2];
    }
    *handle = ret;
    return KR_OK;
}


string KontoIndex::getIndexFilename(string database, vector<string> keyNames) {
    string ret = string(database) + ".index";
    for (auto p : keyNames) {
        ret += "." + p;
    }
    return ret;
}

int KontoIndex::compare(uint* d1, uint* d2, KontoKeyType type) {
    if (type == KT_INT) {
        int p1 = *((int*)d1);
        int p2 = *((int*)d2);
        if (p1<p2) return -1;
        if (p1>p2) return 1;
        return 0;
    } else if (type == KT_FLOAT) {
        double p1 = *((double*)d1);
        double p2 = *((double*)d2);
        if (p1<p2) return -1;
        if (p1>p2) return 1;
        return 0;
    } else if (type == KT_STRING) {
        char* p1 = (char*)d1;
        char* p2 = (char*)d2;
        return strcmp(p1, p2);
    }
    return false;
}

int KontoIndex::compare(uint* record, uint* index) {
    int n = keySizes.size();
    int indexPos = 0;
    for (int i=0;i<n;i++) {
        int c = compare(record + keyPositions[i], index + indexPos);
        if (c!=0) return c;
        indexPos += keySizes[i];
    }
    return 0;
}

void KontoIndex::setKey(uint* dest, uint* record, KontoRPos& pos) {
    int n = keySizes.size();
    int indexPos = 0;
    for (int i=0;i<n;i++) 
        dest[3+indexPos] = record[keyPositions[i]];
    dest[0] = pos.page;
    dest[1] = pos.id;
    dest[2] = 0;
}

KontoResult KontoIndex::split(uint pageID) {
    // create a new page
    int oldBufIndex;
    KontoPage oldPage = pmgr->getPage(fileID, pageID, oldBufIndex);
    int newBufIndex;
    KontoPage newPage = pmgr->getPage(fileID, pageCount, newBufIndex);
    // FIXME: 可能出现无法同时读取两个页面的情况吗
    int totalCount = oldPage[POS_PAGE_CHILDCOUNT], splitCount = totalCount / 2;
    // update data in oldpage
    oldPage[POS_PAGE_CHILDCOUNT] = splitCount;
    uint nextPageID = oldPage[POS_PAGE_NEXT];
    uint parentPageID = oldPage[POS_PAGE_PARENT];
    oldPage[POS_PAGE_NEXT] = pageCount;
    pmgr->markDirty(oldBufIndex);
    // update data in newpage
    newPage[POS_PAGE_CHILDCOUNT] = splitCount - totalCount;
    newPage[POS_PAGE_NODETYPE] = oldPage[POS_PAGE_NODETYPE];
    newPage[POS_PAGE_PREV] = pageID;
    newPage[POS_PAGE_NEXT] = nextPageID;
    newPage[POS_PAGE_PARENT] = oldPage[POS_PAGE_PARENT];
    if (newPage[POS_PAGE_NODETYPE] == NODETYPE_INNER) 
        memcpy(
            newPage + POS_PAGE_DATA, 
            oldPage + POS_PAGE_DATA + splitCount * (1+indexSize),
            (totalCount - splitCount) * (1 + indexSize) * 4);
    else
        memcpy(
            newPage + POS_PAGE_DATA,
            oldPage + POS_PAGE_DATA + splitCount * (3+indexSize),
            (totalCount - splitCount) * (3 + indexSize) * 4);
    uint* newPageKey = new uint[indexSize];
    if (newPage[POS_PAGE_NODETYPE] == NODETYPE_INNER)
        memcpy(newPageKey, newPage + POS_PAGE_DATA + 1, indexSize * 4);
    else 
        memcpy(newPageKey, newPage + POS_PAGE_DATA + 3, indexSize * 4);
    pmgr->markDirty(newBufIndex);
    // update children
    if (newPage[POS_PAGE_NODETYPE] == NODETYPE_INNER) {
        int childrenPageCount = splitCount - totalCount;
        uint* childrenPageID = new uint[childrenPageCount];
        for (int i=0;i<childrenPageCount;i++) 
            childrenPageID[i] = newPage[POS_PAGE_DATA + i*(1+indexSize)];
        int oldPageLastChildrenID = oldPage[POS_PAGE_DATA + (splitCount-1) * (1+indexSize)];
        for (int i=0;i<childrenPageCount;i++) {
            int childBufIndex;
            KontoPage childPage = pmgr->getPage(fileID, childrenPageID[i], childBufIndex);
            childPage[POS_PAGE_PARENT] = pageCount;
            pmgr->markDirty(childBufIndex);
        }
        int childBufIndex;
        KontoPage childPage = pmgr->getPage(fileID, oldPageLastChildrenID, childBufIndex);
        childPage[POS_PAGE_NEXT] = 0;
        pmgr->markDirty(childBufIndex);
        childPage = pmgr->getPage(fileID, childrenPageID[0], childBufIndex);
        childPage[POS_PAGE_PREV] = 0;
        pmgr->markDirty(childBufIndex);
        delete[] childrenPageID;
    }
    // update data in nextpage
    if (nextPageID != 0) {
        int nextBufIndex;
        KontoPage nextPage = pmgr->getPage(fileID, nextPageID, nextBufIndex);
        nextPage[POS_PAGE_PREV] = pageCount;
        pmgr->markDirty(nextBufIndex);
    }
    // update data in parentpage
    if (parentPageID != 0) {
        int parentBufIndex;
        KontoPage parentPage = pmgr->getPage(fileID, parentPageID, parentBufIndex);
        // find the pageid
        assert(parentPage[POS_PAGE_NODETYPE] == NODETYPE_INNER);
        int i = 0; 
        while (parentPage[POS_PAGE_DATA + i * (1+indexSize)] != pageID) i++;
        int childCount = parentPage[POS_PAGE_CHILDCOUNT];
        // update
        memmove(
            parentPage + POS_PAGE_DATA + (i+2) * (1+indexSize),
            parentPage + POS_PAGE_DATA + (i+1) * (1+indexSize),
            (childCount - i - 1) * (1 + indexSize) * 4
        );
        memcpy(
            parentPage + POS_PAGE_DATA + (i+1) * (1+indexSize) + 1,
            newPageKey,
            (1+indexSize) * 4);
        parentPage[POS_PAGE_DATA + (i+1)*(1+indexSize)] = pageCount;
        parentPage[POS_PAGE_CHILDCOUNT] ++;
        pmgr->markDirty(parentBufIndex);
        pageCount ++; 
        if (POS_PAGE_DATA + (parentPage[POS_PAGE_CHILDCOUNT] + 1) * (1+indexSize) >= PAGE_INT_NUM)
            split(parentPageID);
    } else {
        // reload old page, transfer to another new page
        oldPage = pmgr->getPage(fileID, pageID, oldBufIndex);
        uint* oldPageKey = new uint[indexSize];
        if (oldPage[POS_PAGE_NODETYPE] == NODETYPE_INNER)
            memcpy(oldPageKey, oldPage + POS_PAGE_DATA + 1, indexSize * 4);
        else    
            memcpy(oldPageKey, oldPage + POS_PAGE_DATA + 3, indexSize * 4);
        int anotherBufIndex;
        // anotherPageID = pageCount + 1;
        KontoPage anotherPage = pmgr->getPage(fileID, pageCount + 1, anotherBufIndex);
        memcpy(anotherPage, oldPage, PAGE_INT_NUM * 4);
        anotherPage[POS_PAGE_PARENT] = 1;
        pmgr->markDirty(anotherBufIndex);
        if (anotherPage[POS_PAGE_NODETYPE] == NODETYPE_INNER) {
            int childrenPageCount = anotherPage[POS_PAGE_CHILDCOUNT];
            uint* childrenPageID = new uint[childrenPageCount];
            for (int i=0;i<childrenPageCount;i++) 
                childrenPageID[i] = anotherPage[POS_PAGE_DATA + i*(1+indexSize)];
            for (int i=0;i<childrenPageCount;i++) {
                int childBufIndex;
                KontoPage childPage = pmgr->getPage(fileID, childrenPageID[i], childBufIndex);
                childPage[POS_PAGE_PARENT] = pageCount + 1;
                pmgr->markDirty(childBufIndex);
            }
            delete[] childrenPageID;
        }
        newPage = pmgr->getPage(fileID, pageCount, newBufIndex);
        newPage[POS_PAGE_PREV] = pageCount + 1;
        newPage[POS_PAGE_PARENT] = 1;
        pmgr->markDirty(newBufIndex);
        // refresh root page
        int rootBufIndex;
        KontoPage rootPage = pmgr->getPage(fileID, 1, rootBufIndex);
        rootPage[POS_PAGE_CHILDCOUNT] = 2;
        rootPage[POS_PAGE_NODETYPE] = NODETYPE_INNER;
        rootPage[POS_PAGE_PREV] = rootPage[POS_PAGE_NEXT] = 0;
        rootPage[POS_PAGE_PARENT] = 0;
        rootPage[POS_PAGE_DATA + 0] = pageCount + 1;
        memcpy(rootPage + POS_PAGE_DATA + 1, oldPageKey, indexSize * 4);
        rootPage[POS_PAGE_DATA + 1 + indexSize] = pageCount;
        memcpy(rootPage + POS_PAGE_DATA + 2 + indexSize, newPageKey, indexSize * 4);
        pmgr->markDirty(rootBufIndex);
        delete[] oldPageKey;
        pageCount += 2;
    }
    // refresh meta page for pagecount
    int metaBufIndex;
    KontoPage metaPage = pmgr->getPage(fileID, 1, metaBufIndex);
    metaPage[POS_META_PAGECOUNT] = pageCount;
    pmgr->markDirty(metaBufIndex);
    delete[] newPageKey;
    return KR_OK;
}

KontoResult KontoIndex::insertRecur(uint* record, KontoRPos& pos, uint pageID) {
    cout << "insertRecur pos=(" << pos.page << "," << pos.id << ")" << " pageid=" << pageID << endl; 
    int bufindex;
    KontoPage page = pmgr->getPage(fileID, pageID, bufindex);
    cout << "fileid = " << fileID << "pageid = " << pageID << endl;
    cout << "got page = " << page << endl;
    uint nodetype = page[POS_PAGE_NODETYPE];
    uint childcount = page[POS_PAGE_CHILDCOUNT];
    cout << "get nodetype=" << nodetype << ", childcount=" << childcount << endl;
    if (nodetype == NODETYPE_LEAF) {
        uint iter = 0;
        cout << "before iteration" << endl;
        while (true) {
            if (iter>=childcount) break;
            uint* indexData = page+POS_PAGE_DATA+iter*(3+indexSize)+3;
            if (compare(record, indexData) < 0) break;
            iter++;
        }
        cout << "iter = " << iter << " before memmove" << endl;
        memmove(
            page + POS_PAGE_DATA + (iter+1) * (3+indexSize), 
            page + POS_PAGE_DATA + iter * (3+indexSize), 
            (childcount - iter) * (3+indexSize) * 4);
        setKey(page + POS_PAGE_DATA + iter * (3+indexSize), record, pos);
        page[POS_PAGE_CHILDCOUNT] = ++childcount;
        if (POS_PAGE_DATA + childcount * (3+indexSize) >= PAGE_INT_NUM) 
            split(pageID);
    } else {
        uint iter = 0;
        while (true) {
            if (iter>=childcount) break;
            uint* indexData = page + POS_PAGE_DATA + iter*(1+indexSize)+1;
            if (compare(record, indexData) < 0) break;
            iter++;
        }
        iter--; if (iter<0) iter = 0;
        insertRecur(record, pos, page[POS_PAGE_DATA + iter * (1+indexSize)]);
    }
    return KR_OK;
}

KontoResult KontoIndex::insert(uint* record, KontoRPos& pos) {
    return insertRecur(record, pos, 1);
}

KontoResult KontoIndex::queryIposRecur(uint* record, KontoIPos& out, uint pageID, bool equal) {
    int bufindex;
    KontoPage page = pmgr->getPage(fileID, pageID, bufindex);
    uint nodetype = page[POS_PAGE_NODETYPE];
    uint childcount = page[POS_PAGE_CHILDCOUNT];
    if (nodetype == NODETYPE_LEAF) {
        uint iter = 0;
        while (true) {
            if (iter>=childcount) break;
            uint* indexData = page+POS_PAGE_DATA+iter*(3+indexSize)+3;
            int compareResult = compare(record, indexData);
            if (equal && compareResult < 0) break;
            else if (!equal && compareResult <= 0) break;
            iter++;
        }
        iter--; if (iter<0) return KR_NOT_FOUND;
        out = KontoIPos(pageID, iter);
        return KR_OK;
    } else {
        uint iter = 0;
        while (true) {
            if (iter>=childcount) break;
            uint* indexData = page + POS_PAGE_DATA + iter*(1+indexSize)+1;
            int compareResult = compare(record, indexData);
            if (equal && compareResult < 0) break;
            else if (!equal && compareResult <= 0) break;
            iter++;
        }
        iter--; if (iter<0) return KR_NOT_FOUND;
        return queryIposRecur(record, out, page[POS_PAGE_DATA + iter * (1+indexSize)], equal);
    }
    return KR_NOT_FOUND;
}

KontoResult KontoIndex::queryIpos(uint* record, KontoIPos& out, bool equal) {
    return queryIposRecur(record, out, 1, equal);
}

KontoResult KontoIndex::remove(uint* record) {
    KontoIPos query;
    KontoResult qres = queryIpos(record, query, true);
    if (qres == KR_NOT_FOUND) return KR_NOT_FOUND;
    int pageBufIndex;
    KontoPage page = pmgr->getPage(fileID, query.page, pageBufIndex);
    page[POS_PAGE_DATA + (3+indexSize) * query.id + 2] |= FLAGS_DELETED;
    pmgr->markDirty(pageBufIndex);
    return KR_OK;
}

KontoRPos KontoIndex::getRPos(KontoIPos& pos) {
    int pageBufIndex;
    KontoPage page = pmgr->getPage(fileID, pos.page, pageBufIndex);
    int rpage = page[POS_PAGE_DATA + (3+indexSize) * pos.id + 0];
    int rid = page[POS_PAGE_DATA + (3+indexSize) * pos.id + 1];
    return KontoRPos(rpage, rid);
}

KontoResult KontoIndex::queryLE(uint* record, KontoRPos& out) {
    KontoIPos query;
    KontoResult qres = queryIpos(record, query, true);
    if (qres == KR_NOT_FOUND) return KR_NOT_FOUND;
    KontoRPos rpos = getRPos(query);
    out = rpos;
    return KR_OK;
}

KontoResult KontoIndex::queryL(uint* record, KontoRPos& out) {
    KontoIPos query;
    KontoResult qres = queryIpos(record, query, false);
    if (qres == KR_NOT_FOUND) return KR_NOT_FOUND;
    KontoRPos rpos = getRPos(query);
    out = rpos;
    return KR_OK;
}

KontoResult KontoIndex::getNext(KontoIPos& pos, KontoIPos& out) {
    int pageID = pos.page, id = pos.id;
    int pageBufIndex; KontoPage page;
    page = pmgr->getPage(fileID, pageID, pageBufIndex);
    if (id>=page[POS_PAGE_CHILDCOUNT]) {
        while (page[POS_PAGE_NEXT] == 0) {
            pageID = page[POS_PAGE_PARENT];
            if (pageID == 0) return KR_LAST_IPOS;
            page = pmgr->getPage(fileID, pageID, pageBufIndex);
        }
        pageID = page[POS_PAGE_NEXT];
        page = pmgr->getPage(fileID, pageID, pageBufIndex);
        while (page[POS_PAGE_NODETYPE] == NODETYPE_INNER) {
            assert(page[POS_PAGE_CHILDCOUNT] > 0);
            pageID = page[POS_PAGE_DATA];
            page = pmgr->getPage(fileID, pageID, pageBufIndex);
        }
        out = KontoIPos(pageID, 0);
        return KR_OK;
    } else {
        out = KontoIPos(pageID, id+1);
        return KR_OK;
    }
}

KontoResult KontoIndex::close() {
    pmgr->closeFile(fileID);
    return KR_OK;
}

void KontoIndex::debugPrintKey(uint* ptr) {
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
            default:
                printf("BADTYPE");
        }
        indexpos += keySizes[i];
    }
    printf(")");
}

void KontoIndex::debugPrint() {
    int metaBufIndex;
    KontoPage metaPage = pmgr->getPage(fileID, 0, metaBufIndex);
    printf("=============[(%d) Filename: ", fileID); cout << filename << "]=============" << endl;
    printf("PageCount = %d\n", pageCount);
    assert(pageCount == metaPage[POS_META_PAGECOUNT]);
    printf("KeyCount = %d\n", keyPositions.size());
    assert(keyPositions.size() == metaPage[POS_META_KEYCOUNT]);
    printf("IndexSize = %d\n", indexSize);
    printf("Keyfields: \n");
    for (int i=0;i<keyPositions.size(); i++) {
        printf("    [%d] type=%d, pos=%d, size=%d\n",
            i, keyTypes[i], keyPositions[i], keySizes[i]);
        assert(keyTypes[i] == metaPage[POS_META_KEYFIELDS + i * 3 + 0]);
        assert(keyPositions[i] == metaPage[POS_META_KEYFIELDS + i * 3 + 1]);
        assert(keySizes[i] == metaPage[POS_META_KEYFIELDS + i * 3 + 2]);
    }
    printf("\n");
    debugPrintPage(1);
}

void KontoIndex::debugPrintPage(int pageID) {
    int pageBufIndex;
    KontoPage page = pmgr->getPage(fileID, pageID, pageBufIndex);
    printf("-----[PageId: %d]-----\n", pageID);
    printf("NodeType = %d\n", page[POS_PAGE_NODETYPE]);
    printf("ChildCount = %d\n", page[POS_PAGE_CHILDCOUNT]);
    printf("PrevBroPage = %d, NextBroPage = %d\n", page[POS_PAGE_PREV], page[POS_PAGE_NEXT]);
    printf("ParentPage = %d\n", page[POS_PAGE_PARENT]);
    int cnt = page[POS_PAGE_CHILDCOUNT];
    int type = page[POS_PAGE_NODETYPE];
    for (int i=0;i<cnt;i++) {
        if (type==NODETYPE_INNER) {
            printf("    [%d] page=%d, key=", i, page[POS_PAGE_DATA + i * (1+indexSize)]); 
            debugPrintKey(page + POS_PAGE_DATA + i * (1+indexSize) + 1);
            printf("\n");
        } else {
            printf("    [%d] rpos=(%d,%d), del=%d, key=", 
                i, page[POS_PAGE_DATA + i * (3+indexSize)], 
                page[POS_PAGE_DATA + i * (3+indexSize) + 1],
                page[POS_PAGE_DATA + i * (3+indexSize) + 2]);
            debugPrintKey(page + POS_PAGE_DATA + i*(3+indexSize) + 3);
            printf("\n");
        }
    }
    printf("\n");
    if (type==NODETYPE_INNER) {
        for (int i=0;i<cnt;i++) 
            debugPrintPage(page[POS_PAGE_DATA + i * (1+indexSize)]);
    }
} 