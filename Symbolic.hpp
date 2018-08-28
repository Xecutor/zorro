#include <utility>

#ifndef __ZORRO_SYMBOLIC_HPP__
#define __ZORRO_SYMBOLIC_HPP__

#include <map>
#include <list>
#include <string>
#include <vector>
#include <algorithm>
#include <memory.h>
#include <stdlib.h>
#include "Debug.hpp"
#include "Value.hpp"
#include "ZString.hpp"
#include "ZArray.hpp"
#include "ZMap.hpp"
#include "ZSet.hpp"
#include "ZHash.hpp"
#include "Exceptions.hpp"
#include "NameList.hpp"
#include "ZVMOpsDefs.hpp"
#include "SynTree.hpp"

namespace zorro {

enum SymbolType {
    sytUnknown,
    sytGlobalScope,
    sytConstant,
    sytGlobalVar,
    sytLocalVar,
    sytClosedVar,
    sytTemporal,
    sytFunction,
    sytClass,
    sytClassMember,
    sytMethod,
    sytNamespace,
    sytProperty,
    sytAttr
};

struct SymInfo;
struct ScopeSym;
struct SymbolsInfo;

struct SymMap : ZHash<SymInfo*> {
    void clear(ZMemory* mem, SymInfo* parent);

    SymInfo* findSymbol(const Name& nm)
    {
        SymInfo** ptr = getPtr(nm.val.get());
        return ptr ? *ptr : nullptr;
    }

    SymInfo* findSymbol(ZString* nm)
    {
        SymInfo** ptr = getPtr(nm);
        return ptr ? *ptr : nullptr;
    }

    void insert(const Name& nm, SymInfo* info);

    void insert(ZStringRef nm, SymInfo* info);
};

struct ExtraInfo {
    virtual ~ExtraInfo() = default;

    virtual ExtraInfo* clone()const=0;

    template<class T>
    T& as()
    {
        return *reinterpret_cast<T*>(this);
    }
};

struct ScopeSymSmartRef {
    ScopeSymSmartRef(ScopeSym* argRef = nullptr);

    ScopeSymSmartRef(const ScopeSymSmartRef& other);

    ScopeSymSmartRef& operator=(const ScopeSymSmartRef& other);

    ~ScopeSymSmartRef();

    void reset(ScopeSym* argRef);

    ClassInfo* asClass()
    {
        return (ClassInfo*) ref;
    }

    FuncInfo* asFunc()
    {
        return (FuncInfo*) ref;
    }

    explicit operator bool()const
    {
        return ref != nullptr;
    }

    ScopeSym* ref;
};


enum TypeSpec {
    tsUnknown,
    tsDefined,
    tsOneOf
};

struct TypeInfo {
    TypeInfo() : ts(tsUnknown), vt(vtNil)
    {
    }

    TypeInfo(ValueType argVt) : ts(tsDefined), vt(argVt)
    {
    }

    TypeInfo(ClassInfo* argClassInfo, ValueType argVt = vtObject);

    TypeInfo(FuncInfo* argFuncInfo);

    void merge(const TypeInfo& other)
    {
        if(other.ts == tsUnknown)
        {
            return;
        }
        if(ts == tsUnknown)
        {
            *this = other;
            return;
        } else if(ts == tsDefined)
        {
            if(other.ts == tsDefined && isSame(other))
            {
                if(vt == vtArray || vt == vtSet || vt == vtMap)
                {
                    addToArr(other.arr);
                }
                return;
            }
            arr.push_back(*this);
            ts = tsOneOf;
        }
        if(other.ts == tsDefined)
        {
            if(!haveInArr(other))
            {
                arr.push_back(other);
            }
        } else
        {
            merge(other.arr);
        }
    }

    void merge(const std::vector<TypeInfo>& argArr)
    {
        for(auto& ti:argArr)
        {
            merge(ti);
        }
    }

    void addToArr(const std::vector<TypeInfo>& argArr)
    {
        for(auto& ti:argArr)
        {
            addToArr(ti);
        }
    }

    void addToArr(const TypeInfo& other)
    {
        if(other.ts == tsUnknown)
        {
            return;
        }
        if(other.ts == tsDefined)
        {
            if(!haveInArr(other))
            {
                arr.push_back(other);
            }
        } else
        {
            for(auto& ti : other.arr)
            {
                if(!haveInArr(ti))
                {
                    arr.push_back(ti);
                }
            }
        }
    }

    bool isContainer() const
    {
        return ts == tsDefined && (vt == vtArray || vt == vtSet || vt == vtMap);
    }

    bool isSame(const TypeInfo& other) const
    {
        if(ts != other.ts)
        {
            return false;
        }
        if(ts == tsDefined)
        {
            if(vt == vtObject || vt == vtClass)
            {
                return symRef.ref == other.symRef.ref;
            }
            if(vt == vtFunc)
            {
                return symRef.ref == other.symRef.ref;
            }
            return vt == other.vt;
        }
        if(arr.size() != other.arr.size())
        {
            return false;
        }
        for(auto& ti:arr)
        {
            if(!other.haveInArr(ti))
            {
                return false;
            }
        }
        return true;
    }

    bool haveInArr(const TypeInfo& ti) const
    {
        for(auto& ownTi:arr)
        {
            if(ownTi.isSame(ti))
            {
                return true;
            }
        }
        return false;
    }

    TypeSpec ts;
    ValueType vt;
    ScopeSymSmartRef symRef;
    std::vector<TypeInfo> arr;

    struct base_iterator {
        size_t idx;

        base_iterator(size_t argIdx) : idx(argIdx)
        {
        }

        bool operator==(const base_iterator& other) const
        {
            return idx == other.idx;
        }

        bool operator!=(const base_iterator& other) const
        {
            return idx != other.idx;
        }

        void operator++()
        {
            ++idx;
        }

        void operator++(int)
        {
            ++idx;
        }
    };

    struct iterator : base_iterator {
        TypeInfo& self;

        iterator(size_t argIdx, TypeInfo& argSelf) : base_iterator(argIdx), self(argSelf)
        {
        }

        iterator(const iterator& other) : base_iterator(other.idx), self(other.self)
        {
        }

        TypeInfo* operator->() const
        {
            return self.ts == tsOneOf ? &self.arr[idx] : &self;
        }

        TypeInfo& operator*() const
        {
            return self.ts == tsOneOf ? self.arr[idx] : self;
        }

        iterator& operator=(const iterator&) = delete;
    };

    struct const_iterator : base_iterator {
        const TypeInfo& self;

        const_iterator(size_t argIdx, const TypeInfo& argSelf) : base_iterator(argIdx), self(argSelf)
        {
        }

        const_iterator(const const_iterator& other) : base_iterator(other.idx), self(other.self)
        {
        }

        const TypeInfo* operator->() const
        {
            return self.ts == tsOneOf ? &self.arr[idx] : &self;
        }

        const TypeInfo& operator*() const
        {
            return self.ts == tsOneOf ? self.arr[idx] : self;
        }

        const_iterator& operator=(const const_iterator&) = delete;

        const_iterator& operator=(const_iterator&&) = delete;
    };

    iterator begin()
    {
        return iterator(0, *this);
    }

    iterator end()
    {
        return iterator(ts == tsDefined ? 1 : ts == tsUnknown ? 0 : arr.size(), *this);
    }

    const_iterator begin() const
    {
        return const_iterator(0, *this);
    }

    const_iterator end() const
    {
        return const_iterator(ts == tsDefined ? 1 : ts == tsUnknown ? 0 : arr.size(), *this);
    }
};

struct SymInfo : RefBase {
    SymbolType st;
    bool closed = false;
    static const size_t invalidIndexValue = static_cast<size_t>(-1);
    size_t index = invalidIndexValue;
    Name name;
    SymInfo* replacedSymbol = nullptr;
    ExtraInfo* extra = nullptr;
    TypeInfo tinfo;

    SymInfo(Name argName, SymbolType argSt) : st(argSt), name(std::move(argName))
    {
    }

    SymInfo(const SymInfo&) = delete;
    SymInfo& operator=(const SymInfo&) = delete;

    virtual ~SymInfo()
    {
        delete extra;
    }

    virtual SymMap* getSymbols()
    {
        return nullptr;
    }

    virtual SymInfo* getParent()
    {
        return nullptr;
    }

    ScopeSym* scope()
    {
        return reinterpret_cast<ScopeSym*>(this);
    }
};


struct AttrSym : SymInfo {
    AttrSym(const Name& argName) : SymInfo(argName, sytAttr)
    {
    }
    AttrSym(const AttrSym&) = delete;
    AttrSym(AttrSym&&) = delete;
    AttrSym& operator=(const AttrSym&) = delete;
    AttrSym& operator=(AttrSym&&) = delete;
};

struct BlockInfo {
    Name name;
    BlockInfo* parent;
    std::vector<OpBase**> nexts;
    std::vector<OpBase**> redos;
    std::vector<OpBase**> breaks;
    std::list<BlockInfo> nested;
    int triesEntered;

    BlockInfo(Name argName, BlockInfo* argParent) : name(argName), parent(argParent), triesEntered(0)
    {

    }
};

struct LiterInfo;


struct ScopeSym : SymInfo {
    ScopeSym(Name argName, SymbolType argSt, ScopeSym* argParent, SymbolsInfo* argSi);

    ScopeSym(const ScopeSym&) = delete;

    ~ScopeSym() override
    {
        symMap.clear(mem, this);
        attrMap.clear(mem, this);
        for(auto ptr:tempSymbols)
        {
            delete ptr;
        }
        ZString* key;
        LiterList** lst;
        ZHash<LiterList*>::Iterator it(literMap);
        while(it.getNext(key, lst))
        {
            if(key->unref())
            {
                mem->freeZString(key);
            }
            delete *lst;
        }
        for(auto& ptr : refs)
        {
            ptr->ref = nullptr;
        }
    }

    ScopeSym& operator=(const ScopeSym&) = delete;

    FileLocation end;

    typedef std::list<LiterInfo*> LiterList;
    ZHash<LiterList*> literMap;

    SymMap symMap;
    std::vector<SymInfo*> locals;
    SymMap attrMap;
    ScopeSym* parent = nullptr;
    ScopeSym* tmpChild = nullptr;
    std::vector<BlockInfo> blocks;
    BlockInfo* currentBlock = nullptr;
    std::vector<OpArg> closedVars;

    struct ClosedFuncInfo {
        OpArg src;
        OpArg dst;

        ClosedFuncInfo(index_type argSrc, index_type argDst) : src(atGlobal, argSrc), dst(atLocal, argDst)
        {
        }
    };

    typedef std::vector<ClosedFuncInfo> ClosedFuncVector;
    ClosedFuncVector closedFuncs;
    std::list<SymInfo*> freeTemporals;
    std::list<SymInfo*> tempSymbols;
    std::map<size_t, SymInfo*> acquiredTemporals;
    std::list<ScopeSym*> usedNs;
    SymbolsInfo* si;
    ZMemory* mem;
    int triesEntered;
    bool selfClosed;

    std::set<ScopeSymSmartRef*> refs;

    bool isClosure()
    {
        return !closedVars.empty() || selfClosed;
    }

    bool hasClosedFuncStorage(index_type gidx, index_type& lidx)
    {
        for(auto& cv : closedFuncs)
        {
            if(cv.src.idx == gidx)
            {
                lidx = cv.dst.idx;
                return true;
            }
        }
        return false;
    }

    void enterTry()
    {
        triesEntered++;
        if(currentBlock)
        {
            currentBlock->triesEntered++;
        }
    }

    void leaveTry()
    {
        triesEntered--;
        if(currentBlock)
        {
            currentBlock->triesEntered--;
        }
    }


    size_t acquireTemporal()
    {
        SymInfo* tmp;
        if(!freeTemporals.empty())
        {
            tmp = freeTemporals.back();
            freeTemporals.pop_back();
        } else
        {
            size_t rindex;
            if(st == sytGlobalScope)
            {
                rindex = acquiredTemporals.size();
            } else
            {
                rindex = symMap.getCount();
            }

            char rname[32];
            snprintf(rname, sizeof(rname), "temp-%u", static_cast<unsigned>(rindex));
            ZStringRef nm(mem, mem->allocZString(rname));
            tmp = new SymInfo(Name(nm, FileLocation()), sytTemporal);
            symMap.insert(nm, tmp);
            tmp->index = rindex;
        }
        acquiredTemporals[tmp->index] = tmp;
        return tmp->index;
    }

    void releaseTemporal(size_t argIndex)
    {
        auto it = acquiredTemporals.find(argIndex);
        if(it == acquiredTemporals.end())
        {
            abort();
        }
        freeTemporals.push_back(it->second);
        acquiredTemporals.erase(it);
    }

    size_t registerClosedSymbol(SymInfo* infoPtr, OpArg srcVar)
    {
        infoPtr->index = closedVars.size();
        closedVars.push_back(srcVar);
        symMap.insert(infoPtr->name, infoPtr);
        return infoPtr->index;
    }

    SymMap* getSymbols() override
    {
        return &symMap;
    }

    SymInfo* getParent() override
    {
        return parent;
    }

    void fullName(std::string& out) const
    {
        if(parent)
        {
            parent->fullName(out);
            if(parent->st == sytNamespace)
            {
                if(!out.empty())
                {
                    out += "::";
                }
            } else //if(parent->st!=syt)
            {
                if(!out.empty())
                {
                    out += ".";
                }
            }
        }
        if(st == sytGlobalScope)
        {
            return;
        }
        out += name.val.c_str();
    }

};

struct GlobalScope : ScopeSym {
    SymMap sc;//string consts
    SymMap ic;//int consts
    SymMap dc;//double consts
    GlobalScope(SymbolsInfo* argSi) : ScopeSym(Name(), sytGlobalScope, nullptr, argSi)
    {
    }

    GlobalScope(const GlobalScope&) = delete;

    GlobalScope(GlobalScope&&) = delete;

    GlobalScope& operator=(GlobalScope&&) = delete;

    GlobalScope& operator=(const GlobalScope&) = delete;

    void clear();

    void clearGlobals();
};

struct FuncInfo : ScopeSym {
    FuncInfo(Name argName, ScopeSym* argParent, SymbolsInfo* argSi) :
        ScopeSym(argName, sytFunction, argParent, argSi)
    {
    }

    FuncInfo(const FuncInfo&) = delete;

    FuncInfo& operator=(const FuncInfo&) = delete;

    ~FuncInfo() override
    {
        if(!defValEntries.empty())
        {
            ZCode zc((ZorroVM*) mem, defValEntries.front());
            if(varArgEntryLast)
            {
                varArgEntryLast->next = nullptr;
                ZCode zc2((ZorroVM*) mem, entry);
            }
        } else
        {
            ZCode zc((ZorroVM*) mem, entry);
        }
    }

    ZorroCFunc cfunc = nullptr;
    OpBase* entry = nullptr;
    std::vector<OpBase*> defValEntries;
    OpBase* varArgEntry = nullptr;
    OpBase* namedArgEntry = nullptr;
    OpBase* varArgEntryLast = nullptr;
    index_type argsCount = 0;
    index_type localsCount = 0;
    TypeInfo rvtype;
    Statement* def = nullptr;
    bool inTypeFill = false;
    bool namedArgs = false;
};

struct LiterInfo : FuncInfo {
    LiterInfo(Name argName, ScopeSym* argParent, SymbolsInfo* argSi) : FuncInfo(argName, argParent, argSi)
    {
    }

    LiterInfo(const LiterInfo&) = delete;

    LiterInfo(LiterInfo&&) = delete;

    LiterInfo& operator=(const LiterInfo&) = delete;

    LiterInfo& operator=(LiterInfo&&) = delete;

    typedef std::list<LiterArg> ArgList;
    ArgList markers;
};


struct ClassInfo;

struct MethodInfo : FuncInfo {
    MethodInfo(Name argName, ScopeSym* argParent, ClassInfo* argClass, SymbolsInfo* argSi) :
        FuncInfo(argName, argParent, argSi), owningClass(argClass)
    {
        st = sytMethod;
    }

    ~MethodInfo() override
    {
        if(lastOp)
        {
            lastOp->next = nullptr;
        }
    }

    MethodInfo(const MethodInfo&) = delete;

    MethodInfo(MethodInfo&&) = delete;

    MethodInfo& operator=(const MethodInfo&) = delete;

    MethodInfo& operator=(MethodInfo&&) = delete;

    ZorroCMethod cmethod = nullptr;
    ClassInfo* owningClass;
    OpBase* lastOp = nullptr;//to cut off destructor code of parent class
    size_t localIndex = invalidIndexValue;//index in methods table
    size_t overIndex = invalidIndexValue;//index of overriden method
    bool specialMethod = false;
};

struct AttrInfo {
    struct AttrMapping {
        index_type original;
        index_type mapped;
        bool canBeOverriden;

        AttrMapping(index_type argOriginal, index_type argMapped) :
            original(argOriginal), mapped(argMapped), canBeOverriden(false)
        {
        }

        AttrMapping(const AttrMapping& argOther) : original(argOther.original), mapped(argOther.mapped),
            canBeOverriden(true)
        {
        }

        bool operator<(const AttrMapping& rhs) const
        {
            return original < rhs.original;
        }

        bool operator<(index_type idx) const
        {
            return original < idx;
        }
    };

    typedef std::vector<AttrMapping> AttrMapVector;
    AttrMapVector attrs;

    bool addAttr(index_type idx, index_type mappedIdx)
    {
        auto it = std::lower_bound(attrs.begin(), attrs.end(), idx);
        if(it != attrs.end() && it->original == idx)
        {
            if(it->canBeOverriden)
            {
                it->mapped = mappedIdx;
                return true;
            }
            return false;
        }
        attrs.insert(it, AttrMapping(idx, mappedIdx));
        return true;
    }

    index_type hasAttr(index_type idx) const
    {
        auto it = std::lower_bound(attrs.begin(), attrs.end(), idx);
        return it == attrs.end() ? 0 : it->original == idx ? it->mapped : 0;
    }
};

struct ClassMember : SymInfo {
    ClassMember(const Name& argName) : SymInfo(argName, sytClassMember)
    {
    }

    ClassMember(const ClassMember&) = delete;
    ClassMember(ClassMember&&) = delete;
    ClassMember& operator=(const ClassMember&) = delete;
    ClassMember& operator=(ClassMember&&) = delete;

    void copyFrom(const ClassMember& other)
    {
        closed = other.closed;
        index = other.index;
        name = other.name;
        extra = other.extra ? other.extra->clone() : nullptr;
        attrs = other.attrs;
        owningClass = other.owningClass;
    }


    AttrInfo attrs;
    ClassInfo* owningClass = nullptr;
};

enum ClassSpecialMethod {
    csmUnknown,
    csmConstructor,
    csmDestructor,
    csmGetIndex,
    csmSetIndex,
    csmGetKey,
    csmSetKey,
    csmGetProp,
    csmSetProp,
    csmAdd,
    csmRAdd,
    csmSAdd,
    csmSub,
    csmSSub,
    csmDiv,
    csmSDiv,
    csmMul,
    csmSMul,
    csmCall,
    csmCopy,
    csmFormat,
    csmBoolCheck,
    csmLess,
    csmCount,
};

struct ClassInfo : ScopeSym {
    ClassInfo(Name argName, ScopeSym* argParent, SymbolsInfo* argSi) : ScopeSym(argName, sytClass, argParent, argSi)
    {
        st = sytClass;
        for(auto& specialMethod : specialMethods)
        {
            specialMethod = 0;
        }
    }

    ~ClassInfo() override;

    ClassInfo(const ClassInfo&) = delete;

    ClassInfo(ClassInfo&&) = delete;

    ClassInfo& operator=(const ClassInfo&) = delete;

    ClassInfo& operator=(ClassInfo&&) = delete;

    struct HiddenCtorArg {
        Name name;
        size_t argIdx;
        size_t memIdx;

        HiddenCtorArg(Name argName, size_t argArgIdx, size_t argMemIdx) : name(argName), argIdx(argArgIdx),
            memIdx(argMemIdx)
        {
        }
    };

    typedef std::list<HiddenCtorArg> HiddenArgsList;
    HiddenArgsList hiddenArgs;

    typedef std::vector<ClassInfo*> ChildrenVector;
    ChildrenVector children;

    Statement* def = nullptr;;
    ScopeSymSmartRef parentClass;
    size_t specialMethods[csmCount];
    size_t membersCount = 0;
    std::vector<MethodInfo*> methodsTable;
    std::vector<ClassMember*> members;
    AttrInfo attrs;
    bool nativeClass = false;
    //int methods;
    /*
    Value* mArr;
    void addMethod(Value argMethod)
    {
      if(!mArr)mArr=new Value[1];
      else
      {
        Value* newArr=new Value[methods+1];
        memcpy(newArr,mArr,sizeof(Value)*methods);
      }
      mArr[methods++]=argMethod;
    }*/
    bool isInParents(ClassInfo* ci)
    {
        ClassInfo* ptr = this;
        while(ptr)
        {
            if(ci == ptr)
            {
                return true;
            }
            ptr = ptr->parentClass.asClass();
        }
        return false;
    }
};

struct ClassPropertyInfo : SymInfo {
    ClassPropertyInfo(Name argName) : SymInfo(argName, sytProperty)
    {
    }

    ClassPropertyInfo(const ClassPropertyInfo&) = delete;
    ClassPropertyInfo(ClassPropertyInfo&&) = delete;
    ClassPropertyInfo& operator=(const ClassPropertyInfo&) = delete;
    ClassPropertyInfo& operator=(ClassPropertyInfo&&) = delete;

    size_t getIdx = invalidIndexValue;
    MethodInfo* getMethod = nullptr;
    size_t setIdx = invalidIndexValue;
    MethodInfo* setMethod = nullptr;
};

struct NsInfo : ScopeSym {
    NsInfo(Name argName, ScopeSym* argParent, SymbolsInfo* argSi) : ScopeSym(argName, sytNamespace, argParent, argSi)
    {
    }

    NsInfo(const NsInfo&) = delete;

    NsInfo(NsInfo&&) = delete;

    NsInfo& operator=(const NsInfo&) = delete;

    NsInfo& operator=(NsInfo&&) = delete;
};

class OutputBuffer;

class InputBuffer;

struct SymbolsInfo {

    //typedef std::vector<Value> ValVector;
    typedef std::vector<SymInfo*> SymVector;
    ZMemory* mem;
    GlobalScope global;
    Value* globals;
    size_t globalsSize;
    size_t globalsCount;
    std::vector<size_t> freeGlobals;
    SymVector info;
    size_t stdEnd;

    ScopeSym* currentScope = nullptr;
    ClassInfo* currentClass = nullptr;

    size_t nilIdx;
    size_t trueIdx;
    size_t falseIdx;
    ZStringRef nilStr, trueStr, falseStr, object;

    ZHash<ClassSpecialMethod> csmMap;
    const char* csmNames[csmCount];


    /*ZStringRef mkZString(const std::string& str)
    {
      return mem->mkZString(str.c_str(),str.length());
    }*/

    SymbolsInfo(ZMemory* argMem) : mem(argMem), global(this)
    {
        global.name = mem->mkZString("global");
        globalsCount = 0;
        globalsSize = 512;
        globals = new Value[globalsSize];
        memset(globals, 0, sizeof(Value) * globalsSize);
        memset(csmNames, 0, sizeof(csmNames));
        init();
        nilStr = mem->mkZString("nil");
        nilIdx = registerGlobalSymbol(nilStr, new SymInfo(nilStr, sytConstant));
        globals[nilIdx].flags = ValFlagConst;
        trueStr = mem->mkZString("true");
        trueIdx = registerGlobalSymbol(trueStr, new SymInfo(trueStr, sytConstant));
        globals[trueIdx] = BoolValue(true);
        globals[trueIdx].flags = ValFlagConst;
        falseStr = mem->mkZString("false");
        falseIdx = registerGlobalSymbol(falseStr, new SymInfo(falseStr, sytConstant));
        globals[falseIdx] = BoolValue(false);
        globals[falseIdx].flags = ValFlagConst;
        object = mem->mkZString("Object");
        stdEnd = info.size();

        addCsmMap("create", csmConstructor);
        addCsmMap("destroy", csmDestructor);
        addCsmMap("getIndex", csmGetIndex);
        addCsmMap("setIndex", csmSetIndex);
        addCsmMap("getKey", csmGetKey);
        addCsmMap("setKey", csmSetKey);
        addCsmMap("add", csmAdd);
        addCsmMap("sadd", csmSAdd);
        addCsmMap("radd", csmRAdd);
        addCsmMap("sub", csmSub);
        addCsmMap("ssub", csmSSub);
        addCsmMap("div", csmDiv);
        addCsmMap("sdiv", csmSDiv);
        addCsmMap("mul", csmMul);
        addCsmMap("smul", csmSMul);
        addCsmMap("call", csmCall);
        addCsmMap("copy", csmCopy);
        addCsmMap("format", csmFormat);
        addCsmMap("getProp", csmGetProp);
        addCsmMap("setProp", csmSetProp);
        addCsmMap("boolCheck", csmBoolCheck);
        addCsmMap("less", csmLess);
    }

    SymbolsInfo(const SymbolsInfo&) = delete;

    SymbolsInfo(SymbolsInfo&&) = delete;

    SymbolsInfo& operator=(const SymbolsInfo) = delete;

    SymbolsInfo& operator=(SymbolsInfo&&) = delete;

    void init()
    {
        currentScope = &global;
        currentClass = 0;
    }

    void addCsmMap(const char* str, ClassSpecialMethod csm)
    {
        ZString* zs = mem->allocZString(str);
        csmMap.insert(zs, csm);
        csmNames[csm] = str;
    }


    ClassSpecialMethod getCsm(ZStringRef name)
    {
        ClassSpecialMethod* ptr = csmMap.getPtr(name.get());
        return ptr ? *ptr : csmUnknown;
    }

    ClassSpecialMethod getCsm(const Name& name)
    {
        return getCsm(name.val);
    }


    void clear()
    {
        global.clearGlobals();
        for(size_t i = 0; i < globalsCount; i++)
        {
            //if(info[i]->st==sytGlobalVar)
            {
                //mem->assign(globals[i],NilValue);
                mem->unref(globals[i]);
            }
        }
        global.clear();
        delete[] globals;
        globals = nullptr;
    }

    ~SymbolsInfo()
    {
    }

    size_t getGlobalTemporals()
    {
        return global.freeTemporals.size();
    }

    size_t newGlobal()
    {
        if(!freeGlobals.empty())
        {
            size_t rv = freeGlobals.back();
            freeGlobals.pop_back();
            return rv;
        }
        if(globalsCount == globalsSize)
        {
            globalsSize += 512;
            Value* newGlobals = new Value[globalsSize];
            memcpy(newGlobals, globals, sizeof(Value) * globalsCount);
            memset(newGlobals + globalsCount, 0, sizeof(Value) * 512);
            delete[] globals;
            globals = newGlobals;
        }
        return globalsCount++;
    }

    void returnScope()
    {
        if(currentScope->st == sytFunction || currentScope->st == sytMethod)
        {
            FuncInfo* fi = (FuncInfo*) currentScope;
            fi->localsCount = fi->symMap.getCount() - fi->argsCount;
        }
        if(currentScope == currentClass)
        {
            SymInfo* ptr = currentScope->getParent();
            while(ptr && ptr->st != sytClass)
            {
                ptr = ptr->getParent();
            }
            currentClass = (ClassInfo*) ptr;
        }
        currentScope = (ScopeSym*) currentScope->getParent();
        if(!currentScope)
        {
            currentScope = &global;
        }
    }

    void enterBlock(Name argName)
    {
        if(currentScope->currentBlock)
        {
            currentScope->currentBlock =
                &*currentScope->currentBlock->nested.insert(
                    currentScope->currentBlock->nested.end(),
                    BlockInfo(argName, currentScope->currentBlock));
        } else
        {
            currentScope->currentBlock = &*currentScope->blocks.insert(currentScope->blocks.end(),
                                                                       BlockInfo(argName, currentScope->currentBlock));
        }
    }

    void leaveBlock()
    {
        if(currentScope->currentBlock->triesEntered > 0)
        {
            abort();
        }
        currentScope->currentBlock = currentScope->currentBlock->parent;
    }

    BlockInfo* getBlock(Name nm)
    {
        if(!nm.val)
        {
            return currentScope->currentBlock;
        }
        BlockInfo* ptr = currentScope->currentBlock;
        while(ptr)
        {
            if(ptr->name.val.get() && *ptr->name.val == *nm.val)
            {
                return ptr;
            }
            ptr = ptr->parent;
        }
        return 0;
    }

    size_t registerGlobalSymbol(Name name, SymInfo* infoPtr)
    {
        size_t index = newGlobal();
        infoPtr->index = index;
        if(index >= info.size())
        {
            info.resize(index + 1);
        }
        info[index] = infoPtr;
        if(currentScope->st == sytNamespace)
        {
            currentScope->getSymbols()->insert(name, infoPtr);
            global.getSymbols()->insert(getFullName(name), infoPtr);
        } else
        {
            global.getSymbols()->insert(name, infoPtr);
        }
        return index;
    }

    void freeGlobal(size_t idx)
    {
        mem->unref(globals[idx]);
        freeGlobals.push_back(idx);
        info[idx] = nullptr;
    }

    size_t registerLocalSymbol(SymInfo* infoPtr)
    {
        size_t index = currentScope->getSymbols()->getCount();
        infoPtr->index = index;
        currentScope->locals.push_back(infoPtr);
        currentScope->getSymbols()->insert(infoPtr->name, infoPtr);
        return index;
    }

    size_t registerScopedGlobal(SymInfo* infoPtr)
    {
        size_t index = newGlobal();
        infoPtr->index = index;
        info.resize(index + 1);
        info[index] = infoPtr;
        currentScope->getSymbols()->insert(infoPtr->name, infoPtr);
        return index;
    }

    void replaceLocalSymbol(SymInfo* infoPtr)
    {
        SymMap& sm = *(currentScope->getSymbols());
        SymInfo* ptr = sm.findSymbol(infoPtr->name);
        infoPtr->replacedSymbol = ptr;
        sm.insert(infoPtr->name, infoPtr);
    }

    void restoreLocalSymbol(SymInfo* infoPtr)
    {
        SymMap& sm = *(currentScope->getSymbols());
        infoPtr->ref();
        currentScope->tempSymbols.push_back(infoPtr);
        sm.insert(infoPtr->name, infoPtr->replacedSymbol);
    }

    size_t registerLocalVar(ZStringRef name)
    {
        size_t rv = registerLocalSymbol(new SymInfo(name, sytLocalVar));
        FuncInfo* fi = (FuncInfo*) currentScope;
        fi->localsCount = fi->symMap.getCount() - fi->argsCount;
        return rv;
    }

    size_t registerVar(Name name, bool& isLocal)
    {
        if(currentScope->st == sytGlobalScope || currentScope->st == sytNamespace)
        {
            isLocal = false;
            size_t rv = registerGlobalSymbol(name, new SymInfo(name, sytGlobalVar));
            DPRINT("register global %s as %u\n", name.val.c_str(), static_cast<unsigned int>(rv));
            return rv;
        } else
        {
            isLocal = true;
            size_t rv = registerLocalSymbol(new SymInfo(name, sytLocalVar));
            DPRINT("register local %s as %u\n", name.val.c_str(), static_cast<unsigned int>(rv));
            return rv;
        }
    }

    size_t acquireTemp()
    {
        return currentScope->acquireTemporal();
    }

    void releaseTemp(size_t index)
    {
        currentScope->releaseTemporal(index);
    }

    size_t registerArg(Name name)
    {
        return registerLocalSymbol(new SymInfo(name, sytLocalVar));
    }

    ClassInfo* registerClass(Name name)
    {
        ClassInfo* cinfo = new ClassInfo(name, currentScope, this);
        cinfo->tinfo = TypeInfo(cinfo, vtClass);
        currentClass = cinfo;
        size_t index = registerGlobalSymbol(name, cinfo);
        Value val;
        val.vt = vtClass;
        val.flags = ValFlagConst;
        //cinfo->ref();
        val.classInfo = cinfo;
        //val.classInfo->ref();
        //val.classInfo->ref();
        DPRINT("name=%s, cinfo=%p, classValue=%p\n", name.val.c_str(), cinfo, val.classInfo);
        globals[index] = val;
        currentScope = cinfo;
        return cinfo;
    }

    ClassInfo* enterClass(Name name)
    {
        ScopeSym* sym = (ScopeSym*) currentScope->symMap.findSymbol(name);
        if(!sym)
        {
            return nullptr;
        }
        currentScope = sym;
        currentClass = (ClassInfo*) sym;
        return currentClass;
    }

    void deriveMembers(Symbol* parent)
    {
        if(parent)
        {
            SymInfo* pc = getSymbol(*parent);
            if(!pc)
            {
                throw UndefinedSymbolException(*parent);
            }
            if(pc->st != sytClass)
            {
                ZTHROW(CGException, parent->name.pos, "Symbol %{} is not class", parent->toString());
            }
            ClassInfo* parentClass = (ClassInfo*) pc;
            if(!parentClass->parentClass)
            {
                ZTHROW(CGException, parent->name.pos, "Class %{} is not defined yet", parent->toString());
            }
            currentClass->parentClass = parentClass;
            ((ClassInfo*) pc)->children.push_back(currentClass);
        } else
        {
            currentClass->parentClass = (ClassInfo*) getGlobalSymbol(object);
            return;
        }
        ClassInfo* pc = currentClass->parentClass.asClass();
        memcpy(currentClass->specialMethods, pc->specialMethods, sizeof(currentClass->specialMethods));
        currentClass->specialMethods[csmConstructor] = 0;
        currentClass->specialMethods[csmDestructor] = 0;
        currentClass->symMap.assign(mem, pc->symMap);
        currentClass->membersCount = pc->membersCount;
        currentClass->methodsTable = pc->methodsTable;
        SymMap::Iterator it(currentClass->symMap);
        ZString* str;
        SymInfo** val;
        while(it.getNext(str, val))
        {
            if(!*val)
            {
                continue;
            }
            if((*val)->st != sytClassMember)
            {
                (*val)->ref();
                continue;
            }
            ClassMember* cm = (ClassMember*) *val;
            if(cm->owningClass == currentClass)
            {
                continue;
            }
            ClassMember* ncm = new ClassMember(cm->name);
            ncm->copyFrom(*cm);
            ncm->refCount = 1;
            *val = ncm;
            if(ncm->index >= currentClass->members.size())
            {
                currentClass->members.resize(ncm->index + 1, 0);
            }
            currentClass->members[ncm->index] = ncm;
        }
    }

    void registerNativeClassSpecialMethod(ClassSpecialMethod csm, ZorroCMethod meth)
    {
        if(!currentClass)
        {
            ZTHROW(CGException, FileLocation(), "Attempt to register native class special method in outside of class");
        }
        ClassInfo* cinfo = currentClass;
        std::string mnamestr = "on ";
        mnamestr += csmNames[csm];
        ZStringRef mname = getStringConstVal(mnamestr.c_str());
        MethodInfo* m = new MethodInfo(mname, currentScope, currentClass, this);
        m->index = registerGlobalSymbol(getFullName(mname), m);//ci->methods++;
        m->localIndex = cinfo->methodsTable.size();
        m->specialMethod = true;
        m->cmethod = meth;
        cinfo->methodsTable.push_back(m);
        cinfo->specialMethods[csm] = m->index;
        Value dval;
        dval.vt = vtCMethod;
        dval.cmethod = m;
        dval.flags = 0;
        globals[m->index] = dval;
        cinfo->symMap.insert(mname, m);
    }

    ClassInfo* registerNativeClass(Name name, ZorroCMethod ctor, ZorroCMethod dtor)
    {
        ClassInfo* cinfo = new ClassInfo(name, currentScope, this);
        cinfo->parentClass = (ClassInfo*) getGlobalSymbol(object);
        currentClass = cinfo;
        cinfo->nativeClass = true;
        cinfo->tinfo = TypeInfo(cinfo, vtClass);
        size_t index = registerGlobalSymbol(name, cinfo);
        Value cval;
        cval.vt = vtClass;
        cval.flags = ValFlagConst;
        //cinfo->ref();
        cval.classInfo = cinfo;
        globals[index] = cval;
        currentScope = cinfo;

        if(ctor)
        {
            registerNativeClassSpecialMethod(csmConstructor, ctor);
            /*ZStringRef cname=getStringConstVal("on create");
            MethodInfo* cm=new MethodInfo(cname,currentScope,currentClass,this);
            cm->cmethod=ctor;
            cm->index=registerGlobalSymbol(getFullName(cname),cm);//ci->methods++;
            cm->localIndex=cinfo->methodsTable.size();
            cinfo->methodsTable.push_back(cm);
            cinfo->specialMethods[csmConstructor]=cm->index;
            Value val;
            val.vt=vtCMethod;
            val.cmethod=cm;
            val.flags=0;
            globals[cm->index]=val;
            cinfo->symMap.insert(cname,cm);*/
        }

        if(dtor)
        {
            registerNativeClassSpecialMethod(csmDestructor, dtor);
            /*
            ZStringRef dname=getStringConstVal("on destroy");
            MethodInfo* dm=new MethodInfo(dname,currentScope,currentClass,this);
            dm->index=registerGlobalSymbol(getFullName(dname),dm);//ci->methods++;
            dm->localIndex=cinfo->methodsTable.size();
            dm->cmethod=dtor;
            cinfo->methodsTable.push_back(dm);
            cinfo->specialMethods[csmDestructor]=dm->index;
            Value dval;
            dval.vt=vtCMethod;
            dval.cmethod=dm;
            dval.flags=0;
            globals[dm->index]=dval;
            cinfo->symMap.insert(dname,dm);*/
        }
        return cinfo;
    }

    bool isClass()
    {
        return currentScope->st == sytClass;
    }

    ClassMember* registerMember(Name name)
    {
        if(currentScope->st != sytClass)
        {
            ZTHROW(CGException, name.pos, "Invalid scope to class member registration");
        }
        ClassInfo* ci = (ClassInfo*) currentScope;
        SymInfo* ptr = ci->getSymbols()->findSymbol(name);
        if(!ptr)
        {
            ClassMember* m = new ClassMember(name);
            m->owningClass = currentClass;
            m->index = ci->membersCount++;
            ci->symMap.insert(name.val, m);
            ci->members.push_back(m);
            return m;
        } else
        {
            if(ptr->st != sytClassMember)
            {
                ZTHROW(CGException, name.pos, "registerMember failed, names conflict(%{})", name);
            }
            return (ClassMember*) ptr;
        }
    }

    ClassMember* getMember(Name name)
    {
        return (ClassMember*) currentClass->symMap.findSymbol(name);
    }

    MethodInfo* registerCMethod(ZStringRef name, ZorroCMethod func)
    {
        if(currentScope->st != sytClass)
        {
            ZTHROW0(ZorroException, "invalid scope for class cmethod registration:%{}", name);
        }
        //ClassInfo* ci=(ClassInfo*)currentScope;
        MethodInfo* m = new MethodInfo(name, currentClass, currentClass, this);
        m->cmethod = func;
        m->index = registerGlobalSymbol(getFullName(name), m);//ci->methods++;
        m->localIndex = currentClass->methodsTable.size();
        currentClass->methodsTable.push_back(m);
        Value val;
        val.vt = vtCMethod;
        val.cmethod = m;
        val.flags = 0;
        globals[m->index] = val;
        currentScope->getSymbols()->insert(name, m);
        return m;
    }

    MethodInfo* registerMethod(Name name, size_t argsCount, bool allowOverride = true)
    {
        if(!currentClass)
        {
            ZTHROW(CGException, name.pos, "Invalid scope for class method '%{}' registration", name);
        }
        //ClassInfo* ci=(ClassInfo*)currentScope;
        SymInfo* psym = currentClass->symMap.findSymbol(name);
        if(psym && psym->st != sytMethod)
        {
            ZTHROW(CGException, name.pos, "Cannot override non-method %{} in class %{}", name, currentClass->name);
        }
        MethodInfo* m = new MethodInfo(name, currentScope, currentClass, this);
        if(psym && allowOverride)
        {
            MethodInfo* om = ((MethodInfo*) psym);
            m->localIndex = om->localIndex;
            currentClass->methodsTable[m->localIndex] = m;
            m->overIndex = currentClass->methodsTable.size();
            currentClass->methodsTable.push_back(om);
        } else
        {
            m->localIndex = currentClass->methodsTable.size();
            currentClass->methodsTable.push_back(m);
        }

        m->index = registerGlobalSymbol(getFullName(name), m);//ci->methods++;
        m->argsCount = argsCount;
        Value val;
        val.vt = vtMethod;
        val.func = m;
        val.flags = 0;
        globals[m->index] = val;
        currentScope->getSymbols()->insert(name, m);
        currentScope = m;
        return m;
    }

    MethodInfo* enterMethod(Name name)
    {
        MethodInfo* rv = (MethodInfo*) currentScope->symMap.findSymbol(name);
        if(rv)
        {
            currentScope = rv;
        }
        return rv;
    }


    ClassPropertyInfo* registerProperty(Name name)
    {
        if(!currentClass)
        {
            ZTHROW(CGException, name.pos, "Invalid scope for class property '%{}' registration", name);
        }
        ClassPropertyInfo* p = new ClassPropertyInfo(name);
        currentScope->getSymbols()->insert(name, p);
        return p;
    }


    std::string getFullNameStr(Name name)
    {
        std::string fullName = name.val.c_str();
        SymInfo* ptr = currentScope;
        do
        {
            fullName = std::string(ptr->name.val.c_str()) + "." + fullName;
        } while((ptr = ptr->getParent()) != nullptr);
        return fullName;
    }

    ZStringRef getFullName(Name name)
    {
        std::string fullName = getFullNameStr(name);
        return mem->mkZString(fullName.c_str(), fullName.length());
    }

    LiterInfo* registerLiter(Name name, size_t argsCount)
    {
        LiterInfo* fi = new LiterInfo(name, currentScope, this);
        size_t index;
        if(currentScope->st != sytGlobalScope)
        {
            index = registerGlobalSymbol(getFullName(name), fi);
            currentScope->symMap.insert(name.val, fi);
        } else
        {
            index = registerGlobalSymbol(name, fi);
        }
        fi->argsCount = argsCount;
        Value val;
        val.vt = vtFunc;
        val.func = fi;
        val.flags = 0;
        //val.func->ref();
        globals[index] = val;
        currentScope = fi;
        return fi;
    }

    FuncInfo* registerFunc(Name name, size_t argsCount)
    {
        FuncInfo* fi = new FuncInfo(name, currentScope, this);
        size_t index;
        if(currentScope->st != sytGlobalScope)
        {
            index = registerGlobalSymbol(getFullName(name), fi);
            currentScope->symMap.insert(name.val, fi);
        } else
        {
            index = registerGlobalSymbol(name, fi);
        }
        fi->argsCount = argsCount;
        Value val;
        val.vt = vtFunc;
        val.func = fi;
        val.flags = 0;
        //val.func->ref();
        globals[index] = val;
        currentScope = fi;
        return fi;
    }

    FuncInfo* enterFunc(Name name)
    {
        ScopeSym* sym = (ScopeSym*) currentScope->symMap.findSymbol(name);
        if(!sym)
        {
            ZTHROW(CGException, name.pos, "failed to enter func %{}", name);
        }
        currentScope = sym;
        return (FuncInfo*) sym;
    }

    size_t registerCFunc(ZStringRef name, ZorroCFunc func)
    {
        FuncInfo* fi = new FuncInfo(name, currentScope, this);
        fi->cfunc = func;
        fi->tinfo = TypeInfo(fi);
        size_t index = registerGlobalSymbol(name, fi);
        Value val;
        val.vt = vtCFunc;
        val.cfunc = fi;
        val.flags = 0;
        globals[index] = val;
        return index;
    }


    size_t getIntConst(ZStringRef name)
    {
        SymInfo** it = global.ic.getPtr(name.get());
        if(!it)
        {
            size_t index = newGlobal();
            SymInfo* infoPtr = new SymInfo(name, sytConstant);
            infoPtr->index = index;
            info.push_back(infoPtr);
            global.ic.insert(name, infoPtr);
            globals[index] = IntValue(ZString::parseInt(name.c_str()), true);
            return index;
        } else
        {
            return (*it)->index;
        }
    }

    size_t getDoubleConst(ZStringRef name)
    {
        SymInfo** it = global.dc.getPtr(name.get());
        if(!it)
        {
            size_t index = newGlobal();
            SymInfo* infoPtr = new SymInfo(name, sytConstant);
            infoPtr->index = index;
            info.push_back(infoPtr);
            global.dc.insert(name, infoPtr);
            globals[index] = DoubleValue(ZString::parseDouble(name.c_str()), true);
            return index;
        } else
        {
            return (*it)->index;
        }
    }

    size_t getStringConst(ZStringRef name)
    {
        SymInfo** it = global.sc.getPtr(name.get());
        if(!it)
        {
            size_t index = newGlobal();
            SymInfo* infoPtr = new SymInfo(name, sytConstant);
            infoPtr->index = index;
            info.push_back(infoPtr);
            global.sc.insert(name, infoPtr);
            globals[index] = StringValue(name, true);
            globals[index].str->ref();
            return index;
        } else
        {
            //globals[(*it)->index].str->ref();
            return (*it)->index;
        }
    }

    ZStringRef getStringConstVal(const char* name)
    {
        SymInfo** it = global.sc.getPtr(name);
        if(!it)
        {
            size_t index = newGlobal();
            ZStringRef nm = mem->mkZString(name);
            SymInfo* infoPtr = new SymInfo(nm, sytConstant);
            infoPtr->index = index;
            info.push_back(infoPtr);
            global.sc.insert(nm, infoPtr);
            globals[index] = StringValue(nm, true);
            globals[index].str->ref();
            return nm;
        } else
        {
            return (*it)->name.val;
        }
    }

    size_t registerArray()
    {
        ZArray* arr = mem->allocZArray();
        arr->ref();
        char name[32];
        snprintf(name, sizeof(name), "array-%p", arr);
        ZStringRef nm = mem->mkZString(name);
        size_t index = registerGlobalSymbol(nm, new SymInfo(nm, sytConstant));
        globals[index] = ArrayValue(arr, true);
        return index;
    }

    size_t registerMap()
    {
        ZMap* map = mem->allocZMap();
        map->ref();
        char name[32];
        snprintf(name, sizeof(name), "map-%p", map);
        ZStringRef nm = mem->mkZString(name);
        size_t index = registerGlobalSymbol(nm, new SymInfo(nm, sytConstant));
        globals[index] = MapValue(map, true);
        return index;
    }

    size_t registerSet()
    {
        ZSet* set = mem->allocZSet();
        set->ref();
        char name[32];
        snprintf(name, sizeof(name), "set-%p", set);
        ZStringRef nm = mem->mkZString(name);
        size_t index = registerGlobalSymbol(nm, new SymInfo(nm, sytConstant));
        globals[index] = SetValue(set, true);
        return index;
    }

    size_t registerRegExp()
    {
        RegExpVal* re = mem->allocRegExp();
        re->ref();
        char name[32];
        snprintf(name, sizeof(name), "regexp-%p", re);
        ZStringRef nm = mem->mkZString(name);
        size_t index = registerGlobalSymbol(nm, new SymInfo(nm, sytConstant));
        Value val;
        val.vt = vtRegExp;
        val.flags = 0;
        val.regexp = re;
        globals[index] = val;
        return index;
    }

    size_t registerRange()
    {
        Range* r = mem->allocRange();
        r->ref();
        char name[32];
        snprintf(name, sizeof(name), "range-%p", r);
        ZStringRef nm = mem->mkZString(name);
        size_t index = registerGlobalSymbol(nm, new SymInfo(nm, sytConstant));
        globals[index] = RangeValue(r, true);
        return index;
    }

    bool enterNamespace(Name name)
    {
        if(currentScope->st != sytGlobalScope && currentScope->st != sytNamespace)
        {
            ZTHROW(CGException, name.pos, "Namespaces can only be used at global scope");
        }
        SymInfo* ptr = currentScope->getSymbols()->findSymbol(name);
        if(ptr)
        {
            SymInfo& ns = *ptr;
            if(ns.st != sytNamespace)
            {
                return false;
            }
            currentScope = (ScopeSym*) ptr;
            return true;
        }
        ScopeSym* ss = new NsInfo(name, currentScope, this);
        currentScope->getSymbols()->insert(name, ss);
        currentScope = ss;
        return true;
    }

    size_t registerAttr(const Name& name)
    {
        AttrSym* attr = new AttrSym(name);
        size_t index = newGlobal();
        attr->index = index;
        info.resize(index + 1);
        info[index] = attr;
        currentScope->attrMap.insert(name, attr);
        return index;
    }

    AttrSym* getAttr(Symbol sym)
    {
        ScopeSym* ptr;
        SymInfo* att;
        if(sym.ns)
        {
            ptr = &global;
            for(auto& nsVal: sym.ns->values)
            {
                ptr = (ScopeSym*) ptr->getSymbols()->findSymbol(nsVal);
                if(!ptr)
                {
                    return 0;
                }
            }
            return (AttrSym*) ptr->attrMap.findSymbol(sym.name);
        } else
        {
            ptr = currentScope;
            while(ptr)
            {
                att = (AttrSym*) ptr->attrMap.findSymbol(sym.name);
                if(att)
                {
                    return (AttrSym*) att;
                }
                ptr = (ScopeSym*) ptr->getParent();
            }
        }
        return 0;
    }

    void addAlias(SymInfo* sym)
    {
        currentScope->symMap.insert(sym->name, sym);
    }

    SymInfo* getSymbol(Symbol sym);

    LiterInfo* getLiter(ExprList* lst);

    SymInfo* getGlobalSymbol(Name name)
    {
        return global.getSymbols()->findSymbol(name);
    }

    void exportApi(OutputBuffer& ob);

    void importApi(InputBuffer& ib);

};


}


#endif
