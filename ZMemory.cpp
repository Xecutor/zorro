#include "ZorroVM.hpp"
#include <string.h>
#include "kst/RegExp.hpp"
#include "kst/Format.hpp"

namespace zorro {


std::string ZMemory::getUsageReport()
{
    std::string rv;
#define addToReport(poolname) rv+=FORMAT("%s=%d;",#poolname,poolname.getAllocatedCount())
    addToReport(strPool);
    addToReport(segPool);
    addToReport(slcPool);
    addToReport(zaPool);
    addToReport(mapPool);
    addToReport(setPool);
    addToReport(zmnPool);
    addToReport(zmdPool);
    addToReport(zmaPool);
    addToReport(zsnPool);
    addToReport(zsdPool);
    addToReport(zsaPool);
    addToReport(refPool);
    addToReport(wrefPool);
    addToReport(keyRefPool);
    addToReport(rangePool);
    addToReport(objPool);
    addToReport(nobjPool);
    addToReport(membRefPool);
    addToReport(dlgPool);
    addToReport(clsPool);
    addToReport(corPool);
    addToReport(fiterPool);
    addToReport(rxPool);
    return rv;
}

char* ZMemory::allocStr(size_t size)
{
    if(size <= 8)
    {
        return str8.alloc()->buf;
    } else if(size <= 16)
    {
        return str16.alloc()->buf;
    } else if(size <= 32)
    {
        return str32.alloc()->buf;
    } else
    {
        return new char[size];
    }
}

void ZMemory::freeStr(char* str, size_t size)
{
    if(size <= 8)
    {
        StrPoolItem<8>* val = (StrPoolItem<8>*) str;
        str8.free(val);
    } else if(size <= 16)
    {
        StrPoolItem<16>* val = (StrPoolItem<16>*) str;
        str16.free(val);
    } else if(size <= 32)
    {
        StrPoolItem<32>* val = (StrPoolItem<32>*) str;
        str32.free(val);
    } else
    {
        delete[] str;
    }
}


ZMemory::~ZMemory()
{

}

ZArray* ZMemory::allocZArray()
{
    ZArray* rv = zaPool.alloc();
    rv->refCount = 0;
    rv->weakRefId = 0;
    rv->init(this);
    return rv;
}

void ZMemory::freeZArray(ZArray* val)
{
    val->clear();
    zaPool.free(val);
}


ZMap* ZMemory::allocZMap()
{
    ZMap* rv = mapPool.alloc();
    rv->refCount = 0;
    rv->weakRefId = 0;
    rv->m_mem = this;
    return rv;
}

void ZMemory::freeZMap(ZMap* val)
{
    val->clear();
    mapPool.free(val);
}

ZMapNode* ZMemory::allocZMapNode()
{
    return zmnPool.alloc();
}

void ZMemory::freeZMapNode(ZMapNode* val)
{
    zmnPool.free(val);
}


ZMapDataNode* ZMemory::allocZMapDataNode()
{
    ZMapDataNode* rv = zmdPool.alloc();
    rv->m_keyval.m_key.vt = vtNil;
    rv->m_keyval.m_key.flags = 0;
    rv->m_keyval.m_value.vt = vtNil;
    rv->m_keyval.m_value.flags = 0;
    return rv;
}

void ZMemory::freeZMapDataNode(ZMapDataNode* val)
{
    zmdPool.free(val);
}

ZMapDataArrayNode* ZMemory::allocZMapDataArrayNode()
{
    return zmaPool.alloc();
}

void ZMemory::freeZMapDataArrayNode(ZMapDataArrayNode* val)
{
    return zmaPool.free(val);
}


ZSet* ZMemory::allocZSet()
{
    ZSet* rv = setPool.alloc();
    rv->refCount = 0;
    rv->weakRefId = 0;
    rv->m_mem = this;
    return rv;
}

void ZMemory::freeZSet(ZSet* val)
{
    val->clear();
    setPool.free(val);
}

ZSetNode* ZMemory::allocZSetNode()
{
    return zsnPool.alloc();
}

void ZMemory::freeZSetNode(ZSetNode* val)
{
    zsnPool.free(val);
}


ZSetDataNode* ZMemory::allocZSetDataNode()
{
    ZSetDataNode* rv = zsdPool.alloc();
    rv->m_val.vt = vtNil;
    rv->m_val.flags = 0;
    return rv;
}

void ZMemory::freeZSetDataNode(ZSetDataNode* val)
{
    zsdPool.free(val);
}

ZSetDataArrayNode* ZMemory::allocZSetDataArrayNode()
{
    return zsaPool.alloc();
}

void ZMemory::freeZSetDataArrayNode(ZSetDataArrayNode* val)
{
    return zsaPool.free(val);
}


ZString* ZMemory::allocZString()
{
    ZString* rv = strPool.alloc();
    rv->init();
    rv->refCount = 0;
    rv->weakRefId = 0;
    return rv;
}

ZString* ZMemory::allocZString(const char* argStr, uint32_t argLen)
{
    ZString* rv = strPool.alloc();
    rv->assign(this, argStr, argLen);
    rv->refCount = 0;
    rv->weakRefId = 0;
    return rv;
}

Coroutine* ZMemory::allocCoroutine()
{
    Coroutine* rv = corPool.alloc();
    rv->refCount = 0;
    rv->weakRefId = 0;
    return rv;
}

RegExpVal* ZMemory::allocRegExp()
{
    RegExpVal* rv = rxPool.alloc();
    rv->refCount = 0;
    rv->weakRefId = 0;
    rv->val = new kst::RegExp;
    rv->marr = 0;
    rv->marrSize = 0;
    rv->narr = 0;
    rv->narrSize = 0;
    return rv;
}

void ZMemory::freeRegExp(RegExpVal* val)
{
    if(val->marr)
    {
        delete val->marr;
    }
    if(val->narr)
    {
        delete val->narr;
    }
    delete val->val;
    rxPool.free(val);
}


ZStringRef ZMemory::mkZString(const char* str, size_t len)
{
    if(len == (size_t) -1)
    {
        len = strlen(str);
    }
    const char* ptr = str;
    const char* end = str + len;
    bool hasHighBit = false;
    while(ptr < end)
    {
        if((uint8_t) *ptr++ >= 0x80)
        {
            hasHighBit = true;
            break;
        }
    }
    if(!hasHighBit)
    {
        return ZStringRef(this, allocZString(str, len));
    } else
    {
        uint32_t ulen = ZString::calcU8Symbols(str, len);
        char* buf = allocStr(ulen * 2);
        char* bptr = buf;
        const char* send = str + len;
        while(str < send)
        {
            ZString::utf8ToUcs2(str, bptr);
        }
        ZString* zs = allocZString();
        zs->init(buf, ulen * 2, true);
        return ZStringRef(this, zs);
    }
}

ZStringRef ZMemory::mkZString(const uint16_t* str, size_t len)
{
    if(len == (size_t) -1)
    {
        len = 0;
        const uint16_t* ptr = str;
        while(*ptr)
        {
            ++ptr;
            ++len;
        }
    }
    const uint16_t* ptr = str;
    const uint16_t* end = str + len;
    bool hasHighBit = false;
    while(ptr < end)
    {
        if(*ptr >= 128)
        {
            hasHighBit = true;
            break;
        }
        ++ptr;
    }
    ZString* zs = allocZString();
    if(hasHighBit)
    {
        zs->assign(this, str, len * 2);
    } else
    {
        char* astr = this->allocStr(len + 1);
        ptr = str;
        char* dst = astr;
        while(ptr < end)
        {
            *dst = static_cast<char>(*ptr);
            ++ptr;
            ++dst;
        }
        *dst = 0;
        zs->init(astr, len, false);
    }
    return ZStringRef(this, zs);
}


Value* ZMemory::allocVArray(size_t size)
{
    switch(size)
    {
        case 0:
            return NULL;
#define VACASE(n) case n:return val##n.alloc()->buf
        VACASE(1);
        VACASE(2);
        VACASE(3);
        VACASE(4);
        VACASE(5);
        VACASE(6);
        VACASE(7);
        VACASE(8);
        VACASE(9);
        VACASE(10);
        VACASE(11);
        VACASE(12);
        VACASE(13);
        VACASE(14);
        VACASE(15);
        VACASE(16);
#undef VA
        default:
            return new Value[size];
    }
}

void ZMemory::freeVArray(Value* val, size_t size)
{
    switch(size)
    {
        case 0:
            return;
#define VFCASE(n) case n:val##n.free((ValPoolItem<n>*)val);break
        VFCASE(1);
        VFCASE(2);
        VFCASE(3);
        VFCASE(4);
        VFCASE(5);
        VFCASE(6);
        VFCASE(7);
        VFCASE(8);
        VFCASE(9);
        VFCASE(10);
        VFCASE(11);
        VFCASE(12);
        VFCASE(13);
        VFCASE(14);
        VFCASE(15);
        VFCASE(16);
        default:
            delete[] val;
            break;
    }
}

}

