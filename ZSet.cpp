#include "ZSet.hpp"
#include "ZString.hpp"

namespace zorro {

uint32_t ZSet::hashFunc(const Value& key) const
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
        case vtString:
            return key.str->getHashCode();
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

/*bool ZSet::isEqual(const Value& argKey1,const Value& argKey2)const
{
  const Value* v1=argKey1.vt==vtRef || argKey1.vt==vtWeakRef?&argKey1.valueRef->value:&argKey1;
  const Value* v2=argKey2.vt==vtRef || argKey2.vt==vtWeakRef?&argKey2.valueRef->value:&argKey2;
  if(v1->vt!=v2->vt)
  {
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
