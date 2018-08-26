#ifndef __ZORRO_ZVMOPSDEPS_HPP__
#define __ZORRO_ZVMOPSDEPS_HPP__

#include <kst/Format.hpp>
#include "FileReader.hpp"
#include "RefBase.hpp"
#include <vector>
#include "Debug.hpp"

namespace zorro {

class ZorroVM;

struct OpBase;

typedef void (* OpFunc)(ZorroVM*, OpBase*);

enum OpType {
    otPush,
    otAssign,
    otAdd,
    otSAdd,
    otSub,
    otSSub,
    otMul,
    otSMul,
    otDiv,
    otSDiv,
    otMod,
    otSMod,
    otMatch,
    otPreInc,
    otPostInc,
    otPreDec,
    otPostDec,
    otLess,
    otGreater,
    otEqual,
    otLessEq,
    otGreaterEq,
    otNotEqual,
    otNot,
    otNeg,
    otCopy,
    otGetType,
    otIn,
    otIs,
    otBitOr,
    otBitAnd,
    otMakeRef,
    otMakeWeakRef,
    otMakeCor,
    otGetIndex,
    otMakeIndex,
    otMakeArray,
    otInitArrayItem,
    otSetArrayItem,
    otGetKey,
    otSetKey,
    otMakeKey,
    otMakeMap,
    otInitMapItem,
    otMakeSet,
    otInitSetItem,
    otGetProp,
    otSetProp,
    otGetPropOpt,
    otMakeMemberRef,
    otMakeRange,
    otForInit,
    otForStep,
    otForInit2,
    otForStep2,
    otForCheckCoroutine,
    otJump,
    otJumpIfInited,
    otJumpIfLess,
    otJumpIfGreater,
    otJumpIfLessEq,
    otJumpIfGreaterEq,
    otJumpIfEqual,
    otJumpIfNotEqual,
    otJumpIfNot,
    otJumpIfIn,
    otJumpIfIs,
    otCondJump,
    otJumpFallback,
    otReturn,
    otCall,
    otNamedArgsCall,
    otCallMethod,
    otNamedArgsCallMethod,
    otInitDtor,
    otFinalDestroy,
    otEnterTry,
    otLeaveCatch,
    otThrow,
    otMakeClosure,
    otMakeCoroutine,
    otYield,
    otEndCoroutine,
    otMakeConst,
    otRangeSwitch,
    otHashSwitch,
    otFormat,
    otCombine,
    otCount,
    otGetAttr
};

enum OpArgType : uint8_t {
    atNul,
    atGlobal,
    atLocal,
    atMember,
    atClosed,
    atStack
};


const char* getArgTypeName(int at);

/*
  for source arg type - if atStack, it's the same as atLocal, but requires clean afterwards.
  for dst arg type - if atStack - push to stack, it's func arg.
 */

typedef uint32_t index_type;

struct OpArg {
    OpArgType at;
    bool isTemporal;
    index_type idx;

    //OpArg():at(atNul),idx(0){}
    OpArg(OpArgType argAt = atNul, index_type argIdx = 0, bool argIsTemporal = false) :
        at(argAt), isTemporal(argIsTemporal), idx(argIdx)
    {
    }

    OpArg& tmp()
    {
        isTemporal = true;
        return *this;
    }

    bool operator==(const OpArg& rhs) const
    {
        return at == rhs.at && idx == rhs.idx;
    }

    inline std::string toStr() const
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "%s%s[%d]", getArgTypeName(at), isTemporal ? "T" : "", idx);
        return buf;
    }

    static void inttostr(int val, std::string& out)
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "%d", val);
        out += buf;
    }
};

inline void customformat(kst::FormatBuffer& buf, const OpArg& oa, int, int)
{
    const std::string& tmp = oa.toStr();
    buf.Append(tmp.c_str(), tmp.length());
}


const char* getOpName(int ot);

const char* getArgTypeName(int at);

struct OpBase;
typedef std::vector<OpBase*> OpsVector;
typedef std::vector<OpArg*> ArgsVector;

struct OpBase {
    OpFunc op;
    OpBase* next;
    FileLocation pos;
    int ot;
    int seq;

    OpBase() : next(0), seq(0)
    {
    }

    virtual ~OpBase()
    {
    }

    virtual void dump(std::string& out) = 0;

    virtual void getArgs(ArgsVector&)
    {
    }

    virtual void getBranches(OpsVector&)
    {
    }
};


struct ZCode : RefBase {
    ZorroVM* vm;
    OpBase* code;

    ZCode(ZorroVM* argVm = 0, OpBase* argCode = 0) : vm(argVm), code(argCode)
    {
    }

    ~ZCode();

    void getAll(OpsVector& allOps);
};

class ZCodeRef {
protected:
    mutable ZCode* code;
public:
    ZCodeRef(ZCode* argCode = 0) : code(argCode)
    {
        if(code)
        {
            code->ref();
        }
    }

    ZCodeRef(const ZCodeRef& argOther) : code(argOther.code)
    {
        if(code)
        {
            code->ref();
        }
    }

    ~ZCodeRef()
    {
        unref();
    }

    OpBase* release() const
    {
        OpBase* rv = code->code;
        code->code = 0;
        return rv;
    }

    void unref() const
    {
        if(code && code->unref())
        {
            delete code;
        }
        code = 0;
    }

    ZCodeRef& operator=(const ZCodeRef& argOther)
    {
        if(this == &argOther)
        {
            return *this;
        }
        unref();
        code = argOther.code;
        if(code)
        {
            code->ref();
        }
        return *this;
    }

    OpBase* operator->()
    {
        return code->code;
    }

    OpBase*& operator*()
    {
        return code->code;
    }

    OpBase* operator->() const
    {
        return code->code;
    }

    OpBase* operator*() const
    {
        return code->code;
    }

    ZCode* get()
    {
        return code;
    }

    const ZCode* get() const
    {
        return code;
    }
};


}

#endif
