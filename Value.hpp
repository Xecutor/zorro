#pragma once

#include <stdint.h>
#include <stdio.h>
#include <string>
#include "RefBase.hpp"

namespace kst {
class RegExp;

struct SMatch;
struct NamedMatch;
}

namespace zorro {

enum ValueType : uint8_t {
    vtNil,         //0
    vtBool,        //1
    vtInt,         //2
    vtDouble,      //3
    vtCFunc,       //4
    vtCMethod,     //5
    vtLvalue,      //6
    vtClass,       //7
    vtFunc,        //8
    vtMethod,      //9
    vtRefTypeBase, //10
    vtString = vtRefTypeBase,//10
    vtArray,       //11
    vtMap,         //12
    vtSet,         //13
    vtRegExp,      //14
    vtRef,         //15
    vtWeakRef,     //16
    vtSegment,     //17
    vtSlice,       //18
    vtKeyRef,      //19
    vtMemberRef,   //20
    vtRange,       //21
    vtForIterator, //22
    vtObject,      //23
    vtNativeObject,//24
    vtDelegate,    //25
    vtCDelegate,   //26
    vtClosure,     //27
    vtCoroutine,   //28
    vtUser,        //29
    vtCount
};

#define ZISREFTYPE(val) ((val)->vt>=vtRefTypeBase)

const unsigned char ValFlagNone = 0u;
const unsigned char ValFlagConst = 1u;
const unsigned char ValFlagLvalue = 2u;
const unsigned char ValFlagARef = 4u;
const unsigned char ValFlagInited = 8u;

struct ValueRef;
struct WeakRef;
class ZString;
class ZStringRef;
struct ZArray;
class ZMap;
class ZSet;
struct Segment;
struct Slice;
class ZorroVM;
struct KeyRef;
struct MemberRef;
struct Range;
struct FuncInfo;
struct MethodInfo;
struct ClassInfo;
struct Object;
struct NativeObject;
struct Delegate;
struct Closure;
struct Coroutine;
struct ForIterator;
struct RegExpVal;

typedef void (* ZorroCFunc)(ZorroVM*);

struct Value;

typedef void (* ZorroCMethod)(ZorroVM*, Value* self);

struct Value {
    unsigned char vt;
    unsigned char flags;
    unsigned char atLvalue;
    union {
        ValueRef* valueRef;
        WeakRef* weakRef;
        RefBase* refBase;
        ZString* str;
        ZArray* arr;
        ZMap* map;
        ZSet* set;
        Segment* seg;
        Slice* slice;
        KeyRef* keyRef;
        MemberRef* membRef;
        Range* range;
        FuncInfo* func;
        MethodInfo* method;
        FuncInfo* cfunc;
        MethodInfo* cmethod;
        ClassInfo* classInfo;
        Object* obj;
        NativeObject* nobj;
        Delegate* dlg;
        Closure* cls;
        Coroutine* cor;
        ForIterator* iter;
        RegExpVal* regexp;
        uint32_t idx;
        int64_t iValue;
        double dValue;
        bool bValue;
    };
};

struct ValueRef : RefBase {
    Value value;
};

struct WeakRef : ValueRef {
    WeakRef* wrPrev;
    WeakRef* wrNext;
    Value cont;
};

struct Segment : RefBase {
    Value cont;
    Value getValue;
    int64_t segBase;//for proper segment enumeration with index
    int64_t segStart;
    int64_t segEnd;
    int step;
};

struct Slice : RefBase {
    Value cont;
    Value indeces;
    size_t index; //for iteration
};

struct KeyRef : RefBase {
    Value obj;
    Value name;
};

struct MemberRef : RefBase {
    Value obj;
    ZString* name;
    Value getValue;
    size_t memberIndex;
    bool simpleMember;
};

struct Range : RefBase {
    int64_t start, end, step;
};

struct ForIterator : RefBase {
    Value cont;
    void* ptr;
    ForIterator* next;
};


struct Object : RefBase {
    ClassInfo* classInfo;
    Value* members;
};

struct NativeObject : RefBase {
    ClassInfo* classInfo;
    void* userData;

    template<class T>
    T& as()
    {
        return *reinterpret_cast<T*>(userData);
    }
};

struct Delegate : RefBase {
    Value obj;
    MethodInfo* method;
};


struct Closure : RefBase {
    Value* closedValues;
    FuncInfo* func;
    Value self;
    size_t closedCount;
};

struct RegExpVal : RefBase {
    kst::RegExp* val;
    kst::SMatch* marr;
    size_t marrSize;
    kst::NamedMatch* narr;
    size_t narrSize;
};

static const Value NilValue = {0, 0, 0, {nullptr}};

inline Value IntValue(int64_t value, bool isConst = false)
{
    Value rv;
    rv.vt = vtInt;
    rv.iValue = value;
    rv.flags = isConst ? ValFlagConst : ValFlagNone;
    return rv;
}

inline Value DoubleValue(double value, bool isConst = false)
{
    Value rv;
    rv.vt = vtDouble;
    rv.dValue = value;
    rv.flags = isConst ? ValFlagConst : ValFlagNone;
    return rv;
}

inline Value BoolValue(bool val)
{
    Value rv;
    rv.vt = vtBool;
    rv.bValue = val;
    rv.flags = ValFlagNone;
    return rv;
}


Value StringValue(ZStringRef& str, bool isConst = false);

Value StringValue(ZString* str, bool isConst = false);

Value ArrayValue(ZArray* arr, bool isConst = false);

Value RangeValue(Range* rng, bool isConst = false);

Value MapValue(ZMap* map, bool isConst = false);

Value SetValue(ZSet* set, bool isConst);

Value KeyRefValue(KeyRef* prop, bool isConst = false);

Value NObjValue(ZorroVM* vm, ClassInfo* cls, void* ptr);


inline Value LvalueRefValue(uint32_t idx, unsigned char atLvalue)
{
    Value rv;
    rv.vt = vtLvalue;
    rv.flags = ValFlagARef | ValFlagLvalue;
    rv.atLvalue = atLvalue;
    rv.idx = idx;
    return rv;
}

inline const char* getValueTypeName(int vt)
{
    switch((ValueType) vt)
    {
        case vtNil:
            return "nil";
        case vtBool:
            return "bool";
        case vtInt:
            return "int";
        case vtDouble:
            return "double";
        case vtString:
            return "string";
        case vtArray:
            return "array";
        case vtMap:
            return "map";
        case vtSet:
            return "set";
        case vtRegExp:
            return "regexp";
        case vtRef:
            return "ref";
        case vtWeakRef:
            return "weakref";
        case vtLvalue:
            return "lvalue";
        case vtCFunc:
            return "cfunc";
        case vtCMethod:
            return "cmethod";
        case vtMethod:
            return "method";
        case vtFunc:
            return "func";
        case vtSegment:
            return "segment";
        case vtSlice:
            return "slice";
        case vtKeyRef:
            return "key";
        case vtMemberRef:
            return "member";
        case vtRange:
            return "range";
        case vtForIterator:
            return "for iterator";
        case vtClass:
            return "class";
        case vtObject:
            return "object";
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
        case vtUser:
            return "user";
        case vtCount:
            break;
    }
    return "invalid";
}

}
