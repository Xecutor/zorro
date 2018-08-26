#define __STDC_FORMAT_MACROS

#include "ZorroVM.hpp"
#include <kst/RegExp.hpp>
#include <stdexcept>
#include "ZVMOps.hpp"
#include "ZBuilder.hpp"
#include <math.h>

#ifdef _MSC_VER
#define snprintf _snprintf
#endif


namespace zorro {

#define INITCALL(vm, cop, ret, argsCount, selfVal, clsVal, overVal) \
    CallFrame* cf=vm->ctx.callStack.push();\
    cf->callerOp=(OpDstBase*)cop;\
    cf->retOp=ret;\
    cf->localBase=vm->ctx.dataStack.size()-argsCount;\
    cf->args=argsCount;\
    cf->funcIdx=0;\
    cf->selfCall=selfVal;\
    cf->closedCall=clsVal;\
    cf->overload=overVal

#define INITOCALL(vm, argsCount, selfVal, clsVal, overVal) \
    CallFrame* cf=vm->ctx.callStack.push();\
    cf->callerOp=(OpDstBase*)vm->ctx.lastOp;\
    cf->retOp=vm->ctx.nextOp;\
    cf->localBase=vm->ctx.dataStack.size()-argsCount;\
    cf->args=argsCount;\
    cf->funcIdx=0;\
    cf->selfCall=selfVal;\
    cf->closedCall=clsVal;\
    cf->overload=overVal


#define GETDST(da) (da.at==atStack?&(*vm->ctx.dataStack.push()=NilValue):(vm->ctx.dataPtrs[da.at]+da.idx))
#define DSTARG  OpArg dstArg=((OpDstBase*)vm->ctx.lastOp)->dst

#define OVERGUARD(stmt) {OpBase* oldNext=vm->ctx.nextOp;stmt;if(oldNext!=vm->ctx.nextOp && vm->ctx.callStack.stackTop->overload)return;}

static void throwOutOfBounds(ZorroVM* vm, const std::string& msg, int64_t idx, int64_t maxSize)
{
    if(idx < 0)
    {
        ZTHROWR(OutOfBoundsException, vm, "%s: %{} <0", msg, idx);
    } else
    {
        ZTHROWR(OutOfBoundsException, vm, "%s: %{} >= %{}", msg, idx, maxSize);
    }
}

std::string ValueToString(ZorroVM* vm, const Value& v)
{
    char buf[64];
    switch((ValueType) v.vt)
    {
        case vtNil:
            return "nil";
        case vtBool:
            return v.bValue ? "true" : "false";
        case vtInt:
            return FORMAT("%{}", v.iValue);
        case vtDouble:
            snprintf(buf, 64, "%lf", v.dValue);
            return buf;
        case vtString:
        {
            std::string rv = v.str->c_str(vm);
            return rv;
        }
            break;
        case vtArray:
        {
            std::string rv = "[";
            ZArray& za = *v.arr;
            for(size_t i = 0; i < za.getCount(); ++i)
            {
                if(i != 0)
                {
                    rv += ",";
                }
                rv += ValueToString(vm, za.getItem(i));
            }
            rv += "]";
            return rv;
        }
        case vtSegment:
        {
            Segment& seg = *v.seg;
            std::string rv;
            if(seg.cont.vt == vtArray)
            {
                rv = "[";
                ZArray& za = *seg.cont.arr;
                int64_t start = seg.segStart;
                if(start < 0)
                {
                    start = 0;
                }
                if(start < (int64_t) za.getCount())
                {
                    int64_t end = seg.segEnd;
                    if(end >= (int64_t) za.getCount())
                    {
                        end = za.getCount();
                    }
                    if(seg.step > 0)
                    {
                        for(auto i = static_cast<size_t>(start); i < static_cast<size_t>(end); ++i)
                        {
                            if(i != (size_t) start)
                            {
                                rv += ",";
                            }
                            rv += ValueToString(vm, za.getItem(i));
                        }
                    } else
                    {
                        for(int64_t i = end - 1; i >= start; --i)
                        {
                            if(i != end - 1)
                            {
                                rv += ",";
                            }
                            rv += ValueToString(vm, za.getItem((size_t) i));
                        }
                    }
                }
                rv += "]";
            } else if(seg.cont.vt == vtString)
            {
                ZString& str = *seg.cont.str;
                int64_t start = seg.segStart;
                if(start < 0)
                {
                    start = 0;
                }
                if(start < str.getLength())
                {
                    int64_t end = seg.segEnd;
                    if(end >= str.getLength())
                    {
                        end = str.getLength();
                    }
                    if(seg.step > 0)
                    {
                        rv.assign(str.c_substr(vm, static_cast<uint32_t>(start), static_cast<uint32_t>(end - start)));
                    } else
                    {
                        std::string tmp = str.c_substr(vm, static_cast<uint32_t>(start),
                                                       static_cast<uint32_t>(end - start));
                        if(!tmp.empty())
                        {
                            //i will wrap after reaching 0
                            for(size_t i = tmp.length() - 1; i < tmp.length(); --i)
                            {
                                rv += tmp[i];
                            }
                        }
                    }
                }
            }
            return rv;
        }
        case vtRange:
        {
            if(v.range->step == 1)
            {
                return FORMAT("%{}..%{}", v.range->start, v.range->end);
            } else
            {
                return FORMAT("%{}..%{}:%{}", v.range->start, v.range->end, v.range->step);
            }
        }
        case vtForIterator:
            return "for iterator";
        case vtMap:
        {
            std::string rv;
            rv = "{";
            ZMap& zm = *v.map;
            if(zm.empty())
            {
                rv += "=>";
            }
            bool first = true;
            for(auto& it : zm)
            {
                if(first)
                {
                    first = false;
                } else
                {
                    rv += ",";
                }
                rv += ValueToString(vm, it.m_key);
                rv += "=>";
                rv += ValueToString(vm, it.m_value);
            }
            rv += "}";
            return rv;
        }
        case vtLvalue:
            return "lvalue:" + ValueToString(vm, vm->getLValue(const_cast<Value&>(v)));
        case vtUser:
            return "unimplemented:user";
        case vtSet:
        {
            std::string rv;
            rv = "{";
            ZSet& zs = *v.set;
            bool first = true;
            for(auto& z : zs)
            {
                if(first)
                {
                    first = false;
                } else
                {
                    rv += ",";
                }
                rv += ValueToString(vm, z);
            }
            rv += "}";
            return rv;
        }
        case vtRef:
            return "ref:" + ValueToString(vm, v.valueRef->value);
        case vtWeakRef:
            return "weak:" + ValueToString(vm, v.valueRef->value);
        case vtMethod:
            snprintf(buf, sizeof(buf), "method %p/%u", v.func->entry, static_cast<unsigned int>(v.func->index));
            return buf;
        case vtFunc:
            snprintf(buf, sizeof(buf), "func %p/%u", v.func->entry, static_cast<unsigned int>(v.func->index));
            return buf;
        case vtKeyRef:
            return "key";
        case vtMemberRef:
            return "property";
        case vtCFunc:
            return "cfunc";
        case vtCMethod:
            return "cmethod";
        case vtClass:
            return "class";
        case vtSlice:
        {
            Slice& slc = *v.slice;
            Value& cnt = vm->getValue(slc.cont);
            ZArray& idx = *slc.indeces.arr;
            if(cnt.vt == vtString)
            {
                std::string rv;
                size_t sz = idx.getCount();
                const char* str = cnt.str->c_str(vm);
                size_t l = cnt.str->getLength();
                for(size_t i = 0; i < sz; ++i)
                {
                    Value& idxVal = vm->getValue(idx.getItem(i));
                    size_t rIdx;
                    if(idxVal.vt == vtInt)
                    {
                        rIdx = static_cast<size_t>(idxVal.iValue);
                    } else
                    {
                        ZTHROW0(ZorroException, "ValueToString: invalid string index value type %{} in slice",
                                getValueTypeName(idxVal.vt));
                    }
                    if(rIdx >= l)
                    {
                        ZTHROW0(ZorroException, "ValueToString: string index is out of bounds: %{} >= ${} ", rIdx, l);
                    }
                    rv += str[rIdx];
                }
                return rv;
            } else if(cnt.vt == vtMap)
            {
                std::string rv = "[";
                size_t sz = idx.getCount();
                ZMap& zm = *cnt.map;
                for(size_t i = 0; i < sz; ++i)
                {
                    Value& idxVal = vm->getValue(idx.getItem(i));
                    ZMap::iterator it = zm.find(idxVal);
                    if(it == zm.end())
                    {
                        ZTHROW0(ZorroException, "ValueToString: non existing map key: %{}", ValueToString(vm, idxVal));
                    }
                    if(i != 0)
                    {
                        rv += ",";
                    }
                    rv += ValueToString(vm, it->m_value);
                }
                rv += "]";
                return rv;
            }
            if(cnt.vt == vtArray)
            {
                std::string rv = "[";
                size_t sz = idx.getCount();
                ZArray& za = *cnt.arr;
                size_t l = za.getCount();
                for(size_t i = 0; i < sz; ++i)
                {
                    Value& idxVal = vm->getValue(idx.getItem(i));
                    size_t rIdx;
                    if(idxVal.vt == vtInt)
                    {
                        rIdx = static_cast<size_t>(idxVal.iValue);
                    } else
                    {
                        ZTHROW0(ZorroException, "ValueToString: invalid array index value type %{} in slice",
                                getValueTypeName(idxVal.vt));
                    }
                    if(rIdx >= l)
                    {
                        ZTHROW0(ZorroException, "ValueToString: array index is out of bounds: %{} >= ${} ", rIdx, l);
                    }
                    if(i != 0)
                    {
                        rv += ",";
                    }
                    rv += ValueToString(vm, za.getItem(rIdx));
                }
                rv += "]";
                return rv;
            } else
            {
                return "unsupported slice";
            }
        }
        case vtObject:
            return FORMAT("object-%{}", (void*) v.obj);
        case vtNativeObject:
            return "native object";
        case vtDelegate:
            return "delegate";
        case vtCDelegate:
            return "cdelegate";
        case vtClosure:
            return "closure";
        case vtCoroutine:
            return "coroutine";
        case vtRegExp:
            return FORMAT("regexp-%{}", (void*) v.regexp);
        case vtCount:
            break;
    }
    return "";
}


Value StringValue(ZStringRef& str, bool isConst)
{
    Value rv;
    rv.vt = vtString;
    rv.str = str.get();
    rv.flags = isConst ? ValFlagConst : ValFlagNone;
    return rv;
}

Value StringValue(ZString* str, bool isConst)
{
    Value rv;
    rv.vt = vtString;
    rv.str = str;
    rv.flags = isConst ? ValFlagConst : ValFlagNone;
    return rv;
}

Value ArrayValue(ZArray* arr, bool isConst)
{
    Value rv;
    rv.vt = vtArray;
    rv.arr = arr;
    rv.flags = isConst ? ValFlagConst : ValFlagNone;
    return rv;
}

Value RangeValue(Range* rng, bool isConst)
{
    Value rv;
    rv.vt = vtRange;
    rv.range = rng;
    rv.flags = isConst ? ValFlagConst : ValFlagNone;
    return rv;
}

Value MapValue(ZMap* map, bool isConst)
{
    Value rv;
    rv.vt = vtMap;
    rv.map = map;
    rv.flags = isConst ? ValFlagConst : ValFlagNone;
    return rv;
}

Value SetValue(ZSet* set, bool isConst)
{
    Value rv;
    rv.vt = vtSet;
    rv.set = set;
    rv.flags = isConst ? ValFlagConst : ValFlagNone;
    return rv;
}


Value KeyRefValue(KeyRef* kr, bool isConst)
{
    Value rv;
    rv.vt = vtKeyRef;
    rv.keyRef = kr;
    rv.flags = isConst ? ValFlagConst : ValFlagNone;
    return rv;
}

Value NObjValue(ZorroVM* vm, ClassInfo* cls, void* ptr)
{
    Value rv;
    rv.vt = vtNativeObject;
    rv.flags = ValFlagNone;
    rv.nobj = vm->allocNObj();
    rv.nobj->classInfo = cls;
    rv.nobj->userData = ptr;
    return rv;
}


static void assignToLvalue(ZorroVM* vm, Value* l, const Value* r, Value*)
{
    ZASSIGN(vm, &vm->ctx.dataPtrs[l->atLvalue][l->idx], r);
}

static void assignLvalueToAny(ZorroVM* vm, Value* l, const Value* r, Value*)
{
    if(r->flags & ValFlagARef)
    {
        ZUNREF(vm, l);
        *l = *r;
        l->flags &= ~ValFlagARef;
    } else
    {
        ZASSIGN(vm, l, &vm->ctx.dataPtrs[r->atLvalue][r->idx]);
    }
}


static void assignToRef(ZorroVM* vm, Value* l, const Value* r, Value*)
{
    ZASSIGN(vm, &l->valueRef->value, r);
}

static void assignRefToAnyScalar(ZorroVM* vm, Value* l, const Value* r, Value*)
{
    if(r->flags & ValFlagARef)
    {
        r->valueRef->ref();
        *l = *r;
        l->flags &= ~ValFlagARef;
    } else
    {
        ZASSIGN(vm, l, &r->valueRef->value);
    }
}

static void assignRefToAnyRefType(ZorroVM* vm, Value* l, const Value* r, Value*)
{
    if(r->flags & ValFlagARef)
    {
        if(l != r)
        {
            ZUNREF(vm, l);
            r->valueRef->ref();
            *l = *r;
            l->flags &= ~ValFlagARef;
        }
    } else
    {
        ZASSIGN(vm, l, &r->valueRef->value);
    }
}


static void assignRefToRef(ZorroVM* vm, Value* l, const Value* r, Value*)
{
    ZASSIGN(vm, &l->valueRef->value, &r->valueRef->value);
}


static void assignSegmentToAny(ZorroVM* vm, Value* l, const Value* r, Value*)
{
    if(r->flags & ValFlagARef)
    {
        Segment& seg = *r->seg;
        if(seg.cont.vt == vtArray)
        {
            ZArray& za = *seg.cont.arr;
            Value* item = &za.getItemRef(static_cast<size_t>(seg.segStart));
            ZASSIGN(vm, l, item);
        } else if(seg.cont.vt == vtObject)
        {
            ZASSIGN(vm, l, &seg.getValue);
        }
        //r->flags&=~ValFlagARef;
    } else
    {
        ZUNREF(vm, l);
        *l = *r;
        r->refBase->ref();
    }
}

static void assignAnyToSegment(ZorroVM* vm, Value* l, const Value* r, Value*)
{
    if(l->flags & ValFlagARef)
    {
        Segment& seg = *l->seg;
        if(seg.cont.vt == vtArray)
        {
            ZArray& za = *seg.cont.arr;
            if(ZISREFTYPE(r))
            {
                za.isSimpleContent = false;
            }
            Value* item = &za.getItemRef(static_cast<size_t>(seg.segStart));
            ZASSIGN(vm, item, r);
        } else if(seg.cont.vt == vtObject)
        {
            ZASSIGN(vm, &seg.getValue, r);
        }
//    l->flags&=~ValFlagARef;
    } else
    {
        ZUNREF(vm, l);
        ZASSIGN(vm, l, r);
    }
}

static void assignAnyToKeyRef(ZorroVM* vm, Value* l, const Value* r, Value*)
{
    KeyRef& p = *l->keyRef;
    if(p.obj.vt == vtMap)
    {
        ZMap& zm = *p.obj.map;
        zm.insert(p.name, *r);
    } else
    {
        ZTHROWR(TypeException, vm, "invalid key reference base type:%{}", getValueTypeName(p.obj.vt));
    }
}

static void assignAnyToMemberRef(ZorroVM* vm, Value* l, const Value* r, Value*)
{
    if(l->flags & ValFlagARef)
    {
        ZUNREF(vm, l);
        ZASSIGN(vm, l, r);
        l->flags = 0;
    } else
    {
        MemberRef& m = *l->membRef;
        if(m.simpleMember)
        {
            Value* mv = m.obj.obj->members + m.memberIndex;
            ZASSIGN(vm, mv, r);
        } else
        {
            ZASSIGN(vm, &m.getValue, r);
        }
    }
}

static void assignMemberRefToAny(ZorroVM* vm, Value* l, const Value* r, Value*)
{
    if(r->flags & ValFlagARef)
    {
        ZUNREF(vm, l);
        *l = *r;
        r->refBase->ref();
        l->flags = 0;
        //ZASSIGN(vm,l,r);
    } else
    {
        MemberRef& m = *r->membRef;
        if(m.simpleMember)
        {
            Value* mv = m.obj.obj->members + m.memberIndex;
            ZASSIGN(vm, l, mv);
        } else
        {
            ZASSIGN(vm, l, &m.getValue);
        }
    }
}

static void assignKeyRefToAny(ZorroVM* vm, Value* l, const Value* r, Value*)
{
    if(r->flags & ValFlagARef)
    {
        ZUNREF(vm, l);
        *l = *r;
        r->refBase->ref();
        l->flags = 0;
        //ZASSIGN(vm,l,r);
    } else
    {
        KeyRef& kr = *r->keyRef;
        ZMap::iterator it = kr.obj.map->find(kr.name);
        if(it != kr.obj.map->end())
        {
            ZASSIGN(vm, l, &it->m_value);
        } else
        {
            ZTHROWR(NoSuchKeyException, vm, "No such key %{} in map", ValueToString(vm, kr.name));
        }
    }
}


static void assignScalarToRefType(ZorroVM* vm, Value* l, const Value* r, Value*)
{
    ZUNREF(vm, l);
    *l = *r;
}

static void assignRefTypeToScalar(ZorroVM* /*vm*/, Value* l, const Value* r, Value*)
{
    *l = *r;
    l->refBase->ref();
}

static void assignRefTypeToRefType(ZorroVM* vm, Value* l, const Value* r, Value*)
{
    if(l->refBase == r->refBase)
    {
        return;
    }
    ZUNREF(vm, l);
    *l = *r;
    l->refBase->ref();
}

#define BINREFOPS(name, matrix) \
    static void name##RefAny(ZorroVM* vm,Value* l,const Value* r,Value* dst)\
    {\
      vm->matrix[l->valueRef->value.vt][r->vt](vm,&l->valueRef->value,r,dst);\
    }\
    static void name##AnyRef(ZorroVM* vm,Value* l,const Value* r,Value* dst)\
    {\
      vm->matrix[l->vt][r->valueRef->value.vt](vm,l,&r->valueRef->value,dst);\
    }\
    static void name##RefRef(ZorroVM* vm,Value* l,const Value* r,Value* dst)\
    {\
      vm->matrix[l->valueRef->value.vt][r->valueRef->value.vt](vm,&l->valueRef->value,&r->valueRef->value,dst);\
    }

#define BINBOOLREFOPS(name, matrix) \
    static bool name##RefAny(ZorroVM* vm,const Value* l,const Value* r)\
    {\
      return vm->matrix[l->valueRef->value.vt][r->vt](vm,&l->valueRef->value,r);\
    }\
    static bool name##AnyRef(ZorroVM* vm,const Value* l,const Value* r)\
    {\
      return vm->matrix[l->vt][r->valueRef->value.vt](vm,l,&r->valueRef->value);\
    }\
    static bool name##RefRef(ZorroVM* vm,const Value* l,const Value* r)\
    {\
      return vm->matrix[l->valueRef->value.vt][r->valueRef->value.vt](vm,&l->valueRef->value,&r->valueRef->value);\
    }


BINREFOPS(add, addMatrix)

BINREFOPS(sub, subMatrix)

BINREFOPS(bitOr, bitOrMatrix)

BINREFOPS(bitAnd, bitAndMatrix)


#define NOP(name, resType, resMember, op) static void name(ZorroVM* vm,Value* l,const Value* r,Value* dst) \
    { \
      if(!dst)return;\
      if(dst->vt==vtRef){ dst=&dst->valueRef->value;}\
      ZUNREF(vm,dst);\
      dst->vt=resType;\
      dst->flags=0;\
      dst->resMember=op; \
    }

#define NOPZ(name, resType, resMember, checkMember, opName, op) static void name(ZorroVM* vm,Value* l,const Value* r,Value* dst) \
    { \
      if(r->checkMember==0)ZTHROWR(ArithmeticException,vm,"%{} by zero",opName);\
      if(!dst)return;\
      if(dst->vt==vtRef){ dst=&dst->valueRef->value;}\
      ZUNREF(vm,dst);\
      dst->vt=resType;\
      dst->flags=0;\
      dst->resMember=op; \
    }


NOP(addIntInt, vtInt, iValue, l->iValue + r->iValue)

NOP(addIntDouble, vtDouble, dValue, l->iValue + r->dValue)

NOP(addDoubleInt, vtDouble, dValue, l->dValue + r->iValue)

NOP(addDoubleDouble, vtDouble, dValue, l->dValue + r->dValue)

NOP(subIntInt, vtInt, iValue, l->iValue - r->iValue)

NOP(subIntDouble, vtDouble, dValue, l->iValue - r->dValue)

NOP(subDoubleInt, vtDouble, dValue, l->dValue - r->iValue)

NOP(subDoubleDouble, vtDouble, dValue, l->dValue - r->dValue)

NOP(mulIntInt, vtInt, iValue, l->iValue * r->iValue)

NOP(mulIntDouble, vtDouble, dValue, l->iValue * r->dValue)

NOP(mulDoubleInt, vtDouble, dValue, l->dValue * r->iValue)

NOP(mulDoubleDouble, vtDouble, dValue, l->dValue * r->dValue)

NOPZ(modIntInt, vtInt, iValue, iValue, "Module", l->iValue % r->iValue)

NOPZ(modIntDouble, vtDouble, dValue, dValue, "Module", fmod(l->iValue, r->dValue))

NOPZ(modDoubleInt, vtDouble, dValue, iValue, "Module", fmod(l->dValue, r->iValue))

NOPZ(modDoubleDouble, vtDouble, dValue, dValue, "Module", fmod(l->dValue, r->dValue))


NOPZ(divIntInt, vtInt, iValue, iValue, "Division", l->iValue / r->iValue)

NOPZ(divIntDouble, vtDouble, dValue, dValue, "Division", l->iValue / r->dValue)

NOPZ(divDoubleInt, vtDouble, dValue, iValue, "Division", l->dValue / r->iValue)

NOPZ(divDoubleDouble, vtDouble, dValue, dValue, "Division", l->dValue / r->dValue)

NOP(bitOrIntInt, vtInt, iValue, l->iValue | r->iValue)

NOP(bitAndIntInt, vtInt, iValue, l->iValue & r->iValue)


BINREFOPS(mul, mulMatrix)

BINREFOPS(div, divMatrix)

BINREFOPS(mod, modMatrix)


static void mulStrInt(ZorroVM* vm, Value* l, const Value* r, Value* dst)
{
    ZString& str = *l->str;
    int64_t cnt = r->iValue;
    if(cnt < 0)
    {
        cnt = 0;
    }
    uint32_t sl = str.getDataSize();
    auto dataSize = static_cast<size_t>(sl * cnt);
    char* data = vm->allocStr(dataSize + (str.getCharSize() == 1 ? 1 : 0));
    if(!data)
    {
        ZTHROWR(RuntimeException, vm, "Attempt to allocate %{} bytes for string failed", dataSize);
    }
    char* ptr = data;
    const char* srcData = str.getDataPtr();
    for(int64_t i = 0; i < cnt; ++i)
    {
        memcpy(ptr, srcData, sl);
        ptr += sl;
    }
    if(str.getCharSize() == 1)
    {
        *ptr = 0;
    }
    Value v;
    v.vt = vtString;
    v.flags = ValFlagNone;
    v.str = vm->allocZString();
    v.str->init(data, dataSize, str.getCharSize() != 1);
    ZASSIGN(vm, dst, &v);
}

static void divStrStr(ZorroVM* vm, Value* l, const Value* r, Value* dst)
{
    ZString& str = *l->str;
    const ZString& str2 = *r->str;
    Value v;
    v.vt = vtArray;
    v.flags = ValFlagNone;
    ZArray& za = *(v.arr = vm->allocZArray());
    uint32_t lastPos = 0;
    int pos;
    while((pos = str.find(str2, lastPos)) != -1)
    {
        za.pushAndRef(StringValue(str.substr(vm, lastPos, pos - lastPos)));
        lastPos = pos + str2.getLength();
    }
    za.pushAndRef(StringValue(str.substr(vm, lastPos, str.getLength() - lastPos)));
    ZASSIGN(vm, dst, &v);
}

static void divStrRegExp(ZorroVM* vm, Value* l, const Value* r, Value* dst)
{
    ZString& str = *l->str;
    int cs = str.getCharSize();
    kst::RegExp& rx = *r->regexp->val;
    Value v;
    v.vt = vtArray;
    v.flags = ValFlagNone;
    ZArray& za = *(v.arr = vm->allocZArray());
    size_t lastPos = 0;
    if(cs == 1)
    {
        const char* start = str.getDataPtr();
        const char* end = start + str.getLength();
        kst::MatchInfo sm(r->regexp->marr, r->regexp->marrSize, r->regexp->narr, r->regexp->narrSize);
        while(rx.SearchEx(start, start + lastPos, end, sm))
        {
            za.pushAndRef(StringValue(str.substr(vm, lastPos, sm.br[0].start - lastPos)));
            for(size_t i = 1; i < sm.brCount; ++i)
            {
                za.pushAndRef(StringValue(str.substr(vm, static_cast<uint32_t>(sm.br[i].start),
                                                     static_cast<uint32_t>(sm.br[i].end - sm.br[i].start))));
            }
            lastPos = static_cast<size_t>(sm.br[0].end);
        }
        za.pushAndRef(StringValue(str.substr(vm, lastPos, str.getLength() - lastPos)));
    } else
    {
        const uint16_t* start = (uint16_t*) str.getDataPtr();
        const uint16_t* end = start + str.getLength();
        kst::MatchInfo sm(r->regexp->marr, r->regexp->marrSize, r->regexp->narr, r->regexp->narrSize);
        while(rx.SearchEx(start, start + lastPos, end, sm))
        {
            za.pushAndRef(StringValue(str.substr(vm, lastPos, sm.br[0].start - lastPos)));
            for(size_t i = 1; i < sm.brCount; ++i)
            {
                za.pushAndRef(StringValue(str.substr(vm, static_cast<uint32_t>(sm.br[i].start),
                                                     static_cast<uint32_t>(sm.br[i].end - sm.br[i].start))));
            }
            lastPos = static_cast<size_t>(sm.br[0].end);
        }
        za.pushAndRef(StringValue(str.substr(vm, lastPos, str.getLength() - lastPos)));
    }
    ZASSIGN(vm, dst, &v);
}

static void mulCallableArray(ZorroVM* vm, Value* l, const Value* r, Value* /*dst*/)
{
    ZArray& za = *r->arr;
    size_t sz = za.getCount();
    for(size_t i = 0; i < sz; ++i)
    {
        vm->ctx.dataStack.push(za.getItem(i));
        if(ZISREFTYPE(vm->ctx.dataStack.stackTop))
        {
            vm->ctx.dataStack.stackTop->refBase->ref();
        }
    }
    INITOCALL(vm, sz, false, false, false);
    vm->callOps[l->vt](vm, l);
}

static void addStrStr(ZorroVM* vm, Value* l, const Value* r, Value* dst)
{
    Value v;
    v.vt = vtString;
    v.flags = ValFlagNone;
    v.str = ZString::concat(vm, l->str, r->str);
    ZASSIGN(vm, dst, &v);
}

static void saddStrStr(ZorroVM* vm, Value* l, const Value* r, Value* dst)
{
    Value v;
    v.vt = vtString;
    v.flags = ValFlagNone;
    v.str = ZString::concat(vm, l->str, r->str);
    ZASSIGN(vm, l, &v);
    if(dst)
    {
        ZASSIGN(vm, dst, &v);
    }
}

static void saddStrSegment(ZorroVM* vm, Value* l, const Value* r, Value* dst)
{
    if(r->seg->cont.vt != vtString)
    {
        ZTHROWR(TypeException, vm, "attempt to add segment with base type %{} to string",
                getArgTypeName(r->seg->cont.vt));
    }
    ZStringRef ss(vm, r->seg->cont.str->substr(vm, static_cast<uint32_t>(r->seg->segStart),
                                               static_cast<uint32_t>(r->seg->segEnd - r->seg->segStart)));
    Value v;
    v.vt = vtString;
    v.flags = ValFlagNone;
    v.str = ZString::concat(vm, l->str, ss.get());
    ZASSIGN(vm, l, &v);
    if(dst)
    {
        ZASSIGN(vm, dst, &v);
    }
}


static void addSegmentSegment(ZorroVM* vm, Value* l, const Value* r, Value* dst)
{
    if(!dst)
    {
        return;
    }
    Segment& ls = *l->seg;
    Value* lv = &ls.cont;
    Segment& rs = *r->seg;
    Value* rv = &rs.cont;
    if(lv->vt == vtString && rv->vt == vtString)
    {
        Value v;
        v.vt = vtString;
        v.flags = ValFlagNone;
        ZString* s1 = lv->str->substr(vm, static_cast<uint32_t>(ls.segStart),
                                      static_cast<uint32_t>(ls.segEnd - ls.segStart));
        ZString* s2 = rv->str->substr(vm, static_cast<uint32_t>(rs.segStart),
                                      static_cast<uint32_t>(rs.segEnd - rs.segStart));
        v.str = ZString::concat(vm, s1, s2);
        vm->freeZString(s1);
        vm->freeZString(s2);
        ZASSIGN(vm, dst, &v);
    } else if(lv->vt == vtArray && rv->vt == vtArray)
    {
        Value v;
        v.vt = vtArray;
        v.flags = ValFlagNone;
        v.arr = vm->allocZArray();
        v.arr->append(*lv->arr);
        v.arr->append(*rv->arr);
        ZASSIGN(vm, dst, &v);
    } else
    {
        ZTHROWR(TypeException, vm, "attempt to add segments with base types %{} and %{}", getValueTypeName(lv->vt),
                getValueTypeName(rv->vt));
    }
}

static void addSegmentStr(ZorroVM* vm, Value* l, const Value* r, Value* dst)
{
    Segment& ls = *l->seg;
    Value* lv = &ls.cont;
    if(lv->vt != vtString)
    {
        ZTHROWR(TypeException, vm, "attempt to add segment with base type {} to string", getValueTypeName(lv->vt));
    }
    ZString* s1 = lv->str->substr(vm, static_cast<uint32_t>(ls.segStart),
                                  static_cast<uint32_t>(ls.segEnd - ls.segStart));
    Value v;
    v.vt = vtString;
    v.flags = ValFlagNone;
    v.str = ZString::concat(vm, s1, r->str);
    vm->freeZString(s1);
    ZASSIGN(vm, dst, &v);
}

static void addStrSegment(ZorroVM* vm, Value* l, const Value* r, Value* dst)
{
    Segment& rs = *r->seg;
    Value* rv = &rs.cont;
    if(rv->vt != vtString)
    {
        ZTHROWR(TypeException, vm, "attempt to add segment with base type {} to string", getValueTypeName(rv->vt));
    }
    ZString* s1 = rv->str->substr(vm, static_cast<uint32_t>(rs.segStart),
                                  static_cast<uint32_t>(rs.segEnd - rs.segStart));
    Value v;
    v.vt = vtString;
    v.flags = ValFlagNone;
    v.str = ZString::concat(vm, l->str, s1);
    vm->freeZString(s1);
    ZASSIGN(vm, dst, &v);
}


static void addArrArr(ZorroVM* vm, Value* l, const Value* r, Value* dst)
{
    if(!dst)
    {
        return;
    }
    Value rv;
    rv.vt = vtArray;
    rv.flags = ValFlagNone;
    ZArray& za = *(rv.arr = l->arr->copy());
    za.append(*r->arr);
    ZASSIGN(vm, dst, &rv);
}

#define SOP(name, op) static void name(ZorroVM* vm,Value* l,const Value* r,Value* dst) \
{\
  op;\
  if(dst)\
  {\
    ZASSIGN(vm,dst,l);\
  }\
}
#define SOPZ(name, checkMember, opName, op) static void name(ZorroVM* vm,Value* l,const Value* r,Value* dst) \
{\
  if(r->checkMember==0)ZTHROWR(ArithmeticException,vm,"%{} by zero",opName);\
  op;\
  if(dst)\
  {\
    ZASSIGN(vm,dst,l);\
  }\
}


SOP(saddArrScalar, l->arr->push(*r))

SOP(saddSetAny, l->set->insert(*r))

SOP(saddIntInt, l->iValue += r->iValue)

SOP(saddDoubleInt, l->dValue += r->iValue)

SOP(saddIntDouble, l->iValue += static_cast<int64_t>(r->dValue))

SOP(saddDoubleDouble, l->dValue += r->dValue)

static void saddArrRefCounted(ZorroVM* vm, Value* l, const Value* r, Value* dst)
{
    l->arr->push(*r);
    l->arr->isSimpleContent = false;
    r->refBase->ref();
    if(dst)
    {
        ZASSIGN(vm, dst, l);
    }
}

static void saddArrArr(ZorroVM* vm, Value* l, const Value* r, Value* dst)
{
    l->arr->append(*r->arr);
    if(dst)
    {
        ZASSIGN(vm, dst, l);
    }
}


static void saddSetSet(ZorroVM* vm, Value* l, const Value* r, Value* dst)
{
    l->set->insert(r->set->begin(), r->set->end());
    if(dst)
    {
        ZASSIGN(vm, dst, l);
    }
}

static void saddMapMap(ZorroVM* vm, Value* l, const Value* r, Value* dst)
{
    l->map->insert(r->map->begin(), r->map->end());
    if(dst)
    {
        ZASSIGN(vm, dst, l);
    }
}

SOP(ssubIntInt, l->iValue -= r->iValue)

SOP(ssubDoubleInt, l->dValue -= r->iValue)

SOP(ssubIntDouble, l->iValue -= static_cast<int64_t>(r->dValue))

SOP(ssubDoubleDouble, l->dValue -= r->dValue)

SOP(ssubMapAny, l->map->erase(*r))


SOP(smulIntInt, l->iValue *= r->iValue)

SOP(smulDoubleInt, l->dValue *= r->iValue)

SOP(smulIntDouble, l->iValue *= static_cast<int64_t>(r->dValue))

SOP(smulDoubleDouble, l->dValue *= r->dValue)

SOPZ(sdivIntInt, iValue, "Division", l->iValue /= r->iValue)

SOPZ(sdivDoubleInt, iValue, "Division", l->dValue /= r->iValue)

SOPZ(sdivIntDouble, dValue, "Division", l->iValue /= static_cast<int64_t>(r->dValue))

SOPZ(sdivDoubleDouble, dValue, "Division", l->dValue /= r->dValue)

SOPZ(smodIntInt, iValue, "Module", l->iValue %= r->iValue)

SOPZ(smodDoubleInt, iValue, "Module", l->dValue = fmod(l->dValue, r->iValue))

SOPZ(smodIntDouble, dValue, "Module", l->iValue %= (int64_t) r->dValue)

SOPZ(smodDoubleDouble, dValue, "Module", l->dValue = fmod(l->dValue, r->dValue))


static void sdivNObjAny(ZorroVM* vm, Value* l, const Value* r, Value* dst)
{
    size_t midx = l->nobj->classInfo->specialMethods[csmSDiv];
    if(!midx)
    {
        ZTHROWR(TypeException, vm, "Native class %{} do not have operator sdiv", l->nobj->classInfo->name);
    }
    vm->pushValue(*r);
    vm->callCMethod(l, midx, 1);
    if(dst)
    {
        ZASSIGN(vm, dst, l);
    }
}

static void smulNObjAny(ZorroVM* vm, Value* l, const Value* r, Value* dst)
{
    size_t midx = l->nobj->classInfo->specialMethods[csmSMul];
    if(!midx)
    {
        ZTHROWR(TypeException, vm, "Native class %{} do not have operator smul", l->nobj->classInfo->name);
    }
    vm->pushValue(*r);
    vm->callCMethod(l, midx, 1);
    if(dst)
    {
        ZASSIGN(vm, dst, l);
    }
}

static void addNObjAny(ZorroVM* vm, Value* l, const Value* r, Value* dst)
{
    size_t midx = l->nobj->classInfo->specialMethods[csmAdd];
    if(!midx)
    {
        ZTHROWR(TypeException, vm, "Native class %{} do not have operator add", l->nobj->classInfo->name);
    }
    vm->pushValue(*r);
    vm->callCMethod(l, midx, 1);
    if(dst)
    {
        ZASSIGN(vm, dst, &vm->result);
    }
    vm->clearResult();
}


static void divNObjAny(ZorroVM* vm, Value* l, const Value* r, Value* dst)
{
    size_t midx = l->nobj->classInfo->specialMethods[csmDiv];
    if(!midx)
    {
        ZTHROWR(TypeException, vm, "Native class %{} do not have operator div", l->nobj->classInfo->name);
    }
    vm->pushValue(*r);
    vm->callCMethod(l, midx, 1);
    if(dst)
    {
        ZASSIGN(vm, dst, &vm->result);
    }
    vm->clearResult();
}

static void mulNObjAny(ZorroVM* vm, Value* l, const Value* r, Value* dst)
{
    size_t midx = l->nobj->classInfo->specialMethods[csmMul];
    if(!midx)
    {
        ZTHROWR(TypeException, vm, "Native class %{} do not have operator mul", l->nobj->classInfo->name);
    }
    vm->pushValue(*r);
    vm->callCMethod(l, midx, 1);
    if(dst)
    {
        ZASSIGN(vm, dst, &vm->result);
    }
    vm->clearResult();
}


static void divObjAny(ZorroVM* vm, Value* l, const Value* r, Value* /*dst*/)
{
    size_t midx = l->obj->classInfo->specialMethods[csmDiv];
    if(!midx)
    {
        ZTHROWR(TypeException, vm, "Class %{} do not have operator div", l->obj->classInfo->name);
    }
    vm->pushValue(*r);
    vm->callMethod(l, midx, 1, true);
}

static void saddIndexAny(ZorroVM* vm, Value* l, const Value* r, Value* dst)
{
    Segment& s = *l->seg;
    if(s.cont.vt == vtArray)
    {
        ZArray& za = *s.cont.arr;
        if(s.segStart < 0 || s.segStart >= (int64_t) za.getCount())
        {
            throwOutOfBounds(vm, "Segment index of array out of bounds", s.segStart, za.getCount());
        }
        Value* val = &za.getItemRef(static_cast<size_t>(s.segStart));
        vm->saddMatrix[val->vt][r->vt](vm, val, r, dst);
    } else if(s.cont.vt == vtObject)
    {
        Value* val = &s.getValue;
        OVERGUARD(vm->saddMatrix[val->vt][r->vt](vm, val, r, dst));
        size_t midx = s.cont.obj->classInfo->specialMethods[csmSetIndex];
        if(!midx)
        {
            ZTHROWR(TypeException, vm, "Class %{} do not have operator setIndex", s.cont.obj->classInfo->name);
        }
        Value idx = IntValue(s.segStart);
        vm->pushValue(idx);
        vm->pushValue(*val);
        vm->callMethod(&s.cont, midx, 2);
    } else if(s.cont.vt == vtNativeObject)
    {
        ClassInfo* ci = s.cont.nobj->classInfo;
        size_t gidx = ci->specialMethods[csmGetIndex];
        if(!gidx)
        {
            ZTHROWR(TypeException, vm, "Class %{} do not have operator getIndex", ci->name);
        }
        Value idx = IntValue(s.segStart);
        vm->pushValue(idx);
        vm->callCMethod(&s.cont, gidx, 1);
        Value* val = &vm->result;
        {
            OpBase* oldNext = vm->ctx.nextOp;
            vm->saddMatrix[val->vt][r->vt](vm, val, r, dst);
            if(oldNext != vm->ctx.nextOp)
            {
                vm->clearResult();
                return;
            }
        }
        size_t sidx = ci->specialMethods[csmSetIndex];
        if(!sidx)
        {
            ZTHROWR(TypeException, vm, "Class %{} do not have operator setIndex", ci->name);
        }
        vm->pushValue(idx);
        vm->pushValue(*val);
        vm->callCMethod(&s.cont, sidx, 2);
        vm->clearResult();
    }
}


static void saddKeyRefAny(ZorroVM* vm, Value* l, const Value* r, Value* dst)
{
    KeyRef& p = *l->keyRef;
    ZMap& zm = *p.obj.map;
    ZMap::iterator it = zm.find(p.name);
    if(it == zm.end())
    {
        ZTHROWR(NoSuchKeyException, vm, "No such key %{} in map", ValueToString(vm, p.name));
    }
    Value& v = it->m_value;
    vm->saddMatrix[v.vt][r->vt](vm, &v, r, dst);
}


BINREFOPS(sadd, saddMatrix)

static void ssubKeyRefAny(ZorroVM* vm, Value* l, const Value* r, Value* dst)
{
    KeyRef& p = *l->keyRef;
    ZMap& zm = *p.obj.map;
    ZMap::iterator it = zm.find(p.name);
    if(it == zm.end())
    {
        ZTHROWR(NoSuchKeyException, vm, "No such key %{} in map", ValueToString(vm, p.name));
    }
    Value& v = it->m_value;
    vm->ssubMatrix[v.vt][r->vt](vm, &v, r, dst);
}

static void
mutateMemberRef(ZorroVM* vm, Value* l, const Value* r, Value* dst, ZorroVM::BinOpFunc (& matrix)[vtCount][vtCount])
{
    MemberRef& p = *l->membRef;
    if(p.obj.vt == vtObject)
    {
        if(p.simpleMember)
        {
            Value* m = p.obj.obj->members + p.memberIndex;
            matrix[m->vt][r->vt](vm, m, r, dst);
            return;
        }
        ClassInfo* ci = p.obj.obj->classInfo;
        SymInfo* si = ci->symMap.findSymbol(p.name);
        if(!si)
        {
            if(ci->specialMethods[csmSetProp])
            {
                Value* val = &p.getValue;
                OVERGUARD(matrix[val->vt][r->vt](vm, val, r, dst));
                if(((OpDstBase*) vm->ctx.lastOp)->dst.at == atStack && vm->ctx.lastOp->ot != otPostInc)
                {
                    size_t sidx = ci->specialMethods[csmGetProp];
                    if(sidx)
                    {
                        vm->pushValue(StringValue(p.name));
                        vm->callMethod(&p.obj, sidx, 1);
                    }
                }
                vm->pushValue(StringValue(p.name));
                Value* arg = vm->pushData();
                ZASSIGN(vm, arg, &p.getValue);
                vm->callMethod(&p.obj, ci->specialMethods[csmSetProp], 2);
                return;
            }
            ZTHROWR(TypeException, vm, "Class %{} do not have property %{}", ci->name, ZStringRef(vm, p.name));
        }
        Value* mem = p.obj.obj->members;
        if(si->st == sytProperty)
        {
            auto* cp = (ClassPropertyInfo*) si;
            Value tmp = NilValue;
            Value* v;
            if(cp->getMethod)
            {
                v = &p.getValue;
            } else if(cp->getIdx != SymInfo::invalidIndexValue)
            {
                if(cp->getIdx == cp->setIdx)
                {
                    v = mem + cp->getIdx;
                } else
                {
                    v = &tmp;
                    ZASSIGN(vm, v, mem + cp->getIdx);
                }
            } else
            {
                ZTHROWR(TypeException, vm, "Property %{} of class %{} is write only", ZStringRef(vm, p.name), ci->name);
            }

            OVERGUARD(matrix[v->vt][r->vt](vm, v, r, dst));
            if(cp->setMethod)
            {
                if(((OpDstBase*) vm->ctx.lastOp)->dst.at == atStack && vm->ctx.lastOp->ot != otPostInc)
                {
                    if(cp->getMethod)
                    {
                        vm->callMethod(&p.obj, cp->getMethod, 0);
                    } else
                    {
                        Value* oldDst = vm->ctx.dataStack.stack + vm->ctx.callStack.stackTop->localBase - 1;
                        ZASSIGN(vm, oldDst, v);
                    }
                }
                Value* arg = vm->pushData();
                ZASSIGN(vm, arg, v);
                vm->callMethod(&p.obj, cp->setMethod, 1);
            } else
            {
                if(cp->setIdx != cp->getIdx)
                {
                    ZASSIGN(vm, mem + cp->setIdx, v);
                }
            }
        } else
        {
            ZTHROWR(TypeException, vm, "%{} is not property or member of class %{}", ZStringRef(vm, p.name), ci->name);
        }
    } else if(p.obj.vt == vtNativeObject)
    {
        ClassInfo* ci = p.obj.nobj->classInfo;
        SymInfo* sym = ci->symMap.findSymbol(p.name);
        if(!sym)
        {
            ZTHROWR(TypeException, vm, "Class %{} do not have property %{}", ci->name, ZStringRef(vm, p.name));
        }
        if(sym->st != sytProperty)
        {
            ZTHROWR(TypeException, vm, "%{} is not property or member of class %{}", ZStringRef(vm, p.name), ci->name);
        }
        auto* cp = (ClassPropertyInfo*) sym;
        if(!cp->getMethod || !cp->setMethod)
        {
            if(!cp->getMethod)
            {
                ZTHROWR(TypeException, vm, "Property %{} of class %{} is write only", ZStringRef(vm, p.name), ci->name);
            } else
            {
                ZTHROWR(TypeException, vm, "Property %{} of class %{} is read only", ZStringRef(vm, p.name), ci->name);
            }
        }
        //vm->callCMethod(&p.obj,cp->getMethod->index,0);
        Value* m = &p.getValue;//&vm->result;
        matrix[m->vt][r->vt](vm, m, r, dst);
        vm->pushValue(*m);
        vm->clearResult();
        vm->callCMethod(&p.obj, cp->setMethod->index, 1);
        vm->clearResult();
    } else
    {
        ZTHROWR(TypeException, vm, "Attempt to mutate property %{} of value of type %{}", ZStringRef(vm, p.name),
                getValueTypeName(p.obj.vt));
    }
}


static void saddMemberRefAny(ZorroVM* vm, Value* l, const Value* r, Value* dst)
{
    Value tmpDst = NilValue;
    mutateMemberRef(vm, l, r, dst ? &tmpDst : nullptr, vm->saddMatrix);
    if(dst)
    {
        ZASSIGN(vm, dst, &tmpDst);
        ZUNREF(vm, &tmpDst);
    }
}

static void ssubMemberRefAny(ZorroVM* vm, Value* l, const Value* r, Value* dst)
{
    Value tmpDst = NilValue;
    mutateMemberRef(vm, l, r, dst ? &tmpDst : nullptr, vm->ssubMatrix);
    if(dst)
    {
        ZASSIGN(vm, dst, &tmpDst);
        ZUNREF(vm, &tmpDst);
    }
}


#define OVERLOAD1(mname, csm) \
    static void mname##ObjAny(ZorroVM* vm,Value* l,const Value* r,Value* dst)\
{\
  (void)dst;\
  ClassInfo* ci=l->obj->classInfo;\
  if(!ci->specialMethods[csm])\
  {\
    ZTHROWR(TypeException,vm,"Class %{} do not have operator %{}",ci->name,#mname);\
  }\
  vm->ctx.dataStack.push(*r);\
  if(ZISREFTYPE(r))\
  {\
    r->refBase->ref();\
  }\
  vm->callMethod(l,vm->symbols.globals[ci->specialMethods[csm]].method,1);\
}

#define ROVERLOAD1(mname, csm) \
    static void mname##ObjAny(ZorroVM* vm,Value* l,const Value* r,Value* dst)\
{\
  (void)dst;\
  ClassInfo* ci=r->obj->classInfo;\
  if(!ci->specialMethods[csm])\
  {\
    ZTHROWR(TypeException,vm,"Class %{} do not have operator %{}",ci->name,#mname);\
  }\
  vm->ctx.dataStack.push(*l);\
  if(ZISREFTYPE(l))\
  {\
    l->refBase->ref();\
  }\
  vm->callMethod((Value*)r,vm->symbols.globals[ci->specialMethods[csm]].method,1);\
}


#define OVERLOAD2(mname, csm) \
    static void mname##ObjAny(ZorroVM* vm,Value* l,const Value* r,Value* dst)\
{\
  ClassInfo* ci=l->obj->classInfo;\
  if(!ci->specialMethods[csm])\
  {\
    ZTHROWR(TypeException,vm,"Class %{} do not have operator %{}",ci->name,#mname);\
  }\
  vm->ctx.dataStack.push(*r);\
  if(ZISREFTYPE(r))\
  {\
    r->refBase->ref();\
  }\
  vm->ctx.dataStack.push(*dst);\
  if(ZISREFTYPE(dst))\
  {\
    dst->refBase->ref();\
  }\
  vm->callMethod(l,vm->symbols.globals[ci->specialMethods[csm]].method,2);\
}

OVERLOAD1(add, csmAdd)

ROVERLOAD1(radd, csmRAdd)

OVERLOAD1(sadd, csmSAdd)

OVERLOAD1(sub, csmSub)

OVERLOAD1(ssub, csmSSub)


static void ssubSetAny(ZorroVM* vm, Value* l, const Value* r, Value* dst)
{
    l->set->erase(*r);
    if(dst)
    {
        ZASSIGN(vm, dst, l);
    }
}


static void incInt(ZorroVM* /*vm*/, Value* l)
{
    ++l->iValue;
}


static void incDouble(ZorroVM* /*vm*/, Value* l)
{
    ++l->dValue;
}

static void incRef(ZorroVM* vm, Value* l)
{
    vm->incOps[l->valueRef->value.vt](vm, &l->valueRef->value);
}

static void incSegment(ZorroVM* vm, Value* l)
{
    Segment& seg = *l->seg;
    if(seg.cont.vt == vtArray)
    {
        Value& val = seg.cont.arr->getItemRef(static_cast<size_t>(l->seg->segStart));
        vm->incOps[val.vt](vm, &val);
    } else if(seg.cont.vt == vtObject)
    {
        ClassInfo* ci = seg.cont.obj->classInfo;
        size_t midx = seg.cont.obj->classInfo->specialMethods[csmSetIndex];
        if(!midx)
        {
            ZTHROWR(TypeException, vm, "Class %{} do not have operator setIndex", ci->name);
        }
        OVERGUARD(vm->incOps[seg.getValue.vt](vm, &seg.getValue));
        Value idxVal = IntValue(seg.segStart);
        if(((OpDstBase*) vm->ctx.lastOp)->dst.at == atStack && vm->ctx.lastOp->ot != otPostInc)
        {
            size_t sidx = ci->specialMethods[csmGetIndex];
            if(sidx)
            {
                vm->pushValue(idxVal);
                vm->callMethod(&seg.cont, sidx, 1);
            }
        }

        vm->pushValue(idxVal);
        vm->pushValue(seg.getValue);
        vm->callMethod(&seg.cont, midx, 2);
    }
}

static void decSegment(ZorroVM* vm, Value* l)
{
    Segment& seg = *l->seg;
    if(seg.cont.vt == vtArray)
    {
        Value& val = seg.cont.arr->getItemRef(static_cast<size_t>(l->seg->segStart));
        vm->incOps[val.vt](vm, &val);
    } else if(seg.cont.vt == vtObject)
    {
        ClassInfo* ci = seg.cont.obj->classInfo;
        size_t midx = seg.cont.obj->classInfo->specialMethods[csmSetIndex];
        if(!midx)
        {
            ZTHROWR(TypeException, vm, "Class %{} do not have operator setIndex", ci->name);
        }
        OVERGUARD(vm->decOps[seg.getValue.vt](vm, &seg.getValue));
        Value idxVal = IntValue(seg.segStart);
        if(((OpDstBase*) vm->ctx.lastOp)->dst.at == atStack && vm->ctx.lastOp->ot != otPostDec)
        {
            size_t sidx = ci->specialMethods[csmGetIndex];
            if(sidx)
            {
                vm->pushValue(idxVal);
                vm->callMethod(&seg.cont, sidx, 1);
            }
        }

        vm->pushValue(idxVal);
        vm->pushValue(seg.getValue);
        vm->callMethod(&seg.cont, midx, 2);
    }
}


static void incKeyRef(ZorroVM* vm, Value* l)
{
    Value* obj = &l->keyRef->obj;
    ZMap::iterator it = obj->map->find(l->keyRef->name);
    if(it == obj->map->end())
    {
        Value v = IntValue(1);
        obj->map->insert(l->keyRef->name, v);
        return;
    }
    Value* v = &it->m_value;
    vm->incOps[v->vt](vm, v);
}

static void decKeyRef(ZorroVM* vm, Value* l)
{
    Value* obj = &l->keyRef->obj;
    ZMap::iterator it = obj->map->find(l->keyRef->name);
    if(it == obj->map->end())
    {
        Value v = IntValue(-1);
        obj->map->insert(l->keyRef->name, v);
        return;
    }
    Value* v = &it->m_value;
    vm->decOps[v->vt](vm, v);
}


static void incMemberRef(ZorroVM* vm, Value* l)
{
    Value one = IntValue(1);
    mutateMemberRef(vm, l, &one, nullptr, vm->saddMatrix);
}

static void decMemberRef(ZorroVM* vm, Value* l)
{
    Value one = IntValue(1);
    mutateMemberRef(vm, l, &one, nullptr, vm->ssubMatrix);
}


static void decInt(ZorroVM* /*vm*/, Value* l)
{
    --l->iValue;
}

static void decDouble(ZorroVM* /*vm*/, Value* l)
{
    --l->dValue;
}

static void decRef(ZorroVM* vm, Value* l)
{
    vm->decOps[l->valueRef->value.vt](vm, &l->valueRef->value);
}

static void errorTFunc(ZorroVM* vm, Value* l, const Value* a, const Value*, Value*)
{
    std::string msg = "Invalid operands for ";
    msg += getOpName(vm->ctx.lastOp->ot);
    msg += " ";
    msg += getValueTypeName(l->vt);
    msg += " and ";
    msg += getValueTypeName(a->vt);
    ZTHROWR(RuntimeException, vm, "%{}", msg);
}

static void errorFunc(ZorroVM* vm, Value* l, const Value* r, Value*)
{
    std::string msg = "Invalid operands for ";
    msg += getOpName(vm->ctx.lastOp->ot);
    msg += " ";
    msg += getValueTypeName(l->vt);
    msg += " and ";
    msg += getValueTypeName(r->vt);
    ZTHROWR(RuntimeException, vm, "%{}", msg);
}

static bool errorBoolFunc(ZorroVM* vm, const Value* l, const Value* r)
{
    std::string msg = "Invalid operands ";
    msg += getValueTypeName(l->vt);
    msg += " and ";
    msg += getValueTypeName(r->vt);
    msg += " at\n";
    ZTHROWR(RuntimeException, vm, "%{}", msg);
    return false;
}

static void errorUnOp(ZorroVM* vm, Value* v, Value*)
{
    std::string msg = "Invalid operand type ";
    msg += getValueTypeName(v->vt);
    msg += " for op ";
    msg += getOpName(vm->ctx.lastOp->ot);
    ZTHROWR(RuntimeException, vm, "%{}", msg);
}

static void errorUnFunc(ZorroVM* vm, Value* v)
{
    std::string msg = "Invalid operand type ";
    msg += getValueTypeName(v->vt);
    msg += " for op ";
    msg += getOpName(vm->ctx.lastOp->ot);
    ZTHROWR(RuntimeException, vm, "%{}", msg);
}

static void
errorFmtOp(ZorroVM* vm, Value* v, int /*w*/, int /*p*/, ZString* /*flags*/, Value* /*extra*/, Value* /*dst*/)
{
    std::string msg = "Invalid format operand ";
    msg += getValueTypeName(v->vt);
    ZTHROWR(RuntimeException, vm, "%{}", msg);
}

static bool errorForOp(ZorroVM* vm, Value* val, Value* /*var*/, Value* /*tmp*/)
{
    ZTHROWR(RuntimeException, vm, "Invalid type for enumeration %{}", getValueTypeName(val->vt));
}

static bool errorFor2Op(ZorroVM* vm, Value* val, Value* /*var1*/, Value* /*var2*/, Value* /*tmp*/)
{
    ZTHROWR(RuntimeException, vm, "Invalid type for enumeration %{}", getValueTypeName(val->vt));
}


/*
static void pushConst(ZorroVM* vm,const Value* v)
{
  Value val=*v;
  val.flags&=~ValFlagConst;
  vm->pushData(val);
}

static void pushRef(ZorroVM* vm,const Value* v)
{
  vm->pushData(*v);
}
*/

static void refRef(ZorroVM* vm, Value* src, Value* dst)
{
    if(dst)
    {
        ZUNREF(vm, dst);
        *dst = *src;
        src->refBase->ref();
        dst->flags = ValFlagARef;
    }
}


static void refSimple(ZorroVM* vm, Value* src, Value* dst)
{
    ZUNREF(vm, dst);
    ValueRef* val = vm->allocRef();
    val->ref();
    val->value = *src;
    if(ZISREFTYPE(src))
    {
        src->refBase->ref();
    }
    dst->vt = vtRef;
    dst->flags = 0;
    dst->valueRef = val;
}

static void refLvalue(ZorroVM* vm, Value* src, Value* dst)
{
    Value& val = vm->getLValue(*src);
    if(val.vt != vtRef)
    {
        ValueRef* rval = vm->allocRef();
        rval->ref();
        rval->value = val;
        if(&val != dst)
        {
            if(ZISREFTYPE(&val))
            {
                val.refBase->ref();
            }
            ZUNREF(vm, dst);
        }
        dst->vt = vtRef;
        dst->valueRef = rval;
        if(&val != dst)
        {
            dst->flags = ValFlagARef;
            ZASSIGN(vm, &val, dst);
        }
        dst->flags = 0;
    } else
    {
        val.flags |= ValFlagARef;
        ZASSIGN(vm, dst, &val);
        val.flags = 0;
    }
}

static void refSegment(ZorroVM* vm, Value* src, Value* dst)
{
    Segment& seg = *src->seg;
    if(seg.cont.vt != vtArray)
    {
        ZTHROWR(TypeException, vm, "Attempt to create reference to index of type %{}", getValueTypeName(seg.cont.vt));
    }
    if(seg.segEnd - seg.segStart != 1)
    {
        ZTHROWR(TypeException, vm, "Attempt to create reference to more then one element of segment");
    }
    ZArray& za = *seg.cont.arr;
    if(seg.segStart < 0 || seg.segStart >= (int64_t) za.getCount())
    {
        throwOutOfBounds(vm, "Segment index of array out of bounds", seg.segStart, za.getCount());
    }
    Value& val = za.getItemRef(static_cast<size_t>(seg.segStart));
    if(val.vt != vtRef)
    {
        ValueRef* rval = vm->allocRef();
        rval->value = val;
        if(ZISREFTYPE(&val))
        {
            val = NilValue;
        }
        ZUNREF(vm, dst);
        dst->vt = vtRef;
        dst->valueRef = rval;
        dst->flags = ValFlagARef;
        ZASSIGN(vm, &val, dst);
        dst->flags = 0;
    } else
    {
        val.flags |= ValFlagARef;
        ZASSIGN(vm, dst, &val);
        val.flags = 0;
    }
}

static void refKeyRef(ZorroVM* vm, Value* src, Value* dst)
{
    KeyRef& p = *src->keyRef;
    Value* item;
    if(p.obj.vt == vtMap)
    {
        ZMap::iterator it = p.obj.map->insert(p.name, NilValue, false);
        item = &it->m_value;
    } else
    {
        ZTHROWR(TypeException, vm, "Attempt to create reference to key of type %{}", getValueTypeName(p.obj.vt));
    }
    if(item->vt == vtRef)
    {
        item->flags |= ValFlagARef;
        ZASSIGN(vm, dst, item)
        dst->flags = ValFlagARef;
    } else
    {
        ZUNREF(vm, dst);
        ValueRef* rval = vm->allocRef();
        rval->ref();//item
        rval->ref();//dst
        rval->value = *item;//even if item is ref, now rval->value holds it
        //vm->assign(rval->value,*item);
        dst->vt = vtRef;
        dst->flags = ValFlagARef;
        dst->valueRef = rval;
        *item = *dst;//here item holds
        dst->flags = ValFlagARef;
    }
}

static void refMemberRef(ZorroVM* vm, Value* src, Value* dst)
{
    MemberRef& p = *src->membRef;
    ClassInfo* ci = p.obj.obj->classInfo;
    SymInfo** pinfo = ci->symMap.getPtr(p.name);
    Value* item = nullptr;
    if(!pinfo)
    {
        if(ci->specialMethods[csmGetProp])
        {
            item = &p.getValue;
        } else
        {
            ZTHROWR(TypeException, vm, "Class %{} do not have property or member %{}", ci->name,
                    ZStringRef(vm, p.name));
        }
    }
    if(!item)
    {
        SymInfo* info = *pinfo;
        if(info->st != sytClassMember)
        {
            if(info->st == sytProperty)
            {
                auto* cp = (ClassPropertyInfo*) info;
                if(cp->getMethod)
                {
                    item = &p.getValue;
                } else if(cp->getIdx != SymInfo::invalidIndexValue)
                {
                    item = p.obj.obj->members + cp->getIdx;
                }
            }
            if(!item)
            {
                ZTHROWR(TypeException, vm, "%{} is not property or data member of class %{}", ZStringRef(vm, p.name),
                        ci->name);
            }
        } else
        {
            item = p.obj.obj->members + info->index;
        }
    }

    if(item->vt == vtRef)
    {
        item->flags |= ValFlagARef;
        ZASSIGN(vm, dst, item)
        dst->flags = ValFlagARef;
    } else
    {
        ZUNREF(vm, dst);
        ValueRef* rval = vm->allocRef();
        rval->ref();//item
        rval->ref();//dst
        rval->value = *item;//even if item is ref, now rval->value holds it
        //vm->assign(rval->value,*item);
        dst->vt = vtRef;
        dst->flags = ValFlagARef;
        dst->valueRef = rval;
        *item = *dst;//here item holds
        dst->flags = ValFlagARef;
    }
}


static bool anyToFalse(ZorroVM* /*vm*/, const Value* /*src*/)
{
    return false;
}

static bool anyToTrue(ZorroVM* /*vm*/, const Value* /*src*/)
{
    return true;
}


static bool objToBool(ZorroVM* vm, const Value* src)
{
    ClassInfo* ci = src->obj->classInfo;
    bool lastOpCondJump =
        vm->ctx.lastOp->ot == otCondJump || vm->ctx.lastOp->ot == otNot;// || vm->ctx.lastOp->ot==otJumpIfNot;
    if(ci->specialMethods[csmBoolCheck])
    {
        OpBase* fb = nullptr;
        if(vm->ctx.lastOp->ot == otCondJump || vm->ctx.lastOp->ot == otJumpIfNot)
        {
            auto* jmp = (OpJumpBase*) vm->ctx.lastOp;
            fb = &jmp->fallback;
            //lastOpCondJump=false;
        } else if(vm->ctx.lastOp->ot == otJumpFallback)
        {
            fb = vm->ctx.lastOp;
        } else if(vm->ctx.lastOp->ot == otCall)
        {
            fb = vm->ctx.nextOp;
        }
        if(fb)
        {
            if(vm->ctx.lastOp->ot != otCall)
            {
                vm->ctx.lastOp = fb;
            }
            vm->ctx.nextOp = fb;
            vm->callMethod((Value*) src, ci->specialMethods[csmBoolCheck], 0, false);
        }
    } else
    {
        lastOpCondJump = lastOpCondJump || vm->ctx.lastOp->ot == otJumpIfNot;
    }
    return lastOpCondJump;
}


static bool refToBool(ZorroVM* vm, const Value* src)
{
    return vm->boolOps[src->valueRef->value.vt](vm, &src->valueRef->value);
}

static bool boolToBool(ZorroVM* /*vm*/, const Value* src)
{
    return src->bValue;
}

static bool intToBool(ZorroVM* /*vm*/, const Value* src)
{
    return src->iValue != 0;
}

static bool doubleToBool(ZorroVM* /*vm*/, const Value* src)
{
    return src->dValue != 0;
}

static bool intToNotBool(ZorroVM* /*vm*/, const Value* src)
{
    return src->iValue == 0;
}

static bool doubleToNotBool(ZorroVM* /*vm*/, const Value* src)
{
    return src->dValue == 0;
}

/*
static void doubleToBool(ZorroVM* vm,Value* src)
{
  vm->assignReg(BoolValue(src->dValue!=0));
}


static void eqBoolAny(ZorroVM* vm,Value* l,const Value* r,Value* dst)
{
  ZUNREF(vm,*dst);
  bool lv=l->bValue;
  vm->boolOps[r->vt](vm,(Value*)r);
  vm->assignReg(BoolValue(lv==vm->reg1.bValue));
}

static void eqAnyBool(ZorroVM* vm,Value* l,const Value* r)
{
  bool rv=r->bValue;
  vm->boolOps[l->vt](vm,l);
  vm->assignReg(BoolValue(rv==vm->reg1.bValue));
}

*/

#define BOP(name, op) static bool name(ZorroVM* vm,const Value* l,const Value* r)\
{\
  (void)vm;\
  (void)l;\
  (void)r;\
  return op;\
}

BOP(eqStrStr, *l->str == *r->str)

BOP(neqStrStr, !(*l->str == *r->str))

BOP(eqAlwaysFalse, false)

BOP(eqAlwaysTrue, true)

BOP(eqMapMap, l->map == r->map)

BOP(eqSetSet, l->set == r->set)

BOP(neqSetSet, l->set != r->set)

BOP(eqCloClo, l->cls == r->cls)

BOP(neqCloClo, l->cls != r->cls)

BOP(eqCorCor, l->cor == r->cor)

BOP(neqCorCor, l->cor != r->cor)

BOP(eqBoolBool, l->bValue == r->bValue)

BOP(eqIntInt, l->iValue == r->iValue)

BOP(eqIntDouble, l->iValue == r->dValue)

BOP(eqDoubleInt, l->dValue == r->iValue)

BOP(eqDoubleDouble, l->dValue == r->dValue)

BOP(eqClassClass, l->classInfo == r->classInfo)

BOP(eqObjObj, l->obj == r->obj)

BOP(eqFunFun, l->func == r->func)

BOP(eqArrArr, l->arr == r->arr)

BOP(eqMethMeth, l->method == r->method)

BOP(neqBoolBool, l->bValue != r->bValue)

BOP(neqIntInt, l->iValue != r->iValue)

BOP(neqIntDouble, l->iValue != r->dValue)

BOP(neqDoubleInt, l->dValue != r->iValue)

BOP(neqDoubleDouble, l->dValue != r->dValue)

BOP(neqClassClass, l->classInfo != r->classInfo)

BOP(inAnyMap, r->map->find(*l) != r->map->end())

BOP(inAnySet, r->set->find(*l) != r->set->end())
//BOP(inIntRange,l->iValue>=r->range->start && l->iValue<=r->range->end)
BOP(inStrObj, r->obj->classInfo->symMap.findSymbol(l->str) != nullptr)

static bool inIntRange(ZorroVM* vm, const Value* l, const Value* r)
{
    (void) vm;
    return l->iValue >= r->range->start && l->iValue <= r->range->end &&
           (r->range->step == 0 || ((l->iValue - r->range->start) % r->range->step) == 0);
}

static bool eqStrSegment(ZorroVM* vm, const Value* l, const Value* r)
{
    if(r->seg->cont.vt != vtString)
    {
        return false;
    }
    int64_t start = r->seg->segStart;
    int64_t len = r->seg->segEnd - start;
    ZString* str = r->seg->cont.str;
    auto strLen = (int64_t) str->getLength();
    if(start < 0 || start >= strLen || start + len > strLen)
    {
        return false;
    }
    return l->str->equalsTo(str->c_str(vm) + start, static_cast<size_t>(len));//!!todo fix this!
}

static bool eqSegmentStr(ZorroVM* vm, const Value* l, const Value* r)
{
    if(l->seg->cont.vt != vtString)
    {
        return false;
    }
    int64_t start = l->seg->segStart;
    int64_t len = l->seg->segEnd - start;
    ZString* str = l->seg->cont.str;
    auto strLen = (int64_t) str->getLength();
    if(start < 0 || start >= strLen || start + len > strLen)
    {
        return false;
    }
    return r->str->equalsTo(str->c_str(vm) + start, static_cast<size_t>(len));//!!todo fix this!
}

static bool eqSegmentSegment(ZorroVM* /*vm*/, const Value* l, const Value* r)
{
    if(l->seg->cont.vt != vtString || r->seg->cont.vt != vtString)
    {
        return false;
    }
    int64_t start1 = l->seg->segStart;
    int64_t len1 = l->seg->segEnd - start1;
    ZString* str1 = l->seg->cont.str;
    auto strLen1 = (int64_t) str1->getLength();
    if(start1 < 0 || start1 >= strLen1 || start1 + len1 > strLen1)
    {
        return false;
    }

    int64_t start2 = r->seg->segStart;
    int64_t len2 = r->seg->segEnd - start1;
    ZString* str2 = r->seg->cont.str;
    auto strLen2 = (int64_t) str2->getLength();
    if(start2 < 0 || start2 >= strLen2 || start2 + len2 > strLen2)
    {
        return false;
    }
    if(len1 != len2)
    {
        return false;
    }

    return ZString::compare(str1->getDataPtr() + start1, str1->getCharSize(), static_cast<uint32_t>(len1),
                            str2->getDataPtr(), str2->getCharSize(), static_cast<uint32_t>(len2)) == 0;//!!todo fix this!
}


static bool eqRefAny(ZorroVM* vm, const Value* l, const Value* r)
{
    return vm->eqMatrix[l->valueRef->value.vt][r->vt](vm, &l->valueRef->value, r);
}

static bool eqAnyRef(ZorroVM* vm, const Value* l, const Value* r)
{
    return vm->eqMatrix[l->vt][r->valueRef->value.vt](vm, l, &r->valueRef->value);
}


static bool eqDlgDlg(ZorroVM* vm, const Value* l, const Value* r)
{
    return l->dlg->method == r->dlg->method &&
           vm->eqMatrix[l->dlg->obj.vt][r->dlg->obj.vt](vm, &l->dlg->obj, &r->dlg->obj);
}

/*static bool inAnyRef(ZorroVM* vm,Value* l,const Value* r)
{
  return vm->inMatrix[l->vt][r->valueRef->value.vt](vm,l,&r->valueRef->value);
}

static bool inRefRef(ZorroVM* vm,Value* l,const Value* r)
{
  return vm->inMatrix[l->valueRef->value.vt][r->valueRef->value.vt](vm,&l->valueRef->value,&r->valueRef->value);
}*/

BINBOOLREFOPS(in, inMatrix)

BOP(isNilClass, r->classInfo == vm->nilClass)

BOP(isBoolClass, r->classInfo == vm->boolClass)

BOP(isIntClass, r->classInfo == vm->intClass)

BOP(isDoubleClass, r->classInfo == vm->doubleClass)

BOP(isStringClass, r->classInfo == vm->stringClass)

BOP(isArrayClass, r->classInfo == vm->arrayClass)

BOP(isSetClass, r->classInfo == vm->setClass)

BOP(isMapClass, r->classInfo == vm->mapClass)

BOP(isClassClass, r->classInfo == vm->classClass)

BOP(isObjClass, l->obj->classInfo->isInParents(r->classInfo))

BOP(isNObjClass, l->nobj->classInfo->isInParents(r->classInfo))

BOP(isClosureClass, vm->clsClass->isInParents(r->classInfo))

BOP(isCoroutingClass, vm->corClass->isInParents(r->classInfo))

BOP(isDelegateClass, vm->dlgClass->isInParents(r->classInfo))

BOP(isFuncClass, vm->funcClass->isInParents(r->classInfo))


BINBOOLREFOPS(is, isMatrix)


BOP(lessStrStr, l->str->compare(*r->str) < 0)

BOP(greaterStrStr, l->str->compare(*r->str) > 0)

BOP(lessEqStrStr, l->str->compare(*r->str) <= 0)

BOP(greaterEqStrStr, l->str->compare(*r->str) >= 0)

BOP(lessIntInt, l->iValue < r->iValue)

BOP(lessIntDouble, l->iValue < r->dValue)

BOP(lessDoubleInt, l->dValue < r->iValue)

BOP(lessDoubleDouble, l->dValue < r->dValue)

BOP(lessEqIntInt, l->iValue <= r->iValue)

BOP(lessEqIntDouble, l->iValue <= r->dValue)

BOP(lessEqDoubleInt, l->dValue <= r->iValue)

BOP(lessEqDoubleDouble, l->dValue <= r->dValue)

BOP(greaterIntInt, l->iValue > r->iValue)

BOP(greaterIntDouble, l->iValue > r->dValue)

BOP(greaterDoubleInt, l->dValue > r->iValue)

BOP(greaterDoubleDouble, l->dValue > r->dValue)

BOP(greaterEqIntInt, l->iValue >= r->iValue)

BOP(greaterEqIntDouble, l->iValue >= r->dValue)

BOP(greaterEqDoubleInt, l->dValue >= r->iValue)

BOP(greaterEqDoubleDouble, l->dValue >= r->dValue)

static bool lessRefAny(ZorroVM* vm, const Value* l, const Value* r)
{
    return vm->lessMatrix[l->valueRef->value.vt][r->vt](vm, &l->valueRef->value, r);
}

static bool lessEqRefAny(ZorroVM* vm, const Value* l, const Value* r)
{
    return vm->lessEqMatrix[l->valueRef->value.vt][r->vt](vm, &l->valueRef->value, r);
}

static bool lessAnyRef(ZorroVM* vm, const Value* l, const Value* r)
{
    return vm->lessMatrix[l->vt][r->valueRef->value.vt](vm, l, &r->valueRef->value);
}

static bool lessEqAnyRef(ZorroVM* vm, const Value* l, const Value* r)
{
    return vm->lessEqMatrix[l->vt][r->valueRef->value.vt](vm, l, &r->valueRef->value);
}

static bool greaterRefAny(ZorroVM* vm, const Value* l, const Value* r)
{
    return vm->greaterMatrix[l->valueRef->value.vt][r->vt](vm, &l->valueRef->value, r);
}

static bool greaterEqRefAny(ZorroVM* vm, const Value* l, const Value* r)
{
    return vm->greaterEqMatrix[l->valueRef->value.vt][r->vt](vm, &l->valueRef->value, r);
}

static bool greaterAnyRef(ZorroVM* vm, const Value* l, const Value* r)
{
    return vm->greaterMatrix[l->vt][r->valueRef->value.vt](vm, l, &r->valueRef->value);
}

static bool greaterEqAnyRef(ZorroVM* vm, const Value* l, const Value* r)
{
    return vm->greaterMatrix[l->vt][r->valueRef->value.vt](vm, l, &r->valueRef->value);
}

static bool lessObjAny(ZorroVM* vm, const Value* l, const Value* r)
{
    ClassInfo* ci = l->obj->classInfo;
    if(!ci->specialMethods[csmLess])
    {
        ZTHROWR(TypeException, vm, "Class %{} do not have operator less", ci->name);
    }
    OpBase* fb = nullptr;
    if(vm->ctx.lastOp->ot == otJumpIfLess)
    {
        auto* jmp = (OpJumpBase*) vm->ctx.lastOp;
        fb = &jmp->fallback;
    } else if(vm->ctx.lastOp->ot == otJumpFallback)
    {
        fb = vm->ctx.lastOp;
    }
    if(fb)
    {
        vm->ctx.lastOp = fb;
        vm->ctx.nextOp = fb;
        vm->pushValue(*r);
        vm->callMethod((Value*) l, ci->specialMethods[csmLess], 1, false);
    }
    return true;
}


static void makeNObjectMemberRef(ZorroVM* vm, Value* l, const Value* r, Value* dst)
{
    Value res;
    res.vt = vtMemberRef;
    res.flags = ValFlagARef;
    MemberRef& m = *(res.membRef = vm->allocMembRef());
    m.obj = *l;
    SymInfo* sym = l->nobj->classInfo->symMap.findSymbol(r->str);
    m.getValue = NilValue;
    if(sym && sym->st == sytProperty)
    {
        ClassPropertyInfo& cp = *(ClassPropertyInfo*) sym;
        if(cp.getMethod)
        {
            vm->callCMethod(&m.obj, cp.getMethod->index, 0);
            ZASSIGN(vm, &m.getValue, &vm->result);
            vm->clearResult();
        }
    }
    l->refBase->ref();
    m.name = r->str;
    r->str->ref();
    m.simpleMember = false;
    ZASSIGN(vm, dst, &res);

}

static void makeObjectMemberRef(ZorroVM* vm, Value* l, const Value* r, Value* dst)
{
    Value res;
    res.vt = vtMemberRef;
    res.flags = ValFlagARef;

    MemberRef& m = *(res.membRef = vm->allocMembRef());
    m.obj = *l;
    m.getValue = NilValue;
    l->refBase->ref();
    m.name = r->str;
    r->str->ref();
    m.simpleMember = false;
    ZASSIGN(vm, dst, &res);
    //dst->flags=ValFlagARef;

    ClassInfo* ci = l->obj->classInfo;
    SymInfo* sym = ci->symMap.findSymbol(r->str);
    if(sym)
    {
        if(sym->st == sytProperty)
        {
            auto* cp = (ClassPropertyInfo*) sym;
            if(cp->setIdx == SymInfo::invalidIndexValue && cp->setMethod == nullptr)
            {
                ZTHROWR(TypeException, vm, "Property %{} of class %{} is read only", ZStringRef(vm, r->str), ci->name);
            }
            if(cp->getMethod)
            {
                vm->callMethod(l, cp->getMethod, 0);
                //dst->flags=ValFlagARef;
            }
            if(!cp->setMethod)
            {
                m.simpleMember = true;
                m.memberIndex = cp->setIdx;
            }
        } else
        {
            m.simpleMember = true;
            m.memberIndex = sym->index;
        }
    } else
    {
        size_t idx = ci->specialMethods[csmGetProp];
        if(idx)
        {
            if(!ci->specialMethods[csmSetProp])
            {
                ZTHROWR(TypeException, vm, "Class %{} do not have operator setProp", ci->name);
            }
            Value* nmarg = vm->pushData();
            *nmarg = *r;
            r->str->ref();
            vm->callMethod(l, idx, 1);
        }
    }
}

static void makeObjectMemberRefFromRef(ZorroVM* vm, Value* l, const Value* r, Value* dst)
{
    vm->mkMemberMatrix[l->valueRef->value.vt][r->vt](vm, &l->valueRef->value, r, dst);
}


static void makeArrayIndex(ZorroVM* vm, Value* l, const Value* r, Value* dst)
{
    Value tmp = *l;
    size_t index = (size_t) r->iValue;
    ZUNREF(vm, dst);

    dst->vt = vtSegment;
    dst->flags = ValFlagARef;
    dst->seg = vm->allocSegment();
    dst->seg->ref();
    dst->seg->cont = tmp;
    dst->seg->cont.arr->ref();
    //vm->assign(res.seg->cont,*l);
    dst->seg->segStart = index;
    dst->seg->segEnd = index + 1;
    dst->seg->step = 1;
    if(l == dst)
    {
        ZUNREF(vm, &tmp);
    }
}

static void makeSliceIndex(ZorroVM* vm, Value* l, const Value* r, Value* dst)
{
    Value* cnt = &l->slice->cont;
    ZArray& za = *l->slice->indeces.arr;
    int64_t idx = r->iValue;
    if(idx < 0 || idx >= (int64_t) za.getCount())
    {
        throwOutOfBounds(vm, "Slice index of array out of bounds", idx, za.getCount());
    }
    Value* rIdx = &za.getItem(static_cast<size_t>(idx));
    if(cnt->vt == vtMap)
    {
        vm->mkKeyRefMatrix[cnt->vt][rIdx->vt](vm, cnt, rIdx, dst);
    } else if(cnt->vt == vtObject)
    {
        vm->mkMemberMatrix[cnt->vt][rIdx->vt](vm, cnt, rIdx, dst);
    } else
    {
        vm->mkIndexMatrix[cnt->vt][rIdx->vt](vm, cnt, rIdx, dst);
    }


}

static void makeObjectIndex(ZorroVM* vm, Value* l, const Value* r, Value* dst)
{
    size_t midx = l->obj->classInfo->specialMethods[csmGetIndex];
    if(!midx)
    {
        ZTHROWR(TypeException, vm, "Class %{} do not have operator getIndex", l->obj->classInfo->name);
    }

    Value tmp = *l;
    auto index = (size_t) r->iValue;
    ZUNREF(vm, dst);

    dst->vt = vtSegment;
    dst->flags = ValFlagARef;
    Segment& seg = *(dst->seg = vm->allocSegment());
    seg.ref();
    seg.cont = tmp;
    seg.cont.obj->ref();
    seg.segStart = index;
    seg.segEnd = index + 1;
    seg.step = 1;
    seg.getValue = NilValue;
    if(l == dst)
    {
        ZUNREF(vm, &tmp);
    }
    vm->pushValue(*r);
    vm->callMethod(l, midx, 1);
}

static void makeNObjectIndex(ZorroVM* vm, Value* l, const Value* r, Value* dst)
{
    size_t midx = l->nobj->classInfo->specialMethods[csmGetIndex];
    if(!midx)
    {
        ZTHROWR(TypeException, vm, "Class %{} do not have operator getIndex", l->nobj->classInfo->name);
    }

    Value tmp = *l;
    auto index = (size_t) r->iValue;
    ZUNREF(vm, dst);

    dst->vt = vtSegment;
    dst->flags = ValFlagARef;
    Segment& seg = *(dst->seg = vm->allocSegment());
    seg.ref();
    seg.cont = tmp;
    seg.cont.nobj->ref();
    seg.segStart = index;
    seg.segEnd = index + 1;
    seg.step = 1;
    seg.getValue = NilValue;
    if(l == dst)
    {
        ZUNREF(vm, &tmp);
    }
}


static void setArrayIndex(ZorroVM* vm, Value* arr, const Value* idx, const Value* item, Value* dst)
{
    Value* dstItem = &arr->arr->getItemRef((size_t) idx->iValue);
    ZASSIGN(vm, dstItem, item);
    if(ZISREFTYPE(item))
    {
        arr->arr->isSimpleContent = false;
    }
    if(dst)
    {
        ZASSIGN(vm, dst, item);
    }
}

static void setSliceIndex(ZorroVM* vm, Value* arr, const Value* vidx, const Value* item, Value* dst)
{
    Value* cnt = &arr->slice->cont;
    ZArray& za = *arr->slice->indeces.arr;
    int64_t idx = vidx->iValue;
    if(idx < 0 || idx >= (int64_t) za.getCount())
    {
        throwOutOfBounds(vm, "Slice index out of bounds", idx, za.getCount());
    }
    Value* rIdx = &za.getItem(static_cast<size_t>(idx));
    if(cnt->vt == vtMap)
    {
        vm->setKeyMatrix[cnt->vt][rIdx->vt](vm, cnt, rIdx, item, dst);
    } else if(cnt->vt == vtObject)
    {
        vm->setMemberMatrix[cnt->vt][rIdx->vt](vm, cnt, rIdx, item, dst);
    } else
    {
        vm->setIndexMatrix[cnt->vt][rIdx->vt](vm, cnt, rIdx, item, dst);
    }

}

static void makeArrayIndexRefInt(ZorroVM* vm, Value* l, const Value* r, Value* dst)
{
    vm->mkIndexMatrix[l->valueRef->value.vt][r->vt](vm, &l->valueRef->value, r, dst);
}

static void makeArrayIndexArrRef(ZorroVM* vm, Value* l, const Value* r, Value* dst)
{
    vm->mkIndexMatrix[l->vt][r->valueRef->value.vt](vm, l, &r->valueRef->value, dst);
}


static void makeArrayIndexRefRef(ZorroVM* vm, Value* l, const Value* r, Value* dst)
{
    vm->mkIndexMatrix[l->valueRef->value.vt][r->valueRef->value.vt](vm, &l->valueRef->value, &r->valueRef->value, dst);
}


static void indexArrayInt(ZorroVM* vm, Value* l, const Value* r, Value* dst)
{
    if((size_t) r->iValue >= l->arr->getCount() || r->iValue < 0)
    {
        throwOutOfBounds(vm, "Array index is out of bounds", r->iValue, l->arr->getCount());
    }
    Value* item = &l->arr->getItem((size_t) r->iValue);
    ZASSIGN(vm, dst, item);
}

static void indexStrInt(ZorroVM* vm, Value* l, const Value* r, Value* dst)
{
    if((size_t) r->iValue >= l->str->getLength() || r->iValue < 0)
    {
        throwOutOfBounds(vm, "String index is out of bounds", r->iValue, l->str->getLength());
    }
    if(!dst)
    {
        return;
    }
    Value val;
    val.vt = vtSegment;
    val.flags = ValFlagNone;
    Segment& seg = *(val.seg = vm->allocSegment());
    seg.cont = *l;
    l->str->ref();
    seg.segStart = r->iValue;
    seg.segEnd = r->iValue + 1;
    seg.step = 1;
    ZASSIGN(vm, dst, &val);
}


static void indexSegmentInt(ZorroVM* vm, Value* l, const Value* r, Value* dst)
{
    if(l->seg->cont.vt == vtArray)
    {
        int64_t idx = l->seg->step > 0 ? l->seg->segStart + r->iValue : l->seg->segEnd - r->iValue - 1;
        if((size_t) idx >= l->seg->cont.arr->getCount() || idx < 0 ||
           (l->seg->step > 0 ? idx >= l->seg->segEnd : idx <= l->seg->segStart))
        {
            throwOutOfBounds(vm, "Array index is out of bounds", idx, l->seg->cont.arr->getCount());
        }
        Value* item = &l->seg->cont.arr->getItem((size_t) idx);
        ZASSIGN(vm, dst, item);
    } else //string
    {
        ZTHROWR(RuntimeException, vm, "Index of string segment is not implemented yet");
    }
}

static void indexSliceInt(ZorroVM* vm, Value* l, const Value* r, Value* dst)
{
    Value* cnt = &l->slice->cont;
    ZArray& za = *l->slice->indeces.arr;
    int64_t idx = r->iValue;
    if(idx < 0 || idx >= (int64_t) za.getCount())
    {
        throwOutOfBounds(vm, "Slice index is out of bounds", idx, za.getCount());
    }
    Value* rIdx = &za.getItem(static_cast<size_t>(idx));
    if(cnt->vt == vtMap)
    {
        vm->getKeyMatrix[cnt->vt][rIdx->vt](vm, cnt, rIdx, dst);
    } else if(cnt->vt == vtObject)
    {
        vm->getMemberMatrix[cnt->vt][rIdx->vt](vm, cnt, rIdx, dst);
    } else
    {
        vm->getIndexMatrix[cnt->vt][rIdx->vt](vm, cnt, rIdx, dst);
    }
}

static void indexAnyArray(ZorroVM* vm, Value* l, const Value* r, Value* dst)
{
    if(!dst)
    {
        return;
    }
    Slice* slc = vm->allocSlice();
    Value res;
    res.vt = vtSlice;
    res.flags = ValFlagNone;
    res.slice = slc;
    ZASSIGN(vm, &slc->cont, l);
    ZASSIGN(vm, &slc->indeces, r);
    ZASSIGN(vm, dst, &res);
}


static void getArrayIndexRefInt(ZorroVM* vm, Value* l, const Value* r, Value* dst)
{
    vm->getIndexMatrix[l->valueRef->value.vt][r->vt](vm, &l->valueRef->value, r, dst);
}

static void getArrayIndexArrRef(ZorroVM* vm, Value* l, const Value* r, Value* dst)
{
    vm->getIndexMatrix[l->vt][r->valueRef->value.vt](vm, l, &r->valueRef->value, dst);
}

static void getArrayIndexRefRef(ZorroVM* vm, Value* l, const Value* r, Value* dst)
{
    vm->getIndexMatrix[l->valueRef->value.vt][r->valueRef->value.vt](vm, &l->valueRef->value, &r->valueRef->value, dst);
}

OVERLOAD1(getIndex, csmGetIndex)

//OVERLOAD2(setIndex,csmSetIndex)
static void setIndexObjAny(ZorroVM* vm, Value* l, const Value* a, const Value* r, Value* dst)
{
    ClassInfo* ci = l->obj->classInfo;
    if(!ci->specialMethods[csmSetIndex])
    {
        ZTHROWR(TypeException, vm, "Class %{} do not have operator setIndex", ci->name);
    }
    vm->pushValue(*a);
    vm->pushValue(*r);
    if(dst)
    {
        ZASSIGN(vm, dst, r);
    }
    vm->callMethod(l, vm->symbols.globals[ci->specialMethods[csmSetIndex]].method, 2);
}


OVERLOAD1(getKey, csmGetKey)

//OVERLOAD2(setKey,csmSetKey)
static void setKeyObjAny(ZorroVM* vm, Value* l, const Value* a, const Value* r, Value* dst)
{
    ClassInfo* ci = l->obj->classInfo;
    if(!ci->specialMethods[csmSetKey])
    {
        ZTHROWR(TypeException, vm, "Class %{} do not have operator setKet", ci->name);
    }
    vm->pushValue(*a);
    vm->pushValue(*r);
    if(dst)
    {
        ZASSIGN(vm, dst, r);
    }
    vm->callMethod(l, vm->symbols.globals[ci->specialMethods[csmSetKey]].method, 2);
}


static void getIndexNObjInt(ZorroVM* vm, Value* l, const Value* r, Value* dst)
{
    ClassInfo* ci = l->nobj->classInfo;
    size_t gidx = ci->specialMethods[csmGetIndex];
    if(!gidx)
    {
        ZTHROWR(TypeException, vm, "Native class %{} do not have operator getIndex", ci->name);
    }
    vm->ctx.dataStack.push(*r);
    if(ZISREFTYPE(r))
    {
        r->refBase->ref();
    }
    vm->callCMethod(l, gidx, 1);
    ZASSIGN(vm, dst, &vm->result);
    vm->clearResult();

}

static void setIndexNObjInt(ZorroVM* vm, Value* l, const Value* a, const Value* r, Value* dst)
{
    ClassInfo* ci = l->nobj->classInfo;
    size_t sidx = ci->specialMethods[csmSetIndex];
    if(!sidx)
    {
        ZTHROWR(TypeException, vm, "Native class %{} do not have operator setIndex", ci->name);
    }
    vm->pushValue(*a);
    vm->pushValue(*r);
    vm->callCMethod(l, sidx, 2);
    if(dst)
    {
        ZASSIGN(vm, dst, r);
    }
    vm->clearResult();

}


/*static void indexObjAny(ZorroVM* vm,Value* l,const Value* r,Value* dst)
{
  ClassInfo* ci=l->obj->classInfo;
  if(!ci->specialMethods[csmGetIndex])
  {
    ZTHROW0(ZorroException,"getIndex not defined in class %{}",ci->name);
  }
  vm->ctx.dataStack.push(*r);
  if(ZISREFTYPE(r))
  {
    r->refBase->ref();
  }
  vm->callMethod(l,vm->symbols.globals[ci->specialMethods[csmGetIndex]].method,1);
}*/



static void makeSegment(ZorroVM* vm, Value* l, const Value* r, Value* dst)
{
    if(!dst)
    {
        return;
    }
    Value res;
    res.vt = vtSegment;
    res.flags = ValFlagNone;
    res.seg = vm->allocSegment();
    res.seg->cont = *l;
    l->refBase->ref();
    if(r->range->step == 1)
    {
        res.seg->segStart = r->range->start;
        res.seg->segEnd = r->range->end + 1;
        res.seg->step = 1;
    } else if(r->range->step == -1)
    {
        res.seg->segStart = r->range->end;
        res.seg->segEnd = r->range->start + 1;
        res.seg->step = -1;
    } else
    {
        ZTHROWR(RuntimeException, vm, "Invalid value for step of segment:%{}", r->range->step);
    }
    ZASSIGN(vm, dst, &res);
}

static void makeMapKeyRef(ZorroVM* vm, Value* l, const Value* r, Value* dst)
{
    KeyRef* kr = vm->ZorroVM::allocKeyRef();
    kr->obj = *l;
    kr->name = *r;
    if(ZISREFTYPE(l))
    {
        l->refBase->ref();
    }
    if(ZISREFTYPE(r))
    {
        r->refBase->ref();
    }
    Value val;
    val.vt = vtKeyRef;
    val.flags = ValFlagARef;
    val.keyRef = kr;
    ZASSIGN(vm, dst, &val);
    dst->flags = ValFlagARef;
}

static void getKeyValue(ZorroVM* vm, Value* l, const Value* r, Value* dst)
{
    ZMap::iterator it = l->map->find(*r);
    if(it == l->map->end())
    {
        ZTHROWR(NoSuchKeyException, vm, "No such key %{} in map", ValueToString(vm, *r));
    }
    ZASSIGN(vm, dst, &it->m_value);
}

static void setKeyValue(ZorroVM* vm, Value* l, const Value* a, const Value* r, Value* dst)
{
    l->map->insert(*a, *r);
    if(dst)
    {
        ZASSIGN(vm, dst, r);
    }
}


struct FormatFlags {
    bool hex;
    bool uchex;
    bool left;
    bool right;
    bool zero;

    FormatFlags() : hex(false), uchex(false), left(false), right(false), zero(false)
    {

    }
};

static void parseFlags(ZorroVM* vm, ZString* flags, FormatFlags& ff)
{
    if(!flags)
    {
        return;
    }
    CStringWrap wrap(nullptr, nullptr, 0);
    const char* begin = flags->c_str(vm, wrap);
    const char* end = begin + wrap.getLength();
    while(begin != end)
    {
        switch(*begin)
        {
            case 'X':
                ff.hex = ff.uchex = true;
                break;
            case 'x':
                ff.hex = true;
                break;
            case 'l':
                ff.left = true;
                break;
            case 'r':
                ff.right = true;
                break;
            case 'z':
                ff.zero = true;
                break;
        }
        ++begin;
    }
}

static void formatRef(ZorroVM* vm, Value* v, int w, int p, ZString* flags, Value* extra, Value* dst)
{
    vm->fmtOps[v->valueRef->value.vt](vm, &v->valueRef->value, w, p, flags, extra, dst);
}

static void formatNil(ZorroVM* vm, Value* /*v*/, int w, int p, ZString* flags, Value* extra, Value* dst)
{
    Value val;
    val.vt = vtString;
    val.flags = ValFlagNone;
    val.str = vm->symbols.nilStr.get();
    vm->fmtOps[val.vt](vm, &val, w, p, flags, extra, dst);
}

static void formatBool(ZorroVM* vm, Value* v, int w, int p, ZString* flags, Value* extra, Value* dst)
{
    Value val;
    val.vt = vtString;
    val.flags = ValFlagNone;
    val.str = v->bValue ? vm->symbols.trueStr.get() : vm->symbols.falseStr.get();
    vm->fmtOps[val.vt](vm, &val, w, p, flags, extra, dst);
}


static void formatInt(ZorroVM* vm, Value* v, int w, int /*p*/, ZString* flags, Value*, Value* dst)
{
    if(!dst)
    {
        return;
    }
    kst::FormatBuffer buf;
    FormatFlags ff;
    parseFlags(vm, flags, ff);
    if(ff.hex)
    {
        if(ff.uchex)
        {
            kst::format((buf.getArgList(), "%{X}", v->iValue));
            //len=snprintf(buf, sizeof(buf), "%" PRIx64,v->iValue);
        } else
        {
            //len=snprintf(buf, sizeof(buf), "%" PRIx64,v->iValue);
            kst::format((buf.getArgList(), "%{x}", v->iValue));
        }
    } else
    {
        //len=snprintf(buf, sizeof(buf), "%" PRId64,v->iValue);
        kst::format((buf.getArgList(), "%{}", v->iValue));
    }
    size_t len = buf.Length();
    ZString* res = vm->allocZString();
    size_t slen = len;
    if(w > static_cast<int>(slen))
    {
        slen = static_cast<size_t>(w);
    }
    char* strbuf = vm->allocStr(slen + 1);
    if(w > static_cast<int>(len))
    {
        if(ff.left)
        {
            memcpy(strbuf, buf.Str(), buf.Length());
            memset(strbuf + len, ' ', w - len);
        } else
        {
            memset(strbuf, ff.zero ? '0' : ' ', w - len);
            memcpy(strbuf + w - len, buf.Str(), buf.Length());
        }
    } else
    {
        memcpy(strbuf, buf.Str(), buf.Length());
    }
    strbuf[slen] = 0;
    res->init(strbuf, slen, false);
    Value val;
    val.vt = vtString;
    val.flags = ValFlagNone;
    val.str = res;
    ZASSIGN(vm, dst, &val);
}

static void formatDouble(ZorroVM* vm, Value* v, int w, int p, ZString* flags, Value*, Value* dst)
{
    if(!dst)
    {
        return;
    }
    char buf[64];
    FormatFlags ff;
    parseFlags(vm, flags, ff);
    auto len = static_cast<size_t>(snprintf(buf, 64, "%.*lf", p, v->dValue));
    ZString* res = vm->allocZString();
    size_t slen = len;
    if(w > static_cast<int>(slen))
    {
        slen = static_cast<size_t>(w);
    }
    char* strbuf = vm->allocStr(slen + 1);
    if(w > static_cast<int>(len))
    {
        if(ff.left)
        {
            memcpy(strbuf, buf, len);
            memset(strbuf + len, ' ', w - len);
        } else
        {
            memset(strbuf, ff.zero ? '0' : ' ', w - len);
            memcpy(strbuf + w - len, buf, len);
        }
    } else
    {
        memcpy(strbuf, buf, len);
    }
    strbuf[slen] = 0;
    res->init(strbuf, slen, false);
    Value val;
    val.vt = vtString;
    val.flags = ValFlagNone;
    val.str = res;
    ZASSIGN(vm, dst, &val);
}


static void formatStr(ZorroVM* vm, Value* v, int w, int p, ZString* flags, Value*, Value* dst)
{
    if(!dst)
    {
        return;
    }
    if(w == -1 && p == -1)
    {
        ZASSIGN(vm, dst, v);
        return;
    }
    FormatFlags ff;
    parseFlags(vm, flags, ff);
    ZString* src = v->str;
    size_t len = src->getLength();
    int cs = src->getCharSize();
    if(p > 0 && static_cast<int>(len) > p)
    {
        len = static_cast<size_t>(p);
    }
    size_t slen = len;
    if(static_cast<int>(slen) < w)
    {
        slen = static_cast<size_t>(w);
    }
    if(slen < len)
    {
        slen = len;
    }
    char* strbuf = vm->allocStr(cs == 1 ? slen + 1 : slen * 2);
    if(slen > len)
    {
        if(ff.right)
        {
            if(cs == 1)
            {
                memset(strbuf, ' ', slen - len);
                memcpy(strbuf + slen - len, src->getDataPtr(), len);
            } else
            {
                char* ptr = strbuf;
                for(size_t i = 0; i < slen - len; ++i)
                {
                    ZString::putUcs2Char(ptr, ' ');
                }
                memcpy(ptr, src->getDataPtr(), len * cs);
            }
        } else
        {
            memcpy(strbuf, src->getDataPtr(), len * cs);
            if(cs == 1)
            {
                memset(strbuf + len, ' ', slen - len);
            } else
            {
                char* ptr = strbuf + (slen - len) * cs;
                for(size_t i = 0; i < slen - len; ++i)
                {
                    ZString::putUcs2Char(ptr, ' ');
                }
            }
        }
    } else
    {
        if(ff.right)
        {
            memcpy(strbuf, src->getDataPtr() + src->getLength() * cs - slen * cs, slen * cs);
        } else
        {
            memcpy(strbuf, src->getDataPtr(), slen * cs);
        }
    }
    strbuf[slen] = 0;
    ZString* str = vm->allocZString();
    str->init(strbuf, cs == 1 ? slen : slen * 2, cs != 1);
    Value res;
    res.vt = vtString;
    res.flags = ValFlagNone;
    res.str = str;
    ZASSIGN(vm, dst, &res);
}


static void
formatArrEx(ZorroVM* vm, Value* v, int w, int p, ZString* flags, Value* extra, Value* dst, size_t from, size_t till)
{
    if(!dst)
    {
        return;
    }
    ZString* delim = nullptr;
    size_t delimLength = 0;
    CStringWrap wrap(nullptr, nullptr, 0);
    if(extra)
    {
        if(extra->vt == vtString)
        {
            delim = extra->str;
            delimLength = delim->getLength();
        }
    }
    ZArray* src = v->arr;
    size_t sz = till - from;
    Value arr16[16];
    Value* arr = sz <= 16 ? arr16 : new Value[sz];
    Value tmp = NilValue;
    size_t len = 0;
    int cs = 1;
    for(size_t idx = from; idx < till; ++idx)
    {
        Value& item = src->getItem(idx);
        vm->fmtOps[item.vt](vm, &item, w, p, flags, extra, &tmp);
        if(item.vt == vtObject)
        {
            OpBase* retOp = vm->ctx.callStack.stackTop->retOp;
            vm->ctx.callStack.stackTop->retOp = nullptr;
            vm->resume();
            vm->ctx.nextOp = retOp;
            if(dst->vt != vtString)
            {
                ZTHROWR(TypeException, vm, "Format operator of class %{} returned value of type %{}",
                        item.obj->classInfo->name, getValueTypeName(dst->vt));
            }
            ZASSIGN(vm, &tmp, dst);
        }
        arr[idx] = tmp;
        tmp.str->ref();
        len += tmp.str->getLength();
        if(tmp.str->getCharSize() != 1)
        {
            cs = 2;
        }
        if(idx < sz - 1)
        {
            len += delimLength;
        }
        ZUNREF(vm, &tmp);
    }
    ZString* str = vm->allocZString();
    char* strBuf = vm->allocStr(cs * len + (cs & 1));
    char* ptr = strBuf;
    Value res;
    res.vt = vtString;
    res.flags = ValFlagNone;
    res.str = str;
    for(size_t idx = from; idx < till; ++idx)
    {
        ZString& astr = *arr[idx].str;
        if(cs == 2)
        {
            astr.uappend(ptr);
        } else
        {
            size_t l = arr[idx].str->getLength();
            memcpy(ptr, arr[idx].str->c_str(vm), l);
            ptr += l;
        }
        ZUNREF(vm, arr + idx);
        if(delim && idx < sz - 1)
        {
            if(cs == 2)
            {
                delim->uappend(ptr);
            } else
            {
                memcpy(ptr, delim->getDataPtr(), delimLength);
                ptr += delimLength;
            }
        }
    }
    if(sz > 16)
    {
        delete[] arr;
    }
    if(cs == 1)
    {
        *ptr = 0;
    }
    str->init(strBuf, len * cs, cs == 2);

    ZASSIGN(vm, dst, &res);
}

static void formatArr(ZorroVM* vm, Value* v, int w, int p, ZString* flags, Value* extra, Value* dst)
{
    formatArrEx(vm, v, w, p, flags, extra, dst, 0, v->arr->getCount());
}

static void formatSeg(ZorroVM* vm, Value* v, int w, int p, ZString* flags, Value* extra, Value* dst)
{
    if(!dst)
    {
        return;
    }
    Segment& seg = *v->seg;
    if(seg.cont.vt == vtString)
    {
        ZStringRef str(vm, seg.cont.str->substr(vm, static_cast<uint32_t>(seg.segStart),
                                                static_cast<uint32_t>(seg.segEnd - seg.segStart)));
        Value tmpStr = StringValue(str);
        formatStr(vm, &tmpStr, w, p, flags, nullptr, dst);
    } else if(seg.cont.vt == vtArray)
    {
        auto from = static_cast<size_t>(seg.segStart);
        if(from > seg.cont.arr->getCount())
        {
            from = seg.cont.arr->getCount();
        }
        auto to = static_cast<size_t>(seg.segEnd);
        if(to > seg.cont.arr->getCount())
        {
            to = seg.cont.arr->getCount();
        }
        if(to < from)
        {
            to = from;
        }
        formatArrEx(vm, v, w, p, flags, extra, dst, from, to);
    } else
    {
        ZTHROWR(TypeException, vm, "Invalid type for segment container:%{}", getValueTypeName(seg.cont.vt));
    }
}

static void formatObj(ZorroVM* vm, Value* v, int w, int p, ZString* flags, Value* extra, Value* /*dst*/)
{
    ClassInfo* ci = v->obj->classInfo;
    size_t m = ci->specialMethods[csmFormat];
    if(!m)
    {
        ZTHROWR(TypeException, vm, "Class %{} do not have operator format", v->obj->classInfo->name);
    }
    vm->pushValue(IntValue(w));
    vm->pushValue(IntValue(p));
    if(flags)
    {
        vm->pushValue(StringValue(flags));
    } else
    {
        vm->pushValue(NilValue);
    }
    if(extra)
    {
        vm->pushValue(*extra);
    } else
    {
        vm->pushValue(NilValue);
    }
    vm->callMethod(v, m, 4, true);
}


#define PREPDST(type) \
    if(dst->vt==vtRef){ dst=&dst->valueRef->value;}\
    ZUNREF(vm,dst);\
    dst->vt=type;\
    dst->flags=0


static void countString(ZorroVM* vm, Value* val, Value* dst)
{
    PREPDST(vtInt);
    dst->iValue = val->str->getLength();
}

static void countArray(ZorroVM* vm, Value* val, Value* dst)
{
    PREPDST(vtInt);
    dst->iValue = val->arr->getCount();
}

static void countSlice(ZorroVM* vm, Value* val, Value* dst)
{
    PREPDST(vtInt);
    dst->iValue = val->slice->indeces.arr->getCount();
}

static void countSegment(ZorroVM* vm, Value* val, Value* dst)
{
    PREPDST(vtInt);
    dst->iValue = val->seg->segEnd - val->seg->segStart;
}

static void countSet(ZorroVM* vm, Value* val, Value* dst)
{
    PREPDST(vtInt);
    dst->iValue = val->set->size();
}

static void countMap(ZorroVM* vm, Value* val, Value* dst)
{
    PREPDST(vtInt);
    dst->iValue = val->map->size();
}

static void countRef(ZorroVM* vm, Value* val, Value* dst)
{
    vm->countOps[val->valueRef->value.vt](vm, &val->valueRef->value, dst);
}

static void negInt(ZorroVM* vm, Value* val, Value* dst)
{
    PREPDST(vtInt);
    dst->iValue = -val->iValue;
}

static void negDouble(ZorroVM* vm, Value* val, Value* dst)
{
    PREPDST(vtDouble);
    dst->dValue = -val->dValue;
}

static void negRef(ZorroVM* vm, Value* val, Value* dst)
{
    vm->negOps[val->valueRef->value.vt](vm, &val->valueRef->value, dst);
}

static void negArray(ZorroVM* vm, Value* val, Value* dst)
{
    if(!dst)
    {
        return;
    }
    Value rv;
    rv.vt = vtSegment;
    rv.flags = ValFlagNone;
    Segment& seg = *(rv.seg = vm->allocSegment());
    seg.segStart = 0;
    seg.segEnd = val->arr->getCount();
    seg.cont = *val;
    seg.cont.arr->ref();
    seg.step = -1;
    ZASSIGN(vm, dst, &rv);
}

static void copySimple(ZorroVM* vm, Value* val, Value* dst)
{
    if(!dst)
    {
        return;
    }
    ZASSIGN(vm, dst, val);
}

static void copyMap(ZorroVM* vm, Value* val, Value* dst)
{
    if(!dst)
    {
        return;
    }
    Value map;
    map.vt = vtMap;
    map.flags = ValFlagNone;
    map.map = val->map->copy();
    ZASSIGN(vm, dst, &map);
}

static void copySet(ZorroVM* vm, Value* val, Value* dst)
{
    if(!dst)
    {
        return;
    }
    Value set;
    set.vt = vtSet;
    set.flags = ValFlagNone;
    set.set = val->set->copy();
    ZASSIGN(vm, dst, &set);
}

static void copyArray(ZorroVM* vm, Value* val, Value* dst)
{
    if(!dst)
    {
        return;
    }
    Value arr;
    arr.vt = vtArray;
    arr.flags = ValFlagNone;
    arr.arr = val->arr->copy();
    ZASSIGN(vm, dst, &arr);
}

static void copySegment(ZorroVM* vm, Value* val, Value* dst)
{
    if(!dst)
    {
        return;
    }
    Value* sv = &val->seg->cont;
    if(sv->vt == vtString)
    {
        ZString* str = sv->str;
        int64_t start = val->seg->segStart;
        if(start < 0)
        {
            start = 0;
        }
        int64_t length = val->seg->segEnd - start;
        if(length > str->getLength())
        {
            length = str->getLength();
        }
        Value res;
        res.vt = vtString;
        res.flags = ValFlagNone;
        res.str = str->substr(vm, static_cast<uint32_t>(start), static_cast<uint32_t>(length));
        ZASSIGN(vm, dst, &res);
    } else if(sv->vt == vtArray)
    {
        ZArray* arr = sv->arr;
        int64_t start = val->seg->segStart;
        int64_t length = val->seg->segEnd - start;
        int64_t arrlen = arr->getCount();
        if(start < 0 || start > arrlen || start + length > arrlen)
        {
            throwOutOfBounds(vm, "Array index of segment is out of bounds", start > arrlen ? start : start + length,
                             arrlen);
        }
        ZArray* outArr = vm->allocZArray();
        outArr->resize(static_cast<size_t>(length));
        for(int64_t i = 0; i < length; ++i)
        {
            Value* srcVal = &arr->getItem(static_cast<size_t>(start + i));
            Value* dstVal = &outArr->getItemRef(static_cast<size_t>(i));
            if(ZISREFTYPE(srcVal))
            {
                ZASSIGN(vm, dstVal, srcVal);
                outArr->isSimpleContent = false;
            } else
            {
                *dstVal = *srcVal;
            }
        }
        Value res;
        res.vt = vtArray;
        res.flags = ValFlagNone;
        res.arr = outArr;
        ZASSIGN(vm, dst, &res);
    } else
    {
        ZTHROWR(TypeException, vm, "Invalid type for segment container:%{}", getValueTypeName(sv->vt));
    }
}

static void copySlice(ZorroVM* vm, Value* val, Value* dst)
{
    if(!dst)
    {
        return;
    }
    Value* sv = &val->slice->cont;
    if(sv->vt == vtString)
    {
        ZString* str = sv->str;
        Value res;
        res.vt = vtString;
        res.flags = ValFlagNone;
        res.str = vm->allocZString();
        ZArray& za = *val->slice->indeces.arr;
        const char* strData = str->getDataPtr();
        int cs = str->getCharSize();
        int dcs = cs;
        if(cs != 1)
        {
            dcs = 1;
            for(size_t i = 0; i < za.getCount(); ++i)
            {
                Value& idx = za.getItem(i);
                if(idx.vt != vtInt)
                {
                    ZTHROWR(TypeException, vm, "Invalid type for slice index:%{}", getValueTypeName(idx.vt));
                }
                ZString::char_type wc;
                memcpy(&wc, strData + idx.iValue * cs, static_cast<size_t>(cs));
                if(wc > 255)
                {
                    dcs = cs;
                    break;
                }
            }
        }
        char* newStr;
        if(dcs == 1)
        {
            newStr = vm->allocStr(za.getCount() + 1);
            for(size_t i = 0; i < za.getCount(); ++i)
            {
                Value& idx = za.getItem(i);
                if(idx.vt != vtInt)
                {
                    vm->freeStr(newStr, za.getCount() + 1);
                    ZTHROWR(TypeException, vm, "Invalid type for slice index:%{}", getValueTypeName(idx.vt));
                }
                if(idx.iValue < 0 || idx.iValue >= str->getLength())
                {
                    vm->freeStr(newStr, za.getCount() + 1);
                    throwOutOfBounds(vm, "Slice of string index out of bounds", idx.iValue, str->getLength());
                }
                ZString::char_type wc;
                memcpy(&wc, strData + idx.iValue * cs, static_cast<size_t>(cs));
                newStr[i] = static_cast<char>(wc);
            }
            newStr[za.getCount()] = 0;
        } else
        {
            newStr = vm->allocStr(za.getCount() * dcs);
            for(size_t i = 0; i < za.getCount(); ++i)
            {
                Value& idx = za.getItem(i);
                if(idx.vt != vtInt)
                {
                    vm->freeStr(newStr, za.getCount() * dcs);
                    ZTHROWR(TypeException, vm, "Invalid type for slice index:%{}", getValueTypeName(idx.vt));
                }
                if(idx.iValue < 0 || idx.iValue >= str->getLength())
                {
                    vm->freeStr(newStr, za.getCount() * dcs);
                    throwOutOfBounds(vm, "Slice of string index out of bounds", idx.iValue, str->getLength());
                }
                memcpy(newStr + i * dcs, strData + idx.iValue * cs, static_cast<size_t>(dcs));
            }
        }
        if(dcs == 1)
        {
            res.str->init(newStr, za.getCount(), false);
        } else
        {
            res.str->init(newStr, za.getCount() * dcs, true);
        }
        ZASSIGN(vm, dst, &res);
    } else if(sv->vt == vtArray)
    {
        ZArray* arr = sv->arr;
        ZArray& za = val->slice->indeces.arr[0];
        ZArray* outArr = vm->allocZArray();
        outArr->resize(za.getCount());
        for(int64_t i = 0; i < (int64_t) za.getCount(); ++i)
        {
            Value& idx = za.getItem(static_cast<size_t>(i));
            if(idx.vt != vtInt)
            {
                vm->freeZArray(outArr);
                ZTHROWR(TypeException, vm, "Invalid type for slice index:%{}", getValueTypeName(idx.vt));
            }
            if(idx.iValue < 0 || static_cast<size_t>(idx.iValue) >= arr->getCount())
            {
                throwOutOfBounds(vm, "Slice of string index out of bounds", idx.iValue, arr->getCount());
            }
            Value* srcVal = &arr->getItem(static_cast<size_t>(idx.iValue));
            Value* dstVal = &outArr->getItemRef(static_cast<size_t>(i));
            if(ZISREFTYPE(srcVal))
            {
                ZASSIGN(vm, dstVal, srcVal);
                outArr->isSimpleContent = false;
            } else
            {
                *dstVal = *srcVal;
            }
        }
        Value res;
        res.vt = vtArray;
        res.flags = ValFlagNone;
        res.arr = outArr;
        ZASSIGN(vm, dst, &res);
    } else
    {
        ZTHROWR(TypeException, vm, "Invalid type for slice container:%{}", getValueTypeName(sv->vt));
    }
}

static void copyNObject(ZorroVM* vm, Value* val, Value* dst)
{
    size_t midx = val->nobj->classInfo->specialMethods[csmCopy];
    if(!midx)
    {
        ZTHROWR(TypeException, vm, "Native class %{} do not have operator copy", val->nobj->classInfo->name);
    }
    //vm->symbols.globals[midx].cmethod->cmethod(vm,val);
    vm->callCMethod(val, midx, 0);
    ZASSIGN(vm, dst, &vm->result);
    vm->clearResult();
}

static void copyObject(ZorroVM* vm, Value* val, Value* /*dst*/)
{
    size_t midx = val->nobj->classInfo->specialMethods[csmCopy];
    if(!midx)
    {
        ZTHROWR(TypeException, vm, "Class %{} do not have operator copy", val->nobj->classInfo->name);
    }
    vm->callMethod(val, midx, 0);
}

#define GETTYPEOP(name, type) static void getType##name(ZorroVM* vm,Value* val,Value* dst)\
{\
  (void)val;\
  if(!dst)return;\
  Value cls;\
  cls.vt = vtClass;\
  cls.flags = 0;\
  cls.classInfo = type;\
  ZASSIGN(vm,dst,&cls);\
}

GETTYPEOP(Nil, vm->nilClass)

GETTYPEOP(Bool, vm->boolClass)

GETTYPEOP(Int, vm->intClass)

GETTYPEOP(Double, vm->doubleClass)

GETTYPEOP(String, vm->stringClass)

GETTYPEOP(Array, vm->arrayClass)

GETTYPEOP(Set, vm->setClass)

GETTYPEOP(Map, vm->mapClass)

GETTYPEOP(Class, vm->classClass)

GETTYPEOP(Object, val->obj->classInfo)

GETTYPEOP(NObject, val->nobj->classInfo)

GETTYPEOP(Func, vm->funcClass)

GETTYPEOP(Closure, vm->clsClass)

static void getTypeRef(ZorroVM* vm, Value* val, Value* dst)
{
    Value* v = &val->valueRef->value;
    vm->getTypeOps[v->vt](vm, v, dst);
}

OpBase* ZorroVM::getFuncEntry(FuncInfo* f, CallFrame* cf)
{
    index_type actualArgs = cf->args;
    if(f->argsCount == actualArgs)
    {
        cf->args += f->namedArgs;
        return f->entry;
    } else if(actualArgs > f->argsCount && f->varArgEntry)
    {
        Value arr;
        arr.vt = vtArray;
        arr.flags = ValFlagNone;
        ZArray& za = *(arr.arr = allocZArray());
        za.ref();
        Value* ptr = ctx.dataStack.stackTop - (actualArgs - f->argsCount);
        Value* end = ctx.dataStack.stackTop;
        for(; ptr <= end; ++ptr)
        {
            za.push(*ptr);
            if(ZISREFTYPE(ptr))
            {
                za.isSimpleContent = false;
            }
            ptr->vt = vtNil;
            ptr->flags = 0;
        }
        ctx.dataStack.stackTop -= (actualArgs - f->argsCount + 1);
        ctx.dataStack.push(arr);
        cf->args = f->argsCount;
        return f->varArgEntry;
    }
    size_t missParams;
    if(f->defValEntries.empty() || (missParams = f->argsCount - actualArgs) > f->defValEntries.size())
    {
        ZTHROWR(RuntimeException, this, "Invalid number of arguments for function %{}", f->name);
    }
    cf->args = f->argsCount + f->namedArgs;
    ctx.dataStack.pushBulk(missParams);
    return f->defValEntries[f->defValEntries.size() - missParams];
}

OpBase* ZorroVM::getFuncEntryNArgs(FuncInfo* f, CallFrame* cf)
{
    index_type actualArgs = cf->args - 1;
    if(actualArgs == f->argsCount - 1 && f->namedArgs)
    {
        cf->args = f->argsCount + f->namedArgs;
        return f->entry;
    }
    if(actualArgs >= f->argsCount && !f->namedArgs)
    {
        ZTHROWR(RuntimeException, this, "Too many arguments for func %{}", f->name);
    }
    Value argsMapVal = *ctx.dataStack.stackTop;
    ZMap* argsMap = argsMapVal.map;
    if(actualArgs + argsMap->size() < f->argsCount - f->defValEntries.size())
    {
        ZTHROWR(RuntimeException, this, "Not enough arguments for func %{}", f->name);
    }
    *ctx.dataStack.stackTop = NilValue;
    ctx.dataStack.pop();
    int defValIdx = -1;
    for(index_type i = actualArgs; i < f->argsCount; ++i)
    {
        ZMap::iterator it = argsMap->find(StringValue(f->locals[i]->name.val));
        if(it == argsMap->end())
        {
            if(f->defValEntries.empty() || defValIdx + 1 == static_cast<int>(f->defValEntries.size()))
            {
                ZUNREF(this, &argsMapVal);
                ZTHROWR(RuntimeException, this, "Argument %{} not defined in func %{} call", f->locals[i]->name.val,
                        f->name);
            }
            if(defValIdx == -1)
            {
                defValIdx = static_cast<int>(f->defValEntries.size() - (f->argsCount - actualArgs));
            }
            pushValue(NilValue);
            continue;
        }
        pushValue(it->m_value);
        ctx.dataStack.stackTop->flags |= ValFlagInited;
        argsMap->erase(it);
    }
    if(!f->namedArgs)
    {
        if(!argsMap->empty())
        {
            ZTHROWR(RuntimeException, this, "Extra arguments in call of func %{}", f->name);
        }
    } else
    {
        pushValue(argsMapVal);
    }
    ZUNREF(this, &argsMapVal);
    cf->args = f->argsCount + f->namedArgs;
    if(defValIdx != -1)
    {
        return f->defValEntries[static_cast<size_t>(defValIdx)];
    }
    return f->namedArgs ? f->namedArgEntry : f->entry;
}

static void callObj(ZorroVM* vm, Value* obj)
{
    ClassInfo* ci = obj->obj->classInfo;
    if(!ci->specialMethods[csmCall])
    {
        ZTHROWR(TypeException, vm, "Class %{} do not have operator call", ci->name);
    }
    MethodInfo* meth = vm->symbols.globals[ci->specialMethods[csmCall]].method;
    CallFrame* cf = vm->ctx.callStack.stackTop;
    cf->funcIdx = meth->index;
    OpBase* entry = vm->getFuncEntry(meth, cf);
    vm->ctx.dataPtrs[atLocal] = vm->ctx.dataStack.stack + cf->localBase;
    vm->ctx.dataPtrs[atMember] = obj->obj->members;
    if(meth->localsCount)
    {
        vm->ctx.dataStack.pushBulk(meth->localsCount);
    }
    vm->ctx.dataPtrs[atLocal][cf->args] = *obj;
    obj->refBase->ref();
    vm->ctx.nextOp = entry;

}

static void ncallFunc(ZorroVM* vm, Value* val)
{
    FuncInfo* f = val->func;
    CallFrame* cf = vm->ctx.callStack.stackTop;
    cf->funcIdx = f->index;
    OpBase* entry = vm->getFuncEntryNArgs(f, cf);
    vm->ctx.dataStack.pushBulk(f->localsCount);
    vm->ctx.dataPtrs[atLocal] = vm->ctx.dataStack.stack + cf->localBase;
    vm->ctx.nextOp = entry;
}


static void callFunc(ZorroVM* vm, Value* val)
{
    FuncInfo* f = val->func;
    CallFrame* cf = vm->ctx.callStack.stackTop;
    cf->funcIdx = f->index;
    OpBase* entry = vm->getFuncEntry(f, cf);
    vm->ctx.dataStack.pushBulk(f->localsCount);
    vm->ctx.dataPtrs[atLocal] = vm->ctx.dataStack.stack + cf->localBase;
    vm->ctx.nextOp = entry;
}

static void callCor(ZorroVM* vm, Value* val)
{
    Coroutine* cor = val->cor;
    if(!cor->ctx.nextOp)
    {
        ZTHROWR(RuntimeException, vm, "Attempt to call finished coroutine");
    }
    CallFrame* cf = vm->ctx.callStack.stackTop;
    cor->result = cf->callerOp->dst;
    if(cf->args)
    {
        ZTHROWR(RuntimeException, vm, "Invalid number of arguments for coroutine call");
    }
    vm->ctx.callStack.pop();
    vm->ctx.swap(cor->ctx);
}

static void callCFunc(ZorroVM* vm, Value* val)
{
    CallFrame* cf = vm->ctx.callStack.stackTop;
    cf->funcIdx = val->cfunc->index;
    vm->result = NilValue;
    OpBase* nxt = vm->ctx.nextOp;
    val->cfunc->cfunc(vm);
    if(vm->ctx.nextOp == nxt)
    {
        vm->cleanStack(cf->localBase);
        vm->ctx.callStack.pop();

        OpArg dstArg = ((OpDstBase*) vm->ctx.lastOp)->dst;
        if(dstArg.at != atNul)
        {
            Value* dst = GETDST(dstArg);
            ZASSIGN(vm, dst, &vm->result);
        }
        vm->clearResult();
    }
}

static void callClass(ZorroVM* vm, Value* val)
{
    ClassInfo* classInfo = val->classInfo;
    CallFrame* cf = vm->ctx.callStack.stackTop;
    size_t ctorIdx = classInfo->specialMethods[csmConstructor];
    Value* ctor = ctorIdx ? &vm->symbols.globals[classInfo->specialMethods[csmConstructor]] : nullptr;
    if(ctor && ctor->vt == vtCMethod)
    {
        cf->funcIdx = classInfo->specialMethods[csmConstructor];
        OpBase* lastOp = vm->ctx.lastOp;
        ctor->cmethod->cmethod(vm, val);
        vm->cleanStack(cf->localBase);
        vm->ctx.callStack.pop();
        //vm->ctx.dataPtrs[atLocal]=vm->ctx.dataStack.stack+vm->ctx.callStack.stackTop->localBase;
        if(lastOp == vm->ctx.lastOp)
        {
            DSTARG;
            if(dstArg.at != atNul)
            {
                Value* dst = GETDST(dstArg);
                ZASSIGN(vm, dst, &vm->result);
            }
        }
        vm->clearResult();
        return;
    }
    if(classInfo->nativeClass)
    {
        vm->ctx.callStack.pop();
        ZTHROWR(TypeException, vm, "Native class %{} do not have constructor", classInfo->name);
    }
    Value res;
    res.vt = vtObject;
    res.flags = ValFlagNone;
    res.obj = vm->allocObj();
    if(classInfo->membersCount)
    {
        res.obj->members = vm->allocVArray(classInfo->membersCount);
        memset(res.obj->members, 0, sizeof(Value) * classInfo->membersCount);
    } else
    {
        res.obj->members = nullptr;
    }
    res.obj->classInfo = classInfo;
    classInfo->ref();

    if(ctor)
    {
        FuncInfo* f = ctor->func;
        cf->funcIdx = f->index;
        cf->selfCall = true;
        OpBase* entry = vm->ctx.lastOp->ot == otNamedArgsCall ? vm->getFuncEntryNArgs(f, cf) : vm->getFuncEntry(f, cf);
        vm->ctx.dataPtrs[atLocal] = vm->ctx.dataStack.stack + cf->localBase;
        vm->ctx.dataPtrs[atMember] = res.obj->members;
        vm->ctx.dataStack.pushBulk(f->localsCount);
        Value* self = &ZLOCAL(vm, cf->args);
        ZASSIGN(vm, self, &res);
        //vm->setLocal(op->args,res);
        vm->ctx.nextOp = entry;
    } else
    {
        if(cf->args)
        {
            ZTHROWR(RuntimeException, vm, "Invalid number of arguments for constructor of class %{}", classInfo->name);
        }
        vm->ctx.callStack.pop();
        DSTARG;
        Value* dst = GETDST(dstArg);
        ZASSIGN(vm, dst, &res);
    }
}

static void callDelegate(ZorroVM* vm, Value* val)
{
    CallFrame* cf = vm->ctx.callStack.stackTop;
    Delegate* dlg = val->dlg;
    Value* obj = dlg->obj.vt == vtWeakRef ? &dlg->obj.weakRef->value : &dlg->obj;
    if(obj->vt == vtNil)
    {
        ZTHROWR(RuntimeException, vm, "Object of delegate is nil");
    }
    vm->ctx.dataPtrs[atLocal] = vm->ctx.dataStack.stack + cf->localBase;
    vm->ctx.dataPtrs[atMember] = obj->obj->members;
    cf->selfCall = true;
    FuncInfo* f = dlg->method;
    cf->funcIdx = f->index;
    OpBase* entry = vm->ctx.lastOp->ot == otNamedArgsCall ? vm->getFuncEntryNArgs(f, cf) : vm->getFuncEntry(f, cf);
    vm->ctx.dataStack.pushBulk(f->localsCount);
    Value* self = &ZLOCAL(vm, cf->args);
    ZASSIGN(vm, self, obj);
    vm->ctx.nextOp = entry;
}

static void callCDelegate(ZorroVM* vm, Value* val)
{
    CallFrame* cf = vm->ctx.callStack.stackTop;
    cf->funcIdx = val->dlg->method->index;
    val->dlg->method->cmethod(vm, &val->dlg->obj);
    vm->cleanStack(cf->localBase);
    vm->ctx.callStack.pop();

    DSTARG;
    if(dstArg.at != atNul)
    {
        Value* dst = GETDST(dstArg);
        ZASSIGN(vm, dst, &vm->result);
        vm->clearResult();
    }
}

static void callClosure(ZorroVM* vm, Value* val)
{
    Closure* cls = val->cls;
    FuncInfo* f = cls->func;
    CallFrame* cf = vm->ctx.callStack.stackTop;
    cf->funcIdx = f->index;
    cf->closedCall = true;
    OpBase* entry = vm->getFuncEntry(f, cf);
    vm->ctx.dataPtrs[atLocal] = vm->ctx.dataStack.stack + cf->localBase;
    vm->ctx.dataPtrs[atClosed] = val->cls->closedValues;
    if(cls->self.vt != vtNil)
    {
        Value* self = vm->ctx.dataStack.stack + cf->localBase + f->argsCount;
        ZASSIGN(vm, self, &cls->self);
        cf->selfCall = true;
    }
    vm->ctx.dataStack.pushBulk(f->localsCount);
    Value* clsStore = vm->ctx.dataStack.stackTop;
    ZASSIGN(vm, clsStore, val);
    vm->ctx.nextOp = entry;
}

static void callClassMethod(ZorroVM* vm, Value* val)
{
    CallFrame* cf = vm->ctx.callStack.stackTop;
    CallFrame* of = cf - 1;
    if(!of->selfCall)
    {
        ZTHROWR(RuntimeException, vm, "Cannot call method %{} outside of class", val->method->name);
    }
    Value* self = vm->ctx.dataStack.stack + of->localBase + of->args;
    auto* mi = (MethodInfo*) val->func;
    if(self->obj->classInfo != mi->owningClass && !self->obj->classInfo->isInParents(mi->owningClass))
    {
        ZTHROWR(RuntimeException, vm, "Invalid class %{} to call method %{}", self->obj->classInfo->name,
                val->method->name);
    }
    cf->funcIdx = mi->index;
    OpBase* entry = vm->getFuncEntry(mi, cf);
    vm->ctx.dataPtrs[atLocal] = vm->ctx.dataStack.stack + cf->localBase;
    vm->ctx.dataPtrs[atLocal][cf->args] = *self;
    self->refBase->ref();
    if(mi->localsCount)
    {
        //for(int i=0;i<f->locals;i++)vm->ctx.dataStack.push(NilValue);
        vm->ctx.dataStack.pushBulk(mi->localsCount);
    }
    vm->ctx.nextOp = entry;
}

static void callRef(ZorroVM* vm, Value* val)
{
    vm->callOps[val->valueRef->value.vt](vm, &val->valueRef->value);
}

static bool initForRef(ZorroVM* vm, Value* val, Value* var, Value* tmp)
{
    val = &val->valueRef->value;
    return vm->initForOps[val->vt](vm, val, var, tmp);
}

static bool initForRange(ZorroVM* vm, Value* val, Value* var, Value* tmp)
{
    Range r = *val->range;
    if((r.start > r.end && r.step > 0) || (r.start < r.end && r.step < 0))
    {
        return false;
    }
    Value varVal = IntValue(r.start);
    ZASSIGN(vm, var, &varVal);
    ZUNREF(vm, tmp);
    tmp->vt = vtRange;
    tmp->flags = 0;
    tmp->range = vm->allocRange();
    tmp->range->ref();
    tmp->range->start = r.start;
    tmp->range->end = r.end;
    tmp->range->step = r.step;
    return true;
}

static bool stepForRange(ZorroVM* vm, Value* /*val*/, Value* var, Value* tmp)
{
    if(var->vt != vtInt)
    {
        ZUNREF(vm, var);
        var->vt = vtInt;
        var->flags = 0;
    }
    Range& r = *tmp->range;
    var->iValue = r.start += r.step;
    return !((r.step > 0 && var->iValue > r.end) || (r.step < 0 && var->iValue < r.end));
}

static bool initForArray(ZorroVM* vm, Value* val, Value* var, Value* tmp)
{
    if(val->arr->getCount() == 0)
    {
        return false;
    }
    ZUNREF(vm, tmp);
    tmp->vt = vtSegment;
    tmp->flags = 0;
    tmp->seg = vm->allocSegment();
    tmp->seg->ref();
    tmp->seg->cont = *val;
    val->refBase->ref();
    //vm->assign(tmp.seg->cont,*val);
    tmp->seg->segStart = 0;
    tmp->seg->segEnd = 0;
    tmp->seg->step = 1;
    Value* itemPtr = &tmp->seg->cont.arr->getItem(0);
    ZASSIGN(vm, var, itemPtr);
    return true;
}

static bool initForSegment(ZorroVM* vm, Value* val, Value* var, Value* tmp)
{
    Segment& seg = *val->seg;
    if(seg.cont.vt != vtArray)
    {
        ZTHROWR(TypeException, vm, "Invalid type for segment container:%{}", getValueTypeName(seg.cont.vt));
    }
    ZArray& za = *seg.cont.arr;
    auto cnt = (int64_t) za.getCount();
    if(cnt == 0 || seg.segStart >= cnt || seg.segStart >= seg.segEnd)
    {
        return false;
    }
    ZUNREF(vm, tmp);
    tmp->vt = vtSegment;
    tmp->flags = 0;
    tmp->seg = vm->allocSegment();
    tmp->seg->ref();
    tmp->seg->cont = seg.cont;
    val->refBase->ref();
    //vm->assign(tmp.seg->cont,*val);
    tmp->seg->segBase = seg.segStart;
    tmp->seg->segStart = seg.step > 0 ? seg.segStart : seg.segEnd - 1;
    tmp->seg->segEnd = seg.segEnd;
    tmp->seg->step = seg.step;
    Value* itemPtr = &za.getItem(static_cast<size_t>(tmp->seg->segStart));
    ZASSIGN(vm, var, itemPtr);
    return true;
}


static bool stepForArray(ZorroVM* vm, Value* /*val*/, Value* var, Value* tmp)
{
    Segment& seg = *tmp->seg;
    ZArray& arr = *seg.cont.arr;
    if(seg.step > 0)
    {
        if((seg.segStart += seg.step) >= (int64_t) arr.getCount() || (seg.segEnd && seg.segStart >= seg.segEnd))
        {
            return false;
        }
    } else
    {
        if((seg.segStart += seg.step) < 0 || seg.segStart < seg.segBase)
        {
            return false;
        }
    }
    Value* itemPtr = &arr.getItem(static_cast<size_t>(seg.segStart));
    ZASSIGN(vm, var, itemPtr);
    return true;
}

static bool initForSlice(ZorroVM* vm, Value* val, Value* var, Value* tmp)
{
    Slice& slice = *val->slice;
    if(slice.indeces.arr->getCount() == 0)
    {
        return false;
    }
    ZUNREF(vm, tmp);
    tmp->vt = vtSlice;
    tmp->flags = 0;
    Slice& dslice = *(tmp->slice = vm->allocSlice());
    dslice.cont = slice.cont;
    dslice.cont.refBase->ref();
    dslice.indeces = slice.indeces;
    dslice.indeces.refBase->ref();
    dslice.index = 0;
    Value idx = IntValue(slice.index);
    vm->getIndexMatrix[val->vt][idx.vt](vm, val, &idx, var);
    /*Value& idx=slice.indeces.arr->getItem(0);
  vm->getIndexMatrix[val->vt][idx.vt](vm,&dslice,&idx,var);*/
    return true;
}

static bool stepForSlice(ZorroVM* vm, Value* /*val*/, Value* var, Value* tmp)
{
    Slice& slice = *tmp->slice;
    if(++slice.index >= slice.indeces.arr->getCount())
    {
        return false;
    }
    Value idx = slice.indeces.arr->getItem(slice.index);
    vm->getIndexMatrix[slice.cont.vt][idx.vt](vm, &slice.cont, &idx, var);
    //Value& idx=slice.indeces.arr->getItem(slice.index);
    //vm->getIndexMatrix[slice.cont.vt][vtInt](vm,&slice.cont,&idx,var);
    return true;
}

static bool initForSet(ZorroVM* vm, Value* val, Value* var, Value* tmp)
{
    if(val->set->empty())
    {
        return false;
    }
    ZUNREF(vm, tmp);
    tmp->vt = vtForIterator;
    tmp->flags = 0;
    tmp->iter = val->set->getForIter();
    tmp->iter->ref();
    tmp->iter->cont = *val;
    val->refBase->ref();

    Value* itemPtr = &*val->set->begin();
    ZASSIGN(vm, var, itemPtr);
    return true;
}

static bool stepForSet(ZorroVM* /*vm*/, Value* /*val*/, Value* var, Value* tmp)
{
    if(!tmp->iter->cont.set->nextForIter(tmp->iter))
    {
        return false;
    }
    tmp->iter->cont.set->getForIterValue(tmp->iter, var);
    return true;
}

static bool initForFunc(ZorroVM* vm, Value* val, Value* /*var*/, Value* tmp)
{
    ZUNREF(vm, tmp);
    tmp->vt = vtCoroutine;
    tmp->flags = 0;
    tmp->cor = vm->allocCoroutine();
    tmp->cor->result = ((OpDstBase*) vm->ctx.lastOp)->dst;
    tmp->cor->ref();
    ZVMContext& ctx = tmp->cor->ctx;
    ctx.coroutine = tmp->cor;
    ctx.dataPtrs[atNul] = nullptr;
    ctx.dataPtrs[atGlobal] = vm->symbols.globals;
    ctx.dataPtrs[atLocal] = ctx.dataStack.stack;
    ctx.dataPtrs[atMember] = nullptr;
    ctx.dataPtrs[atClosed] = nullptr;
    vm->ctx.nextOp = ((OpForInit*) vm->ctx.lastOp)->corOp;
    vm->ctx.swap(ctx);
    vm->pushFrame((OpDstBase*) vm->ctx.lastOp, nullptr);
    vm->pushFrame(vm->dummyCallOp, vm->corRetOp, 0, val->func->index);
    vm->ctx.nextOp = val->func->entry;
    vm->ctx.dataStack.pushBulk(val->func->localsCount);

    return true;
}

static bool stepForFunc(ZorroVM* vm, Value* /*val*/, Value* /*var*/, Value* tmp)
{
    if(tmp->cor->ctx.nextOp)
    {
        vm->ctx.nextOp = ((OpForStep*) vm->ctx.lastOp)->corOp;
        tmp->cor->ctx.swap(vm->ctx);
        return true;
    } else
    {
        return false;
    }
}

static bool initForClosure(ZorroVM* vm, Value* val, Value* /*var*/, Value* tmp)
{
    ZUNREF(vm, tmp);
    Closure& cls = *val->cls;
    bool selfCls = cls.self.vt != vtNil;
    tmp->vt = vtCoroutine;
    tmp->flags = 0;
    tmp->cor = vm->allocCoroutine();
    tmp->cor->result = ((OpDstBase*) vm->ctx.lastOp)->dst;
    tmp->cor->ref();
    ZVMContext& ctx = tmp->cor->ctx;
    ctx.coroutine = tmp->cor;
    ctx.dataPtrs[atNul] = nullptr;
    ctx.dataPtrs[atGlobal] = vm->symbols.globals;
    ctx.dataPtrs[atLocal] = ctx.dataStack.stack;
    ctx.dataPtrs[atMember] = selfCls ? cls.self.obj->members : nullptr;
    ctx.dataPtrs[atClosed] = cls.closedValues;
    vm->ctx.nextOp = ((OpForInit*) vm->ctx.lastOp)->corOp;
    vm->ctx.swap(ctx);
    vm->pushFrame((OpDstBase*) vm->ctx.lastOp, nullptr);
    vm->pushFrame(vm->dummyCallOp, vm->corRetOp, 0, cls.func->index);
    vm->ctx.nextOp = cls.func->entry;
    vm->ctx.dataStack.pushBulk(cls.func->localsCount);
    Value* clsVal = ctx.dataStack.stackTop;
    ZASSIGN(vm, clsVal, val);
    if(selfCls)
    {
        ZASSIGN(vm, vm->ctx.dataStack.stack, &cls.self);
    }
    return true;
}

static bool initForDelegate(ZorroVM* vm, Value* val, Value* /*var*/, Value* tmp)
{
    ZUNREF(vm, tmp);
    Delegate& dlg = *val->dlg;
    Value* objVal = dlg.obj.vt == vtWeakRef ? &dlg.obj.weakRef->value : &dlg.obj;
    if(objVal->vt == vtNil)
    {
        ZTHROWR(RuntimeException, vm, "Object of delegate is nil");
    }
    Object& obj = *objVal->obj;
    tmp->vt = vtCoroutine;
    tmp->flags = 0;
    tmp->cor = vm->allocCoroutine();
    tmp->cor->result = ((OpDstBase*) vm->ctx.lastOp)->dst;
    tmp->cor->ref();
    ZVMContext& ctx = tmp->cor->ctx;
    ctx.coroutine = tmp->cor;
    ctx.dataPtrs[atNul] = nullptr;
    ctx.dataPtrs[atGlobal] = vm->symbols.globals;
    ctx.dataPtrs[atLocal] = ctx.dataStack.stack;
    ctx.dataPtrs[atMember] = obj.members;
    ctx.dataPtrs[atClosed] = nullptr;
    vm->ctx.nextOp = ((OpForInit*) vm->ctx.lastOp)->corOp;
    vm->ctx.swap(ctx);
    vm->pushFrame((OpDstBase*) vm->ctx.lastOp, nullptr);
    vm->pushFrame(vm->dummyCallOp, vm->corRetOp, 0, dlg.method->index);
    vm->ctx.nextOp = dlg.method->entry;
    vm->ctx.dataStack.pushBulk(dlg.method->localsCount);
    Value* self = vm->ctx.dataPtrs[atLocal];
    ZASSIGN(vm, self, objVal);
    return true;
}

static bool initFor2Map(ZorroVM* vm, Value* val, Value* var1, Value* var2, Value* iter)
{
    if(val->map->empty())
    {
        return false;
    }
    ZUNREF(vm, iter);

    iter->vt = vtForIterator;
    iter->flags = 0;
    iter->iter = val->map->getForIter();
    iter->iter->ref();
    iter->iter->cont = *val;
    iter->iter->ptr = nullptr;
    val->refBase->ref();

    ZMap::iterator it = val->map->begin();
    Value* keyPtr = &it->m_key;
    Value* valPtr = &it->m_value;
    ZASSIGN(vm, var1, keyPtr);
    ZASSIGN(vm, var2, valPtr);
    return true;
}

static bool stepFor2Map(ZorroVM* /*vm*/, Value* /*val*/, Value* var1, Value* var2, Value* iter)
{
    if(!iter->iter->cont.map->nextForIter(iter->iter))
    {
        return false;
    }
    iter->iter->cont.map->getForIterValue(iter->iter, var1, var2);
    return true;
}

static bool initFor2Array(ZorroVM* vm, Value* val, Value* var1, Value* var2, Value* iter)
{
    if(val->arr->getCount() == 0)
    {
        return false;
    }
    ZUNREF(vm, iter);
    iter->vt = vtSegment;
    iter->flags = 0;
    Segment& seg = *(iter->seg = vm->allocSegment());
    seg.ref();
    seg.cont = *val;
    val->refBase->ref();
    seg.segBase = 0;
    seg.segStart = 0;
    seg.segEnd = 0;
    seg.step = 1;
    Value idx;
    idx.vt = vtInt;
    idx.flags = ValFlagNone;
    idx.iValue = 0;
    Value* itemPtr = &iter->seg->cont.arr->getItem(0);
    ZASSIGN(vm, var1, &idx);
    ZASSIGN(vm, var2, itemPtr);
    return true;
}

static bool initFor2Segment(ZorroVM* vm, Value* val, Value* var1, Value* var2, Value* tmp)
{
    Segment& seg = *val->seg;
    if(seg.cont.vt != vtArray)
    {
        ZTHROWR(TypeException, vm, "Invalid type for segment container:%{}", getValueTypeName(seg.cont.vt));
    }
    ZArray& za = *seg.cont.arr;
    auto cnt = (int64_t) za.getCount();
    if(cnt == 0 || seg.segStart >= cnt || seg.segStart >= seg.segEnd)
    {
        return false;
    }
    ZUNREF(vm, tmp);
    tmp->vt = vtSegment;
    tmp->flags = 0;
    tmp->seg = vm->allocSegment();
    tmp->seg->ref();
    tmp->seg->cont = seg.cont;
    val->refBase->ref();
    //vm->assign(tmp.seg->cont,*val);
    tmp->seg->segBase = seg.segStart;
    tmp->seg->segStart = seg.step > 0 ? seg.segStart : seg.segEnd - 1;
    tmp->seg->segEnd = seg.segEnd;
    tmp->seg->step = seg.step;
    Value idx;
    idx.vt = vtInt;
    idx.flags = ValFlagNone;
    idx.iValue = 0;
    ZASSIGN(vm, var1, &idx);
    Value* itemPtr = &za.getItem(static_cast<size_t>(tmp->seg->segStart));
    ZASSIGN(vm, var2, itemPtr);
    return true;
}


static bool stepFor2Array(ZorroVM* vm, Value* /*val*/, Value* var1, Value* var2, Value* iter)
{
    Segment& seg = *iter->seg;
    ZArray& arr = *seg.cont.arr;
    if(seg.step > 0)
    {
        if((seg.segStart += seg.step) >= (int64_t) arr.getCount() || (seg.segEnd && seg.segStart >= seg.segEnd))
        {
            return false;
        }
    } else
    {
        if((seg.segStart += seg.step) < 0 || seg.segStart < seg.segBase)
        {
            return false;
        }
    }
    Value idx;
    idx.vt = vtInt;
    idx.flags = ValFlagNone;
    idx.iValue = seg.step > 0 ? seg.segStart - seg.segBase : seg.segEnd - 1 - seg.segStart;
    Value* itemPtr = &arr.getItem(static_cast<size_t>(seg.segStart));
    ZASSIGN(vm, var1, &idx);
    ZASSIGN(vm, var2, itemPtr);
    return true;
}

static bool initFor2Slice(ZorroVM* vm, Value* val, Value* var1, Value* var2, Value* tmp)
{
    Slice& slice = *val->slice;
    if(slice.indeces.arr->getCount() == 0)
    {
        return false;
    }
    ZUNREF(vm, tmp);
    tmp->vt = vtSlice;
    tmp->flags = 0;
    Slice& dslice = *(tmp->slice = vm->allocSlice());
    dslice.cont = slice.cont;
    dslice.cont.refBase->ref();
    dslice.indeces = slice.indeces;
    dslice.indeces.refBase->ref();
    dslice.index = 0;
    Value idx = IntValue(dslice.index);
    ZASSIGN(vm, var1, &idx);
    vm->getIndexMatrix[val->vt][idx.vt](vm, val, &idx, var2);
    //Value& idx=slice.indeces.arr->getItem(0);
    //vm->getIndexMatrix[dslice.cont.vt][vtInt](vm,&dslice.cont,&idx,var2);
    return true;
}

static bool stepFor2Slice(ZorroVM* vm, Value* /*val*/, Value* var1, Value* var2, Value* tmp)
{
    Slice& slice = *tmp->slice;
    if(++slice.index >= slice.indeces.arr->getCount())
    {
        return false;
    }
    Value idx = IntValue(slice.index);
    ZASSIGN(vm, var1, &idx);
    vm->getIndexMatrix[tmp->vt][idx.vt](vm, tmp, &idx, var2);
    return true;
}


static void corFunc(ZorroVM* vm, Value* val, Value* dst)
{
    Coroutine* cor = vm->allocCoroutine();
    ZVMContext& ctx = cor->ctx;
    ctx.dataPtrs[atNul] = nullptr;
    ctx.dataPtrs[atGlobal] = vm->symbols.globals;
    ctx.dataPtrs[atLocal] = ctx.dataStack.stack;
    ctx.dataPtrs[atMember] = nullptr;
    ctx.dataPtrs[atClosed] = nullptr;
    ctx.coroutine = cor;
    CallFrame* cf = ctx.callStack.push();
    cf->callerOp = nullptr;
    cf->retOp = nullptr;
    cf->localBase = 0;
    cf->args = 0;
    cf->selfCall = false;
    cf->closedCall = false;
    cf->overload = false;
    cf = ctx.callStack.push();
    cf->callerOp = vm->dummyCallOp;
    cf->retOp = vm->corRetOp;
    cf->localBase = 0;
    cf->args = 0;
    cf->funcIdx = val->func->index;
    cf->selfCall = false;
    cf->closedCall = false;
    cf->overload = false;
    ctx.nextOp = val->func->entry;
    ctx.dataStack.pushBulk(val->func->localsCount);
    Value res;
    res.vt = vtCoroutine;
    res.flags = ValFlagNone;
    res.cor = cor;
    ZASSIGN(vm, dst, &res);
}

static void corCls(ZorroVM* vm, Value* val, Value* dst)
{
    Closure* cls = val->cls;
    Coroutine* cor = vm->allocCoroutine();
    ZVMContext& ctx = cor->ctx;
    ctx.dataPtrs[atNul] = nullptr;
    ctx.dataPtrs[atGlobal] = vm->symbols.globals;
    ctx.dataPtrs[atLocal] = ctx.dataStack.stack;
    ctx.dataPtrs[atMember] = nullptr;
    ctx.dataPtrs[atClosed] = cls->closedValues;
    ctx.coroutine = cor;
    CallFrame* cf = ctx.callStack.push();
    cf->callerOp = nullptr;
    cf->retOp = nullptr;
    cf->localBase = 0;
    cf->args = 0;
    cf->selfCall = false;
    cf->closedCall = false;
    cf->overload = false;
    cf = ctx.callStack.push();
    cf->callerOp = vm->dummyCallOp;
    cf->retOp = vm->corRetOp;
    cf->localBase = 0;
    cf->args = 0;
    cf->funcIdx = cls->func->index;
    cf->selfCall = false;
    cf->closedCall = false;
    cf->overload = false;
    ctx.nextOp = cls->func->entry;
    ctx.dataStack.pushBulk(cls->func->localsCount);
    if(cls->self.vt != vtNil)
    {
        Value* self = ctx.dataStack.stack + cf->localBase + cls->func->argsCount;
        ZASSIGN(vm, self, &cls->self);
        ctx.dataPtrs[atMember] = self->obj->members;
        cf->selfCall = true;
    }
    Value* clsStore = ctx.dataStack.stackTop;
    ZASSIGN(vm, clsStore, val);
    Value res;
    res.vt = vtCoroutine;
    res.flags = ValFlagNone;
    res.cor = cor;
    ZASSIGN(vm, dst, &res);
}


static void corDlg(ZorroVM* vm, Value* val, Value* dst)
{
    Delegate& dlg = *val->dlg;
    Value* objVal = dlg.obj.vt == vtWeakRef ? &dlg.obj.weakRef->value : &dlg.obj;
    Object& obj = *objVal->obj;
    Coroutine* cor = vm->allocCoroutine();
    ZVMContext& ctx = cor->ctx;
    ctx.dataPtrs[atNul] = nullptr;
    ctx.dataPtrs[atGlobal] = vm->symbols.globals;
    ctx.dataPtrs[atLocal] = ctx.dataStack.stack;
    ctx.dataPtrs[atMember] = obj.members;
    ctx.dataPtrs[atClosed] = nullptr;
    ctx.coroutine = cor;
    CallFrame* cf = ctx.callStack.push();
    cf->callerOp = nullptr;
    cf->retOp = nullptr;
    cf->localBase = 0;
    cf->args = 0;
    cf->selfCall = false;
    cf->closedCall = false;
    cf->overload = false;
    cf = ctx.callStack.push();
    cf->callerOp = vm->dummyCallOp;
    cf->retOp = vm->corRetOp;
    cf->localBase = 0;
    cf->args = 0;
    cf->selfCall = true;
    cf->closedCall = false;
    cf->overload = false;
    ctx.nextOp = dlg.method->entry;
    ctx.dataStack.pushBulk(dlg.method->localsCount);
    Value* self = ctx.dataStack.stack;
    ZASSIGN(vm, self, objVal);
    Value res;
    res.vt = vtCoroutine;
    res.flags = ValFlagNone;
    res.cor = cor;
    ZASSIGN(vm, dst, &res);
}

#define CLEARWEAK if(val->refBase->weakRefId)vm->clearWeakRefs(val->refBase->weakRefId)

static void unrefString(ZorroVM* vm, Value* val)
{
    CLEARWEAK;
    DPRINT("delete str(%s)\n", val->str->c_str(vm));
    val->str->clear(vm);
    vm->freeZString(val->str);
}

static void unrefRef(ZorroVM* vm, Value* val)
{
    CLEARWEAK;
    DPRINT("delete ref\n");
    ZUNREF(vm, &val->valueRef->value);
    vm->ZorroVM::freeRef(val->valueRef);
}

static void unrefWRef(ZorroVM* vm, Value* val)
{
    DPRINT("delete wref\n");
    WeakRef* wr = val->weakRef;
    WeakRef* base = vm->wrStorage[wr->weakRefId - 1];
    if(wr->wrNext == wr)
    {
        vm->unregisterWeakRef(wr->weakRefId);
        if(wr->value.vt != vtNil)
        {
            wr->value.refBase->weakRefId = 0;
        }
    } else
    {
        wr->wrPrev->wrNext = wr->wrNext;
        wr->wrNext->wrPrev = wr->wrPrev;
        if(wr == base)
        {
            vm->wrStorage[wr->weakRefId - 1] = wr->wrNext;
        }
    }
    ZUNREF(vm, &wr->cont);
    vm->ZorroVM::freeWRef(wr);
}

static void unrefArray(ZorroVM* vm, Value* val)
{
    CLEARWEAK;
    DPRINT("delete arr\n");
    ZArray& za = *(val->arr);
    size_t count = za.getCount();
    if(!za.isSimpleContent)
    {
        for(size_t i = 0; i < count; ++i)
        {
            ZUNREF(vm, &za.getItemRef(i));
        }
    }
    vm->ZorroVM::freeZArray(val->arr);

}

static void unrefSegment(ZorroVM* vm, Value* val)
{
    CLEARWEAK;
    DPRINT("delete segment\n");
    ZUNREF(vm, &val->seg->cont);
    vm->ZorroVM::freeSegment(val->seg);
}

static void unrefSlice(ZorroVM* vm, Value* val)
{
    CLEARWEAK;
    DPRINT("delete slice\n");
    ZUNREF(vm, &val->slice->cont);
    ZUNREF(vm, &val->slice->indeces);
    vm->ZorroVM::freeSlice(val->slice);
}

static void unrefKeyRef(ZorroVM* vm, Value* val)
{
    CLEARWEAK;
    DPRINT("delete prop\n");
    ZUNREF(vm, &val->keyRef->obj);
    ZUNREF(vm, &val->keyRef->name);
    vm->ZorroVM::freeKeyRef(val->keyRef);
}

static void unrefRange(ZorroVM* vm, Value* val)
{
    CLEARWEAK;
    DPRINT("delete range\n");
    vm->ZorroVM::freeRange(val->range);
}

static void unrefMap(ZorroVM* vm, Value* val)
{
    CLEARWEAK;
    DPRINT("delete map\n");
    ZMap& zm = *val->map;
    for(ZMap::iterator it = zm.begin(), end = zm.end(); it != end; ++it)
    {
        ZUNREF(vm, &it->m_key);
        ZUNREF(vm, &it->m_value);
    }
    vm->ZorroVM::freeZMap(val->map);
}

static void unrefSet(ZorroVM* vm, Value* val)
{
    CLEARWEAK;
    DPRINT("delete set\n");
    /*
  ZSet& zs=*val->set;
  for(ZSet::iterator it=zs.begin(),end=zs.end();it!=end;++it)
  {
    ZUNREF(vm,&*it);
  }*/
    vm->ZorroVM::freeZSet(val->set);
}


static void unrefObj(ZorroVM* vm, Value* val)
{
    DPRINT("delete obj %p\n", val->obj);
    Object& zo = *val->obj;
    if(vm->running)
    {
        if(zo.classInfo)
        {
            if(zo.classInfo->specialMethods[csmDestructor])
            {
                zo.ref();
                INITCALL(vm, vm->dummyCallOp, vm->ctx.nextOp, 0, true, false, false);
                Value* dtor = &vm->symbols.globals[zo.classInfo->specialMethods[csmDestructor]];
                cf->funcIdx = dtor->func->index;

                DPRINT("dtor localbase=%d\n", cf->localBase);

                vm->ctx.dataPtrs[atLocal] = vm->ctx.dataStack.stack + cf->localBase;
                vm->ctx.dataPtrs[atMember] = zo.members;
                vm->ctx.destructorStack.push(*val);
                vm->ctx.nextOp = dtor->func->entry;
                return;
            } else
            {
                CLEARWEAK;
                for(size_t i = 0; i < zo.classInfo->membersCount; i++)
                {
                    ZUNREF(vm, &zo.members[i]);
                }
                vm->freeVArray(zo.members, zo.classInfo->membersCount);
                if(zo.classInfo->unref())
                {
                    delete zo.classInfo;
                }
                zo.classInfo = nullptr;
            }
        } else
        {
            CLEARWEAK;
        }
    } else
    {
        CLEARWEAK;
        for(size_t i = 0; i < zo.classInfo->membersCount; i++)
        {
            ZUNREF(vm, &zo.members[i]);
        }
        vm->freeVArray(zo.members, zo.classInfo->membersCount);
        if(zo.classInfo->unref())
        {
            delete zo.classInfo;
        }
        zo.classInfo = nullptr;
    }

    vm->ZorroVM::freeObj(val->obj);
}

static void unrefNObj(ZorroVM* vm, Value* val)
{
    CLEARWEAK;
    size_t didx = val->nobj->classInfo->specialMethods[csmDestructor];
    if(didx)
    {
        vm->symbols.globals[didx].cmethod->cmethod(vm, val);
    }
    val->nobj->classInfo->unref();
    vm->freeNObj(val->nobj);
}

static void unrefDlg(ZorroVM* vm, Value* val)
{
    CLEARWEAK;
    DPRINT("delete [c]dlg\n");
    ZUNREF(vm, &val->dlg->obj);
    vm->freeDlg(val->dlg);
}


static void unrefCls(ZorroVM* vm, Value* val)
{
    CLEARWEAK;
    DPRINT("delete cls\n");
    Closure* cls = val->cls;
    for(Value* ptr = cls->closedValues, * end = cls->closedValues + cls->closedCount; ptr != end; ++ptr)
    {
        ZUNREF(vm, ptr);
    }
    ZUNREF(vm, &cls->self);
    vm->freeVArray(cls->closedValues, cls->closedCount);
    vm->freeClosure(cls);
}

static void unrefCor(ZorroVM* vm, Value* val)
{
    CLEARWEAK;
    DPRINT("delete cor\n");
    ZVMContext& ctx = val->cor->ctx;
    Value* ptr = ctx.dataStack.stack;
    Value* end = ctx.dataStack.stackTop;
    while(ptr <= end)
    {
        ZUNREF(vm, ptr);
        ++ptr;
    }
    ctx.dataStack.reset();
    ctx.callStack.reset();
    ctx.catchStack.reset();
    vm->freeCoroutine(val->cor);
}

static void unrefForIter(ZorroVM* vm, Value* val)
{
    CLEARWEAK;
    DPRINT("delete for iter\n");
    ZUNREF(vm, &val->iter->cont);
    vm->freeForIterator(val->iter);
}

static void unrefMembRef(ZorroVM* vm, Value* val)
{
    CLEARWEAK;
    DPRINT("delete member ref\n");
    ZUNREF(vm, &val->membRef->obj);
    ZUNREF(vm, &val->membRef->getValue);
    val->membRef->name->unref();
    vm->freeMembRef(val->membRef);
}

static void unrefRegExp(ZorroVM* vm, Value* val)
{
    CLEARWEAK;
    DPRINT("delete regexp\n");
    vm->freeRegExp(val->regexp);
}


static void getNativeObjData(ZorroVM* vm, ClassInfo* classInfo, Value* l, const Value* r, Value* dst)
{
    if(!dst)
    {
        return;
    }
    SymInfo** infoPtr = classInfo->symMap.getPtr(r->str);
    if(!infoPtr)
    {
        ZTHROWR(TypeException, vm, "%{} is not property or member of class %{}", ZStringRef(vm, r->str),
                classInfo->name);
    }
    SymInfo* info = *infoPtr;
    if(info->st == sytMethod)
    {
        Value* mv = &vm->symbols.globals[info->index];
        Value res;
        res.flags = ValFlagNone;
        Delegate* dlg = vm->allocDlg();
        dlg->obj = *l;
        if(ZISREFTYPE(l))
        {
            l->refBase->ref();
        }
        if(mv->vt == vtCMethod)
        {
            dlg->method = mv->cmethod;
            res.vt = vtCDelegate;
        } else
        {
            dlg->method = mv->method;
            res.vt = vtDelegate;
        }
        res.dlg = dlg;
        ZASSIGN(vm, dst, &res);
    } else if(info->st == sytProperty)
    {
        auto* cp = (ClassPropertyInfo*) info;
        if(!cp->getMethod)
        {
            ZTHROWR(TypeException, vm, "Property %{} of class %{} cannot be read", info->name, classInfo->name);
        }
        vm->callCMethod(l, cp->getMethod, 0);
        ZASSIGN(vm, dst, &vm->result);
        vm->clearResult();
    } else
    {
        ZTHROWR(TypeException, vm, "Unknown type of member %{} of native class %{}", info->name, classInfo->name);
    }
}

static void getStringProp(ZorroVM* vm, Value* l, const Value* r, Value* dst)
{
    getNativeObjData(vm, vm->stringClass, l, r, dst);
}

static void getArrayProp(ZorroVM* vm, Value* l, const Value* r, Value* dst)
{
    getNativeObjData(vm, vm->arrayClass, l, r, dst);
}


static void getNObjectProp(ZorroVM* vm, Value* l, const Value* r, Value* dst)
{
    getNativeObjData(vm, l->nobj->classInfo, l, r, dst);
}

static void getClassProp(ZorroVM* vm, Value* l, const Value* r, Value* dst)
{
    getNativeObjData(vm, vm->classClass, l, r, dst);
}

static void getCorProp(ZorroVM* vm, Value* l, const Value* r, Value* dst)
{
    getNativeObjData(vm, vm->corClass, l, r, dst);
}


static void getObjectMember(ZorroVM* vm, Value* l, const Value* r, Value* dst)
{
    ClassInfo* ci = l->obj->classInfo;
    SymInfo** infoPtr = ci->symMap.getPtr(r->str);
    if(!infoPtr)
    {
        if(ci->specialMethods[csmGetProp])
        {
            vm->ctx.dataStack.push(*r);
            if(ZISREFTYPE(r))
            {
                r->refBase->ref();
            }
            vm->callMethod(l, ci->specialMethods[csmGetProp], 1);
            return;
        }
        ZTHROWR(TypeException, vm, "%{} is not property or member of class %{}", ZStringRef(vm, r->str), ci->name);
    }
    SymInfo* info = *infoPtr;
    if(info->st == sytMethod)
    {
        Value res;
        res.flags = ValFlagNone;
        Delegate* dlg = vm->allocDlg();
        dlg->obj = *l;
        dlg->obj.obj->ref();
        dlg->method = vm->symbols.globals[info->index].method;
        res.vt = vtDelegate;
        res.dlg = dlg;
        ZASSIGN(vm, dst, &res);
        //ZASSIGN(vm,*dst,vm->symbols.globals[info->index]);
    } else if(info->st == sytClassMember)
    {
        Value* v = &l->obj->members[info->index];
        if(v->vt == vtMethod)
        {
            if(((MethodInfo*) v->func)->owningClass != l->obj->classInfo &&
               !l->obj->classInfo->isInParents(((MethodInfo*) v->func)->owningClass))
            {
                ZTHROWR(RuntimeException, vm, "Invalid class %{} to construct delegate with method %{}", ci->name,
                        v->method->name);
            }
            Value res;
            res.flags = ValFlagNone;
            Delegate* dlg = vm->allocDlg();
            dlg->obj = *l;
            dlg->obj.obj->ref();
            dlg->method = v->method;
            res.vt = vtDelegate;
            res.dlg = dlg;
            ZASSIGN(vm, dst, &res);
        } else
        {
            ZASSIGN(vm, dst, v);
        }
    } else if(info->st == sytProperty)
    {
        auto* p = (ClassPropertyInfo*) info;
        if(p->getIdx != SymInfo::invalidIndexValue)
        {
            Value* v = &l->obj->members[p->getIdx];
            ZASSIGN(vm, dst, v);
        } else if(p->getMethod)
        {
            vm->callMethod(l, p->getMethod, 0);
        } else
        {
            ZTHROWR(TypeException, vm, "Property %{} of class %{} is write only", p->name, ci->name);
        }
    }
}

static void getObjectMethod(ZorroVM* vm, Value* l, const Value* r, Value* dst)
{
    if(!dst)
    {
        return;
    }
    ClassInfo* ci = l->obj->classInfo;
    MethodInfo* mi = r->method;
    if(ci != mi->owningClass && !ci->isInParents(mi->owningClass))
    {
        ZTHROWR(RuntimeException, vm, "Invalid class %{} to construct delegate with method %{}", ci->name, mi->name);
    }
    Value res;
    Delegate* dlg = vm->allocDlg();
    dlg->obj = *l;
    dlg->obj.obj->ref();
    dlg->method = r->method;
    res.vt = vtDelegate;
    res.flags = ValFlagNone;
    res.dlg = dlg;
    ZASSIGN(vm, dst, &res);
}

static void setRefMember(ZorroVM* vm, Value* l, const Value* a, const Value* r, Value* dst)
{
    vm->setMemberMatrix[l->valueRef->value.vt][a->vt](vm, &l->valueRef->value, a, r, dst);
}

static void setObjectMember(ZorroVM* vm, Value* l, const Value* a, const Value* r, Value* dst)
{
    ClassInfo* ci = l->obj->classInfo;
    SymInfo** infoPtr = ci->symMap.getPtr(a->str);
    if(!infoPtr)
    {
        if(ci->specialMethods[csmSetProp])
        {
            vm->ctx.dataStack.push(*a);
            a->refBase->ref();
            vm->ctx.dataStack.push(*r);
            if(ZISREFTYPE(r))
            {
                r->refBase->ref();
            }
            if(dst)
            {
                ZASSIGN(vm, dst, r);
            }
            vm->callMethod(l, ci->specialMethods[csmSetProp], 2);
            return;
        }
        ZTHROWR(TypeException, vm, "%{} is not property or member of class %{}", ZStringRef(vm, a->str), ci->name);
    }
    SymInfo* info = *infoPtr;
    if(info->st == sytClassMember)
    {
        ZASSIGN(vm, &l->obj->members[info->index], r);
    } else if(info->st == sytProperty)
    {
        auto* p = (ClassPropertyInfo*) info;
        if(p->setIdx != SymInfo::invalidIndexValue)
        {
            Value* v = &l->obj->members[p->setIdx];
            ZASSIGN(vm, v, r);
        } else if(p->setMethod)
        {
            Value* arg = vm->ctx.dataStack.push();
            ZASSIGN(vm, arg, r);
            vm->callMethod(l, p->setMethod, 1);
        } else
        {
            ZTHROWR(TypeException, vm, "Property %{} of class %{} is read only", info->name, ci->name);
        }
    } else
    {
        ZTHROWR(TypeException, vm, "%{} is not data member or property of class %{}", info->name, ci->name);
    }
    if(dst)
    {
        ZASSIGN(vm, dst, r);
    }
}

static void setNObjectMember(ZorroVM* vm, Value* l, const Value* a, const Value* r, Value* dst)
{
    ClassInfo* ci = l->obj->classInfo;
    SymInfo** infoPtr = ci->symMap.getPtr(a->str);
    if(!infoPtr)
    {
        if(ci->specialMethods[csmSetProp])
        {
            vm->ctx.dataStack.push(*a);
            a->refBase->ref();
            vm->pushValue(*r);
            vm->callCMethod(l, ci->specialMethods[csmSetProp], 2);
            if(dst)
            {
                ZASSIGN(vm, dst, r);
            }
            return;
        }
        ZTHROWR(TypeException, vm, "%{} is not property or member of native class %{}", ZStringRef(vm, a->str),
                ci->name);
    }
    SymInfo* info = *infoPtr;
    if(info->st == sytProperty)
    {
        auto* p = (ClassPropertyInfo*) info;
        if(p->setMethod)
        {
            Value* arg = vm->ctx.dataStack.push();
            ZASSIGN(vm, arg, r);
            vm->callCMethod(l, p->setMethod, 1);
        } else
        {
            ZTHROWR(TypeException, vm, "Property %{} of native class %{} is read only", info->name, ci->name);
        }
    } else
    {
        ZTHROWR(TypeException, vm, "%{} is not data member or property of class %{}", info->name, ci->name);
    }
    if(dst)
    {
        ZASSIGN(vm, dst, r);
    }
}


static void getRefProp(ZorroVM* vm, Value* l, const Value* r, Value* dst)
{
    vm->getMemberMatrix[l->valueRef->value.vt][r->vt](vm, &l->valueRef->value, r, dst);
}

Value ZorroVM::mkWeakRef(Value* src)
{
    Value rv;
    rv.vt = vtWeakRef;
    rv.flags = ValFlagNone;
    bool fakeRv = false;
    if(src->vt == vtRef)
    {
        src = &src->valueRef->value;
    }
    if(src->vt == vtDelegate)
    {
        rv = *src;
        src = &src->dlg->obj;
        fakeRv = true;
    } else if(src->vt == vtClosure)
    {
        rv = *src;
        src = &src->cls->self;
        fakeRv = true;
    }
    if(src->vt == vtRef)
    {
        src = &src->valueRef->value;
    }
    WeakRef* wr = allocWRef();
    if(src->refBase->weakRefId)
    {
        wr->weakRefId = src->refBase->weakRefId;
        WeakRef* base = wrStorage[wr->weakRefId - 1];
        wr->wrNext = base->wrNext;
        wr->wrPrev = base;
        base->wrNext->wrPrev = wr;
        base->wrNext = wr;
    } else
    {
        src->refBase->weakRefId = registerWeakRef(wr);
    }
    if(src->vt == vtWeakRef)
    {
        wr->value = src->valueRef->value;
    } else
    {
        wr->value = *src;
    }
    wr->cont = NilValue;
    if(!fakeRv)
    {
        rv.weakRef = wr;
    } else
    {
        ZUNREF(this, src);
        src->vt = vtWeakRef;
        src->flags = 0;
        src->weakRef = wr;
    }
    return rv;
}

void ZorroVM::clearWeakRefs(unsigned index)
{
    WeakRef* ptr = wrStorage[index - 1];
    WeakRef* end = ptr;
    do
    {
        if(ptr->cont.vt != vtNil)
        {
            if(ptr->cont.weakRef->value.vt == vtMap)
            {
                ptr->cont.weakRef->value.map->erase(ptr->value);
            } else if(ptr->cont.weakRef->value.vt == vtSet)
            {
                ptr->cont.weakRef->value.set->erase(ptr->value);
            }
        }
        ptr->value = NilValue;
        ptr = ptr->wrNext;
    } while(ptr != end);
}


void ZorroVM::throwValue(Value* obj)
{
    Value classVal = NilValue;
    getTypeOps[obj->vt](this, obj, &classVal);
    ClassInfo* cls = classVal.classInfo;
    Coroutine* corArr[32];
    size_t corCnt = 0;

    for(;;)
    {
        while(ctx.catchStack.size())
        {
            CatchInfo ci = *ctx.catchStack.stackTop;
            ctx.catchStack.pop();
            if(ci.exList)
            {
                bool found = false;
                for(index_type i = 0; i < ci.exCount; i++)
                {
                    ClassInfo* ptr = ci.exList[i];
                    if(cls->isInParents(ptr))
                    {
                        found = true;
                        break;
                    }
                }
                if(!found)
                {
                    continue;
                }
            }
            size_t lbase = ctx.callStack.stack[ci.callLevel - 1].localBase;
            Value* dst = ctx.dataStack.stack + lbase + ci.idx;
            ZASSIGN(this, dst, obj);
            while(ctx.callStack.size() > ci.callLevel)
            {
                ctx.callStack.pop();
            }
            ctx.nextOp = ci.catchOp;
            CallFrame* cf = ctx.callStack.stackTop;
            ctx.dataPtrs[atLocal] = ctx.dataStack.stack + cf->localBase;
            if(cf->selfCall)
            {
                auto* callerOp = (OpCallBase*) cf->callerOp;
                ctx.dataPtrs[atMember] = ctx.dataPtrs[atLocal][callerOp->args].obj->members;
            }
            if(cf->closedCall)
            {
                Value* clsv = ctx.dataStack.stack + ci.stackLevel - 1;
                ctx.dataPtrs[atClosed] = clsv->cls->closedValues;
            }
            for(size_t i = corCnt - 1; i < corCnt; --i)
            {
                ZVMContext& cctx = corArr[i]->ctx;
                for(Value* ptr = cctx.dataStack.stack, * end = cctx.dataStack.stackTop + 1; ptr != end; ++ptr)
                {
                    ZUNREF(this, ptr);
                }
                cctx.nextOp = nullptr;
            }
            cleanStack(ci.stackLevel);
            return;
        }
        if(ctx.coroutine)
        {
            if(corCnt == 32)
            {
                ZTHROWR(RuntimeException, this, "Too deep nested coroutine throw");
            }
            corArr[corCnt++] = ctx.coroutine;
            ctx.swap(ctx.coroutine->ctx);
            continue;
        }
        break;
    }
    if(obj->vt == vtString)
    {
        ZTHROWR(RuntimeException, this, "Uncought exception of type %{}", obj->str->c_str(this));
    } else
    {
        if(obj->vt == vtObject)
        {
            ZTHROWR(RuntimeException, this, "Uncought exception of type %{}", obj->obj->classInfo->name);
        } else
        {
            ZTHROWR(RuntimeException, this, "Uncought exception of type %{}", getValueTypeName(obj->vt));
        }
    }
}

void ZorroVM::getStackTrace(StackTraceVector& trace, bool skipLevel)
{
    CallFrame* top = ctx.callStack.stackTop;
    CallFrame* prev = top;
    top += skipLevel ? 0 : 1;
    CallFrame* btm = ctx.callStack.stack;
    ZVMContext* curCtx = &ctx;
    std::string funcName;
    std::string text;
    std::string fileName;
    while(--top >= btm)
    {
        if(!top->retOp)
        {
            if(top == btm && curCtx->coroutine)
            {
                prev = top;
                curCtx = &curCtx->coroutine->ctx;
                top = curCtx->callStack.stackTop + 1;
                btm = curCtx->callStack.stack;
                continue;
            }
        }
        if(top->funcIdx == 0)
        {
            funcName = "global scope";
        } else
        {
            FuncInfo* fi = symbols.globals[top->funcIdx].func;
            funcName.clear();
            fi->fullName(funcName);
        }
        if(prev->callerOp && prev->callerOp->pos.fileRd)
        {
            fileName = prev->callerOp->pos.fileRd->getEntry()->name;
            text = FORMAT("%{}:%{}:%{} (%{})", fileName, prev->callerOp->pos.line + 1, prev->callerOp->pos.col + 1,
                          funcName);
        } else
        {
            fileName = "???";
            text = FORMAT("???: (%{})", funcName);
        }
        trace.push_back(StackTraceItem(text, funcName, fileName, &prev->callerOp->pos));
        prev = top;
    }

}

std::string ZorroVM::getStackTraceText(bool skipLevel)
{
    StackTraceVector trace;
    getStackTrace(trace, skipLevel);
    std::string rv;
    if(ctx.lastOp)
    {
        rv = ctx.lastOp->pos.backTrace();
        if(ctx.callStack.stackTop->funcIdx != 0)
        {
            rv += FORMAT(" (%{})", symbols.globals[ctx.callStack.stackTop->funcIdx].func->name);
        }
        rv += "\n";
    }
    for(auto& it : trace)
    {
        rv += it.text;
        rv += "\n";
    }
    return rv;
}

void ZorroVM::callCMethod(Value* obj, MethodInfo* meth, index_type argsCount)
{
    INITCALL(this, ctx.lastOp, ctx.nextOp, argsCount, false, false, false);
    cf->funcIdx = meth->index;
    Value* saveLocal = ctx.dataPtrs[atLocal];
    ctx.dataPtrs[atLocal] = ctx.dataStack.stack + cf->localBase;
    meth->cmethod(this, obj);
    cleanStack(cf->localBase);
    ctx.callStack.pop();
    ctx.dataPtrs[atLocal] = saveLocal;
}

void ZorroVM::callCMethod(Value* obj, index_type methIdx, index_type argsCount)
{
    INITCALL(this, ctx.lastOp, ctx.nextOp, argsCount, false, false, false);
    cf->funcIdx = methIdx;
    Value* saveLocal = ctx.dataPtrs[atLocal];
    ctx.dataPtrs[atLocal] = ctx.dataStack.stack + cf->localBase;
    ZGLOBAL(this, methIdx).cmethod->cmethod(this, obj);
    cleanStack(cf->localBase);
    ctx.callStack.pop();
    ctx.dataPtrs[atLocal] = saveLocal;
}

void ZorroVM::callMethod(Value* obj, index_type midx, index_type argsCount, bool isOverload)
{
    callMethod(obj, symbols.globals[midx].method, argsCount, isOverload);
}

void ZorroVM::callMethod(Value* obj, MethodInfo* meth, index_type argsCount, bool isOverload)
{
    INITCALL(this, ctx.lastOp, ctx.nextOp, argsCount, true, false, isOverload);
    cf->funcIdx = meth->index;
    DPRINT("call method %s, localBase=%d\n", meth->name.val.c_str(), cf->localBase);
    OpBase* mentry = getFuncEntry(meth, cf);
    ctx.dataPtrs[atLocal] = ctx.dataStack.stack + cf->localBase;
    ctx.dataPtrs[atMember] = obj->obj->members;
    if(meth->localsCount)
    {
        //for(int i=0;i<f->locals;i++)vm->ctx.dataStack.push(NilValue);
        ctx.dataStack.pushBulk(meth->localsCount);
    }
    ctx.dataPtrs[atLocal][argsCount] = *obj;
    obj->refBase->ref();
    ctx.nextOp = mentry;

}

void ZorroVM::callSomething(Value* func, index_type argsCount)
{
    INITCALL(this, ctx.lastOp, ctx.nextOp, argsCount, false, false, false);
    callOps[func->vt](this, func);
}

void ZorroVM::returnAndResume(OpBase* op, const Value& val)
{
    ctx.nextOp = op;
    CallFrame* cf = ctx.callStack.stackTop;
    cleanStack(cf->localBase);
    ctx.callStack.pop();

    OpArg dstArg = ((OpDstBase*) ctx.lastOp)->dst;
    if(dstArg.at != atNul)
    {
        ZorroVM* vm = this;
        Value* dst = GETDST(dstArg);
        ZASSIGN(this, dst, &val);
    }
    resume();
}

void ZorroVM::rxMatch(Value* l, Value* r, Value* d, OpArg* vars)
{
    if(l->vt == vtRef || l->vt == vtWeakRef)
    {
        l = &l->valueRef->value;
    }
    if(r->vt == vtRef || r->vt == vtWeakRef)
    {
        r = &r->valueRef->value;
    }
    int cs;
    const char* start;
    const char* end;
    size_t startOff = 0;
    size_t endOff = 0;
    if(l->vt != vtString || r->vt != vtRegExp)
    {
        if(!(l->vt == vtSegment && l->seg->cont.vt == vtString))
        {
            ZTHROWR(TypeException, this, "Invalid type for match operator: %{}", getValueTypeName(l->vt));
        }
    }
    RegExpVal* rxv = r->regexp;
    kst::MatchInfo mi(rxv->marr, rxv->marrSize, rxv->narr, rxv->narrSize);
    ZString* s;
    const Value* sval = l;
    if(l->vt == vtString)
    {
        s = l->str;
        cs = s->getCharSize();
        start = s->getDataPtr();
        end = start + s->getDataSize();
    } else
    {
        const Segment& seg = *l->seg;
        sval = &seg.cont;
        s = seg.cont.str;
        cs = s->getCharSize();
        if(seg.segStart >= 0)
        {
            startOff = static_cast<size_t>(seg.segStart);
        }
        if(startOff >= s->getLength())
        {
            startOff = s->getLength();
        }
        endOff = static_cast<size_t>(seg.segEnd);
        if(endOff < startOff)
        {
            endOff = startOff;
        }
        if(endOff > s->getLength())
        {
            endOff = s->getLength();
        }
        start = s->getDataPtr() + startOff * cs;
        end = s->getDataPtr() + endOff * cs;
    }
    int res;
    if(cs == 1)
    {
        res = rxv->val->Search(start, end, mi);
    } else
    {
        res = rxv->val->Search((uint16_t*) start, (uint16_t*) end, mi);
    }
    if(!res)
    {
        if(d)
        {
            Value nil = NilValue;
            ZASSIGN(this, d, &nil);
        }
    } else
    {
        Value arr;
        arr.vt = vtArray;
        arr.flags = ValFlagNone;
        ZArray& za = *(arr.arr = allocZArray());
        for(size_t i = 0; i < mi.brCount; ++i)
        {
            Value segVal = NilValue;
            if(mi.br[i].start >= 0 && mi.br[i].end >= 0)
            {
                segVal.vt = vtSegment;
                segVal.flags = 0;
                Segment& seg = *(segVal.seg = allocSegment());
                seg.cont = *sval;
                seg.cont.str->ref();
                seg.segStart = startOff + mi.br[i].start;
                seg.segEnd = startOff + mi.br[i].end;
                seg.step = 1;
            }
            za.pushAndRef(segVal);
        }
        if(vars)
        {
            size_t cnt = rxv->val->getNamedBracketsCount();
            for(size_t i = 0; i < cnt; ++i)
            {
                Value* var = &ctx.dataPtrs[vars[i].at][vars[i].idx];
                Value segVal = NilValue;
                kst::SMatch* br = mi.hasNamedMatch(rxv->val->getName(i));
                if(br && br->start >= 0)
                {
                    segVal.vt = vtSegment;
                    segVal.flags = 0;
                    Segment& seg = *(segVal.seg = allocSegment());
                    seg.cont = *sval;
                    seg.cont.str->ref();
                    seg.segStart = startOff + br->start;
                    seg.segEnd = startOff + br->end;
                    seg.step = 1;
                }
                ZASSIGN(this, var, &segVal);
            }
        }
        ZASSIGN(this, d, &arr);
    }
}

ZorroVM::ZorroVM() : symbols(this)/*,ctx(*(new ZVMContext))*/, entry(nullptr)
{
    for(int l = 0; l < vtCount; l++)
    {
        for(int r = 0; r < vtCount; r++)
        {
            if(l < vtRefTypeBase && r < vtRefTypeBase)
            {
                assignMatrix[l][r] = nullptr;
            } else if(l >= vtRefTypeBase && r < vtRefTypeBase)
            {
                assignMatrix[l][r] = assignScalarToRefType;
            } else if(l < vtRefTypeBase && r >= vtRefTypeBase)
            {
                assignMatrix[l][r] = assignRefTypeToScalar;
            } else
            {
                assignMatrix[l][r] = assignRefTypeToRefType;
            }
            addMatrix[l][r] = errorFunc;
            saddMatrix[l][r] = errorFunc;
            ssubMatrix[l][r] = errorFunc;
            subMatrix[l][r] = errorFunc;
            mulMatrix[l][r] = errorFunc;
            smulMatrix[l][r] = errorFunc;
            divMatrix[l][r] = errorFunc;
            sdivMatrix[l][r] = errorFunc;
            modMatrix[l][r] = errorFunc;
            smodMatrix[l][r] = errorFunc;
            lessMatrix[l][r] = errorBoolFunc;
            greaterMatrix[l][r] = errorBoolFunc;
            lessEqMatrix[l][r] = errorBoolFunc;
            greaterEqMatrix[l][r] = errorBoolFunc;
            eqMatrix[l][r] = eqAlwaysFalse;
            neqMatrix[l][r] = eqAlwaysFalse;
            inMatrix[l][r] = errorBoolFunc;
            isMatrix[l][r] = errorBoolFunc;
            mkIndexMatrix[l][r] = errorFunc;
            getIndexMatrix[l][r] = errorFunc;
            setIndexMatrix[l][r] = errorTFunc;
            mkKeyRefMatrix[l][r] = errorFunc;
            getKeyMatrix[l][r] = errorFunc;
            setKeyMatrix[l][r] = errorTFunc;
            getMemberMatrix[l][r] = errorFunc;
            setMemberMatrix[l][r] = errorTFunc;
            mkMemberMatrix[l][r] = errorFunc;
            bitOrMatrix[l][r] = errorFunc;
            bitAndMatrix[l][r] = errorFunc;
        }
        //pushOps[l]=0;
        unrefOps[l] = errorUnFunc;
        refOps[l] = errorUnOp;
        corOps[l] = errorUnOp;
        callOps[l] = errorUnFunc;
        ncallOps[l] = errorUnFunc;
        initForOps[l] = errorForOp;//errorUnFunc;
        stepForOps[l] = errorForOp;//errorUnFunc;
        initFor2Ops[l] = errorFor2Op;
        stepFor2Ops[l] = errorFor2Op;
        boolOps[l] = anyToTrue;
        notOps[l] = anyToFalse;
        incOps[l] = errorUnFunc;
        decOps[l] = errorUnFunc;
        fmtOps[l] = errorFmtOp;
        countOps[l] = errorUnOp;
        negOps[l] = errorUnOp;
        getTypeOps[l] = errorUnOp;
        if(l < vtRefTypeBase)
        {
            copyOps[vtNil] = copySimple;
        } else
        {
            copyOps[l] = errorUnOp;
        }
    }


    for(int l = 0; l < vtCount; l++)
    {
        if(l < vtRefTypeBase)
        {
            assignMatrix[l][vtRef] = assignRefToAnyScalar;
        } else
        {
            assignMatrix[l][vtRef] = assignRefToAnyRefType;
        }
        assignMatrix[l][vtLvalue] = assignLvalueToAny;
        assignMatrix[l][vtMemberRef] = assignMemberRefToAny;
        assignMatrix[l][vtSegment] = assignSegmentToAny;
        assignMatrix[l][vtKeyRef] = assignKeyRefToAny;
        //eqMatrix[l][vtBool]=eqAnyBool;
        addMatrix[l][vtObject] = raddObjAny;
        addMatrix[l][vtRef] = addAnyRef;
        subMatrix[l][vtRef] = subAnyRef;
        mulMatrix[l][vtRef] = mulAnyRef;
        divMatrix[l][vtRef] = divAnyRef;
        modMatrix[l][vtRef] = modAnyRef;
        bitOrMatrix[l][vtRef] = bitOrAnyRef;
        bitAndMatrix[l][vtRef] = bitAndAnyRef;
        lessMatrix[l][vtRef] = lessAnyRef;
        lessEqMatrix[l][vtRef] = lessEqAnyRef;
        greaterMatrix[l][vtRef] = greaterAnyRef;
        greaterEqMatrix[l][vtRef] = greaterEqAnyRef;
        inMatrix[l][vtMap] = inAnyMap;
        inMatrix[l][vtSet] = inAnySet;
        inMatrix[l][vtObject] = inStrObj;
        inMatrix[l][vtRef] = inAnyRef;
        isMatrix[l][vtRef] = isAnyRef;
        saddMatrix[l][vtRef] = saddAnyRef;
        eqMatrix[l][vtRef] = eqAnyRef;
        eqMatrix[l][vtWeakRef] = eqAnyRef;
    }
    for(int r = 0; r < vtCount; r++)
    {
        assignMatrix[vtLvalue][r] = assignToLvalue;
        assignMatrix[vtRef][r] = assignToRef;

        assignMatrix[vtSegment][r] = assignAnyToSegment;
        assignMatrix[vtKeyRef][r] = assignAnyToKeyRef;

        assignMatrix[vtMemberRef][r] = assignAnyToMemberRef;

        //eqMatrix[vtBool][r]=eqBoolAny;
        addMatrix[vtRef][r] = addRefAny;
        subMatrix[vtRef][r] = subRefAny;
        mulMatrix[vtRef][r] = mulRefAny;
        divMatrix[vtRef][r] = divRefAny;
        modMatrix[vtRef][r] = modRefAny;
        bitOrMatrix[vtRef][r] = bitOrRefAny;
        bitAndMatrix[vtRef][r] = bitAndRefAny;

        lessMatrix[vtRef][r] = lessRefAny;
        lessEqMatrix[vtRef][r] = lessEqRefAny;
        lessMatrix[vtObject][r] = lessObjAny;

        greaterMatrix[vtRef][r] = greaterRefAny;
        greaterEqMatrix[vtRef][r] = greaterEqRefAny;

        mkKeyRefMatrix[vtMap][r] = makeMapKeyRef;
        getKeyMatrix[vtMap][r] = getKeyValue;
        setKeyMatrix[vtMap][r] = setKeyValue;
        getKeyMatrix[vtObject][r] = getKeyObjAny;
        setKeyMatrix[vtObject][r] = setKeyObjAny;
        if(r < vtRefTypeBase)
        {
            saddMatrix[vtArray][r] = saddArrScalar;
        } else
        {
            saddMatrix[vtArray][r] = saddArrRefCounted;
        }
        saddMatrix[vtSet][r] = saddSetAny;
        ssubMatrix[vtSet][r] = ssubSetAny;
        saddMatrix[vtKeyRef][r] = saddKeyRefAny;
        ssubMatrix[vtKeyRef][r] = ssubKeyRefAny;
        saddMatrix[vtMemberRef][r] = saddMemberRefAny;
        saddMatrix[vtSegment][r] = saddIndexAny;
        saddMatrix[vtRef][r] = saddRefAny;
        ssubMatrix[vtMemberRef][r] = ssubMemberRefAny;
        if(r != vtRef)
        {
            saddMatrix[vtObject][r] = saddObjAny;
            addMatrix[vtObject][r] = addObjAny;
            ssubMatrix[vtObject][r] = ssubObjAny;
            subMatrix[vtObject][r] = subObjAny;
            addMatrix[vtNativeObject][r] = addNObjAny;
            sdivMatrix[vtNativeObject][r] = sdivNObjAny;
            divMatrix[vtNativeObject][r] = divNObjAny;
            mulMatrix[vtNativeObject][r] = mulNObjAny;
            smulMatrix[vtNativeObject][r] = smulNObjAny;
            divMatrix[vtObject][r] = divObjAny;

            ssubMatrix[vtMap][r] = ssubMapAny;
        }

        inMatrix[vtRef][r] = inRefAny;
        isMatrix[vtRef][r] = isRefAny;
        eqMatrix[vtRef][r] = eqRefAny;
        eqMatrix[vtWeakRef][r] = eqRefAny;

    }

    mkIndexMatrix[vtArray][vtInt] = makeArrayIndex;
    mkIndexMatrix[vtRef][vtInt] = makeArrayIndexRefInt;
    mkIndexMatrix[vtArray][vtRef] = makeArrayIndexArrRef;
    mkIndexMatrix[vtRef][vtRef] = makeArrayIndexRefRef;
    mkIndexMatrix[vtObject][vtInt] = makeObjectIndex;
    mkIndexMatrix[vtNativeObject][vtInt] = makeNObjectIndex;
    mkIndexMatrix[vtSlice][vtInt] = makeSliceIndex;
    getIndexMatrix[vtArray][vtInt] = indexArrayInt;
    getIndexMatrix[vtArray][vtArray] = indexAnyArray;
    getIndexMatrix[vtMap][vtArray] = indexAnyArray;
    getIndexMatrix[vtString][vtArray] = indexAnyArray;
    getIndexMatrix[vtObject][vtArray] = indexAnyArray;
    getIndexMatrix[vtRef][vtInt] = getArrayIndexRefInt;
    getIndexMatrix[vtWeakRef][vtInt] = getArrayIndexRefInt;
    getIndexMatrix[vtArray][vtRef] = getArrayIndexArrRef;
    getIndexMatrix[vtRef][vtRef] = getArrayIndexRefRef;
    getIndexMatrix[vtArray][vtRange] = makeSegment;
    getIndexMatrix[vtString][vtRange] = makeSegment;
    getIndexMatrix[vtSegment][vtInt] = indexSegmentInt;
    getIndexMatrix[vtSlice][vtInt] = indexSliceInt;
    getIndexMatrix[vtString][vtInt] = indexStrInt;
    getIndexMatrix[vtObject][vtInt] = getIndexObjAny;
    setIndexMatrix[vtObject][vtInt] = setIndexObjAny;
    getIndexMatrix[vtNativeObject][vtInt] = getIndexNObjInt;
    setIndexMatrix[vtNativeObject][vtInt] = setIndexNObjInt;

    setIndexMatrix[vtArray][vtInt] = setArrayIndex;
    setIndexMatrix[vtSlice][vtInt] = setSliceIndex;

    getMemberMatrix[vtString][vtString] = getStringProp;
    getMemberMatrix[vtArray][vtString] = getArrayProp;
    getMemberMatrix[vtObject][vtString] = getObjectMember;
    getMemberMatrix[vtObject][vtMethod] = getObjectMethod;
    getMemberMatrix[vtNativeObject][vtString] = getNObjectProp;
    getMemberMatrix[vtClass][vtString] = getClassProp;
    getMemberMatrix[vtCoroutine][vtString] = getCorProp;
    getMemberMatrix[vtRef][vtString] = getRefProp;
    getMemberMatrix[vtWeakRef][vtString] = getRefProp;

    setMemberMatrix[vtRef][vtString] = setRefMember;
    setMemberMatrix[vtObject][vtString] = setObjectMember;
    setMemberMatrix[vtNativeObject][vtString] = setNObjectMember;

    mkMemberMatrix[vtObject][vtString] = makeObjectMemberRef;
    mkMemberMatrix[vtNativeObject][vtString] = makeNObjectMemberRef;
    mkMemberMatrix[vtRef][vtString] = makeObjectMemberRefFromRef;
    mkMemberMatrix[vtWeakRef][vtString] = makeObjectMemberRefFromRef;


    assignMatrix[vtRef][vtRef] = assignRefToRef;

    eqMatrix[vtBool][vtBool] = eqBoolBool;

    eqMatrix[vtInt][vtInt] = eqIntInt;
    eqMatrix[vtInt][vtDouble] = eqIntDouble;
    eqMatrix[vtDouble][vtInt] = eqDoubleInt;
    eqMatrix[vtDouble][vtDouble] = eqDoubleDouble;

    eqMatrix[vtClass][vtClass] = eqClassClass;
    eqMatrix[vtObject][vtObject] = eqObjObj;
    eqMatrix[vtArray][vtArray] = eqArrArr;
    eqMatrix[vtFunc][vtFunc] = eqFunFun;
    eqMatrix[vtCFunc][vtCFunc] = eqFunFun;

    eqMatrix[vtNil][vtNil] = eqAlwaysTrue;
    eqMatrix[vtString][vtString] = eqStrStr;
    eqMatrix[vtString][vtSegment] = eqStrSegment;
    eqMatrix[vtSegment][vtString] = eqSegmentStr;
    eqMatrix[vtSegment][vtSegment] = eqSegmentSegment;
    eqMatrix[vtSet][vtSet] = eqSetSet;
    eqMatrix[vtMap][vtMap] = eqMapMap;
    eqMatrix[vtClosure][vtClosure] = eqCloClo;
    eqMatrix[vtCoroutine][vtCoroutine] = eqCorCor;

    eqMatrix[vtDelegate][vtDelegate] = eqDlgDlg;
    eqMatrix[vtMethod][vtMethod] = eqMethMeth;


    neqMatrix[vtBool][vtBool] = neqBoolBool;
    neqMatrix[vtInt][vtInt] = neqIntInt;
    neqMatrix[vtInt][vtDouble] = neqIntDouble;
    neqMatrix[vtDouble][vtInt] = neqDoubleInt;
    neqMatrix[vtDouble][vtDouble] = neqDoubleDouble;

    neqMatrix[vtClass][vtClass] = neqClassClass;

    neqMatrix[vtNil][vtNil] = eqAlwaysFalse;
    neqMatrix[vtString][vtString] = neqStrStr;
    neqMatrix[vtSet][vtSet] = neqSetSet;
    neqMatrix[vtClosure][vtClosure] = neqCloClo;
    neqMatrix[vtCoroutine][vtCoroutine] = neqCorCor;

    inMatrix[vtRef][vtRef] = inRefRef;
    inMatrix[vtInt][vtRange] = inIntRange;

    lessMatrix[vtInt][vtInt] = lessIntInt;
    lessMatrix[vtDouble][vtInt] = lessDoubleInt;
    lessMatrix[vtInt][vtDouble] = lessIntDouble;
    lessMatrix[vtDouble][vtDouble] = lessDoubleDouble;

    lessMatrix[vtString][vtString] = lessStrStr;

    lessEqMatrix[vtInt][vtInt] = lessEqIntInt;
    lessEqMatrix[vtDouble][vtInt] = lessEqDoubleInt;
    lessEqMatrix[vtInt][vtDouble] = lessEqIntDouble;
    lessEqMatrix[vtDouble][vtDouble] = lessEqDoubleDouble;

    lessEqMatrix[vtString][vtString] = lessEqStrStr;


    greaterMatrix[vtInt][vtInt] = greaterIntInt;
    greaterMatrix[vtDouble][vtInt] = greaterDoubleInt;
    greaterMatrix[vtInt][vtDouble] = greaterIntDouble;
    greaterMatrix[vtDouble][vtDouble] = greaterDoubleDouble;

    greaterMatrix[vtString][vtString] = greaterStrStr;

    greaterEqMatrix[vtInt][vtInt] = greaterEqIntInt;
    greaterEqMatrix[vtDouble][vtInt] = greaterEqDoubleInt;
    greaterEqMatrix[vtInt][vtDouble] = greaterEqIntDouble;
    greaterEqMatrix[vtDouble][vtDouble] = greaterEqDoubleDouble;

    greaterEqMatrix[vtString][vtString] = greaterEqStrStr;


    boolOps[vtBool] = boolToBool;
    boolOps[vtInt] = intToBool;
    boolOps[vtDouble] = doubleToBool;
    boolOps[vtNil] = anyToFalse;
    boolOps[vtRef] = refToBool;
    boolOps[vtObject] = objToBool;

    notOps[vtInt] = intToNotBool;
    notOps[vtDouble] = doubleToNotBool;
    notOps[vtNil] = anyToTrue;
    notOps[vtObject] = anyToFalse;

    addMatrix[vtInt][vtInt] = addIntInt;
    addMatrix[vtInt][vtDouble] = addIntDouble;
    addMatrix[vtDouble][vtInt] = addDoubleInt;
    addMatrix[vtDouble][vtDouble] = addDoubleDouble;

    addMatrix[vtRef][vtRef] = addRefRef;

    addMatrix[vtString][vtString] = addStrStr;
    addMatrix[vtArray][vtArray] = addArrArr;
    addMatrix[vtSegment][vtSegment] = addSegmentSegment;
    addMatrix[vtSegment][vtString] = addSegmentStr;
    addMatrix[vtString][vtSegment] = addStrSegment;

    saddMatrix[vtInt][vtInt] = saddIntInt;
    saddMatrix[vtDouble][vtInt] = saddDoubleInt;
    saddMatrix[vtInt][vtDouble] = saddIntDouble;
    saddMatrix[vtDouble][vtDouble] = saddDoubleDouble;
    saddMatrix[vtRef][vtRef] = saddRefRef;

    saddMatrix[vtArray][vtArray] = saddArrArr;
    saddMatrix[vtMap][vtMap] = saddMapMap;
    saddMatrix[vtSet][vtSet] = saddSetSet;
    saddMatrix[vtString][vtString] = saddStrStr;
    saddMatrix[vtString][vtSegment] = saddStrSegment;

    subMatrix[vtInt][vtInt] = subIntInt;
    subMatrix[vtDouble][vtInt] = subDoubleInt;
    subMatrix[vtInt][vtDouble] = subIntDouble;
    subMatrix[vtDouble][vtDouble] = subDoubleDouble;

    subMatrix[vtRef][vtRef] = subRefRef;

    ssubMatrix[vtInt][vtInt] = ssubIntInt;
    ssubMatrix[vtInt][vtDouble] = ssubIntDouble;
    ssubMatrix[vtDouble][vtInt] = ssubDoubleInt;
    ssubMatrix[vtDouble][vtDouble] = ssubDoubleDouble;


    mulMatrix[vtInt][vtInt] = mulIntInt;
    mulMatrix[vtInt][vtDouble] = mulIntDouble;
    mulMatrix[vtDouble][vtInt] = mulDoubleInt;
    mulMatrix[vtDouble][vtDouble] = mulDoubleDouble;

    mulMatrix[vtRef][vtRef] = mulRefRef;

    mulMatrix[vtString][vtInt] = mulStrInt;

    smulMatrix[vtInt][vtInt] = smulIntInt;
    smulMatrix[vtInt][vtDouble] = smulIntDouble;
    smulMatrix[vtDouble][vtInt] = smulDoubleInt;
    smulMatrix[vtDouble][vtDouble] = smulDoubleDouble;

    divMatrix[vtInt][vtInt] = divIntInt;
    divMatrix[vtInt][vtDouble] = divIntDouble;
    divMatrix[vtDouble][vtInt] = divDoubleInt;
    divMatrix[vtDouble][vtDouble] = divDoubleDouble;

    divMatrix[vtRef][vtRef] = divRefRef;
    divMatrix[vtString][vtString] = divStrStr;
    divMatrix[vtString][vtRegExp] = divStrRegExp;

    modMatrix[vtInt][vtInt] = modIntInt;
    modMatrix[vtInt][vtDouble] = modIntDouble;
    modMatrix[vtDouble][vtInt] = modDoubleInt;
    modMatrix[vtDouble][vtDouble] = modDoubleDouble;

    modMatrix[vtRef][vtRef] = modRefRef;

    smodMatrix[vtInt][vtInt] = smodIntInt;
    smodMatrix[vtInt][vtDouble] = smodIntDouble;
    smodMatrix[vtDouble][vtInt] = smodDoubleInt;
    smodMatrix[vtDouble][vtDouble] = smodDoubleDouble;


    sdivMatrix[vtInt][vtInt] = sdivIntInt;
    sdivMatrix[vtInt][vtDouble] = sdivIntDouble;
    sdivMatrix[vtDouble][vtInt] = sdivDoubleInt;
    sdivMatrix[vtDouble][vtDouble] = sdivDoubleDouble;

    bitOrMatrix[vtInt][vtInt] = bitOrIntInt;
    bitAndMatrix[vtInt][vtInt] = bitAndIntInt;
    bitOrMatrix[vtRef][vtRef] = bitOrRefRef;
    bitAndMatrix[vtRef][vtRef] = bitAndRefRef;

    incOps[vtInt] = incInt;
    incOps[vtDouble] = incDouble;
    incOps[vtRef] = incRef;
    incOps[vtSegment] = incSegment;
    incOps[vtKeyRef] = incKeyRef;
    incOps[vtMemberRef] = incMemberRef;


    decOps[vtInt] = decInt;
    decOps[vtDouble] = decDouble;
    decOps[vtRef] = decRef;
    decOps[vtSegment] = decSegment;
    decOps[vtKeyRef] = decKeyRef;
    decOps[vtMemberRef] = decMemberRef;

    negOps[vtInt] = negInt;
    negOps[vtDouble] = negDouble;
    negOps[vtRef] = negRef;
    negOps[vtArray] = negArray;

    refOps[vtNil] = refSimple;
    refOps[vtInt] = refSimple;
    refOps[vtDouble] = refSimple;
    refOps[vtBool] = refSimple;
    refOps[vtLvalue] = refLvalue;
    refOps[vtSegment] = refSegment;
    refOps[vtKeyRef] = refKeyRef;
    refOps[vtMemberRef] = refMemberRef;
    refOps[vtRef] = refRef;

    corOps[vtFunc] = corFunc;
    corOps[vtDelegate] = corDlg;
    corOps[vtClosure] = corCls;

    unrefOps[vtString] = unrefString;
    unrefOps[vtRef] = unrefRef;
    unrefOps[vtWeakRef] = unrefWRef;
    unrefOps[vtArray] = unrefArray;
    unrefOps[vtSegment] = unrefSegment;
    unrefOps[vtSlice] = unrefSlice;
    unrefOps[vtKeyRef] = unrefKeyRef;
    unrefOps[vtRange] = unrefRange;
    unrefOps[vtMap] = unrefMap;
    unrefOps[vtSet] = unrefSet;
    unrefOps[vtObject] = unrefObj;
    unrefOps[vtNativeObject] = unrefNObj;
    unrefOps[vtDelegate] = unrefDlg;
    unrefOps[vtCDelegate] = unrefDlg;
    unrefOps[vtClosure] = unrefCls;
    unrefOps[vtCoroutine] = unrefCor;
    unrefOps[vtForIterator] = unrefForIter;
    unrefOps[vtMemberRef] = unrefMembRef;
    unrefOps[vtRegExp] = unrefRegExp;

    copyOps[vtMap] = copyMap;
    copyOps[vtSet] = copySet;
    copyOps[vtArray] = copyArray;
    copyOps[vtSegment] = copySegment;
    copyOps[vtSlice] = copySlice;
    copyOps[vtNativeObject] = copyNObject;
    copyOps[vtObject] = copyObject;

    fmtOps[vtInt] = formatInt;
    fmtOps[vtDouble] = formatDouble;
    fmtOps[vtRef] = formatRef;
    fmtOps[vtWeakRef] = formatRef;
    fmtOps[vtString] = formatStr;
    fmtOps[vtArray] = formatArr;
    fmtOps[vtSegment] = formatSeg;
    fmtOps[vtNil] = formatNil;
    fmtOps[vtBool] = formatBool;
    fmtOps[vtObject] = formatObj;

    initForOps[vtRef] = initForRef;
    initForOps[vtRange] = initForRange;
    stepForOps[vtRange] = stepForRange;
    initForOps[vtArray] = initForArray;
    initForOps[vtSegment] = initForSegment;
    stepForOps[vtSegment] = stepForArray;
    initForOps[vtSlice] = initForSlice;
    stepForOps[vtSlice] = stepForSlice;
    initForOps[vtSet] = initForSet;
    stepForOps[vtForIterator] = stepForSet;
    initForOps[vtFunc] = initForFunc;
    stepForOps[vtCoroutine] = stepForFunc;
    initForOps[vtClosure] = initForClosure;
    initForOps[vtDelegate] = initForDelegate;

    initFor2Ops[vtMap] = initFor2Map;
    stepFor2Ops[vtForIterator] = stepFor2Map;
    initFor2Ops[vtArray] = initFor2Array;
    initFor2Ops[vtSegment] = initFor2Segment;
    stepFor2Ops[vtSegment] = stepFor2Array;
    initFor2Ops[vtSlice] = initFor2Slice;
    stepFor2Ops[vtSlice] = stepFor2Slice;

    countOps[vtString] = countString;
    countOps[vtArray] = countArray;
    countOps[vtSlice] = countSlice;
    countOps[vtSegment] = countSegment;
    countOps[vtSet] = countSet;
    countOps[vtMap] = countMap;
    countOps[vtRef] = countRef;

    callOps[vtFunc] = callFunc;
    callOps[vtCFunc] = callCFunc;
    callOps[vtClass] = callClass;
    callOps[vtDelegate] = callDelegate;
    callOps[vtCDelegate] = callCDelegate;
    callOps[vtClosure] = callClosure;
    callOps[vtMethod] = callClassMethod;
    callOps[vtRef] = callRef;
    callOps[vtCoroutine] = callCor;
    callOps[vtObject] = callObj;

    ncallOps[vtFunc] = ncallFunc;
    ncallOps[vtClass] = callClass;
    ncallOps[vtDelegate] = callDelegate;

    mulMatrix[vtFunc][vtArray] = mulCallableArray;
    mulMatrix[vtCFunc][vtArray] = mulCallableArray;
    mulMatrix[vtClass][vtArray] = mulCallableArray;
    mulMatrix[vtDelegate][vtArray] = mulCallableArray;
    mulMatrix[vtCDelegate][vtArray] = mulCallableArray;
    mulMatrix[vtClosure][vtArray] = mulCallableArray;
    mulMatrix[vtMethod][vtArray] = mulCallableArray;

    isMatrix[vtNil][vtClass] = isNilClass;
    isMatrix[vtBool][vtClass] = isBoolClass;
    isMatrix[vtInt][vtClass] = isIntClass;
    isMatrix[vtDouble][vtClass] = isDoubleClass;
    isMatrix[vtString][vtClass] = isStringClass;
    isMatrix[vtArray][vtClass] = isArrayClass;
    isMatrix[vtSet][vtClass] = isSetClass;
    isMatrix[vtMap][vtClass] = isMapClass;
    isMatrix[vtClass][vtClass] = isClassClass;
    isMatrix[vtObject][vtClass] = isObjClass;
    isMatrix[vtNativeObject][vtClass] = isNObjClass;
    isMatrix[vtClosure][vtClass] = isClosureClass;
    isMatrix[vtCoroutine][vtClass] = isCoroutingClass;
    isMatrix[vtDelegate][vtClass] = isDelegateClass;
    isMatrix[vtFunc][vtClass] = isFuncClass;

    isMatrix[vtRef][vtRef] = isRefRef;

    getTypeOps[vtNil] = getTypeNil;
    getTypeOps[vtBool] = getTypeBool;
    getTypeOps[vtInt] = getTypeInt;
    getTypeOps[vtDouble] = getTypeDouble;
    getTypeOps[vtString] = getTypeString;
    getTypeOps[vtArray] = getTypeArray;
    getTypeOps[vtSet] = getTypeSet;
    getTypeOps[vtMap] = getTypeMap;
    getTypeOps[vtClass] = getTypeClass;
    getTypeOps[vtObject] = getTypeObject;
    getTypeOps[vtNativeObject] = getTypeNObject;
    getTypeOps[vtFunc] = getTypeFunc;
    getTypeOps[vtClosure] = getTypeClosure;
    getTypeOps[vtRef] = getTypeRef;

    initStd();

    result = NilValue;

    dummyCallOp = new OpCall(0, OpArg(atLocal, 0), atNul);
    corRetOp = new OpEndCoroutine();
}


ZorroVM::~ZorroVM()
{
    delete corRetOp;
    delete dummyCallOp;
    if(ctx.callStack.size() > 0)
    {
        delete ctx.callStack.stack[0].callerOp;
    }
}

void ZorroVM::setEntry(OpBase* argEntry)
{
    entry = new ZCode(this, argEntry);
}

void ZorroVM::init()
{
    ctx.dataPtrs[atNul] = nullptr;
    ctx.dataPtrs[atGlobal] = symbols.globals;
    //ctx.dataPtrs[atLocal]=ctx.dataStack.stack; //dataStack.resize() will fill this
    ctx.dataPtrs[atMember] = nullptr;
    ctx.dataPtrs[atClosed] = nullptr;

    result = NilValue;
    if(!ctx.callStack.stack)
    {
        ctx.callStack.resize(32);
        CallFrame cf;
        cf.callerOp = new OpCall(0, atNul, atNul);
        cf.retOp = nullptr;
        cf.localBase = 0;
        cf.funcIdx = 0;
        cf.selfCall = false;
        cf.closedCall = false;
        cf.overload = false;
        ctx.callStack.push(cf);
        ctx.coroutine = nullptr;
    }
    if(!ctx.dataStack.stack)
    {
        ctx.dataStack.resize(64);
    }
    ctx.dataStack.pushBulk(symbols.getGlobalTemporals());
}

#ifdef ZVM_STATIC_MATRIX
ZorroVM::UnOpFunc ZorroVM::refOps[vtCount];
ZorroVM::UnOpFunc ZorroVM::corOps[vtCount];
ZorroVM::UnOpFunc ZorroVM::countOps[vtCount];
ZorroVM::UnOpFunc ZorroVM::negOps[vtCount];
ZorroVM::UnOpFunc ZorroVM::getTypeOps[vtCount];
ZorroVM::UnOpFunc ZorroVM::copyOps[vtCount];
ZorroVM::UnBoolFunc ZorroVM::boolOps[vtCount];
ZorroVM::UnBoolFunc ZorroVM::notOps[vtCount];
ZorroVM::UnFunc ZorroVM::incOps[vtCount];
ZorroVM::UnFunc ZorroVM::decOps[vtCount];
ZorroVM::UnFunc ZorroVM::unrefOps[vtCount];
ZorroVM::UnFunc ZorroVM::callOps[vtCount];
ZorroVM::UnFunc ZorroVM::ncallOps[vtCount];
ZorroVM::BinOpFunc ZorroVM::assignMatrix[vtCount][vtCount];
ZorroVM::BinOpFunc ZorroVM::addMatrix[vtCount][vtCount];
ZorroVM::BinOpFunc ZorroVM::saddMatrix[vtCount][vtCount];
ZorroVM::BinOpFunc ZorroVM::subMatrix[vtCount][vtCount];
ZorroVM::BinOpFunc ZorroVM::ssubMatrix[vtCount][vtCount];
ZorroVM::BinOpFunc ZorroVM::mulMatrix[vtCount][vtCount];
ZorroVM::BinOpFunc ZorroVM::smulMatrix[vtCount][vtCount];
ZorroVM::BinOpFunc ZorroVM::divMatrix[vtCount][vtCount];
ZorroVM::BinOpFunc ZorroVM::sdivMatrix[vtCount][vtCount];
ZorroVM::BinOpFunc ZorroVM::modMatrix[vtCount][vtCount];
ZorroVM::BinOpFunc ZorroVM::smodMatrix[vtCount][vtCount];
ZorroVM::BinOpFunc ZorroVM::bitOrMatrix[vtCount][vtCount];
ZorroVM::BinOpFunc ZorroVM::bitAndMatrix[vtCount][vtCount];
ZorroVM::BinBoolFunc ZorroVM::lessMatrix[vtCount][vtCount];
ZorroVM::BinBoolFunc ZorroVM::greaterMatrix[vtCount][vtCount];
ZorroVM::BinBoolFunc ZorroVM::lessEqMatrix[vtCount][vtCount];
ZorroVM::BinBoolFunc ZorroVM::greaterEqMatrix[vtCount][vtCount];
ZorroVM::BinBoolFunc ZorroVM::eqMatrix[vtCount][vtCount];
ZorroVM::BinBoolFunc ZorroVM::neqMatrix[vtCount][vtCount];
ZorroVM::BinBoolFunc ZorroVM::inMatrix[vtCount][vtCount];
ZorroVM::BinBoolFunc ZorroVM::isMatrix[vtCount][vtCount];
ZorroVM::BinOpFunc ZorroVM::mkIndexMatrix[vtCount][vtCount];
ZorroVM::BinOpFunc ZorroVM::getIndexMatrix[vtCount][vtCount];
ZorroVM::TerOpFunc ZorroVM::setIndexMatrix[vtCount][vtCount];
ZorroVM::BinOpFunc ZorroVM::mkKeyRefMatrix[vtCount][vtCount];
ZorroVM::BinOpFunc ZorroVM::getKeyMatrix[vtCount][vtCount];
ZorroVM::TerOpFunc ZorroVM::setKeyMatrix[vtCount][vtCount];
ZorroVM::BinOpFunc ZorroVM::getMemberMatrix[vtCount][vtCount];
ZorroVM::TerOpFunc ZorroVM::setMemberMatrix[vtCount][vtCount];
ZorroVM::BinOpFunc ZorroVM::mkMemberMatrix[vtCount][vtCount];

ZorroVM::ForOpFunc ZorroVM::initForOps[vtCount];
ZorroVM::ForOpFunc ZorroVM::stepForOps[vtCount];
ZorroVM::For2OpFunc ZorroVM::initFor2Ops[vtCount];
ZorroVM::For2OpFunc ZorroVM::stepFor2Ops[vtCount];

ZorroVM::FmtOpFunc ZorroVM::fmtOps[vtCount];
#endif

}
