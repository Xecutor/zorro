#include "ZMap.hpp"
#include "ZString.hpp"
#include "ZorroVM.hpp"

namespace zorro {

uint32_t ZMap::hashFunc(const Value& key) const
{
    switch(key.vt)
    {
        case vtNil:
            return 0xffffffff;
        case vtBool:
            return key.bValue ? 1u : 0u;
        case vtWeakRef:
        case vtRef:
            return hashFunc(key.valueRef->value);
        case vtDouble:
        case vtInt:
            return static_cast<uint32_t>((key.iValue & 0xffffffff) ^ (key.iValue >> 32));
        case vtString:
            return key.str->getHashCode();
        case vtDelegate:
        {
            intptr_t v = (intptr_t) key.dlg->method;
            v ^= hashFunc(key.dlg->obj);
#if INTPTR_MAX > 0xffffffffu
            return ((v >> 4) ^ (v >> 36)) & 0xffffffff;
#else
            return (v>>4)&0xffffffff;
#endif
        }
        case vtSegment:
        {
            Segment& s = *key.seg;
            if(s.cont.vt == vtString)
            {
                const char* sdata = s.cont.str->getDataPtr();
                int cs = s.cont.str->getCharSize();
                return ZString::calcHash(sdata + s.segStart * cs, sdata + s.segEnd * cs);
            }
        }
            /* no break */
        default:
        {
            intptr_t v = (intptr_t) key.arr;
#if INTPTR_MAX > 0xffffffffu
            return ((v >> 4) ^ (v >> 36)) & 0xffffffff;
#else
            return (v>>4)&0xffffffff;
#endif
        }
    }
}

/*
bool ZMap::isEqual(const Value& argKey1,const Value& argKey2)const
{
  //return ((ZorroVM*)m_mem)->eqMatrix[argKey1.vt][argKey2.vt]((ZorroVM*)m_mem,(Value*)&argKey1,&argKey2);

  const Value* v1=argKey1.vt==vtRef || argKey1.vt==vtWeakRef?&argKey1.valueRef->value:&argKey1;
  const Value* v2=argKey2.vt==vtRef || argKey2.vt==vtWeakRef?&argKey2.valueRef->value:&argKey2;
  if(v1->vt!=v2->vt)
  {
    if(v1->vt==vtSegment && v1->seg->cont.vt==vtString && v2->vt==vtString)
    {
      Segment& s=*v1->seg;
      ZString& s1=*s.cont.str;
      ZString& s2=*v2->str;
      int cs1=s1.getCharSize();
      return ZString::compare(s1.getDataPtr()+s.segStart*cs1,s1.getCharSize(),s1.getDataSize()-s.segEnd*cs1,
                              s2.getDataPtr(),s2.getCharSize(),s2.getDataSize());
    }else if(v2->vt==vtSegment && v2->seg->cont.vt==vtString && v1->vt==vtString)
    {
      Segment& s=*v2->seg;
      ZString& s1=*s.cont.str;
      ZString& s2=*v1->str;
      int cs1=s1.getCharSize();
      return ZString::compare(s1.getDataPtr()+s.segStart*cs1,s1.getCharSize(),s1.getDataSize()-s.segEnd*cs1,
                              s2.getDataPtr(),s2.getCharSize(),s2.getDataSize());
    }
    return false;
  }
  switch(v1->vt)
  {
    case vtNil:return true;
    case vtBool:return v1->bValue==v2->bValue;
    case vtInt:return v1->iValue==v2->iValue;
    case vtDouble:return v1->dValue==v2->dValue;
    case vtString:return *(v1->str)==*(v2->str);
    default:
      return v1->refBase==v2->refBase;
  }
}*/


}
