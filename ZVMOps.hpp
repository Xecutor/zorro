#ifndef __ZORRO_ZVMOPS_HPP__
#define __ZORRO_ZVMOPS_HPP__

#include "ZVMOpsDefs.hpp"

namespace zorro {

struct OpPush : OpBase {
    OpArg src;

    OpPush(const OpArg& argSrc);

    virtual ~OpPush()
    {
    }

    void getArgs(ArgsVector& args)
    {
        args.push_back(&src);
    }

    void dump(std::string& out)
    {
        out = "push ";
        out += src.toStr();
    }
};

struct OpDstBase : OpBase {
    OpArg dst;

    OpDstBase(const OpArg& argDst) : dst(argDst)
    {
    }
};

struct OpBinOp : OpDstBase {
    OpArg left, right;

    OpBinOp(const OpArg& argLeft, const OpArg& argRight, const OpArg& argDst) : OpDstBase(argDst),
        left(argLeft), right(argRight)
    {
    }

    void getArgs(ArgsVector& args)
    {
        if(ot != otAssign)
        {
            args.push_back(&left);
        }
        args.push_back(&right);

    }

    virtual ~OpBinOp()
    {
    }

    void dump(std::string& out)
    {
        out = left.toStr();
        out += " ";
        out += getOpName(ot);
        out += " ";
        out += right.toStr();
        out += " -> ";
        out += dst.toStr();
    }
};

struct OpTerOp : OpBinOp {
    OpArg arg;

    OpTerOp(const OpArg& argLeft, const OpArg& argArg, const OpArg& argRight, const OpArg& argDst) :
        OpBinOp(argLeft, argRight, argDst),
        arg(argArg)
    {
    }

    virtual ~OpTerOp()
    {
    }

    void getArgs(ArgsVector& args)
    {
        OpBinOp::getArgs(args);
        args.push_back(&arg);
    }

    void dump(std::string& out)
    {
        out = left.toStr();
        out += " ";
        out += getOpName(ot);
        out += " ";
        out += "[";
        out += arg.toStr();
        out += "] ";
        out += right.toStr();
        out += " -> ";
        out += dst.toStr();
    }
};

#define DEFBINOP(name) struct name:OpBinOp{ \
  name(const OpArg& argLeft,const OpArg& argRight,const OpArg& argDst); \
  virtual ~name(){}\
}

#define DEFTEROP(name) struct name:OpTerOp{ \
  name(const OpArg& argLeft,const OpArg& argArg,const OpArg& argRight,const OpArg& argDst); \
  virtual ~name(){}\
}


DEFBINOP(OpAssign);

DEFBINOP(OpAdd);

DEFBINOP(OpSAdd);

DEFBINOP(OpSub);

DEFBINOP(OpSSub);

DEFBINOP(OpMul);

DEFBINOP(OpSMul);

DEFBINOP(OpDiv);

DEFBINOP(OpSDiv);

DEFBINOP(OpMod);

DEFBINOP(OpSMod);
//DEFBINOP(OpLess);
//DEFBINOP(OpEqual);
DEFBINOP(OpGetIndex);

DEFTEROP(OpSetArrayItem);

DEFBINOP(OpMakeIndex);

DEFBINOP(OpGetKey);

DEFTEROP(OpSetKey);

DEFBINOP(OpMakeKey);

DEFBINOP(OpGetProp);

DEFTEROP(OpSetProp);

DEFBINOP(OpBitOr);

DEFBINOP(OpBitAnd);
//DEFBINOP(OpIn);

struct OpGetPropOpt : OpBinOp {
    OpGetPropOpt(const OpArg& argLeft, const OpArg& argRight, const OpArg& argDst);
};

struct OpMatch : OpBinOp {
    OpArg* vars;

    OpMatch(const OpArg& argLeft, const OpArg& argRight, const OpArg& argDst);

    ~OpMatch()
    {
        if(vars)
        {
            delete[]vars;
        }
    }
};

struct OpUnOp : OpDstBase {
    OpArg src;

    OpUnOp(const OpArg& argSrc, const OpArg& argDst) : OpDstBase(argDst), src(argSrc)
    {
    }

    virtual ~OpUnOp()
    {
    }

    void getArgs(ArgsVector& args)
    {
        args.push_back(&src);
    }

    void dump(std::string& out)
    {
        out = getOpName(ot);
        out += " ";
        out += src.toStr();
        out += " -> ";
        out += dst.toStr();
    }
};

#define DEFUNOP(name) struct name:OpUnOp{ \
    name(const OpArg& argSrc,const OpArg& argDst); \
    virtual ~name(){}\
}

DEFUNOP(OpMakeRef);

DEFUNOP(OpMakeWeakRef);

DEFUNOP(OpMakeCor);

DEFUNOP(OpPreInc);

DEFUNOP(OpPostInc);

DEFUNOP(OpPreDec);

DEFUNOP(OpPostDec);

DEFUNOP(OpMakeArray);

DEFUNOP(OpMakeMap);

DEFUNOP(OpMakeSet);

DEFUNOP(OpCount);

DEFUNOP(OpNeg);

DEFUNOP(OpNot);

DEFUNOP(OpGetType);

DEFUNOP(OpCopy);

struct OpMakeMemberRef : OpDstBase {
    OpArg obj, prop;

    OpMakeMemberRef(const OpArg& argObj, const OpArg& argProp, const OpArg& argDst);

    void getArgs(ArgsVector& args)
    {
        args.push_back(&obj);
        args.push_back(&prop);
    }

    void dump(std::string& out)
    {
        out = "makeMemberRef:";
        out += obj.toStr();
        out += ".";
        out += prop.toStr();
        out += " -> ";
        out += dst.toStr();
    }

};

struct OpInitArrayItem : OpBase {
    OpArg arr, item;
    size_t index;

    OpInitArrayItem(const OpArg& argArr, const OpArg& argItem, size_t argIndex);

    virtual ~OpInitArrayItem()
    {
    }

    void getArgs(ArgsVector& args)
    {
        args.push_back(&arr);
        args.push_back(&item);
    }

    void dump(std::string& out)
    {
        out = "initArrayItem:";
        out += item.toStr();
        out += " -> ";
        out += arr.toStr();
        char buf[32];
        snprintf(buf, sizeof(buf), "[%u]", static_cast<unsigned int>(index));
        out += buf;
    }
};


struct OpInitMapItem : OpBase {
    OpArg map, key, item;

    OpInitMapItem(const OpArg& argMpa, const OpArg& argKey, const OpArg& argItem);

    virtual ~OpInitMapItem()
    {
    }

    void getArgs(ArgsVector& args)
    {
        args.push_back(&map);
        args.push_back(&key);
        args.push_back(&item);
    }

    void dump(std::string& out)
    {
        out = "initMapItem:";
        out += key.toStr();
        out += "=>";
        out += item.toStr();
        out += " -> ";
        out += map.toStr();
    }
};

struct OpInitSetItem : OpBase {
    OpArg set, item;

    OpInitSetItem(const OpArg& argSet, const OpArg& argItem);

    virtual ~OpInitSetItem()
    {
    }

    void getArgs(ArgsVector& args)
    {
        args.push_back(&set);
        args.push_back(&item);

    }

    void dump(std::string& out)
    {
        out = "initSetItem:";
        out += item.toStr();
        out += " -> ";
        out += set.toStr();
    }
};


struct OpMakeRange : OpBase {
    OpArg start, end, step, dst;
    bool inclusive;

    OpMakeRange(const OpArg& argStart, const OpArg& argEnd, const OpArg& argStep, const OpArg& argDst,
                bool argInclusive);

    virtual ~OpMakeRange()
    {
    }

    void getArgs(ArgsVector& args)
    {
        args.push_back(&start);
        args.push_back(&end);
        args.push_back(&step);
    }

    void dump(std::string& out)
    {
        out = "makerange:";
        out += start.toStr();
        out += " .. ";
        out += end.toStr();
        out += " : ";
        out += step.toStr();
        out += " -> ";
        out += dst.toStr();
    }
};

struct OpJump : OpBase {
    size_t localSize;

    OpJump(OpBase* argNext, size_t argLocalSize);

    virtual ~OpJump()
    {
    }

    void dump(std::string& out)
    {
        char buf[256];
        snprintf(buf, sizeof(buf), "jump(%u):%s",
                 static_cast<unsigned int>(localSize), next ? next->pos.backTrace().c_str() : "null");
        out = buf;
    }
};


struct OpJumpBase;

struct OpJumpOverloadFallback : OpDstBase {
    OpJumpOverloadFallback(OpJumpBase* argOwner);

    OpJumpBase* owner;

    void dump(std::string& out);
};

struct OpJumpBase : OpBase {
    OpBase* elseOp;
    OpJumpOverloadFallback fallback;

    OpJumpBase(OpBase* argElseOp) : elseOp(argElseOp), fallback(nullptr)
    {
        fallback.owner = this;
    }

    virtual void getBranches(OpsVector& branches)
    {
        branches.push_back(elseOp);
    }

    virtual ~OpJumpBase()
    {
    }
};

struct OpCondJump : OpJumpBase {
    OpArg src;

    OpCondJump(OpArg argSrc, OpBase* argElseOp = 0);

    virtual ~OpCondJump()
    {
    }

    void getArgs(ArgsVector& args)
    {
        args.push_back(&src);
    }

    void dump(std::string& out)
    {
        out = "ifjump(";
        out += src.toStr();
        out += "):";
        if(next)
        {
            out += next->pos.backTrace();
        }
        if(elseOp)
        {
            out += " else ";
            out += elseOp->pos.backTrace();
        }
    }
};

struct OpJumpIfInited : OpCondJump {
    OpJumpIfInited(OpArg argSrc, OpBase* argInitedOp);

    virtual ~OpJumpIfInited()
    {
    }

    void dump(std::string& out)
    {
        out = "jump if inited(";
        out += src.toStr();
        out += "):";
        if(next)
        {
            out += next->pos.backTrace();
        }
        if(elseOp)
        {
            out += " else ";
            out += elseOp->pos.backTrace();
        }
    }
};


struct OpJumpIfBinOp : OpJumpBase {
    OpArg left, right;

    OpJumpIfBinOp(const OpArg& argLeft, const OpArg& argRight, OpBase* argElseOp = 0) :
        OpJumpBase(argElseOp), left(argLeft), right(argRight)
    {
    }

    void getArgs(ArgsVector& args)
    {
        args.push_back(&left);
        args.push_back(&right);
    }

    virtual ~OpJumpIfBinOp()
    {
    }
};

#define DEFBINOPJUMP(op) struct OpJumpIf##op:OpJumpIfBinOp{\
    OpJumpIf##op(const OpArg& argLeft,const OpArg& argRight,OpBase* argElseOp=0);\
    virtual ~OpJumpIf##op(){} \
    void dump(std::string& out)\
    {\
      out="jumpif:";\
      out+=left.toStr();\
      out+=" " #op " ";\
      out+=right.toStr();\
      out+=" then ";\
      if(next)out+=next->pos.backTrace();\
      if(elseOp)\
      {\
        out+=" else ";\
        out+=elseOp->pos.backTrace();\
      }\
    }\
}

DEFBINOPJUMP(Less);

DEFBINOPJUMP(LessEq);

DEFBINOPJUMP(Greater);

DEFBINOPJUMP(GreaterEq);

DEFBINOPJUMP(Equal);

DEFBINOPJUMP(NotEqual);

DEFBINOPJUMP(Not);

DEFBINOPJUMP(In);

DEFBINOPJUMP(Is);

struct OpCallBase : OpDstBase {
    OpArg func;
    index_type args;

    void getArgs(ArgsVector& argsv)
    {
        argsv.push_back(&func);
    }

    OpCallBase(index_type argArgs, const OpArg& argFunc, const OpArg& argResult) :
        OpDstBase(argResult), func(argFunc), args(argArgs)
    {
    }
};

struct OpCall : OpCallBase {
    OpCall(index_type argArgs, const OpArg& argFunc, const OpArg& argResult);

    virtual ~OpCall()
    {
    }

    void dump(std::string& out)
    {
        char buf[256];
        snprintf(buf, sizeof(buf), "call %s(%u) -> %s", func.toStr().c_str(), args, dst.toStr().c_str());
        out = buf;
    }
};

struct OpNamedArgsCall : OpCallBase {
    OpNamedArgsCall(index_type argArgs, const OpArg& argFunc, const OpArg& argResult);

    virtual ~OpNamedArgsCall()
    {
    }

    void dump(std::string& out)
    {
        char buf[256];
        snprintf(buf, sizeof(buf), "ncall %s(%u) -> %s", func.toStr().c_str(), args, dst.toStr().c_str());
        out = buf;
    }
};


struct OpCallMethod : OpDstBase {
    OpArg self;
    index_type methodIdx;
    index_type args;
    bool namedArgs;

    OpCallMethod(index_type argArgs, const OpArg& argSelf, index_type argMethodIdx, const OpArg& argResult,
                 bool argNamedArgs = false);

    virtual ~OpCallMethod()
    {
    }

    void dump(std::string& out)
    {
        char buf[256];
        snprintf(buf, sizeof(buf), "call %s[%u](%u) -> %s", self.toStr().c_str(), methodIdx, args, dst.toStr().c_str());
        out = buf;
    }
};

struct OpNamedArgsCallMethod : OpCallMethod {
    OpNamedArgsCallMethod(index_type argArgs, const OpArg& argSelf, index_type argMethodIdx, const OpArg& argResult);

    ~OpNamedArgsCallMethod()
    {
    }
};


struct OpReturn : OpBase {
    OpArg result;
    bool refReturn;

    OpReturn(const OpArg& argResult, bool argRefReturn = false);

    OpReturn();

    virtual ~OpReturn()
    {
    }

    void dump(std::string& out)
    {
        out = "return " + result.toStr();
    }
};

struct OpForInit : OpDstBase {
    OpArg target, temp;
    OpBase* endOp;
    OpBase* corOp;

    OpForInit(const OpArg& argVar, const OpArg& argTarget, const OpArg& argTemp);

    virtual ~OpForInit()
    {
    }

    void getArgs(ArgsVector& args)
    {
        args.push_back(&target);
    }

    virtual void getBranches(OpsVector& branches)
    {
        branches.push_back(endOp);
        branches.push_back(corOp);
    }


    void dump(std::string& out)
    {
        out = "init for ";
        out += dst.toStr();
        out += " in ";
        out += target.toStr();
    }
};

struct OpForInit2 : OpForInit {
    OpArg var2;

    OpForInit2(const OpArg& argVar1, const OpArg& argVar2, const OpArg& argTarget, const OpArg& argTemp);

    virtual ~OpForInit2()
    {
    }

    void dump(std::string& out)
    {
        out = "init for ";
        out += dst.toStr();
        out += ",";
        out += var2.toStr();
        out += " in ";
        out += target.toStr();
    }
};

struct OpForCheckCoroutine : OpBase {
    OpArg temp;
    OpBase* endOp;

    OpForCheckCoroutine(OpArg argTemp);

    virtual void getBranches(OpsVector& branches)
    {
        branches.push_back(endOp);
    }

    void dump(std::string& out)
    {
        out = "check for coroutine";
    }
};

struct OpForStep : OpBase {
    OpArg var, temp;
    OpBase* endOp;
    OpBase* corOp;

    OpForStep(const OpArg& argVar, const OpArg& argTemp);

    virtual ~OpForStep()
    {
    }

    virtual void getBranches(OpsVector& branches)
    {
        branches.push_back(endOp);
        branches.push_back(corOp);
    }

    void dump(std::string& out)
    {
        out = "for ";
        out += var.toStr();
        out += " in ";
        out += temp.toStr();
    }
};

struct OpForStep2 : OpForStep {
    OpArg var2;

    OpForStep2(const OpArg& argVar, const OpArg& argVar2, const OpArg& argTemp);

    virtual ~OpForStep2()
    {
    }

    void dump(std::string& out)
    {
        out = "for ";
        out += var.toStr();
        out += ",";
        out += var2.toStr();
        out += " in ";
        out += temp.toStr();
    }
};

struct OpInitDtor : OpBase {
    size_t locals;

    OpInitDtor(size_t argLocals);

    void dump(std::string& out)
    {
        out = "initDtor";
    }
};

struct OpFinalDestroy : OpBase {
    OpFinalDestroy();

    void dump(std::string& out)
    {
        out = "finalDestroy";
    }
};

struct ClassInfo;

struct OpEnterTry : OpBase {
    ClassInfo** exList;
    index_type exCount;
    index_type idx;
    OpBase* catchOp;

    OpEnterTry(ClassInfo** argExList, index_type argExCount, index_type argIdx, OpBase* argCatchOp);

    ~OpEnterTry();

    virtual void getBranches(OpsVector& branches)
    {
        branches.push_back(catchOp);
    }

    void dump(std::string& out)
    {
        out = "enter catch";
    }
};

struct OpLeaveCatch : OpBase {
    OpLeaveCatch();

    void dump(std::string& out)
    {
        out = "leave catch";
    }
};

struct OpThrow : OpBase {
    OpArg obj;

    OpThrow(OpArg argObj);

    void getArgs(ArgsVector& args)
    {
        args.push_back(&obj);
    }

    void dump(std::string& out)
    {
        out = "throw ";
        out += obj.toStr();
    }
};

struct OpMakeClosure : OpDstBase {
    OpArg src;
    OpArg self;
    OpArg* closedVars;
    index_type closedCount;

    OpMakeClosure(OpArg argSrc, OpArg argDst, OpArg argSelf, index_type argClosedCount, OpArg* argClosedVars);

    void getArgs(ArgsVector& args)
    {
        args.push_back(&src);
    }

    void dump(std::string& out)
    {
        out = FORMAT("makeClosure %{} -> %{}", src, dst);
    }
};

struct OpMakeCoroutine : OpDstBase {
    OpArg src;

    OpMakeCoroutine(OpArg argSrc, OpArg argDst);

    void getArgs(ArgsVector& args)
    {
        args.push_back(&src);
    }

    void dump(std::string& out)
    {
        out = FORMAT("makeCoroutine %{} -> %{}", src, dst);
    }
};

struct OpYield : OpBase {
    OpArg result;
    bool refYield;

    OpYield(OpArg argResultIdx, bool argRefYield);

    OpYield();

    void dump(std::string& out)
    {
        out = FORMAT("yield %{}", result.toStr());
    }
};

struct OpEndCoroutine : OpBase {
    OpEndCoroutine();

    void dump(std::string& out)
    {
        out = "end coroutine";
    }
};

struct OpMakeConst : OpBase {
    OpArg var;

    OpMakeConst(OpArg argVar);

    void getArgs(ArgsVector& args)
    {
        args.push_back(&var);
    }

    void dump(std::string& out)
    {
        out = "make const " + var.toStr();
    }
};

struct OpRangeSwitch : OpBase {
    OpArg src;
    int64_t minValue, maxValue;
    OpBase** cases;
    OpBase* defaultCase;

    OpRangeSwitch(OpArg argSrc);

    ~OpRangeSwitch()
    {
        delete[] cases;
    }

    virtual void getBranches(OpsVector& branches)
    {
        branches.insert(branches.end(), cases, cases + maxValue - minValue + 1);
        branches.push_back(defaultCase);
    }

    void getArgs(ArgsVector& args)
    {
        args.push_back(&src);
    }

    void dump(std::string& out)
    {
        out = "range switch " + src.toStr();
    }
};

template<class V>
class ZHash;

struct OpHashSwitch : OpBase {
    OpArg src;
    ZHash<OpBase*>* cases;
    OpBase* defaultCase;
    ZorroVM* vm;

    OpHashSwitch(OpArg argSrc, ZorroVM* argVm);

    ~OpHashSwitch();

    virtual void getBranches(OpsVector& branches);

    void getArgs(ArgsVector& args)
    {
        args.push_back(&src);
    }

    void dump(std::string& out)
    {
        out = "hash switch " + src.toStr();
    }
};

struct OpFormat : OpDstBase {
    OpArg src, width, prec, flags, extra;
    int w, p;

    OpFormat(int argW, int argP, OpArg argSrc, OpArg argDst, OpArg argWidth, OpArg argPrec, OpArg argFlags,
             OpArg argExtra);

    void getArgs(ArgsVector& args)
    {
        args.push_back(&src);
        args.push_back(&dst);
        args.push_back(&width);
        args.push_back(&prec);
        args.push_back(&flags);
        args.push_back(&extra);
    }

    void dump(std::string& out)
    {
        out = "format " + src.toStr();
        out += " -> ";
        out += dst.toStr();
    }
};

struct OpCombine : OpBase {
    OpArg* args;
    size_t count;
    OpArg dst;

    OpCombine(OpArg* argArgs, size_t argCount, OpArg argDst);

    OpCombine(const OpCombine&) = delete;

    OpCombine& operator=(const OpCombine&) = delete;

    ~OpCombine()
    {
        delete[] args;
    }

    void getArgs(ArgsVector& argsv)
    {
        for(size_t i = 0; i < count; i++)
        {
            argsv.push_back(this->args + i);
        }
    }

    void dump(std::string& out)
    {
        out = "combine ";
        for(size_t i = 0; i < count; i++)
        {
            if(i != 0)
            {
                out += ",";
            }
            out += args[i].toStr();
        }
        out += " -> ";
        out += dst.toStr();
    }
};

struct OpGetAttr : OpDstBase {
    OpArg obj, mem, att;

    OpGetAttr(OpArg argObj, OpArg argMem, OpArg argAtt, OpArg argDst);

    void dump(std::string& out)
    {
        out = "getattr ";
        out += obj.toStr();
        out += ".{";
        out += mem.toStr();
        out += ",";
        out += att.toStr();
        out += "} -> ";
        out += dst.toStr();
    }
};

}

#endif
