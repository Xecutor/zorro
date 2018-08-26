#ifndef __ZORRO_ZORROVM_HPP__
#define __ZORRO_ZORROVM_HPP__

#include <map>
#include <string>
#include "Exceptions.hpp"
#include "ZVMOpsDefs.hpp"
#include "Symbolic.hpp"
#include "ZString.hpp"
#include "ZArray.hpp"
#include "ZMap.hpp"
#include "ZSet.hpp"
#include "ZStack.hpp"
#include "Debug.hpp"

namespace zorro {

struct OpDstBase;

struct CallFrame {
    OpDstBase* callerOp;
    OpBase* retOp;
    index_type localBase;
    index_type args;
    index_type funcIdx;
    bool selfCall;
    bool closedCall;
    bool overload;
    /*CallFrame(){}
    CallFrame(index_type argLocalBase,index_type argArgsCount,OpArg argResult,OpBase* argRetOp):
      localBase(argLocalBase),argsCount(argArgsCount),result(argResult),retOp(argRetOp)
    {

    }*/
};

struct CatchInfo {
    ClassInfo** exList;
    OpBase* catchOp;
    index_type exCount;
    index_type idx;
    index_type stackLevel;
    index_type callLevel;

    CatchInfo()
    {
    }

    CatchInfo(ClassInfo** argExList, index_type argExCount, index_type argIdx, index_type argStackLevel,
              index_type argCallLevel, OpBase* argCatchOp) :
        exList(argExList), catchOp(argCatchOp), exCount(argExCount), idx(argIdx), stackLevel(argStackLevel),
        callLevel(argCallLevel)
    {
    }
};

struct ZVMContext {
    void onOtherStackResize()
    {
    }

    void onDataStackResize()
    {
        dataPtrs[atLocal] = dataStack.stack + callStack.stackTop->localBase;
    }

    OpBase* nextOp;
    OpBase* lastOp;
    Value* dataPtrs[atStack];
    ZStack<Value, ZVMContext, &ZVMContext::onDataStackResize> dataStack;
    ZStack<CallFrame, ZVMContext, &ZVMContext::onOtherStackResize> callStack;
    ZStack<Value, ZVMContext, &ZVMContext::onOtherStackResize> destructorStack;
    ZStack<CatchInfo, ZVMContext, &ZVMContext::onOtherStackResize> catchStack;
    Coroutine* coroutine;

    ZVMContext() :
        nextOp(0), lastOp(0), dataStack(this), callStack(this), destructorStack(this), catchStack(this), coroutine(0)
    {

    }

    void swap(ZVMContext& other)
    {
        destructorStack.swap(other.destructorStack);
        dataStack.swap(other.dataStack);
        callStack.swap(other.callStack);
        catchStack.swap(other.catchStack);
        Value* ptr;
        for(size_t i = 0; i < atStack; ++i)
        {
            ptr = dataPtrs[i];
            dataPtrs[i] = other.dataPtrs[i];
            other.dataPtrs[i] = ptr;
        }
        OpBase* op = nextOp;
        nextOp = other.nextOp;
        other.nextOp = op;
        op = lastOp;
        lastOp = other.lastOp;
        other.lastOp = op;
        Coroutine* c = coroutine;
        coroutine = other.coroutine;
        other.coroutine = c;
    }
};

struct Coroutine : RefBase {
    ZVMContext ctx;
    OpArg result;
};


std::string ValueToString(ZorroVM* vm, const Value& v);


#define ZUNREF(vm, val) if(ZISREFTYPE(val)){if((val)->refBase->unref())vm->unrefOps[(val)->vt](vm,val);(val)->vt=vtNil;(val)->flags=0;}
#define ZASSIGN(vm, za_dst, za_src) {\
  if(vm->assignMatrix[(za_dst)->vt][(za_src)->vt]){\
    vm->assignMatrix[(za_dst)->vt][(za_src)->vt](vm,za_dst,za_src,0);\
  }else\
  { \
    *(za_dst)=*(za_src);\
  }\
}

/*
if((za_dst)->flags&ValFlagConst)\
{\
  throw std::runtime_error("attempt to assign to constant");\
}\
*(za_dst)=*(za_src);\
(za_dst)->flags&=~ValFlagConst;\
}\
*/


//#define ZGLOBAL(vm,idx) vm->symbols.globals[idx]
#define ZGLOBAL(vm, idx) vm->ctx.dataPtrs[atGlobal][idx]
//#define ZLOCAL(vm,idx) vm->dataStack.stack[idx+vm->callStack.stackTop->localBase]
#define ZLOCAL(vm, idx) vm->ctx.dataPtrs[atLocal][idx]
//#define ZMEMBER(vm,midx) ZLOCAL(vm,vm->callStack.stackTop->argsCount).obj->members[midx]
#define ZMEMBER(vm, idx) vm->ctx.dataPtrs[atMember][idx]
#define ZCLOSED(vm, idx) vm->ctx.dataPtrs[atClosed][idx]


class ZorroVM : public ZMemory {
public:

    ZorroVM();

    virtual ~ZorroVM();

    ZorroVM(const ZorroVM&) = delete;

    ZorroVM& operator=(const ZorroVM&) = delete;

    void setEntry(OpBase* argEntry);

    void init();

    void initStd();

    void deinit()
    {
        running = false;
        entry = 0;
        unref(symbols.globals[objectClass->index]);
        unref(symbols.globals[stringClass->index]);
        //objectClass->unref();
        //stringClass->unref();
        symbols.clear();
    }

#ifdef DEBUG

    void dumpstack()
    {
        for(Value* i = ctx.dataStack.stack, * e = ctx.dataStack.stackTop + 1; i != e; ++i)
        {
            printf("[%d]=%s\n", (int) (i - ctx.dataStack.stack), ValueToString(this, *i).c_str());
        }
    }

#endif

    void run()
    {
        if(!entry.get()->code)
        {
            return;
        }
        running = true;
        ctx.nextOp = entry.get()->code;
        resume();
    }

    void resume()
    {
#ifdef DEBUG
        std::string dmp;
        ctx.nextOp->dump(dmp);
        DPRINT("next:[%s]%s\n", ctx.nextOp->pos.backTrace().c_str(), dmp.c_str());
#endif
        while(ctx.nextOp)
        {
            ctx.nextOp = (ctx.lastOp = ctx.nextOp)->next;
            ctx.lastOp->op(this, ctx.lastOp);
#ifdef DEBUG
            if(ctx.nextOp)
            {
                ctx.nextOp->dump(dmp);
                DPRINT("next:[%s]%s\n", ctx.nextOp->pos.backTrace().c_str(), dmp.c_str());
            }
#endif
        }
    }

    void step()
    {
        if(ctx.nextOp)
        {
            ctx.nextOp = (ctx.lastOp = ctx.nextOp)->next;
            ctx.lastOp->op(this, ctx.lastOp);
        }
    }

    OpBase* interrupt()
    {
        OpBase* rv = ctx.nextOp;
        ctx.nextOp = 0;
        return rv;
    }

    void returnAndResume(OpBase* op, const Value& val);

    typedef void (* BinOpFunc)(ZorroVM* vm, Value* l, const Value* r, Value* dst);

    typedef void (* TerOpFunc)(ZorroVM* vm, Value* l, const Value* a, const Value* r, Value* dst);

    typedef bool (* BinBoolFunc)(ZorroVM* vm, const Value* l, const Value* r);

    typedef void (* UnOpFunc)(ZorroVM* vm, Value* v, Value* dst);

    typedef bool (* UnBoolFunc)(ZorroVM* vm, const Value* v);

    typedef void (* UnFunc)(ZorroVM* vm, Value* v);

    typedef void (* FmtOpFunc)(ZorroVM* vm, Value* v, int w, int p, ZString* flags, Value* extra, Value* dst);

    typedef bool (* ForOpFunc)(ZorroVM* vm, Value* val, Value* var, Value* tmp);

    typedef bool (* For2OpFunc)(ZorroVM* vm, Value* val, Value* var1, Value* var2, Value* tmp);

#ifdef ZVM_STATIC_MATRIX
#define ZMTXDEF static
#else
#define ZMTXDEF
#endif
    ZMTXDEF UnOpFunc refOps[vtCount];
    ZMTXDEF UnOpFunc corOps[vtCount];
    ZMTXDEF UnOpFunc countOps[vtCount];
    ZMTXDEF UnOpFunc negOps[vtCount];
    ZMTXDEF UnOpFunc getTypeOps[vtCount];
    ZMTXDEF UnOpFunc copyOps[vtCount];
    ZMTXDEF UnBoolFunc boolOps[vtCount];
    ZMTXDEF UnBoolFunc notOps[vtCount];
    ZMTXDEF UnFunc incOps[vtCount];
    ZMTXDEF UnFunc decOps[vtCount];
    ZMTXDEF UnFunc unrefOps[vtCount];
    ZMTXDEF UnFunc callOps[vtCount];
    ZMTXDEF UnFunc ncallOps[vtCount];
    ZMTXDEF BinOpFunc assignMatrix[vtCount][vtCount];
    ZMTXDEF BinOpFunc addMatrix[vtCount][vtCount];
    ZMTXDEF BinOpFunc saddMatrix[vtCount][vtCount];
    ZMTXDEF BinOpFunc subMatrix[vtCount][vtCount];
    ZMTXDEF BinOpFunc ssubMatrix[vtCount][vtCount];
    ZMTXDEF BinOpFunc mulMatrix[vtCount][vtCount];
    ZMTXDEF BinOpFunc smulMatrix[vtCount][vtCount];
    ZMTXDEF BinOpFunc divMatrix[vtCount][vtCount];
    ZMTXDEF BinOpFunc sdivMatrix[vtCount][vtCount];
    ZMTXDEF BinOpFunc modMatrix[vtCount][vtCount];
    ZMTXDEF BinOpFunc smodMatrix[vtCount][vtCount];
    ZMTXDEF BinOpFunc bitOrMatrix[vtCount][vtCount];
    ZMTXDEF BinOpFunc bitAndMatrix[vtCount][vtCount];
    ZMTXDEF BinBoolFunc lessMatrix[vtCount][vtCount];
    ZMTXDEF BinBoolFunc greaterMatrix[vtCount][vtCount];
    ZMTXDEF BinBoolFunc lessEqMatrix[vtCount][vtCount];
    ZMTXDEF BinBoolFunc greaterEqMatrix[vtCount][vtCount];
    ZMTXDEF BinBoolFunc eqMatrix[vtCount][vtCount];
    ZMTXDEF BinBoolFunc neqMatrix[vtCount][vtCount];
    ZMTXDEF BinBoolFunc inMatrix[vtCount][vtCount];
    ZMTXDEF BinBoolFunc isMatrix[vtCount][vtCount];
    ZMTXDEF BinOpFunc mkIndexMatrix[vtCount][vtCount];
    ZMTXDEF BinOpFunc getIndexMatrix[vtCount][vtCount];
    ZMTXDEF TerOpFunc setIndexMatrix[vtCount][vtCount];
    ZMTXDEF BinOpFunc mkKeyRefMatrix[vtCount][vtCount];
    ZMTXDEF BinOpFunc getKeyMatrix[vtCount][vtCount];
    ZMTXDEF TerOpFunc setKeyMatrix[vtCount][vtCount];
    ZMTXDEF BinOpFunc getMemberMatrix[vtCount][vtCount];
    ZMTXDEF TerOpFunc setMemberMatrix[vtCount][vtCount];
    ZMTXDEF BinOpFunc mkMemberMatrix[vtCount][vtCount];

    ZMTXDEF ForOpFunc initForOps[vtCount];
    ZMTXDEF ForOpFunc stepForOps[vtCount];
    ZMTXDEF For2OpFunc initFor2Ops[vtCount];
    ZMTXDEF For2OpFunc stepFor2Ops[vtCount];

    ZMTXDEF FmtOpFunc fmtOps[vtCount];

    void cleanStack(size_t level)
    {
        /*
         * dataStack.size()==level
         * top-stack+1==level
         * top=stack+level-1
         */
        Value* target = ctx.dataStack.stack + level - 1;
        Value* top = ctx.dataStack.stackTop;
        ctx.dataStack.stackTop = target;
        while(target < top)
        {
            ++target;
            ZUNREF(this, target);
        }
    }

    Value* pushData()
    {
        ctx.dataStack.push(NilValue);
        return ctx.dataStack.stackTop;
    }

    CallFrame*
    pushFrame(OpDstBase* caller, OpBase* retOp, index_type argsCount = 0, index_type funcIdx = 0, bool selfCall = false,
              bool closedCall = false, bool overload = false)
    {
        CallFrame* cf = ctx.callStack.push();
        cf->callerOp = caller;
        cf->retOp = retOp;
        cf->localBase = ctx.dataStack.size() - argsCount;
        cf->args = argsCount;
        cf->funcIdx = funcIdx;
        cf->selfCall = selfCall;
        cf->closedCall = closedCall;
        cf->overload = overload;
        return cf;
    }

    void pushValue(const Value& val)
    {
        ctx.dataStack.push(val);
        if(ZISREFTYPE(&val))
        {
            val.refBase->ref();
        }
    }

    index_type getArgsCount()
    {
        return ctx.dataStack.size() - ctx.callStack.stackTop->localBase;
    }

    Value& getLocal(int idx)
    {
        return ctx.dataStack.stack[idx + ctx.callStack.stackTop->localBase];
    }

    Value& getLocalValue(int idx)
    {
        Value* v = ctx.dataStack.stack + idx + ctx.callStack.stackTop->localBase;
        if(v->vt == vtRef || v->vt == vtWeakRef)
        {
            v = &v->valueRef->value;
        }
        return *v;
    }

    void setLocal(int idx, const Value& val)
    {
        ZASSIGN(this, &ctx.dataPtrs[atLocal][idx], &val);
    }

    Value& getGlobal(int idx)
    {
        return symbols.globals[idx];
    }

    Value& getGlobal(const char* name)
    {
        Symbol s(Name(mkZString(name)));
        SymInfo* sym = symbols.getSymbol(s);
        if(!sym)
        {
            throw UndefinedSymbolException(s);
        }
        return symbols.globals[sym->index];
    }

    void setGlobal(int idx, const Value& val)
    {
        //assign(symbols.values[idx],val);
        ZASSIGN(this, &symbols.globals[idx], &val);
    }

    Value& getLValue(Value& val)
    {
        return ctx.dataPtrs[val.atLvalue][val.idx];
    }


    Value& getValue(Value& val)
    {
        if(val.vt == vtLvalue)
        {
            return getLValue(val);
        } else if(val.vt == vtRef || val.vt == vtWeakRef)
        {
            return val.valueRef->value;
        } else
        {
            return val;
        }
    }

    void assign(Value& dst, const Value& src)
    {
        ZASSIGN(this, &dst, &src);
    }

    void unref(Value& dst)
    {
        if(ZISREFTYPE(&dst) && dst.refBase->unref())
        {
            unrefOps[dst.vt](this, &dst);
            dst = NilValue;
        }
    }

    bool isEqual(const Value& l, const Value& r)
    {
        return eqMatrix[l.vt][r.vt](this, &l, &r);
    }


    void clearResult()
    {
        ZUNREF(this, &result);
    }

    void setResult(const Value& src)
    {
        if(&result == &src)
        {
            return;
        }
        ZUNREF(this, &result);
        //reg1=NilValue;
        if(assignMatrix[vtNil][src.vt])
        {
            assignMatrix[vtNil][src.vt](this, &result, &src, 0);
        } else
        {
            result = src;
            result.flags &= ~ValFlagConst;
        }
    }

    OpBase* getFuncEntry(FuncInfo* f, CallFrame* cf);

    OpBase* getFuncEntryNArgs(FuncInfo* f, CallFrame* cf);

    void throwValue(Value* obj);

    void callMethod(Value* obj, MethodInfo* meth, index_type argsCount, bool isOverload = true);

    void callMethod(Value* obj, index_type methIdx, index_type argsCount, bool isOverload = true);

    void callCMethod(Value* obj, MethodInfo* meth, index_type argsCount);

    void callCMethod(Value* obj, index_type methIdx, index_type argsCount);

    void callSomething(Value* func, index_type argsCount);

    void rxMatch(Value* l, Value* r, Value* d, OpArg* vars);

    struct StackTraceItem {
        std::string text;
        std::string func;
        std::string file;
        FileLocation* pos;

        StackTraceItem(const std::string& argText, const std::string& argFunc, const std::string& argFile,
                       FileLocation* argPos) :
            text(argText), func(argFunc), file(argFile), pos(argPos)
        {
        }
    };

    typedef std::vector<StackTraceItem> StackTraceVector;

    void getStackTrace(StackTraceVector& trace, bool skipLevel = true);

    std::string getStackTraceText(bool skipLevel = true);


    ClassInfo* objectClass;
    ClassInfo* nilClass;
    ClassInfo* boolClass;
    ClassInfo* intClass;
    ClassInfo* doubleClass;
    ClassInfo* classClass;
    ClassInfo* stringClass;
    ClassInfo* arrayClass;
    ClassInfo* mapClass;
    ClassInfo* setClass;
    ClassInfo* rangeClass;
    ClassInfo* corClass;
    ClassInfo* funcClass;
    ClassInfo* clsClass;
    ClassInfo* dlgClass;

    SymbolsInfo symbols;

    OpDstBase* dummyCallOp;
    OpBase* corRetOp;
    Value result;

    unsigned registerWeakRef(WeakRef* ptr)
    {
        unsigned index;
        if(wrFreeSlots.empty())
        {
            index = wrStorage.size();
            wrStorage.push_back(0);
        } else
        {
            index = wrFreeSlots.back();
            wrFreeSlots.pop_back();
        }
        wrStorage[index] = ptr;
        ptr->weakRefId = index + 1;
        ptr->wrNext = ptr;
        ptr->wrPrev = ptr;
        return index + 1;
    }

    void unregisterWeakRef(unsigned index)
    {
        wrStorage[index - 1] = 0;
        wrFreeSlots.push_back(index - 1);
    }

    void clearWeakRefs(unsigned index);

    Value mkWeakRef(Value* src);

    std::vector<WeakRef*> wrStorage;
    std::vector<unsigned> wrFreeSlots;

    ZVMContext ctx;


    ZCodeRef entry;
    bool running;
};

}

#endif
