#include "Symbolic.hpp"
#include "InputBuffer.hpp"
#include "OutputBuffer.hpp"

namespace zorro {

void SymMap::insert(const Name& nm, SymInfo* info)
{
    insert(nm.val, info);
}

void SymMap::insert(ZStringRef nm, SymInfo* info)
{
    SymInfo** sptr = getPtr(nm.get());
    if(sptr)
    {
        if((*sptr) && (*sptr)->unref())
        {
            delete *sptr;
        }
        *sptr = info;
    } else
    {
        ZHash<SymInfo*>::insert(nm.get(), info);
    }
    if(info)
    {
        info->ref();
    }
    DPRINT("insert: %s@%p[%d]\n", nm.c_str(), info, info ? info->refCount : 0);
}


void SymMap::clear(ZMemory* mem, SymInfo* /*parent*/)
{
    Iterator it(*this);
    ZString* str;
    SymInfo** valPtr;
    while(it.getNext(str, valPtr))
    {
        SymInfo* val = *valPtr;
        if(val)
        {
            if(val->refCount <= 0)
            {
                DPRINT("WTF@%p\n", val);
            }
            if(val->unref())
            {
                DPRINT("deleting st=%d, name=%s:%p\n", val->st, str->c_str(mem), val);
                delete val;
            } else
            {
                DPRINT("unref st=%d, name=%s:%p[%d]\n", val->st, str->c_str(mem), val, val->refCount);
            }
        }
        if(str->unref())
        {
            str->clear(mem);
            mem->freeZString(str);
        }
    }
    ZHash<SymInfo*>::clear();
}


SymInfo* SymbolsInfo::getSymbol(Symbol sym)
{
    if(sym.ns)
    {
        ScopeSym* arr[] = {currentScope, &global};
        for(int i = sym.global ? 1 : 0; i < 2; ++i)
        {
            ScopeSym* ptr = arr[i];
            bool first = true;
            auto nsIt = ptr->usedNs.begin();
            auto nsEnd = ptr->usedNs.end();
            do
            {
                for(auto& nsVal:sym.ns->values)
                {
                    ptr = (ScopeSym*) ptr->getSymbols()->findSymbol(nsVal);
                    if(!ptr)
                    {
                        break;
                    }
                    if(ptr->st != sytNamespace)
                    {
                        ptr = nullptr;
                        break;
                    }
                }
                if(ptr)
                {
                    return ptr->getSymbols()->findSymbol(sym.name);
                }
                if(first && nsIt != nsEnd)
                {
                    ptr = *nsIt;
                    first = false;
                } else
                {
                    ++nsIt;
                    if(nsIt != nsEnd)
                    {
                        ptr = *nsIt;
                    }
                }
            } while(nsIt != nsEnd);
        }
        return nullptr;
    } else
    {
        ScopeSym* ptr = currentScope;
        ScopeSym* curFunc = ptr->st == sytFunction || ptr->st == sytMethod ? ptr : nullptr;
        bool crossedClassBorder = false;
        while(ptr)
        {
            SymInfo* sptr = ptr->getSymbols()->findSymbol(sym.name);
            auto nsIt = ptr->usedNs.begin();
            for(;;)
            {
                if(sptr)
                {
                    if(crossedClassBorder && sptr->st != sytFunction && sptr->st != sytClass &&
                       sptr->st != sytGlobalVar && sptr->st != sytConstant)
                    {
                        break;
                    }
                    if(ptr != currentScope && (sptr->st == sytLocalVar || sptr->st == sytClosedVar ||
                                               (sptr->st == sytFunction && sptr->scope()->isClosure())))
                    {
                        while(ptr != currentScope)
                        {
                            OpArg src;
                            src.idx = sptr->index;
                            switch(sptr->st)
                            {
                                case sytLocalVar:
                                    src.at = atLocal;
                                    break;
                                case sytClosedVar:
                                    src.at = atClosed;
                                    break;
                                case sytFunction:
                                {
                                    index_type idx;
                                    if(!ptr->hasClosedFuncStorage(sptr->index, idx))
                                    {
                                        ScopeSym* save = currentScope;
                                        try
                                        {
                                            currentScope = ptr;
                                            idx = registerLocalVar(
                                                mem->mkZString(FORMAT("closure-storage:%{}", sptr->index)));
                                            currentScope = save;
                                            ptr->tmpChild->closedFuncs.push_back(
                                                ScopeSym::ClosedFuncInfo(sptr->index, idx));
                                        }
                                        catch(...)
                                        {
                                            currentScope = save;
                                            throw;
                                        }
                                    }
                                    src.at = atLocal;
                                    src.idx = idx;
                                }
                                    break;
                                default:
                                    abort();
                                    break;
                            }
                            ptr = ptr->tmpChild;
                            sptr = new SymInfo(sptr->name, sytClosedVar);
                            ptr->registerClosedSymbol(sptr, src);
                        }
                    } else if(currentClass && currentScope != currentClass && currentScope->parent != currentClass &&
                              (sptr->st == sytClassMember || sptr->st == sytMethod))
                    {
                        if(!currentScope->selfClosed)
                        {
                            currentScope->selfClosed = true;//!!TODO!! fix?
                            SymInfo* selfInfo = new SymInfo(mem->mkZString("self"), sytClosedVar);
                            size_t argsCount = ((FuncInfo*) curFunc)->argsCount;
                            OpArg src(atLocal, argsCount);
                            ptr->registerClosedSymbol(selfInfo, src);
                        }
                    } else if(currentClass && ptr != currentClass &&
                              (sptr->st == sytClassMember || sptr->st == sytMethod))
                    {
                        break;
                    }

                    return sptr;
                }
                if(nsIt != ptr->usedNs.end())
                {
                    sptr = (*nsIt)->symMap.findSymbol(sym.name);
                    ++nsIt;
                } else
                {
                    break;
                }
            }
            if(ptr->st != sytNamespace)
            {
                if(ptr->parent)
                {
                    ptr->parent->tmpChild = ptr;
                }
            }
            if(ptr->st == sytClass)
            {
                crossedClassBorder = true;
            }
            ptr = ptr->parent;
            if(ptr && !curFunc && (ptr->st == sytFunction || ptr->st == sytMethod))
            {
                curFunc = ptr;
            }
        }
    }
    return nullptr;
}

LiterInfo* SymbolsInfo::getLiter(ExprList* lst)
{
    ScopeSym* ptr = currentScope;
    LiterType lt;
    ZString* mrk;
    if(lst->values.front()->et == etVar)
    {
        lt = ltString;
        mrk = lst->values.front()->val.get();
    } else
    {
        if(lst->values.front()->et == etInt)
        {
            lt = ltInt;
        } else
        {
            lt = ltDouble;
        }
        mrk = lst->getSecond()->val.get();
    }
    while(ptr)
    {
        ScopeSym::LiterList** llptr = ptr->literMap.getPtr(mrk);
        if(llptr)
        {
            ScopeSym::LiterList& ll = **llptr;
            for(auto& it : ll)
            {
                auto eit = lst->values.begin();
                if(it->markers.front().lt == ltString)
                {
                    if(lt != ltString)
                    {
                        continue;
                    }
                    return it;
                }
                bool failed = false;
                for(auto& la : it->markers)
                {
                    if(eit == lst->values.end())
                    {
                        if(la.optional)
                        {
                            continue;
                        }
                        failed = true;
                        break;
                    }
                    if((*eit)->et == etInt && la.lt != ltInt && la.lt != ltNumber)
                    {
                        break;
                    }
                    if((*eit)->et == etDouble && la.lt != ltDouble && la.lt != ltNumber)
                    {
                        break;
                    }
                    ++eit;//skip num
                    if(eit == lst->values.end())
                    {
                        failed = la.marker.val;
                        break;
                    }
                    if(!la.marker.val || la.marker.val != (*eit)->val)
                    {
                        if(la.optional)
                        {
                            --eit;
                            continue;
                        } else
                        {
                            break;
                        }
                    }
                    if(eit != lst->values.end())
                    {
                        ++eit;
                    }//skip marker
                }
                if(!failed && eit == lst->values.end())
                {
                    return it;
                }
            }
        }
        ptr = ptr->parent;
    }
    return nullptr;
}

ScopeSym::ScopeSym(Name argName, SymbolType argSt, ScopeSym* argParent, SymbolsInfo* argSi) :
    SymInfo(std::move(argName), argSt), parent(argParent), si(argSi), mem(si->mem), triesEntered(0),
    selfClosed(false)
{
}

void GlobalScope::clear()
{
    symMap.clear(mem, this);
    sc.clear(mem, this);
    ic.clear(mem, this);
    dc.clear(mem, this);
}

void GlobalScope::clearGlobals()
{
    //for(Value* v=si->globals)
    /*
    ZString* str;
    SymInfo** val;
    SymMap::Iterator sit(sc);

    while(sit.getNext(str,val))
    {
      Value* v=&si->globals[(*val)->index];
      DPRINT("clear global str %d(%s)[%d]\n",(*val)->index,getValueTypeName(v->vt),ZISREFTYPE(v)?v->refBase->refCount:0);
      mem->unref(si->globals[(*val)->index]);
    }*/
    /*
    SymMap::Iterator it(symMap);
    DPRINT("symMap.count=%d\n",symMap.getCount());
    while(it.getNext(str,val))
    {
      if(!*val)continue;
      SymInfo& sym=**val;
      if(sym.st==sytFunction || sym.st==sytConstant || sym.st==sytClass || sym.st==sytMethod)
      {
        //Value* v=&si->globals[sym.index];
        DPRINT("clear global idx=%d, type=%s, name=%s, rc=%d\n",sym.index,getValueTypeName(v->vt),
            sym.name.val->c_str(),ZISREFTYPE(v)?v->refBase->refCount:0);
        //mem->unref(si->globals[sym.index]);
      }
    }*/
}

ClassInfo::~ClassInfo()
{
    DPRINT("delete ClassInfo %p\n", this);
}

TypeInfo::TypeInfo(ClassInfo* argClassInfo, ValueType argVt) : ts(tsDefined), vt(argVt), symRef(argClassInfo)
{
}

TypeInfo::TypeInfo(FuncInfo* argFuncInfo) : ts(tsDefined), vt(vtFunc), symRef(argFuncInfo)
{
}


ScopeSymSmartRef::ScopeSymSmartRef(ScopeSym* argRef) : ref(argRef)
{
    if(ref)
    {
        ref->refs.insert(this);
    }
}

ScopeSymSmartRef::ScopeSymSmartRef(const ScopeSymSmartRef& other) : ref(other.ref)
{
    if(ref)
    {
        ref->refs.insert(this);
    }
}

ScopeSymSmartRef& ScopeSymSmartRef::operator=(const ScopeSymSmartRef& other)
{
    if(ref)
    {
        ref->refs.erase(this);
    }
    ref = other.ref;
    if(ref)
    {
        ref->refs.insert(this);
    }
    return *this;
}

ScopeSymSmartRef::~ScopeSymSmartRef()
{
    if(ref)
    {
        ref->refs.erase(this);
    }
}

void ScopeSymSmartRef::reset(ScopeSym* argRef)
{
    ref = argRef;
    if(ref)
    {
        ref->refs.insert(this);
    }
}

static size_t exportNs(SymInfo* sym, OutputBuffer& ob, size_t depth = 0)
{
    if(!sym || sym->st != sytNamespace)
    {
        if(depth == 0)
        {
            ob.set8(0);
        }
        return depth - 1;
    }
    size_t realDepth = exportNs(sym->getParent(), ob, depth + 1);
    if(depth == 0)
    {
        ob.set8(static_cast<uint8_t>(realDepth + 1));
    }
    ob.setCString(sym->name.val.c_str());
    return realDepth;
}

void SymbolsInfo::exportApi(OutputBuffer& ob)
{
    size_t sz = info.size();
    for(size_t i = stdEnd; i < sz; ++i)
    {
        SymInfo* sym = info[i];
        if(!sym)
        {
            continue;
        }
        switch(sym->st)
        {
            case sytFunction:
            {
                FuncInfo& fi = *(FuncInfo*) sym;
                ob.set8(sytFunction);
                exportNs(fi.parent, ob);
                ob.setCString(fi.name.val.c_str());
                ob.set8(static_cast<uint8_t>(fi.rvtype.ts));
                if(fi.rvtype.ts == tsDefined)
                {
                    ob.set8(fi.rvtype.vt);
                    if(fi.rvtype.vt == vtObject || fi.rvtype.vt == vtNativeObject)
                    {
                        exportNs(fi.rvtype.symRef.asClass()->parent, ob);
                        ob.setCString(fi.rvtype.symRef.asClass()->name.val.c_str());
                    }
                }
            }
                break;
            case sytGlobalVar:
            {
                ob.set8(sytGlobalVar);
                exportNs(sym->getParent(), ob);
                ob.setCString(sym->name.val.c_str());
                Value& v = globals[sym->index];
                if(v.vt == vtNativeObject)
                {
                    exportNs(v.nobj->classInfo->getParent(), ob);
                    ob.setCString(v.nobj->classInfo->name.val.c_str());
                } else if(v.vt == vtObject)
                {
                    exportNs(v.obj->classInfo->getParent(), ob);
                    ob.setCString(v.obj->classInfo->name.val.c_str());
                } else
                {
                    ob.set8(0);
                    ob.setCString("");
                }
            }
                break;
            case sytConstant:
            {
                Value& v = globals[sym->index];
                ob.set8(sytConstant);
                exportNs(sym->getParent(), ob);
                ob.set8(v.vt);
                ob.setCString(sym->name.val.c_str());
                switch(v.vt)
                {
                    case vtInt:
                    {
                        ob.set64(static_cast<uint64_t>(v.iValue));
                    }
                        break;
                    case vtString:
                    {
                        ob.setCString(v.str->c_str(mem));
                    }
                        break;
                    default:
                        throw std::runtime_error("exportApi:unsupported const type");
                }
            }
                break;
            case sytClass:
            {
                ob.set8(sytClass);
                ClassInfo& ci = *(ClassInfo*) sym;
                exportNs(sym->getParent(), ob);
                ob.setCString(ci.name.val.c_str());
                size_t cpos = ob.getPos();
                ob.set32(0);
                uint32_t cnt = 0;
                for(auto m:ci.methodsTable)
                {
                    if(m->specialMethod)
                    {
                        continue;
                    }
                    ob.setCString(m->name.val.c_str());
                    ++cnt;
                }
                TempRewind(ob, cpos)->set32(cnt);
                ob.set32(ci.members.size());
                for(auto m:ci.members)
                {
                    ob.setCString(m->name.val.c_str());
                }
            }
                break;
            default:
                break;
        }
    }
}

static int importNs(SymbolsInfo& si, InputBuffer& ib)
{
    si.currentScope = &si.global;
    int depth = ib.get8();
    for(int i = 0; i < depth; ++i)
    {
        ZStringRef nm(si.mem, ib.getCString());
        si.enterNamespace(nm);
    }
    return depth;
}

static void leaveNs(SymbolsInfo& si, int depth)
{
    for(int i = 0; i < depth; ++i)
    {
        si.returnScope();
    }
}

void SymbolsInfo::importApi(InputBuffer& ib)
{
    while(ib.getPos() < ib.getLen())
    {
        uint8_t st = ib.get8();
        int d = importNs(*this, ib);
        switch(st)
        {
            case sytFunction:
            {
                ZStringRef nm(mem, ib.getCString());
                FuncInfo* fi = (FuncInfo*) info[registerCFunc(nm, nullptr)];
                auto ts = static_cast<TypeSpec>(ib.get8());
                if(ts == tsDefined)
                {
                    fi->rvtype.ts = tsDefined;
                    fi->rvtype.vt = (ValueType) ib.get8();
                    if(fi->rvtype.vt == vtObject || fi->rvtype.vt == vtNativeObject)
                    {
                        ScopeSym* ss = currentScope;
                        try
                        {
                            currentScope = &global;
                            importNs(*this, ib);
                            ZStringRef nms(mem, ib.getCString());
                            SymInfo* sym = currentScope->getSymbols()->findSymbol(nms);
                            if(sym && sym->st == sytClass)
                            {
                                fi->rvtype.symRef = (ScopeSym*) sym;
                            }
                            currentScope = ss;
                        }
                        catch(...)
                        {
                            currentScope = ss;
                            break;
                        }
                    }
                }
            }
                break;
            case sytClass:
            {
                ZStringRef nm(mem, ib.getCString());
                registerNativeClass(nm, nullptr, nullptr);
                uint32_t mcnt = ib.get32();
                for(uint32_t i = 0; i < mcnt; ++i)
                {
                    registerCMethod(mem->mkZString(ib.getCString()), nullptr);
                }
                mcnt = ib.get32();
                for(uint32_t i = 0; i < mcnt; ++i)
                {
                    registerMember(mem->mkZString(ib.getCString()));
                }
                returnScope();
            }
                break;
            case sytGlobalVar:
            {
                importNs(*this, ib);
                ZStringRef nm(mem, ib.getCString());
                SymInfo* si = new SymInfo(nm, sytGlobalVar);
                registerGlobalSymbol(nm, si);
                si->tinfo.ts = tsDefined;
                si->tinfo.vt = (ValueType) ib.get8();
                if(si->tinfo.vt == vtObject || si->tinfo.vt == vtNativeObject)
                {
                    importNs(*this, ib);
                    ZStringRef nms(mem, ib.getCString());
                    SymInfo* sym = currentScope->symMap.findSymbol(nms);
                    if(sym && sym->st == sytClass)
                    {
                        si->tinfo.symRef = (ScopeSym*) sym;
                    }
                }
            }
                break;
            case sytConstant:
            {
                switch(ib.get8())
                {
                    case vtInt:
                    {
                        SymInfo* sym = new SymInfo(mem->mkZString(ib.getCString()), sytConstant);
                        sym->tinfo = TypeInfo(vtInt);
                        size_t idx = registerScopedGlobal(sym);
                        globals[idx] = IntValue(static_cast<int64_t>(ib.get64()), true);
                    }
                        break;
                    case vtString:
                    {
                        SymInfo* sym = new SymInfo(mem->mkZString(ib.getCString()), sytConstant);
                        sym->tinfo = TypeInfo(vtString);
                        size_t idx = registerScopedGlobal(sym);
                        ZStringRef str = mem->mkZString(ib.getCString());
                        globals[idx] = StringValue(str, true);
                    }
                        break;
                    default:
                        throw std::runtime_error("importApi: invalid const value type encountered");
                }
            }
                break;
            default:
                throw std::runtime_error("importApi: invalid symbol type encountered");
        }
        leaveNs(*this, d);
    }
}

}

