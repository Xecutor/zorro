#ifndef __ZORRO_ZMEMORY_HPP__
#define __ZORRO_ZMEMORY_HPP__

#include "Value.hpp"
#include "Debug.hpp"

namespace zorro {

struct ZMapNode;
struct ZMapDataNode;
struct ZMapDataArrayNode;

struct ZSetNode;
struct ZSetDataNode;
struct ZSetDataArrayNode;

template<int N, class T>
class MemPool {
    struct PoolPage {
        T items[N];

        PoolPage() : next(0)
        {
        }

        PoolPage* next;

        PoolPage(const PoolPage&) = delete;

        PoolPage& operator=(const PoolPage&) = delete;
    };

    PoolPage* lastPage;
    T* cur, * end;
    ListItem* freeItems;
public:
    MemPool()
    {
        lastPage = 0;
        cur = end = 0;
        freeItems = 0;
    }

    ~MemPool()
    {
        PoolPage* ptr = lastPage;
        while(ptr)
        {
            lastPage = ptr->next;
            delete ptr;
            ptr = lastPage;
        }
    }

    T* alloc()
    {
        if(freeItems)
        {
            T* rv = (T*) freeItems;
            //DPRINT("free=%p, next=%p\n",freeItems,freeItems->next);
            freeItems = freeItems->next;
            return rv;
        }
        if(cur == end)
        {
            PoolPage* newPage = new PoolPage;
            newPage->next = lastPage;
            lastPage = newPage;
            cur = lastPage->items;
            end = lastPage->items + N;
        }
        return cur++;
    }

    void free(T* item)
    {
        //DPRINT("free %p\n",item);
        if(!item)
        {
            return;
        }
        ((ListItem*) item)->next = freeItems;
        freeItems = (ListItem*) item;
    }

    int getAllocatedCount()
    {
        if(!lastPage)
        {
            return 0;
        }
        int rv = cur - lastPage->items;
        PoolPage* ptr = lastPage->next;
        while(ptr)
        {
            rv += N;
            ptr = ptr->next;
        }
        ListItem* lst = freeItems;
        while(lst)
        {
            --rv;
            lst = lst->next;
        }
        return rv;
    }
};


class ZMemory {
public:
    virtual ~ZMemory();

    MemPool<256, ZString> strPool;
    MemPool<256, Segment> segPool;
    MemPool<256, Slice> slcPool;
    MemPool<64, ZArray> zaPool;
    MemPool<64, ZMap> mapPool;
    MemPool<64, ZSet> setPool;
    MemPool<256, ZMapNode> zmnPool;
    MemPool<256, ZMapDataNode> zmdPool;
    MemPool<256, ZMapDataArrayNode> zmaPool;
    MemPool<256, ZSetNode> zsnPool;
    MemPool<256, ZSetDataNode> zsdPool;
    MemPool<256, ZSetDataArrayNode> zsaPool;
    MemPool<64, ValueRef> refPool;
    MemPool<64, WeakRef> wrefPool;
    MemPool<64, KeyRef> keyRefPool;
    MemPool<64, Range> rangePool;
    MemPool<512, Object> objPool;
    MemPool<512, NativeObject> nobjPool;
    MemPool<64, MemberRef> membRefPool;
    MemPool<128, Delegate> dlgPool;
    MemPool<16, Closure> clsPool;
    MemPool<8, Coroutine> corPool;
    MemPool<16, ForIterator> fiterPool;
    MemPool<16, RegExpVal> rxPool;

    std::string getUsageReport();

    template<int N>
    struct StrPoolItem {
        char buf[N];
    };
    MemPool<128, StrPoolItem<8>> str8;
    MemPool<128, StrPoolItem<16>> str16;
    MemPool<128, StrPoolItem<32>> str32;

    template<int N>
    struct ValPoolItem {
        Value buf[N];
    };
    MemPool<128, ValPoolItem<1>> val1;
    MemPool<128, ValPoolItem<2>> val2;
    MemPool<128, ValPoolItem<3>> val3;
    MemPool<128, ValPoolItem<4>> val4;
    MemPool<128, ValPoolItem<5>> val5;
    MemPool<128, ValPoolItem<6>> val6;
    MemPool<128, ValPoolItem<7>> val7;
    MemPool<128, ValPoolItem<8>> val8;
    MemPool<128, ValPoolItem<9>> val9;
    MemPool<128, ValPoolItem<10>> val10;
    MemPool<128, ValPoolItem<11>> val11;
    MemPool<128, ValPoolItem<12>> val12;
    MemPool<128, ValPoolItem<13>> val13;
    MemPool<128, ValPoolItem<14>> val14;
    MemPool<128, ValPoolItem<15>> val15;
    MemPool<128, ValPoolItem<16>> val16;


    Segment* allocSegment()
    {
        Segment* rv = segPool.alloc();
        rv->refCount = 0;
        rv->weakRefId = 0;
        return rv;
    }

    void freeSegment(Segment* argSeg)
    {
        segPool.free(argSeg);
    }

    Slice* allocSlice()
    {
        Slice* rv = slcPool.alloc();
        rv->refCount = 0;
        rv->weakRefId = 0;
        return rv;
    }

    void freeSlice(Slice* argSlc)
    {
        slcPool.free(argSlc);
    }

    Value* allocVArray(size_t size);

    void freeVArray(Value* val, size_t size);

    Value** allocVPtrArray(size_t size)
    {
        return new Value* [size];
    }

    void freeVPtrArray(Value** vptr, size_t /*size*/)
    {
        delete[] vptr;
    }

    ZArray* allocZArray();

    void freeZArray(ZArray* val);

    ZMap* allocZMap();

    void freeZMap(ZMap* val);

    ZMapNode* allocZMapNode();

    void freeZMapNode(ZMapNode* val);

    ZMapDataNode* allocZMapDataNode();

    void freeZMapDataNode(ZMapDataNode* val);

    ZMapDataArrayNode* allocZMapDataArrayNode();

    void freeZMapDataArrayNode(ZMapDataArrayNode* val);

    ZSet* allocZSet();

    void freeZSet(ZSet* val);

    ZSetNode* allocZSetNode();

    void freeZSetNode(ZSetNode* val);

    ZSetDataNode* allocZSetDataNode();

    void freeZSetDataNode(ZSetDataNode* val);

    ZSetDataArrayNode* allocZSetDataArrayNode();

    void freeZSetDataArrayNode(ZSetDataArrayNode* val);


    ValueRef* allocRef()
    {
        ValueRef* rv = refPool.alloc();
        rv->refCount = 0;
        rv->weakRefId = 0;
        return rv;
    }

    void freeRef(ValueRef* val)
    {
        refPool.free(val);
    }

    WeakRef* allocWRef()
    {
        WeakRef* rv = wrefPool.alloc();
        rv->refCount = 0;
        rv->weakRefId = 0;
        return rv;
    }

    void freeWRef(WeakRef* val)
    {
        wrefPool.free(val);
    }

    KeyRef* allocKeyRef()
    {
        KeyRef* rv = keyRefPool.alloc();
        rv->refCount = 0;
        rv->weakRefId = 0;
        return rv;
    }

    void freeKeyRef(KeyRef* val)
    {
        keyRefPool.free(val);
    }

    MemberRef* allocMembRef()
    {
        MemberRef* rv = membRefPool.alloc();
        rv->refCount = 0;
        rv->weakRefId = 0;
        return rv;
    }

    void freeMembRef(MemberRef* val)
    {
        membRefPool.free(val);
    }

    Range* allocRange()
    {
        Range* rv = rangePool.alloc();
        rv->refCount = 0;
        rv->weakRefId = 0;
        return rv;
    }

    void freeRange(Range* val)
    {
        rangePool.free(val);
    }

    ZString* allocZString();

    ZString* allocZString(const char* argStr, uint32_t argLen = (uint32_t) -1);

    void freeZString(ZString* val)
    {
        strPool.free(val);
    }

    char* allocStr(size_t size);/*
  {
    return new char[size];
  }*/
    void freeStr(char* str, size_t size);/*
  {
    //DPRINT("freestr:%s/%p\n",str,str);
    delete [] str;
  }*/

    Object* allocObj()
    {
        Object* rv = objPool.alloc();
        rv->refCount = 0;
        rv->weakRefId = 0;
        return rv;
    }

    void freeObj(Object* val)
    {
        objPool.free(val);
    }

    NativeObject* allocNObj()
    {
        NativeObject* rv = nobjPool.alloc();
        rv->refCount = 0;
        rv->weakRefId = 0;
        return rv;
    }


    void freeNObj(NativeObject* val)
    {
        nobjPool.free(val);
    }


    Delegate* allocDlg()
    {
        Delegate* rv = dlgPool.alloc();
        rv->refCount = 0;
        rv->weakRefId = 0;
        return rv;
    }

    void freeDlg(Delegate* dlg)
    {
        dlgPool.free(dlg);
    }

    Closure* allocClosure()
    {
        Closure* rv = clsPool.alloc();
        rv->refCount = 0;
        rv->weakRefId = 0;
        return rv;
    }

    void freeClosure(Closure* val)
    {
        clsPool.free(val);
    }

    Coroutine* allocCoroutine();

    void freeCoroutine(Coroutine* val)
    {
        corPool.free(val);
    }

    ForIterator* allocForIterator()
    {
        ForIterator* rv = fiterPool.alloc();
        rv->refCount = 0;
        rv->weakRefId = 0;
        return rv;
    }

    void freeForIterator(ForIterator* val)
    {
        fiterPool.free(val);
    }

    RegExpVal* allocRegExp();

    void freeRegExp(RegExpVal* val);

    virtual void assign(Value& dst, const Value& src) = 0;

    virtual void unref(Value& dst) = 0;

    virtual bool isEqual(const Value& dst, const Value& src) = 0;

    ZStringRef mkZString(const char* str, size_t len = (size_t) -1);

    ZStringRef mkZString(const uint16_t* str, size_t len = (size_t) -1);

    virtual Value mkWeakRef(Value*) = 0;
};

}

#endif
