#define __STDC_FORMAT_MACROS

#include "CodeGenerator.hpp"
#include "ZorroVM.hpp"
#include "ZVMOps.hpp"
#include "SynTree.hpp"
#include "Symbolic.hpp"
#include <kst/RegExp.hpp>
#include <stdexcept>
#include <stdlib.h>
#include <map>
#include <memory>

namespace zorro {

struct ScopeGuard {
    SymbolsInfo* si;

    ScopeGuard(SymbolsInfo* argSi) : si(argSi)
    {
    }

    ~ScopeGuard()
    {
        if(si)
        {
            si->returnScope();
        }
    }

    void release()
    {
        si->returnScope();
        si = nullptr;
    }
};

CodeGenerator::CodeGenerator(ZorroVM* argVm) :
    vm(argVm), si(&vm->symbols), initOps(FileLocation(), nullptr),
    currentClass(nullptr), classHasMemberInit(false), interruptedFlow(false)
{
    initOps.vm = vm;
    selfName = vm->mkZString("self");
    nil = OpArg(atGlobal, si->nilIdx);
}

SymInfo* CodeGenerator::isSpecial(Expr* expr)
{
    if(expr->et != etVar)
    {
        return nullptr;
    }
    SymInfo* info = si->getSymbol(expr->getSymbol());
    if(info && (info->st == sytMethod || info->st == sytProperty ||
                (info->st == sytFunction && info->scope()->isClosure())))
    {
        return info;
    } else
    {
        return nullptr;
    }
}

bool CodeGenerator::isSimple(Expr* expr)
{
    switch(expr->et)
    {
        case etPlus:
        case etSPlus:
        case etPreInc:
        case etPostInc:
        case etPreDec:
        case etPostDec:
        case etMinus:
        case etSMinus:
        case etMul:
        case etBitOr:
        case etBitAnd:
        case etSMul:
        case etDiv:
        case etSDiv:
        case etMod:
        case etSMod:
        case etAssign:
        case etLess:
        case etLessEq:
        case etGreaterEq:
        case etNotEqual:
        case etNot:
        case etCopy:
        case etIn:
        case etIs:
        case etGreater:
        case etEqual:
        case etMatch:
        case etOr:
        case etRef:
        case etWeakRef:
        case etCor:
        case etIndex:
        case etTernary:
        case etGetAttr:
        case etGetType:
        case etAnd:
            return false;
        case etNeg:
            if(expr->e1->et == etInt || expr->e1->et == etDouble)
            {
                return true;
            }
            return false;
        case etInt:
        case etDouble:
        case etString:
        case etNil:
        case etTrue:
        case etFalse:
            return true;
        case etNumArg:
            return true;
        case etRegExp:
            return true;
        case etVar:
        {
            if(isSpecial(expr))
            {
                return false;
            }
            return true;
        }
            break;
        case etCall:
        case etProp:
        case etPropOpt:
        case etKey:
        case etArray:
        case etMap:
        case etSet:
        case etPair:
        case etRange:
        case etFormat:
        case etCount:
        case etCombine:
        case etSeqOps:
        case etMapOp:
        case etGrepOp:
            //case etSumOp:
        case etLiteral:
        case etFunc:
            return false;
    }
    return false;
}


static bool isBoolOp(Expr* expr)
{
    switch(expr->et)
    {
        case etLess:
        case etGreater:
        case etEqual:
        case etLessEq:
        case etGreaterEq:
        case etNotEqual:
        case etNot:
        case etIn:
        case etIs:
        case etOr:
        case etAnd:
        case etMatch:
            return true;
        default:
            return false;
    }
}

/*
static bool isBinOp(Expr* expr)
{
  switch(expr->et)
  {
    case etPlus:
    case etPlusEq:
    case etMinus:
    case etMul:
    case etDiv:
    case etLess:
    case etGreater:
    case etEqual:
    case etIn:
    case etOr:
    case etAnd:
    case etIndex:
    case etProp:
      return true;
    default:
      return false;
  }
}
*/

bool CodeGenerator::isConstant(Expr* expr)
{
    if(expr->isDeepConst())
    {
        return true;
    }
    if(expr->et == etVar)
    {
        SymInfo* ptr = si->getSymbol(expr->getSymbol());
        if(ptr && (ptr->st == sytConstant || ptr->st == sytClass || ptr->st == sytFunction))
        {
            return true;
        }
        return false;
    }
    return false;
}

void CodeGenerator::getExprType(Expr* e, TypeInfo& info)
{
    switch(e->et)
    {
        case etPlus:
        case etMinus:
        case etMul:
        case etDiv:
        case etMod:
        {
            getExprType(e->e1, info);
            getExprType(e->e2, info);
        }
            break;
        case etSPlus:
        {
            bool found = false;
            if(e->e1->et == etVar)
            {
                SymInfo* sym = si->getSymbol(e->e1->getSymbol());
                if(sym)
                {
                    for(auto& it : sym->tinfo)
                    {
                        if(it.vt == vtArray || it.vt == vtSet)
                        {
                            TypeInfo tmp;
                            getExprType(e->e2, tmp);
                            it.addToArr(tmp);
                            break;
                        }
                    }
                    found = true;
                    info.merge(sym->tinfo);
                }
            } else if(e->e1->et == etProp || e->e1->et == etPropOpt)
            {
                TypeInfo otmp;
                getExprType(e->e1->e1, otmp);
                for(auto& it : otmp)
                {
                    if(it.ts == tsDefined && it.vt == vtObject && it.symRef)
                    {
                        SymInfo* sym = it.symRef.asClass()->getSymbols()->findSymbol(e->e1->e2->val);
                        if(sym)
                        {
                            for(auto& sit : sym->tinfo)
                            {
                                if(sit.vt == vtArray || sit.vt == vtSet)
                                {
                                    TypeInfo tmp;
                                    getExprType(e->e2, tmp);
                                    sit.addToArr(tmp);
                                    break;
                                }
                            }
                            found = true;
                            info.merge(sym->tinfo);
                        }
                    }
                }
            }
            if(!found)
            {
                getExprType(e->e1, info);
            }
        }
            break;
        case etSMinus:
        case etSMul:
        case etSDiv:
        case etSMod:
        case etPreInc:
        case etPostInc:
        case etPreDec:
        case etPostDec:
        case etNeg:
        case etCopy:
        case etBitOr:
        case etBitAnd:
        case etRef:
        case etWeakRef:
        case etCor:
        {
            getExprType(e->e1, info);
        }
            break;
        case etAssign:
        {
            getExprType(e->e2, info);
            if(info.ts == tsUnknown)
            {
                break;
            }
            if(e->e1->et == etVar)
            {
                SymInfo* sym = si->getSymbol(e->e1->getSymbol());
                if(sym)
                {
                    sym->tinfo.merge(info);
                }
            } else if(e->e1->et == etProp || e->e1->et == etPropOpt)
            {
                TypeInfo oti;
                getExprType(e->e1->e1, oti);
                if(oti.ts == tsUnknown)
                {
                    break;
                }
                for(auto& it : oti)
                {
                    if(it.ts != tsDefined || it.vt != vtObject || !it.symRef)
                    {
                        continue;
                    }
                    SymInfo* sym = it.symRef.asClass()->getSymbols()->findSymbol(e->e1->e2->val);
                    if(sym)
                    {
                        if(sym->st == sytProperty)
                        {
                            auto* cp = (ClassPropertyInfo*) sym;
                            if(cp->setMethod)
                            {
                                if(!cp->setMethod->locals.empty())
                                {
                                    cp->setMethod->locals[0]->tinfo.merge(info);
                                }
                            } else if(cp->setIdx != SymInfo::invalidIndexValue)
                            {
                                it.symRef.asClass()->members[cp->setIdx]->tinfo.merge(info);
                            }
                        } else
                        {
                            sym->tinfo.merge(info);
                        }
                    }
                }
            } else if(e->e1->et == etIndex)
            {
                TypeInfo ati;
                getExprType(e->e1->e1, ati);
                if(ati.ts == tsUnknown)
                {
                    break;
                }
                for(auto& it : ati)
                {
                    if(it.ts != tsDefined || it.vt != vtArray)
                    {
                        continue;
                    }
                    it.addToArr(info);
                }
            }
            //todo: other cases
        }
            break;
        case etLess:
        case etGreater:
        case etLessEq:
        case etGreaterEq:
        case etEqual:
        case etNotEqual:
        case etMatch:
        case etOr:
        case etAnd:
        case etNot:
        case etIn:
        case etTrue:
        case etFalse:
            info.merge(vtBool);
            break;
        case etIs:
        {
            if(e->e1->et == etVar && e->e2->et == etVar)
            {
                SymInfo* sym1 = si->getSymbol(e->e1->getSymbol());
                if(sym1)
                {
                    SymInfo* sym2 = si->getSymbol(e->e2->getSymbol());
                    if(sym2 && sym2->st == sytClass)
                    {
                        sym1->tinfo.merge(TypeInfo((ClassInfo*) sym2));
                    }
                }
            }
            info.merge(vtBool);
        }
            break;
        case etInt:
            info.merge(vtInt);
            break;
        case etDouble:
            info.merge(vtDouble);
            break;
        case etString:
            info.merge(vtString);
            break;
        case etPair:
            break;
        case etRange:
            info.merge(vtRange);
            break;
        case etNil:
            info.merge(vtNil);
            break;
        case etSet:
        case etArray:
        {
            TypeInfo etmp(e->et == etArray ? vtArray : vtSet);
            if(e->lst)
            {
                for(auto& it : *e->lst)
                {
                    TypeInfo tmp;
                    getExprType(it, tmp);
                    etmp.addToArr(tmp);
                }
            }
            info.merge(etmp);
        }
            break;
        case etMap:
        {
            TypeInfo etmp(vtMap);
            if(e->lst)
            {
                for(auto& it : *e->lst)
                {
                    TypeInfo tmp;
                    getExprType(it->e2, tmp);
                    etmp.addToArr(tmp);
                }
            }
            info.merge(etmp);
        }
            break;
        case etVar:
        {
            SymInfo* sym = si->getSymbol(e->getSymbol());
            if(sym)
            {
                info.merge(sym->tinfo);
            }
        }
            break;
        case etNumArg:
        {
            size_t index = static_cast<size_t>(ZString::parseInt(e->val.c_str() + 1));
            if(index > 0)
            {
                --index;
            }
            SymInfo* sym = index < si->currentScope->locals.size() ? si->currentScope->locals[index] : nullptr;
            if(sym)
            {
                info.merge(sym->tinfo);
            }
        }
            break;
        case etCall:
        {
            TypeInfo tmp;
            getExprType(e->e1, tmp);
            for(auto& it : tmp)
            {
                if(it.vt == vtFunc)
                {
                    if(e->lst)
                    {
                        size_t idx = 0;
                        for(auto& ait : *e->lst)
                        {
                            if(idx >= it.symRef.asFunc()->locals.size())
                            {
                                break;
                            }
                            getExprType(ait, it.symRef.asFunc()->locals[idx]->tinfo);
                            ++idx;
                        }
                    }
                    if(it.symRef.asFunc()->rvtype.ts == tsUnknown && it.symRef.asFunc()->def &&
                       !it.symRef.asFunc()->inTypeFill)
                    {
                        auto& fds = it.symRef.asFunc()->def->as<FuncDeclStatement>();
                        ScopeSym* saveScope = si->currentScope;
                        si->currentScope = it.symRef.asFunc();
                        it.symRef.asFunc()->inTypeFill = true;
                        fillTypes(fds.body);
                        it.symRef.asFunc()->inTypeFill = false;
                        si->currentScope = saveScope;
                        //fillTypes(it->funcInfo->def);
                    }
                    info.merge(it.symRef.asFunc()->rvtype);
                } else if(it.vt == vtClass)
                {
                    if(it.symRef.asClass())
                    {
                        info.merge(it.symRef.asClass());
                    }
                } else if(it.vt == vtObject)
                {
                    if(it.symRef.asClass())
                    {
                        size_t midx = it.symRef.asClass()->specialMethods[csmCall];
                        if(midx)
                        {
                            info.merge(si->globals[midx].func->rvtype);
                        }
                    }
                }
            }
        }
            break;
        case etProp:
        case etPropOpt:
        {
            TypeInfo tmp;
            getExprType(e->e1, tmp);
            for(auto& it : tmp)
            {
                if(it.vt == vtObject && it.symRef)
                {
                    if(e->e2->et == etString)
                    {
                        ClassInfo* ci = it.symRef.asClass();
                        SymInfo* sym = ci->getSymbols()->findSymbol(e->e2->val);
                        if(sym)
                        {
                            if(sym->st == sytProperty)
                            {
                                auto* cp = (ClassPropertyInfo*) sym;
                                if(cp->getMethod)
                                {
                                    info.merge(cp->getMethod->rvtype);
                                } else if(cp->getIdx != SymInfo::invalidIndexValue)
                                {
                                    info.merge(it.symRef.asClass()->members[cp->getIdx]->tinfo);
                                }
                            } else
                            {
                                info.merge(sym->tinfo);
                            }
                        } else
                        {
                            size_t midx = ci->specialMethods[csmGetProp];
                            if(midx)
                            {
                                MethodInfo* mi = si->globals[midx].method;
                                info.merge(mi->rvtype);
                            }
                        }
                    } else if(e->e2->et == etVar)
                    {
                        ClassInfo* ci = it.symRef.asClass();
                        for(auto& m:ci->members)
                        {
                            info.merge(m->tinfo);
                        }
                    }
                }
            }
        }
            break;
        case etKey:
        {
            TypeInfo tmp;
            getExprType(e->e1, tmp);
            if(tmp.ts == tsUnknown)
            {
                break;
            }
            for(auto& ti:tmp)
            {
                if(ti.vt == vtMap)
                {
                    info.merge(tmp.arr);
                } else if(ti.vt == vtObject && ti.symRef)
                {
                    size_t midx = ti.symRef.asClass()->specialMethods[csmGetKey];
                    if(midx)
                    {
                        info.merge(si->globals[midx].func->rvtype);
                    }
                }
            }
        }
            break;
        case etIndex:
        {
            TypeInfo tmp;
            getExprType(e->e1, tmp);
            if(tmp.ts == tsUnknown)
            {
                break;
            }
            for(auto ti:tmp)
            {
                if(ti.vt == vtArray)
                {
                    info.merge(tmp.arr);
                } else if(ti.vt == vtObject && ti.symRef)
                {
                    size_t midx = ti.symRef.asClass()->specialMethods[csmGetIndex];
                    if(midx)
                    {
                        info.merge(si->globals[midx].func->rvtype);
                    }
                }
            }
        }
            break;
        case etFunc:
        {
            char name[32];
            snprintf(name, sizeof(name), "anonymous-%p", e);
            Name nm(vm->mkZString(name), e->pos);
            SymInfo* sym = si->getSymbol(nm);
            if(sym && (sym->st == sytFunction || sym->st == sytMethod))
            {
                sym->tinfo = TypeInfo((FuncInfo*) sym);
                info.merge(sym->tinfo);
            }
            break;
        }
        case etCombine:
        case etFormat:
            info.merge(vtString);
            break;
        case etCount:
            info.merge(vtInt);
            break;
        case etTernary:
        {
            TypeInfo tmp1, tmp2;
            getExprType(e->e2, tmp1);
            getExprType(e->e3, tmp2);
            if(tmp1.isSame(tmp2))
            {
                info = tmp1;
            } else
            {
                if(tmp1.ts == tsUnknown)
                {
                    info = tmp2;
                } else if(tmp2.ts == tsUnknown)
                {
                    info = tmp1;
                } else
                {
                    info.ts = tsOneOf;
                    info.arr.push_back(tmp1);
                    info.arr.push_back(tmp2);
                }
            }
            break;
        }
        case etGetAttr:
            break;
        case etGetType:
        {
            info.merge(vtClass);
            TypeInfo tmp;
            getExprType(e->e1, tmp);
            if(tmp.ts == tsDefined)
            {
                if(tmp.vt == vtObject)
                {
                    info.symRef = tmp.symRef;
                }
            }
            break;
        }
        case etLiteral:
        case etSeqOps:
        case etMapOp:
        case etGrepOp:
            break;
            //case etSumOp:break;
        case etRegExp:
            info.merge(vtRegExp);
            break;
    }
}

void CodeGenerator::fillTypes(StmtList* sl)
{
    if(!sl)
    {
        return;
    }
    for(auto& it : *sl)
    {
        fillTypes(it);
    }
}

void CodeGenerator::fillTypes(Statement* stptr)
{
    if(!stptr)
    {
        return;
    }
    Statement& st = *stptr;
    switch(st.st)
    {
        case stNone:
            break;
        case stExpr:
        {
            auto& est = st.as<ExprStatement>();
            TypeInfo tmp;
            getExprType(est.expr, tmp);
            /*if(est.expr->et==etAssign)
        {
          Expr* e=est.expr;
          if(e->e1->et==etVar)
          {
            SymInfo* sym=si->getSymbol(e->e1->getSymbol());
            if(sym)
            {
              if(sym->tinfo.ts==tsUnknown)
              {
                getExprType(est.expr->e2,sym->tinfo);
              }else
              {
                TypeInfo tmp;
                getExprType(est.expr->e2,tmp);
                if(sym->tinfo.ts==tsOneOf)
                {
                  if(!sym->tinfo.haveInArr(tmp))
                  {
                    sym->tinfo.arr.push_back(tmp);
                  }
                }else if(!sym->tinfo.isSame(tmp))
                {
                  TypeInfo tmp2=sym->tinfo;
                  sym->tinfo.ts=tsOneOf;
                  sym->tinfo.arr.push_back(tmp2);
                  sym->tinfo.arr.push_back(tmp);
                }
              }
            }
          }else if(e->e1->et==etProp)
          {
            TypeInfo tmp;
            getExprType(e->e1->e1,tmp);
            if(tmp.ts==tsDefined && tmp.vt==vtObject)
            {
              SymInfo* sym=tmp.classInfo->getSymbols()->findSymbol(e->e1->e2->val);
              if(sym)
              {
                getExprType(e->e2,sym->tinfo);
              }
            }
          }
        }*/
        }
            break;
        case stListAssign:
        {
            auto& las = st.as<ListAssignStatement>();
            if(las.lst2->size() == 1)
            {
                for(auto it = las.lst1->begin(), end = las.lst1->end(); it != end; ++it)
                {
                    Expr aex(etAssign, *it, las.lst2->front());
                    TypeInfo tmp;
                    getExprType(&aex, tmp);
                    aex.e1 = nullptr;
                    aex.e2 = nullptr;
                }
            } else
            {
                for(auto it1 = las.lst1->begin(), end1 = las.lst1->end(), it2 = las.lst2->begin(), end2 = las.lst2->end();
                    it1 != end1 && it2 != end2; ++it1, ++it2)
                {
                    Expr aex(etAssign, *it1, *it2);
                    TypeInfo tmp;
                    getExprType(&aex, tmp);
                    aex.e1 = nullptr;
                    aex.e2 = nullptr;
                }
            }
        }
            break;
        case stFuncDecl:
        {
            auto& fds = st.as<FuncDeclStatement>();
            Name fname = fds.name;
            if(fds.isOnFunc)
            {
                fname = "on " + fname.val;
            }
            FuncInfo* fi = si->enterFunc(fname);
            if(si->currentClass)
            {
                SymInfo* sym = fi->getSymbols()->findSymbol(selfName);
                if(sym)
                {
                    sym->tinfo.merge(TypeInfo(si->currentClass));
                }
            }
            fi->inTypeFill = true;
            fi->tinfo.merge(fi);
            fillTypes(fds.body);
            fi->inTypeFill = false;
            si->returnScope();
        }
            break;
        case stIf:
        {
            auto& is = st.as<IfStatement>();
            TypeInfo tmp;
            getExprType(is.cond, tmp);
            fillTypes(is.thenList);
            fillTypes(is.elseList);
            fillTypes(is.elsifList);
        }
            break;
        case stWhile:
        {
            auto& ws = st.as<WhileStatement>();
            TypeInfo tmp;
            getExprType(ws.cond, tmp);
            fillTypes(ws.body);
        }
            break;
        case stYield:
        case stReturn:
        case stReturnIf:
        {
            auto& rs = st.as<ReturnStatement>();
            auto* fi = (FuncInfo*) si->currentScope;
            if(rs.expr)
            {
                getExprType(rs.expr, fi->rvtype);
            } else
            {
                fi->rvtype.merge(vtNil);
            }
        }
            break;
        case stForLoop:
        {
            auto& fst = st.as<ForLoopStatement>();
            SymInfo* sym = si->getSymbol(fst.vars->values.back());
            TypeInfo tmp;
            getExprType(fst.expr, tmp);
            if(sym)
            {
                for(TypeInfo::iterator it = tmp.begin(), end = tmp.end(); it != end; ++it)
                {
                    if(it->isContainer())
                    {
                        sym->tinfo.merge(tmp.arr);
                    } else if(it->vt == vtRange)
                    {
                        sym->tinfo.merge(vtInt);
                    } else if(it->vt == vtFunc)
                    {
                        sym->tinfo.merge(it->symRef.asFunc()->rvtype);
                    }
                }
            }
            if(fst.vars->values.size() == 2)
            {
                SymInfo* sym1 = si->getSymbol(fst.vars->values.front());
                if(sym1 && tmp.ts == tsDefined)
                {
                    if(tmp.vt == vtArray)
                    {
                        sym1->tinfo.merge(vtInt);
                    }
                }
            }
            fillTypes(fst.body);
        }
            break;
        case stClass:
        {
            auto& cds = st.as<ClassDefStatement>();
            ClassInfo* ci = si->enterClass(cds.name);
            if(!ci)
            {
                ci = si->registerClass(cds.name);
            }
            ci->tinfo.ts = tsDefined;
            ci->tinfo.vt = vtClass;
            ci->tinfo.symRef = ci;
            fillTypes(cds.body);
            si->returnScope();
            //todo args
        }
            break;
        case stClassMemberDef:
        {
            auto& cmd = st.as<ClassMemberDef>();
            if(cmd.value)
            {
                SymInfo* sym = si->currentClass->symMap.findSymbol(cmd.name.val);
                if(!sym)
                {
                    sym = si->registerMember(cmd.name);
                }
                getExprType(cmd.value, sym->tinfo);
            }
        }
            break;
        case stClassAttrDef:
        {
            //todo
        }
            break;
        case stVarList:
        {

        }
            break;
        case stNamespace:
        {
            auto& ns = st.as<NamespaceStatement>();
            int cnt = 0;
            if(ns.ns)
            {
                for(auto& nsVal:ns.ns->values)
                {
                    si->enterNamespace(nsVal);
                    ++cnt;
                }
            } else
            {
                si->enterNamespace(ns.name);
                ++cnt;
            }
            fillTypes(ns.body);
            for(int i = 0; i < cnt; ++i)
            {
                si->returnScope();
            }
        }
            break;
        case stTryCatch:
        {
            auto& tcs = st.as<TryCatchStatement>();
            fillTypes(tcs.tryBody);
            if(tcs.exList)
            {
                SymInfo* sym = si->getSymbol(tcs.var);
                if(sym)
                {
                    for(auto& it : *tcs.exList)
                    {
                        getExprType(it, sym->tinfo);
                    }
                }
            }
        }
            break;
        case stThrow:
        case stBreak:
        case stNext:
        case stRedo:
            break;
        case stSwitch:
        {
            auto& ss = st.as<SwitchStatement>();
            if(ss.casesList)
            {
                for(auto& it : *ss.casesList)
                {
                    fillTypes(it.caseBody);
                }
            }
        }
            break;
        case stEnum:
        {
            auto& es = st.as<EnumStatement>();
            SymInfo* sym = si->getSymbol(es.name);
            if(sym)
            {
                sym->tinfo.merge(vtMap);
                sym->tinfo.addToArr(vtString);
            }
            if(es.items)
            {
                for(auto& item : *es.items)
                {
                    Expr& e = *item;
                    if(e.et == etVar)
                    {
                        SymInfo* esym = si->getSymbol(e.getSymbol());
                        if(esym)
                        {
                            esym->tinfo.merge(vtInt);
                        }
                    } else if(e.et == etAssign)
                    {
                        SymInfo* esym = si->getSymbol(e.e1->getSymbol());
                        if(esym)
                        {
                            getExprType(e.e2, esym->tinfo);
                        }
                    }
                }
            }
        }
            break;
        case stProp:
        {
            auto& ps = st.as<PropStatement>();
            if(ps.getFunc)
            {
                si->enterMethod(ps.getFunc->name);
                fillTypes(ps.getFunc->body);
                si->returnScope();
            }
            if(ps.setFunc)
            {
                si->enterMethod(ps.setFunc->name);
                fillTypes(ps.setFunc->body);
                si->returnScope();
            }
        }
            break;
        case stAttr:
        {

        }
            break;
        case stUse:
        {

        }
            break;
        case stLiter:
        {

        }
            break;
    }
}

void CodeGenerator::generate(StmtList* sl)
{
#ifdef DEBUG
    std::string str;
    if(sl)
    {
        sl->dump(str);
    }
    DPRINT("dump:\n%s", str.c_str());
#endif
    fillNames(sl);
    OpPair op = generateStmtList(sl);
    op += nullptr;
    if(initOps.first.get())
    {
        OpPair tmp = op;
        op = initOps;
        op += tmp;
    }
    /*
  if(!op.skipNext)
  {
    if(op.last)op.last->next=0;
  }else
  {
    op.skipNext=false;
  }
  for(FixVector::iterator it=op.efixes.begin(),end=op.efixes.end();it!=end;++it)
  {
    **it=0;
  }*/
    vm->setEntry(op.first.release());
}

void CodeGenerator::generateMacro(const Name& name, FuncParamList* args, StmtList* sl)
{
    si->enterNamespace(vm->mkZString("MACRO"));
    nil = OpArg(atGlobal, si->nilIdx);
    selfName = vm->mkZString("self");
    FuncInfo* fi = si->registerFunc(name, args ? args->size() : 0);
    ScopeGuard sg(si);
    OpPair fp(FileLocation(), vm);
    if(args)
    {
        for(auto& arg : *args)
        {
            si->registerArg(*arg);
        }
    }
    fp += generateStmtList(sl);
    if(!fp.isLastReturn())
    {
        fp += new OpReturn();
    }
    fi->entry = fp.first.release();
    si->returnScope();
}

void CodeGenerator::checkDuplicate(const Name& nm, const char* type)
{
    SymInfo* sym = si->currentScope->getSymbols()->findSymbol(nm);
    if(sym)
    {
        ZTHROW(CGException, nm.pos, "Duplicate %{} %{}, previously defined at %{}", type, nm, sym->name.pos);
    }
}

void CodeGenerator::fillNames(StmtList* sl, bool deep)
{
    if(!sl)
    {
        return;
    }
    for(auto& it : *sl)
    {
        Statement& st = *it;
        switch(st.st)
        {
            case stExpr:
            {
                auto& est = st.as<ExprStatement>();
                if(est.expr->et == etAssign && est.expr->e1->et == etVar)
                {
                    si->registerScopedGlobal(new SymInfo(est.expr->e1->getSymbol().name, sytGlobalVar));
                }
            }
                break;
            case stFuncDecl:
            {
                auto& f = st.as<FuncDeclStatement>();
                checkDuplicate(f.name, "func");
                FuncInfo* fi = si->registerFunc(f.name, f.argsCount());
                fi->tinfo = TypeInfo(fi);
                fi->def = &st;
                fi->end = f.end;
                if(deep)
                {
                    fillNames(f.body, deep);
                }
                si->returnScope();
            }
                break;
            case stNamespace:
            {
                auto& ns = st.as<NamespaceStatement>();
                ScopeSym* nsPtr = nullptr;
                if(ns.ns)
                {
                    for(auto& nsItem : ns.ns->values)
                    {
                        if(!si->enterNamespace(nsItem))
                        {
                            throw CGException("not a namespace", ns.pos);
                        }
                        si->currentScope->end = ns.end;
                    }
                } else
                {
                    char nsName[64];
                    int l = snprintf(nsName, sizeof(nsName), "anonymous-ns-%p", &ns);
                    ns.name = Name(vm->mkZString(nsName, static_cast<size_t>(l)), ns.pos);
                    si->enterNamespace(ns.name);
                    nsPtr = si->currentScope;
                    si->currentScope->end = ns.end;
                }
                fillNames(ns.body, deep);
                if(ns.ns)
                {
                    for(auto& nsItem:ns.ns->values)
                    {
                        (void) nsItem;
                        si->returnScope();
                    }
                } else
                {
                    si->returnScope();
                    si->currentScope->usedNs.push_back(nsPtr);
                }
            }
                break;
            case stClass:
            {
                auto& c = st.as<ClassDefStatement>();
                checkDuplicate(c.name, "class");
                //currentClass=&c;
                /*if(c.parent)
        {
          SymInfo* sym=si->getSymbol(c.parent->name);
          if(!sym)
          {
            throw UndefinedSymbolException(c.parent->name);
          }
          if(sym->st!=sytClass)
          {
            ZTHROW(CGException,c.parent->name.name.pos,"Symbol %{} is not class",c.parent->name.toString());
          }
        }*/
                ClassInfo* ci = si->registerClass(c.name);
                ci->tinfo = TypeInfo(ci, vtClass);
                ci->def = &st;
                ci->end = c.end;
                if(deep)
                {
                    fillNames(c.body, deep);
                }
                si->returnScope();
                //currentClass=0;
            }
                break;
            case stAttr:
            {
                auto& a = st.as<AttrDeclStatement>();
                checkDuplicate(a.name, "attr");
                si->registerAttr(a.name);
            }
                break;
            case stLiter:
            {
                auto& lst = st.as<LiterStatement>();
                checkDuplicate(lst.name, "liter");
                ScopeSym* curScope = si->currentScope;
                LiterInfo* li = si->registerLiter(lst.name, lst.args->size());
                for(auto& arg:*lst.args)
                {
                    ScopeSym::LiterList* curLst;
                    ScopeSym::LiterList** ll = curScope->literMap.getPtr(arg->marker.val.get());
                    if(!ll)
                    {
                        curLst = new ScopeSym::LiterList();
                        curScope->literMap.insert(arg->marker.val.get(), curLst);
                    } else
                    {
                        curLst = *ll;
                    }
                    curLst->push_back(li);
                    if(!arg->optional)
                    {
                        break;
                    }
                }
                for(auto& arg:*lst.args)
                {
                    li->markers.push_back(*arg);
                    si->registerArg(arg->name);
                }

                si->returnScope();
            }
                break;
            case stClassMemberDef:
            {
                auto& cmd = st.as<ClassMemberDef>();
                si->registerMember(cmd.name);
            }
                break;
            case stEnum:
            {
                auto& est = st.as<EnumStatement>();
                si->registerScopedGlobal(new SymInfo(est.name, sytConstant));
                if(est.items)
                {
                    for(auto& exprPtr:*est.items)
                    {
                        Expr& ex = *exprPtr;
                        Name nm = ex.et == etVar ? ex.getSymbol().name : ex.e1->getSymbol().name;
                        si->registerScopedGlobal(new SymInfo(nm, sytConstant));
                    }
                }
            }
                break;
            default:
                break;
        }
    }
}

void CodeGenerator::fillClassNames(StmtList* sl)
{
    if(!sl)
    {
        return;
    }
    for(auto& it : *sl)
    {
        Statement& st = *it;
        switch(st.st)
        {
            case stFuncDecl:
            {
                auto& f = st.as<FuncDeclStatement>();
                Name fname = f.name;
                bool isCtor = false;
                if(f.isOnFunc)
                {
                    ClassSpecialMethod csm = si->getCsm(f.name.val);
                    isCtor = csm == csmConstructor;
                    fname = "on " + f.name.val;
                }
                size_t argsCount = f.argsCount();
                if(isCtor)
                {
                    argsCount = currentClass->args ? currentClass->args->size() : 0;
                }
                si->registerMethod(fname, argsCount, !isCtor)->end = f.end;
                si->returnScope();
            }
                break;
            case stClassMemberDef:
            {
                auto& m = st.as<ClassMemberDef>();
                si->registerMember(m.name);
            }
                break;
            case stProp:
            {
                auto& pst = st.as<PropStatement>();
                ClassPropertyInfo* p = si->registerProperty(pst.name);
                if(pst.getFunc)
                {
                    p->getMethod = si->registerMethod(pst.getFunc->name, 0);
                    si->returnScope();
                } else if(pst.getName.val)
                {
                    SymInfo* sym = si->getSymbol(pst.getName);
                    if(!sym)
                    {
                        throw UndefinedSymbolException(pst.getName);
                    }
                    if(sym->st == sytClassMember)
                    {
                        p->getIdx = sym->index;
                    } else if(sym->st == sytMethod)
                    {
                        p->getMethod = (MethodInfo*) sym;
                        if(p->getMethod->argsCount != 0)
                        {
                            throw CGException("Property get method shouldn't have arguments", pst.name.pos);
                        }
                    } else
                    {
                        ZTHROW(CGException, pst.name.pos, "Incompatible symbol for property getter role: %{}",
                               pst.name);
                    }
                }
                if(pst.setFunc)
                {
                    p->setMethod = si->registerMethod(pst.setFunc->name, 1);
                    si->returnScope();
                } else if(pst.setName.val)
                {
                    SymInfo* sym = si->getSymbol(pst.setName);
                    if(!sym)
                    {
                        throw UndefinedSymbolException(pst.name);
                    }
                    if(sym->st == sytClassMember)
                    {
                        p->setIdx = sym->index;
                    } else if(sym->st == sytMethod)
                    {
                        p->setMethod = (MethodInfo*) sym;
                        if(p->setMethod->argsCount != 1)
                        {
                            throw CGException("Property set method must have exactly 1 argument", pst.name.pos);
                        }
                    } else
                    {
                        ZTHROW(CGException, pst.name.pos, "Incompatible symbol for property setter role:%{}", pst.name);
                    }
                }
            }
                break;
            default:
                break;
        }
    }
}

CodeGenerator::OpPair CodeGenerator::generateStmtList(StmtList* sl)
{
    OpPair op(FileLocation(), vm);
    if(sl == nullptr)
    {
        return op;
    }
    for(auto& it : *sl)
    {
        if(interruptedFlow)
        {
            warnings.push_back(CGWarning("not reachable", it->pos));
            break;
        }
        generateStmt(op, *it);
    }
    interruptedFlow = false;
    return op;
}


bool CodeGenerator::isIntConst(Expr* ex, int64_t& value)
{
    if(!ex)
    {
        return false;
    }
    if(ex->et == etInt)
    {
        value = ZString::parseInt(ex->val.c_str());
        return true;
    }
    if(ex->et == etNeg && ex->e1->et == etInt)
    {
        value = -ZString::parseInt(ex->val.c_str());;
        return true;
    }
    if(ex->et == etVar)
    {
        SymInfo* ptr = si->getSymbol(ex->getSymbol());
        if(!ptr)
        {
            return false;
        }
        if(ptr->st == sytConstant && si->globals[ptr->index].vt == vtInt)
        {
            value = si->globals[ptr->index].iValue;
            return true;
        }
    }
    return false;
}

bool CodeGenerator::isIntRange(Expr* ex, int64_t& start, int64_t& end, int64_t& step)
{
    if(ex->et != etRange)
    {
        return false;
    }
    step = 1;
    if(!isIntConst(ex->e1, start))
    {
        return false;
    }
    if(!isIntConst(ex->e2, end))
    {
        return false;
    }
    if(ex->e3 && !isIntConst(ex->e3, step))
    {
        return false;
    }
    if(!ex->rangeIncl)
    {
        --end;
    }
    return true;
}

struct ZHashCasesGuard {
    ZHash<OpBase*>* hash;
    ZorroVM* vm;

    ZHashCasesGuard(ZHash<OpBase*>* argHash, ZorroVM* argVm) : hash(argHash), vm(argVm)
    {
    }

    ~ZHashCasesGuard()
    {
        if(hash)
        {
            ZHash<OpBase*>::Iterator it(*hash);
            ZString* str;
            OpBase** op;
            while(it.getNext(str, op))
            {
                if(str->unref())
                {
                    str->clear(vm);
                    vm->freeZString(str);
                }
            }
            delete hash;
        }
    }

    ZHash<OpBase*>* operator->()
    {
        return hash;
    }

    ZHash<OpBase*>* release()
    {
        ZHash<OpBase*>* rv = hash;
        hash = nullptr;
        return rv;
    }
};


void CodeGenerator::generateStmt(OpPair& op, Statement& st)
{
    switch(st.st)
    {
        case stNone:
            return;
        case stExpr:
        {
            ExprContext ec(si);
            size_t tcnt = si->global.acquiredTemporals.size();
            op += generateExpr(st.as<ExprStatement>().expr, ec);
            if(si->global.acquiredTemporals.size() != tcnt)
            {
                DPRINT("unreleased temporals after expr at %s\n", st.pos.backTrace().c_str());
            }
        }
            break;
        case stListAssign:
        {
            auto& las = st.as<ListAssignStatement>();
            bool oneOnRight = las.lst2->size() == 1;
            if(las.lst1->size() != las.lst2->size() && !oneOnRight)
            {
                throw CGException("List assign sizes do not match", st.pos);
            }

            ExprContext ec(si);
            for(auto& it : *las.lst2)
            {
                ec.mkTmpDst();
                op += generateExpr(it, ec);
            }
            size_t idx = 0;
            for(auto& it : *las.lst1)
            {
                OpArg src;
                if(oneOnRight)
                {
                    src = OpArg(atLocal, ec.tmps.front()).tmp();
                } else
                {
                    src = OpArg(atLocal, ec.tmps[idx++]).tmp();
                }
                if(isSimple(it))
                {
                    op += OpPair(st.pos, vm, new OpAssign(getArgType(it, true), src, atNul));
                } else
                {
                    Expr* e1 = it;
                    if(e1->et == etIndex || e1->et == etProp || e1->et == etKey)
                    {
                        Expr* earr = e1->e1;
                        Expr* eidx = e1->e2;
                        ExprContext ec2(si);
                        OpArg arr = genArgExpr(op, earr, ec2);
                        OpArg aidx = genArgExpr(op, eidx, ec2);
                        switch(e1->et)
                        {
                            case etIndex:
                                op += new OpSetArrayItem(arr, aidx, src, atNul);
                                break;
                            case etProp:
                                op += new OpSetProp(arr, aidx, src, atNul);
                                break;
                            case etKey:
                                op += new OpSetKey(arr, aidx, src, atNul);
                                break;
                            default:
                                break;
                        }
                    } else
                    {
                        ExprContext ec2(si);
                        OpArg dst = ec2.mkTmpDst();
                        op += generateExpr(it, ec2.setLvalue());
                        op += OpPair(st.pos, vm, new OpAssign(dst.tmp(), src, atNul));
                    }
                }
            }
        }
            break;
        case stFuncDecl:
        {
            auto& func = st.as<FuncDeclStatement>();
            bool isCtor = false;
            bool isDtor = false;
            bool isClass = si->isClass();
            FuncParamList* args = func.args;
            FuncInfo* fi;
            ClassSpecialMethod csm = csmUnknown;
            if(isClass)
            {
                Name fname = func.name;
                if(func.isOnFunc)
                {
                    csm = si->getCsm(func.name);

                    if(csm == csmConstructor)
                    {
                        if(args)
                        {
                            throw CGException("on create event handler cannot have arguments", st.pos);
                        }
                        isCtor = true;
                        args = currentClass->args;
                        DPRINT("number of args of constructor:%d\n", (int) (args ? args->size() : 0));
                    }
                    if(csm == csmDestructor)
                    {
                        if(args)
                        {
                            throw CGException("on destroy event handler cannot have arguments", st.pos);
                        }
                        isDtor = true;
                        fname = vm->mkZString("on destroy");
                    }
                    fname = "on " + func.name.val;
                }
                fi = si->enterMethod(fname);
                //mi=si->globals[fidx].method;
                if(csm != csmUnknown)
                {
                    si->currentClass->specialMethods[csm] = fi->index;
                }
            } else
            {
                //if(si->currentScope->st==sytGlobalScope)
                //{
                if(si->currentScope->st != sytFunction && si->currentScope->st != sytMethod)
                {
                    fi = si->enterFunc(func.name);//,func.args?func.args->size():0);
                } else
                {
                    fi = si->registerFunc(func.name, func.argsCount());
                }
                //}else
                //{
                /*if(si->currentClass)
          {
            fi=si->registerMethod(func.name,func.argsCount());
          }else
          {*/
                //fi=si->registerFunc(func.name,func.argsCount());
                //}
                //}

            }
            ScopeGuard sg(si);
            OpPair fp(st.pos, vm);
            index_type selfIdx = 0;
            OpPair argOps = genArgs(fp, args, isCtor, selfIdx);

            if(isCtor)
            {
                if(currentClass->parent && si->currentClass->parentClass.asClass()->specialMethods[csmConstructor])
                {
                    ClassParent* p = currentClass->parent;
                    index_type acnt = 0;
                    if(p->args)
                    {
                        for(auto& arg:*p->args)
                        {
                            ExprContext ec(si, atStack);
                            fp += generateExpr(arg, ec);
                            ++acnt;
                        }
                    }
                    ClassInfo* pi = si->currentClass->parentClass.asClass();
                    fp += new OpCallMethod(acnt, OpArg(atLocal, selfIdx),
                                           si->globals[pi->specialMethods[csmConstructor]].method->localIndex, atNul);
                }
                ClassInfo::HiddenArgsList& halst = ((ClassInfo*) si->currentScope->getParent())->hiddenArgs;
                for(auto& arg:halst)
                {
                    fp += OpPair(arg.name.pos, vm,
                                 new OpAssign(OpArg(atMember, arg.memIdx), OpArg(atLocal, arg.argIdx), OpArg()));
                }
                for(auto& cstptr:*currentClass->body)
                {
                    auto& cst = *cstptr;
                    if(cst.st == stClassMemberDef)
                    {
                        auto& cmd = cst.as<ClassMemberDef>();
                        if(cmd.value)
                        {
                            ExprContext ec(si);
                            SymInfo* sym = si->getSymbol(cmd.name);
                            if(!sym)
                            {
                                throw UndefinedSymbolException(cmd.name);
                            }
                            if(sym->st != sytClassMember)
                            {
                                ZTHROW(CGException, cmd.pos, "%{} is not class data member", cmd.name);
                            }
                            ec.dst = OpArg(atMember, sym->index);
                            fp += generateExpr(cmd.value, ec);
                            //fp+=new OpAssign(OpArg(atMember,sym->index),
                            /*ExprStatement st(new Expr(etAssign,new Expr(etVar,cmd.name),cmd.value));
              try{
                generateStmt(fp,st);
              }catch(...)
              {
                st.expr->e2=0;
                throw;
              }
              st.expr->e2=0;*/
                        }
                    }
                }
            }

            try
            {
                fp += generateStmtList(func.body);
            }
            catch(...)
            {
                fi->defValEntries.clear();
                throw;
            }


            if(!fp.last || fp.last->ot != otReturn || !fp.efixes.empty())
            {
                bool genReturn = true;
                if(isDtor)
                {
                    OpBase* initOp = new OpInitDtor(si->currentScope->symMap.getCount());
                    initOp->next = *fp.first;
                    fp.first.get()->code = initOp;
                    if(currentClass->parent)
                    {
                        ClassInfo* parent = (ClassInfo*) si->getSymbol(currentClass->parent->name);
                        size_t pdtor = parent->specialMethods[csmDestructor];
                        if(pdtor)
                        {
                            DPRINT("parent of class %s has dtor\n", currentClass->name.val.c_str());
                            FuncInfo* f = si->globals[pdtor].func;
                            OpBase* jmp = new OpJump(f->entry->next, f->localsCount);
                            ((MethodInfo*) fi)->lastOp = jmp;
                            fp += jmp;
                            genReturn = false;
                        } else
                        {
                            DPRINT("parent of class %s deosn't have dtor\n", parent->name.val.c_str());
                            fp += new OpFinalDestroy;
                        }
                    } else
                    {
                        DPRINT("class %s has no parent\n", currentClass->name.val.c_str());
                        fp += new OpFinalDestroy;
                    }
                }
                if(genReturn)
                {
                    OpBase* ret = isCtor ? new OpReturn(OpArg(atLocal, selfIdx)) : new OpReturn();
                    ret->pos = fp.last ? fp.last->pos : st.pos;
                    fp += ret;
                }
            }

            if(fi->isClosure())
            {
                si->registerLocalVar(vm->mkZString("closure-storage"));
            }

            finArgs(fp, argOps);
            //fi->varArgEntry=fi->entry;
            //si->globals[fidx].func->entry->ref();
        }
            break;
        case stIf:
        {
            auto& ifst = st.as<IfStatement>();
            ExprContext::JVector jmp;
            {
                ExprContext ec(si);
                op += generateExpr(ifst.cond, ec.setIf());
                jmp.swap(ec.jumps);
            }
            op += generateStmtList(ifst.thenList);
            if(ifst.elsifList)
            {
                for(auto& eit : *ifst.elsifList)
                {
                    IfStatement& elsif = (*eit).as<IfStatement>();
                    ExprContext ec(si);
                    OpArg dst = ec.mkTmpDst();
                    OpPair elsifOp(elsif.cond->pos, vm);
                    elsifOp += generateExpr(elsif.cond, ec);
                    dst.tmp();
                    ec.release();
                    ExprContext::JVector oldjmp;
                    oldjmp.swap(jmp);
                    jmp.push_back(new OpCondJump(dst, nullptr));
                    elsifOp += jmp.back();
                    elsifOp += generateStmtList(elsif.thenList);
                    op.chainFixes(elsifOp);
                    for(auto& it : oldjmp)
                    {
                        if(!it->elseOp)
                        {
                            it->elseOp = *elsifOp.first;
                        }
                    }
                    elsifOp.release();
                }
            }
            if(ifst.elseList)
            {
                OpPair elseOp = generateStmtList(ifst.elseList);
                for(auto& it : jmp)
                {
                    if(!it->elseOp)
                    {
                        it->elseOp = *elseOp.first;
                    }
                }
                elseOp.release();
                op.chainFixes(elseOp);
            } else
            {
                for(auto& it : jmp)
                {
                    op.addFix(it->elseOp);
                }
            }
        }
            break;
        case stWhile:
        {
            auto& wst = st.as<WhileStatement>();
            ExprContext::JVector jmp;
            OpPair cond(wst.pos, vm);
            {
                ExprContext ec(si);
                cond = generateExpr(wst.cond, ec.setIf());
                jmp.swap(ec.jumps);
            }
            op += cond;
            si->enterBlock(wst.name);
            OpPair body = generateStmtList(wst.body);
            op += body;
            BlockInfo* blk = si->currentScope->currentBlock;

            for(auto& next : blk->nexts)
            {
                *next = *cond.first;
            }

            for(auto& redo : blk->redos)
            {
                *redo = *body.first;
            }
            op.fixLast(cond.first);
            //op.addFix(jmp->elseOp);
            for(auto& it : blk->breaks)
            {
                op.addFix(*it);
            }
            si->leaveBlock();
            for(auto& it : jmp)
            {
                op.addFix(it->elseOp);
            }
        }
            break;
        case stForLoop:
        {
            auto& fst = st.as<ForLoopStatement>();
            size_t vCnt = fst.vars->values.size();
            if(vCnt != 1 && vCnt != 2)
            {
                throw SyntaxErrorException("Invalid number of variables in for loop", fst.pos);
            }
            op.pos = st.pos;
            //op+=OpPair(st.pos,new OpPush(atGlobal,si->nilIdx));
            OpArg target;
            bool targetTemp = false;
            if(isSimple(fst.expr))
            {
                target = getArgType(fst.expr);
            } else
            {
                target = OpArg(atLocal, si->acquireTemp());
                ExprContext ec(si, target);
                op += generateExpr(fst.expr, ec.setConst());
                targetTemp = true;
            }
            Expr v1(etVar, fst.vars->values.front());
            OpArg var = getArgType(&v1, true);
            OpArg var2;
            if(vCnt == 2)
            {
                Expr v2(etVar);
                auto it = ++fst.vars->values.begin();
                v2.val = it->val;
                var2 = getArgType(&v2, true);
            }

            OpArg temp(atLocal, si->acquireTemp());
            OpForInit* forInit = vCnt == 1 ? new OpForInit(var, target, temp) : new OpForInit2(var, var2, target, temp);
            auto* forCor = new OpForCheckCoroutine(temp);
            forCor->pos = st.pos;
            forInit->corOp = forCor;
            op += forInit;
            OpForStep* forStep = vCnt == 1 ? new OpForStep(var, temp) : new OpForStep2(var, var2, temp);
            forStep->corOp = forCor;
            op += forStep;
            si->enterBlock(fst.name);
            OpPair body = generateStmtList(fst.body);
            BlockInfo* blk = si->currentScope->currentBlock;
            if(*body.first)
            {
                forInit->next = *body.first;
                forCor->next = *body.first;
                op += body;
                op.fixLast(forStep);
            } else
            {
                forInit->next = forStep;
                forCor->next = forStep;
                forStep->next = forStep;
            }
            op.setLast(forCor->endOp = forInit->endOp = forStep->endOp = new OpAssign(temp, nil, atNul));
            for(auto& next : blk->nexts)
            {
                *next = forStep;
            }
            for(auto& it : blk->breaks)
            {
                *it = forInit->endOp;
            }
            for(auto& redo : blk->redos)
            {
                *redo = *body.first;
            }
            si->leaveBlock();
            si->releaseTemp(temp.idx);
            if(targetTemp)
            {
                op += new OpAssign(target, nil, atNul);
                si->releaseTemp(target.idx);
            }
        }
            break;
        case stNext:
        {
            auto& nst = st.as<NextStatement>();
            BlockInfo* blk = si->getBlock(nst.id);
            if(blk == nullptr)
            {
                throw CGException("next not in loop", st.pos);
            }
            for(int i = 0; i < blk->triesEntered; ++i)
            {
                op += new OpLeaveCatch();
            }
            //blk->nexts.insert(blk->nexts.end(),op.efixes.begin(),op.efixes.end());
            //op.efixes.clear();
            if(op.last && !op.skipNext)
            {
                blk->nexts.push_back(&op.last->next);
                op.skipNext = true;
            } else
            {
                op += new OpJump(nullptr, 0);
                op.last->pos = nst.pos;
                blk->nexts.push_back(&op.last->next);
                op.skipNext = true;
            }
            interruptedFlow = true;
        }
            break;
        case stRedo:
        {
            auto& rst = st.as<RedoStatement>();
            BlockInfo* blk = si->getBlock(rst.id);
            if(blk == nullptr)
            {
                throw CGException("redo not in loop", st.pos);
            }
            for(int i = 0; i < blk->triesEntered; ++i)
            {
                op += new OpLeaveCatch();
            }
            blk->redos.insert(blk->redos.end(), op.efixes.begin(), op.efixes.end());
            op.efixes.clear();
            if(op.last && !op.skipNext)
            {
                blk->redos.push_back(&op.last->next);
                op.skipNext = true;
            } else
            {
                op += new OpJump(nullptr, 0);
                op.last->pos = rst.pos;
                blk->redos.push_back(&op.last->next);
                op.skipNext = true;
            }
            interruptedFlow = true;
        }
            break;
        case stBreak:
        {
            auto& bst = st.as<BreakStatement>();
            BlockInfo* blk = si->getBlock(bst.id);
            if(blk == nullptr)
            {
                throw CGException("break not in loop", st.pos);
            }
            for(int i = 0; i < blk->triesEntered; ++i)
            {
                op += new OpLeaveCatch();
            }
            blk->breaks.insert(blk->breaks.end(), op.efixes.begin(), op.efixes.end());
            op.efixes.clear();
            if(op.last)
            {
                if(!op.skipNext)
                {
                    blk->breaks.push_back(&op.last->next);
                    op.skipNext = true;
                }
            } else
            {
                op += new OpJump(nullptr, 0);
                op.last->pos = bst.pos;
                blk->breaks.push_back(&op.last->next);
                op.skipNext = true;
            }
            interruptedFlow = true;
        }
            break;
        case stReturn:
        {
            auto& ret = st.as<ReturnStatement>();
            if(si->currentScope->st != sytFunction && si->currentScope->st != sytMethod)
            {
                throw CGException("Return not in function", st.pos);
            }
            //FuncInfo* fi=(FuncInfo*)si->currentScope;
            for(int i = 0; i < si->currentScope->triesEntered; ++i)
            {
                op += new OpLeaveCatch();
            }
            if(ret.expr)
            {
                /*if(!fi->canReturn)
        {
          throw CGException("This function cannot return value",st.pos);
        }*/
                ExprContext ec(si);
                OpArg res = genArgExpr(op, ret.expr, ec);
                //op+=generateExpr(ret.expr,ec);
                op += OpPair(st.pos, vm, new OpReturn(res, ret.expr->et == etRef));
            } else
            {
                op += OpPair(st.pos, vm, new OpReturn());
            }
            op.last->pos = ret.pos;
            interruptedFlow = true;
        }
            break;
        case stReturnIf:
        {
            auto& ret = st.as<ReturnIfStatement>();
            if(si->currentScope->st != sytFunction && si->currentScope->st != sytMethod)
            {
                throw CGException("Return not in function", st.pos);
            }
            //FuncInfo* fi=(FuncInfo*)si->currentScope;
            ExprContext ec(si);
            OpArg res = genArgExpr(op, ret.expr, ec);
            OpCondJump* jmp;
            op += jmp = new OpCondJump(ec.dst);
            jmp->pos = ret.expr->pos;
            for(int i = 0; i < si->currentScope->triesEntered; ++i)
            {
                op += new OpLeaveCatch();
            }
            op += OpPair(st.pos, vm, new OpReturn(res, ret.expr->et == etRef));
            op.last->pos = ret.pos;
            op.addFix(jmp->elseOp);
        }
            break;
        case stYield:
        {
            auto& yield = st.as<YieldStatement>();
            if(yield.expr)
            {
                /*ExprContext ec(si);
        ec.mkTmpDst();
        op+=generateExpr(yield.expr,ec);*/
                ExprContext ec(si);
                OpArg dst = genArgExpr(op, yield.expr, ec);
                op += OpPair(st.pos, vm, new OpYield(dst, yield.expr->et == etRef));
            } else
            {
                op += OpPair(st.pos, vm, new OpYield());
            }
        }
            break;
        case stClass:
        {
            auto& cds = st.as<ClassDefStatement>();
            if(cds.parent)
            {
                SymInfo* sym = si->getSymbol(cds.parent->name);
                if(!sym)
                {
                    ZTHROW(CGException, cds.parent->name.name.pos, "Class %{} not found class",
                           cds.parent->name.toString());
                }
                if(sym->st != sytClass)
                {
                    ZTHROW(CGException, cds.parent->name.name.pos, "Symbol %{} is not class",
                           cds.parent->name.toString());
                }
                auto* pc = (ClassInfo*) sym;
                if(!pc->parentClass && pc->def)
                {
                    generateStmt(op, *pc->def);
                }
            }
            ClassInfo* ci;
            if(si->currentScope->st == sytGlobalScope || si->currentScope->st == sytNamespace)
            {
                ci = si->enterClass(cds.name);
                if(!ci)
                {
                    ZTHROW(CGException, cds.name.pos, "Internall error, failed to enter class %{}", cds.name);
                }
            } else
            {
                ci = si->registerClass(cds.name);
            }

            if(ci->parentClass)
            {
                si->returnScope();
                return;
            }
            classStack.emplace_back(currentClass, classHasMemberInit);
            currentClass = &cds;
            classHasMemberInit = false;
            if(!ci)
            {
                ci = si->registerClass(cds.name);
            }
            ScopeGuard sg(si);
            si->deriveMembers(cds.parent ? &cds.parent->name : nullptr);
            fillClassNames(cds.body);
            generateStmtList(cds.body);
            if(!ci->specialMethods[csmConstructor])
            {
                FuncParamList* args = cds.args;
                if(args)
                {
                    for(auto& arg : *args)
                    {
                        SymInfo* sym = ci->symMap.findSymbol(*arg);
                        if(sym && sym->st == sytClassMember && ((ClassMember*) sym)->owningClass == ci)
                        {
                            classHasMemberInit = true;
                            break;
                        }
                    }
                }
                if(args || classHasMemberInit ||
                   (currentClass->parent && ci->parentClass.asClass()->specialMethods[csmConstructor]))
                {
                    ZStringRef onCreate = si->getStringConstVal("on create");
                    size_t argsCount = args ? args->size() : 0;
                    MethodInfo* mi = si->registerMethod(onCreate, argsCount, false);
                    ScopeGuard sg2(si);
                    ci->specialMethods[csmConstructor] = mi->index;
                    OpPair fp(st.pos, vm);
                    index_type selfIdx = 0;
                    OpPair argOps = genArgs(fp, args, true, selfIdx);
                    //index_type resultIdx=si->registerLocalVar(result);

                    ClassParent* p = currentClass->parent;
                    size_t acnt = 0;
                    if(p && ci->parentClass.asClass()->specialMethods[csmConstructor])
                    {
                        if(p->args)
                        {
                            for(auto& arg:*p->args)
                            {
                                ExprContext ec(si, atStack);
                                try
                                {
                                    fp += generateExpr(arg, ec);
                                }
                                catch(...)
                                {
                                    mi->defValEntries.clear();
                                    throw;
                                }
                                ++acnt;
                            }
                        }
                        fp += new OpCallMethod(acnt, OpArg(atLocal, selfIdx),
                                               si->globals[ci->parentClass.asClass()->specialMethods[csmConstructor]].method->localIndex,
                                               atNul);
                    }

                    if(classHasMemberInit)
                    {
                        ClassInfo::HiddenArgsList& halst = ci->hiddenArgs;
                        for(auto& it : halst)
                        {
                            fp += OpPair(it.name.pos, vm,
                                         new OpAssign(OpArg(atMember, it.memIdx), OpArg(atLocal, it.argIdx),
                                                      OpArg()));
                        }
                        for(auto& sit : *currentClass->body)
                        {
                            Statement& cst = *sit;
                            if(cst.st == stClassMemberDef)
                            {
                                auto& cmd = cst.as<ClassMemberDef>();
                                if(cmd.value)
                                {
                                    ExprContext ec(si);
                                    SymInfo* sym = si->getSymbol(cmd.name);
                                    if(!sym)
                                    {
                                        throw UndefinedSymbolException(cmd.name);
                                    }
                                    if(sym->st != sytClassMember)
                                    {
                                        ZTHROW(CGException, cmd.pos, "%{} is not class data member", cmd.name);
                                    }
                                    ec.dst = OpArg(atMember, sym->index);
                                    try
                                    {
                                        fp += generateExpr(cmd.value, ec);
                                    }
                                    catch(...)
                                    {
                                        mi->defValEntries.clear();
                                        throw;
                                    }
                                    /*
                  ExprStatement st(new Expr(etAssign,new Expr(etVar,cmd.name),cmd.value));
                  try{
                    generateStmt(fp,st);
                  }catch(...)
                  {
                    st.expr->e2=0;
                    throw;
                  }
                  st.expr->e2=0;*/
                                }
                            }
                        }
                    }
                    //fp+=new OpAssign(OpArg(atLocal,resultIdx),OpArg(atLocal,selfIdx),atNul);
                    fp += new OpReturn(OpArg(atLocal, selfIdx));
                    finArgs(fp, argOps);
                    //si->globals[ctorIdx].func->entry->ref();
                }
            }
            if(!ci->specialMethods[csmDestructor] && ci->parentClass)
            {
                ci->specialMethods[csmDestructor] = ci->parentClass.asClass()->specialMethods[csmDestructor];
            }
            sg.release();
            currentClass = classStack.back().classPtr;
            classHasMemberInit = classStack.back().classHasMemberInit;
            classStack.pop_back();
        }
            break;
        case stVarList:
        {
            auto& vls = st.as<VarListStatement>();
            for(auto& nm:vls.vars->values)
            {
                si->registerMember(nm);
            }
        }
            break;
        case stNamespace:
        {
            auto& ns = st.as<NamespaceStatement>();
            if(ns.ns)
            {
                for(auto& nsVal:ns.ns->values)
                {
                    if(!si->enterNamespace(nsVal))
                    {
                        throw CGException("not a namespace", ns.pos);
                    }
                }
            } else
            {
                si->enterNamespace(ns.name);
            }
            op += generateStmtList(ns.body);
            if(ns.ns)
            {
                for(auto& nsVal:ns.ns->values)
                {
                    (void)nsVal;
                    si->returnScope();
                }
            } else
            {
                si->returnScope();
            }
        }
            break;
        case stTryCatch:
        {
            auto& tc = st.as<TryCatchStatement>();
            std::vector<ClassInfo*> exClasses;
            if(tc.exList)
            {
                for(auto& it : *tc.exList)
                {
                    SymInfo* exClass = si->getSymbol(it->getSymbol());
                    if(!exClass)
                    {
                        throw UndefinedSymbolException(it->getSymbol());
                    }
                    if(exClass->st != sytClass)
                    {
                        ZTHROW(CGException, it->pos, "Symbol %{} is not class.", it->getSymbol());
                    }
                    exClasses.push_back(((ClassInfo*) exClass));
                }
            }
            OpArg exObj = OpArg(atLocal, si->acquireTemp());
            SymInfo* exVarInfo = new SymInfo(tc.var, sytLocalVar);
            exVarInfo->index = exObj.idx;
            si->replaceLocalSymbol(exVarInfo);
            OpPair catchOp(exVarInfo->name.pos, vm);
            catchOp += generateStmtList(tc.catchBody);
            catchOp += new OpAssign(exObj, nil, atNul);
            si->restoreLocalSymbol(exVarInfo);
            ClassInfo** exClInfo = nullptr;
            if(!exClasses.empty())
            {
                exClInfo = new ClassInfo* [exClasses.size()];
                memcpy(exClInfo, &exClasses[0], sizeof(ClassInfo*) * exClasses.size());
            }
            auto* oec = new OpEnterTry(exClInfo, exClasses.size(), exObj.idx, catchOp.first.release());
            si->currentScope->enterTry();
            op += OpPair(tc.pos, vm, oec);
            op += generateStmtList(tc.tryBody);
            op += new OpLeaveCatch();
            si->currentScope->leaveTry();
            op.addFix(catchOp.last->next);
            si->releaseTemp(exObj.idx);
        }
            break;
        case stThrow:
        {
            auto& ts = st.as<ThrowStatement>();
            ExprContext ec(si);
            OpArg ex = ec.mkTmpDst();
            op += generateExpr(ts.ex, ec);
            op += OpPair(ts.pos, vm, new OpThrow(ex.tmp()));
            interruptedFlow = true;
        }
            break;
        case stEnum:
        {
            auto& est = st.as<EnumStatement>();
            SymInfo* einfo = si->currentScope->symMap.findSymbol(est.name);
            index_type eidx;
            if(einfo && einfo->name.pos.offset == est.name.pos.offset)
            {
                eidx = einfo->index;
            } else
            {
                einfo = new SymInfo(est.name, sytConstant);
                eidx = si->registerScopedGlobal(einfo);
            }
            int64_t val = est.bitEnum ? 1 : 0;
            ZMap* m = vm->allocZMap();
            m->ref();
            si->globals[eidx] = MapValue(m, true);
            if(est.items)
            {
                for(auto it = est.items->begin(), end = est.items->end(); it != end; ++it)
                {
                    Expr& ex = **it;
                    if(!(ex.et == etVar || (est.bitEnum && ex.et == etAssign)))
                    {
                        if(ex.et == etAssign)
                        {
                            throw CGException("values not allowed in bitenum", st.pos);
                        } else
                        {
                            throw CGException("invalid expression in enum", st.pos);
                        }
                    }
                    if(est.bitEnum && val == 0)
                    {
                        throw CGException("maximum number of bitenum values is 64", ex.pos);
                    }

                    Name nm = ex.et == etVar ? ex.getSymbol().name : ex.e1->getSymbol().name;

                    index_type idx;
                    SymInfo* vinfo = si->currentScope->symMap.findSymbol(nm);
                    if(vinfo)
                    {
                        idx = vinfo->index;
                    } else
                    {
                        idx = si->registerScopedGlobal(new SymInfo(nm, sytConstant));
                    }
                    Value v;
                    if(ex.et == etVar)
                    {
                        v = IntValue(val, true);
                    } else
                    {
                        if(!fillConstant(ex.e2, v))
                        {
                            ExprContext ec(si, OpArg(atGlobal, idx));
                            op += generateExpr(ex.e2, ec);
                            op += OpPair(st.pos, vm, new OpMakeConst(OpArg(atGlobal, idx)));
                            index_type keyIdx = si->getStringConst(nm.val);
                            op += OpPair(st.pos, vm, new OpInitMapItem(OpArg(atGlobal, eidx), OpArg(atGlobal, idx),
                                                                       OpArg(atGlobal, keyIdx)));
                            v = NilValue;
                        }
                        if(v.vt == vtInt)
                        {
                            val = v.iValue;
                        }
                    }
                    si->globals[idx] = v;
                    Value k = StringValue(nm.val, true);
                    m->insert(v, k);
                    if(est.bitEnum)
                    {
                        val <<= 1;
                    } else
                    {
                        if(v.vt == vtInt)
                        {
                            val++;
                        }
                    }
                }
            }
        }
            break;
        case stSwitch:
        {
            auto& sst = st.as<SwitchStatement>();

            OpArg src;
            if(sst.expr)
            {
                bool allint = true;
                bool allstr = true;
                typedef std::map<int64_t, SwitchCase*> IntMap;
                IntMap ints;
                SwitchCase* def = nullptr;
                Value val;
                int64_t rStart, rEnd, rStep;
                ZHashCasesGuard strCases(new ZHash<OpBase*>(), vm);
                for(auto& it : *sst.casesList)
                {
                    if(!it.caseExpr)
                    {
                        if(def)
                        {
                            throw CGException("Duplicate default case", def->caseBody->front()->pos);
                        }
                        def = &it;
                        continue;
                    }
                    for(auto cit = it.caseExpr->begin(), cend = it.caseExpr->end(); cit != cend; ++cit)
                    {
                        Expr* ex = *cit;
                        if((allstr || allint) && fillConstant(ex, val))
                        {
                            if(allint && val.vt != vtInt)
                            {
                                allint = false;
                                if(!allstr)
                                {
                                    break;
                                }
                            }
                            if(allstr && val.vt != vtString)
                            {
                                allstr = false;
                                if(!allint)
                                {
                                    break;
                                }
                            }
                            if(allint)
                            {
                                if(ints.find(val.iValue) != ints.end())
                                {
                                    throw CGException("Duplicate case", it.caseExpr->pos);
                                }
                                ints.insert(IntMap::value_type(val.iValue, &it));
                            } else if(allstr)
                            {
                                if(strCases->getPtr(val.str))
                                {
                                    throw CGException("Duplicate case", it.caseExpr->pos);
                                }
                                strCases->insert(val.str, (OpBase*) &it);//ugly hack
                            }

                        } else if(allint && isIntRange(ex, rStart, rEnd, rStep))
                        {
                            if(rStep < 0)
                            {
                                int64_t tmp = rStart;
                                rStart = rEnd;
                                rEnd = tmp;
                                rStep = -rStep;
                            }
                            for(int64_t i = rStart; i <= rEnd; i += rStep)
                            {
                                ints.insert(IntMap::value_type(i, &it));
                            }
                        } else
                        {
                            if(ex->et != etVar)
                            {
                                allint = false;
                                allstr = false;
                                break;
                            }
                            SymInfo* sptr = si->getSymbol(ex->getSymbol());
                            if(!sptr)
                            {
                                allint = false;
                                allstr = false;
                                break;
                            }
                            if(sptr->st == sytConstant)
                            {
                                Value* vptr = &si->globals[sptr->index];
                                if(allint && vptr->vt == vtInt)
                                {
                                    if(ints.find(vptr->iValue) != ints.end())
                                    {
                                        throw CGException("Duplicate case", it.caseExpr->pos);
                                    }
                                    ints.insert(IntMap::value_type(vptr->iValue, &it));
                                    allstr = false;
                                } else if(allstr && vptr->vt == vtString)
                                {
                                    if(strCases->getPtr(vptr->str))
                                    {
                                        throw CGException("Duplicate case", it.caseExpr->pos);
                                    }
                                    strCases->insert(vptr->str, (OpBase*) &it);//ugly hack
                                    allint = false;
                                } else
                                {
                                    allstr = false;
                                    allint = false;
                                    break;
                                }
                            } else
                            {
                                allstr = false;
                                allint = false;
                                break;
                            }
                        }
                    }
                    if(!allstr && !allint)
                    {
                        break;
                    }
                }
                if(def && !def->caseBody)
                {
                    def = nullptr;
                }
                allint = allint && !ints.empty();
                allstr = allstr && strCases->getCount();
                ExprContext ec(si);
                src = genArgExpr(op, sst.expr, ec);
                if(allint)
                {
                    int64_t minVal = ints.begin()->first;
                    int64_t maxVal = (--ints.end())->first;
                    //DPRINT("allint switch(%" PRId64 "-%" PRId64 ")\n",minVal,maxVal);
                    if(maxVal - minVal > static_cast<int64_t>(ints.size() / 2) - 10)
                    {
                        auto* sw = new OpRangeSwitch(src);
                        op += OpPair(st.pos, vm, sw);
                        sw->minValue = minVal;
                        sw->maxValue = maxVal;
                        sw->cases = new OpBase* [static_cast<size_t>(maxVal - minVal + 1)];
                        memset(sw->cases, 0, static_cast<size_t>(sizeof(OpBase*) * (maxVal - minVal + 1)));
                        if(def)
                        {
                            OpPair defCase(def->pos, vm);
                            defCase += generateStmtList(def->caseBody);
                            op.chainFixes(defCase);
                            sw->defaultCase = defCase.first.release();
                        } else
                        {
                            op.addFix(sw->defaultCase);
                        }
                        int64_t lastValue = minVal;
                        for(auto& it : ints)
                        {
                            while(lastValue < it.first)
                            {
                                if(def)
                                {
                                    sw->cases[lastValue - minVal] = sw->defaultCase;
                                } else
                                {
                                    op.addFix(sw->cases[lastValue - minVal]);
                                }
                                ++lastValue;
                            }
                            OpPair caseBody(it.second->pos, vm);
                            caseBody += generateStmtList(it.second->caseBody);
                            op.chainFixes(caseBody);
                            if(*caseBody.first)
                            {
                                sw->cases[it.first - minVal] = caseBody.first.release();
                            } else
                            {
                                op.addFix(sw->cases[it.first - minVal]);
                            }
                            lastValue = it.first + 1;
                        }
                        return;
                    }
                } else if(allstr)
                {
                    auto* sw = new OpHashSwitch(src, vm);
                    sw->cases = strCases.release();
                    op += OpPair(st.pos, vm, sw);
                    if(def)
                    {
                        OpPair defCase(def->pos, vm);
                        defCase += generateStmtList(def->caseBody);
                        op.chainFixes(defCase);
                        sw->defaultCase = defCase.first.release();
                    } else
                    {
                        op.addFix(sw->defaultCase);
                    }
                    ZHash<OpBase*>::Iterator it(*sw->cases);
                    ZString* key;
                    OpBase** value;
                    while(it.getNext(key, value))
                    {
                        auto* swcase = (SwitchCase*) *value;
                        try
                        {
                            OpPair caseBody = generateStmtList(swcase->caseBody);
                            if(*caseBody.first)
                            {
                                op.chainFixes(caseBody);
                                *value = caseBody.first.release();
                            } else
                            {
                                op.addFix(*value);
                            }
                        }
                        catch(...)
                        {
                            *value = nullptr;
                            while(it.getNext(key, value))
                            {
                                *value = nullptr;
                            }
                            throw;
                        }
                    }
                    return;
                }
                // generic (probably dynamic) switch
            }
            OpPair defCase = OpPair(FileLocation(), vm);
            OpJumpBase* lastjmp = nullptr;
            for(auto it = sst.casesList->begin(), end = sst.casesList->end(); it != end; ++it)
            {
                if(!it->caseExpr)
                {
                    if(defCase.last)
                    {
                        throw CGException("Duplicate default case", defCase.last->pos);
                    }
                    defCase = OpPair(it->pos, vm);
                    op.chainFixes(defCase);
                    defCase += generateStmtList(it->caseBody);
                    continue;
                }
                OpPair body = generateStmtList(it->caseBody);

                for(auto ex : *it->caseExpr)
                {
                    ExprContext ec(si);
                    OpPair caseOp(ex->pos, vm);
                    OpArg dst = genArgExpr(caseOp, ex, ec);
                    OpJumpBase* jmp;
                    if(sst.expr)
                    {
                        if(ex->et == etRange)
                        {
                            caseOp += jmp = new OpJumpIfIn(src, dst);
                        } else
                        {
                            caseOp += jmp = new OpJumpIfEqual(src, dst);
                        }
                    } else
                    {
                        caseOp += jmp = new OpCondJump(dst, nullptr);
                    }
                    jmp->fallback.pos = jmp->pos;
                    caseOp += body;
                    op.chainFixes(caseOp);
                    if(lastjmp)
                    {
                        lastjmp->elseOp = caseOp.first.release();
                    } else
                    {
                        op += caseOp;
                    }
                    lastjmp = jmp;
                }
                op.chainFixes(body);
            }
            if(*defCase.first)
            {
                if(lastjmp)
                {
                    lastjmp->elseOp = defCase.first.release();
                } else
                {
                    if(op.last)
                    {
                        op.fixLast(defCase.first.release());
                    } else
                    {
                        op += defCase;
                    }
                }
            } else
            {
                op.addFix(lastjmp->elseOp);
            }
            op.chainFixes(defCase);

        }
            break;
        case stProp:
        {
            auto& pst = st.as<PropStatement>();
            ClassPropertyInfo* p = (ClassPropertyInfo*) si->getSymbol(pst.name);
            if(pst.getFunc)
            {
                MethodInfo* gmi = si->enterMethod(pst.getFunc->name);
                ScopeGuard sg(si);
                p->getMethod = (MethodInfo*) si->globals[gmi->index].func;
                si->registerLocalVar(selfName);
                OpPair body = generateStmtList(pst.getFunc->body);
                if(!body.isLastReturn())
                {
                    OpBase* ret = new OpReturn();
                    ret->pos = body.last ? body.last->pos : st.pos;
                    body += ret;
                }
                sg.release();
                gmi->entry = body.first.release();
            }
            if(pst.setFunc)
            {
                MethodInfo* smi = si->enterMethod(pst.setFunc->name);
                ScopeGuard sg(si);
                p->setMethod = (MethodInfo*) si->globals[smi->index].func;
                si->registerArg(*pst.setFunc->args->front());
                si->registerLocalVar(selfName);
                //int ridx=si->registerLocalVar(result);
                OpPair body = generateStmtList(pst.setFunc->body);
                if(!body.isLastReturn())
                {
                    OpBase* ret = new OpReturn();
                    ret->pos = body.last ? body.last->pos : st.pos;
                    body += ret;
                }
                sg.release();
                smi->entry = body.first.release();
            }

        }
            break;
        case stClassMemberDef:
        {
            auto& cmd = st.as<ClassMemberDef>();
            ClassMember* cm = si->getMember(cmd.name);
            if(cmd.attrs)
            {
                for(auto& attr : *cmd.attrs)
                {
                    AttrInstance& att = *attr;
                    AttrSym* as = si->getAttr(att.name->getSymbol());
                    if(!as)
                    {
                        ZTHROW(CGException, att.name->pos, "Attr not found:%{}", att.name->getSymbol());
                    }
                    index_type mappedIdx = as->index;
                    if(att.value)
                    {
                        mappedIdx = si->newGlobal();
                        si->info.resize(mappedIdx + 1);
                        si->info[mappedIdx] = as;
                        if(isConstant(att.value))
                        {
                            fillConstant(att.value, si->globals[mappedIdx]);
                        } else
                        {
                            ExprContext ec(si);
                            ec.dst = OpArg(atGlobal, mappedIdx);
                            initOps += generateExpr(att.value, ec);
                        }
                    }
                    if(!cm->attrs.addAttr(as->index, mappedIdx))
                    {
                        ZTHROW(CGException, att.name->pos, "Duplicate attr:%{}", att.name->getSymbol());
                    }
                }
            }
            if(cmd.value)
            {
                classHasMemberInit = true;
            }
        }
            break;
        case stAttr:
        {
            auto& ads = st.as<AttrDeclStatement>();
            AttrSym* as = si->getAttr(ads.name);
            if(ads.value)
            {
                ExprContext ec(si);
                ec.dst = OpArg(atGlobal, as->index);
                if(isConstant(ads.value))
                {
                    fillConstant(ads.value, si->globals[as->index]);
                } else
                {
                    initOps += generateExpr(ads.value, ec);
                }
            } else
            {
                si->globals[as->index] = BoolValue(true);
            }
        }
            break;
        case stClassAttrDef:
        {
            auto& cad = st.as<ClassAttrDef>();
            if(cad.lst)
            {
                for(auto& it : *cad.lst)
                {
                    AttrInstance& att = *it;
                    AttrSym* as = si->getAttr(att.name->getSymbol());
                    if(!as)
                    {
                        ZTHROW(CGException, att.name->pos, "Attr not found: %{}", att.name->getSymbol());
                    }
                    index_type mappedIdx = as->index;
                    if(att.value)
                    {
                        mappedIdx = si->newGlobal();
                        si->info.resize(mappedIdx + 1);
                        si->info[mappedIdx] = as;
                        if(isConstant(att.value))
                        {
                            fillConstant(att.value, si->globals[mappedIdx]);
                        } else
                        {
                            ExprContext ec(si);
                            ec.dst = OpArg(atGlobal, mappedIdx);
                            initOps += generateExpr(att.value, ec);
                        }
                    }
                    if(!si->currentClass->attrs.addAttr(as->index, mappedIdx))
                    {
                        ZTHROW(CGException, att.name->pos, "Duplicate attr: %{}", att.name->getSymbol());
                    }
                }
            }
        }
            break;
        case stUse:
        {
            auto& us = st.as<UseStatement>();
            SymInfo* sym = si->getSymbol(us.expr->getSymbol());
            if(!sym)
            {
                throw UndefinedSymbolException(us.expr->getSymbol());
            }
            if(sym->st == sytNamespace)
            {
                si->currentScope->usedNs.push_back((ScopeSym*) sym);
            } else
            {
                si->addAlias(sym);
            }
        }
            break;
        case stLiter:
        {
            auto& lst = st.as<LiterStatement>();
            FuncInfo* li = si->enterFunc(lst.name);//si->registerLiter(lst.name,lst.args->size());
            OpPair fp = generateStmtList(lst.body);
            li->entry = fp.first.release();
            si->returnScope();
        }
            break;
    }
}

CodeGenerator::OpPair CodeGenerator::genArgs(OpPair& fp, FuncParamList* args, bool isCtor, index_type& selfIdx)
{
    bool hasVarArg = false;
    bool hasNamedArg = false;
    size_t lastArgIdx = 0;
    auto* fi = (FuncInfo*) si->currentScope;
    OpPair argOps(FileLocation(), vm);
    if(args)
    {
        for(auto ait = args->begin(), aend = args->end(); ait != aend; ++ait)
        {
            FuncParam& p = **ait;
            size_t aidx = SymInfo::invalidIndexValue;
            if(isCtor)
            {
                ClassInfo* ci = si->currentClass;
                SymInfo* sym = si->currentScope->getParent()->getSymbols()->findSymbol(**ait);
                if(sym && sym->st == sytClassMember)
                {
                    auto* cm = (ClassMember*) sym;
                    if(cm->owningClass == ci)
                    {
                        std::string name = "hidden-";
                        name += p.val.c_str();
                        Name nm(si->getStringConstVal(name.c_str()), p.pos);
                        aidx = si->registerArg(nm);
                        ci->hiddenArgs.push_back(ClassInfo::HiddenCtorArg(p, aidx, sym->index));
                    }
                }
            }
            if(aidx == SymInfo::invalidIndexValue)
            {
                if(si->currentScope->getSymbols()->findSymbol(p))
                {
                    fi->defValEntries.clear();
                    ZTHROW(CGException, p.pos, "Duplicate argument %{}", p.val);
                }
                aidx = si->registerArg(p);
            }
            if(p.defValue)
            {
                ExprContext ec(si);
                ec.dst = OpArg(atLocal, aidx);
                OpPair op(p.defValue->pos, vm);
                OpJumpBase* jmp = new OpJumpIfInited(ec.dst, nullptr);
                op += jmp;
                op += generateExpr(p.defValue, ec);
                fi->defValEntries.push_back(*op.first);
                op.addFix(jmp->elseOp);
                argOps += op;
            } else
            {
                if(p.pt != FuncParam::ptNormal)
                {
                    if(ait != --args->end())
                    {
                        fi->defValEntries.clear();
                        throw CGException("Varargs/namedargs parameter can only be last", p.pos);
                    }
                    lastArgIdx = aidx;
                    if(p.pt == FuncParam::ptVarArgs)
                    {
                        hasVarArg = true;
                        argOps += new OpMakeArray(OpArg(), OpArg(atLocal, aidx));
                    } else
                    {
                        hasNamedArg = true;
                        //argOps+=new OpMakeMap(OpArg(),OpArg(atLocal,aidx));
                    }
                    fi->defValEntries.push_back(argOps.last);
                } else if(!fi->defValEntries.empty())
                {
                    fi->defValEntries.clear();
                    throw CGException("Parameter without default value cannot follow parameter with default value",
                                      p.pos);
                }
            }
        }
    }
    if(currentClass)
    {
        selfIdx = si->registerLocalVar(selfName);
    }
    if(hasVarArg)
    {
        ExprContext ec(si);
        OpArg tmp = ec.mkTmpDst();
        fp += new OpMakeArray(OpArg(), tmp);
        fp += new OpInitArrayItem(tmp, OpArg(atLocal, lastArgIdx), 0);
        fp += new OpAssign(OpArg(atLocal, lastArgIdx), tmp.tmp(), OpArg());
        fp.addFix(fi->varArgEntry);
    }
    if(hasNamedArg)
    {
        fi->namedArgs = true;
        --fi->argsCount;
        fp += new OpMakeMap(OpArg(), OpArg(atLocal, lastArgIdx));
        fp.addFix(fi->namedArgEntry);
    }
    return argOps;
}

void CodeGenerator::finArgs(OpPair& fp, OpPair& argOps)
{
    auto* fi = (FuncInfo*) si->currentScope;
    if(argOps.last)
    {
        if(fi->varArgEntry)
        {
            fi->varArgEntryLast = argOps.last;
            argOps.fixLast(fi->varArgEntry);
        } else
        {
            argOps.fixLast(*fp.first);
        }
        argOps.release();
    }
    fi->entry = fp.first.release();
}

OpArg CodeGenerator::getArgType(Expr* expr, bool lvalue)
{
    OpArg rv(atGlobal);
    if(lvalue && isConstant(expr))
    {
        throw CGException("Attempt to modify constant", expr->pos);
    }
    switch(expr->et)
    {
        case etInt:
            rv.idx = si->getIntConst(expr->val);
            break;
        case etDouble:
            rv.idx = si->getDoubleConst(expr->val);
            break;
        case etString:
            rv.idx = si->getStringConst(expr->val);
            break;
        case etNil:
            rv.idx = si->nilIdx;
            break;
        case etTrue:
            rv.idx = si->trueIdx;
            break;
        case etFalse:
            rv.idx = si->falseIdx;
            break;
        case etRegExp:
        {
            size_t idx = si->registerRegExp();
            rv.idx = idx;
            RegExpVal* rxv = si->globals[idx].regexp;
            kst::RegExp* rx = rxv->val;
            const char* rxSrc = expr->val->getDataPtr();
            const char* rxEnd = rxSrc + expr->val->getDataSize();
            int cs = expr->val->getCharSize();
            int res;
            if(cs == 1)
            {
                res = rx->CompileEx(rxSrc, rxEnd, kst::OP_PERLSTYLE | kst::OP_OPTIMIZE, '`');
            } else
            {
                auto* rxSrc2 = (uint16_t*) rxSrc;
                auto* rxEnd2 = (uint16_t*) rxEnd;
                res = rx->CompileEx(rxSrc2, rxEnd2, kst::OP_PERLSTYLE | kst::OP_OPTIMIZE, '`');
            }
            if(!res)
            {
                //expr->val.c_str()
                throw CGException(FORMAT("Invalid regexp %{}(%{}).", expr->val.c_str(), rx->getLastError()), expr->pos);
            }
            rxv->marrSize = static_cast<size_t>(rx->getBracketsCount());
            rxv->marr = new kst::SMatch[rxv->marrSize];
            rxv->narrSize = static_cast<size_t>(rx->getNamedBracketsCount());
            if(rxv->narrSize)
            {
                rxv->narr = new kst::NamedMatch[rxv->narrSize];
            }
        }
            break;
        case etNeg:
        {
            std::string tmp = "-";
            tmp += expr->e1->val.c_str();
            ZStringRef nval = vm->mkZString(tmp.c_str(), tmp.length());
            if(expr->e1->et == etInt)
            {
                rv.idx = si->getIntConst(nval);
            } else
            {
                rv.idx = si->getDoubleConst(nval);
            }
        }
            break;
        case etVar:
        {
            SymInfo* sym = si->getSymbol(expr->getSymbol());
            if(!sym)
            {
                if(lvalue)
                {
                    bool isLocal;
                    rv.idx = si->registerVar(expr->val, isLocal);
                    if(isLocal)
                    {
                        rv.at = atLocal;
                    }
                    break;
                } else
                {
                    throw UndefinedSymbolException(expr->getSymbol());
                }
            }
            switch(sym->st)
            {
                case sytClass:
                case sytFunction:
                case sytConstant:
                    if(lvalue)
                    {
                        ZTHROW(CGException, expr->pos, "Lvalue access to constant:%{}", expr->getSymbol());
                    }
                    rv.idx = sym->index;
                    break;
                case sytGlobalVar:
                    rv.idx = sym->index;
                    break;
                case sytLocalVar:
                    rv = OpArg(atLocal, sym->index);
                    break;
                case sytClosedVar:
                    rv = OpArg(atClosed, sym->index);
                    break;
                case sytClassMember:
                    rv = OpArg(atMember, sym->index);
                    break;
                case sytNamespace:
                    throw CGException("invalid namespace usage", expr->pos);
                default:
                    abort();
                    break;
            }
        }
            break;
        case etNumArg:
        {
            size_t index = static_cast<size_t>(ZString::parseInt(expr->val.c_str() + 1) - 1);
            rv = OpArg(atLocal, index);
        }
            break;
        default:
            abort();
            break;
    }
    return rv;
}

OpArg CodeGenerator::genPropGetter(const FileLocation& pos, OpPair& op, ClassPropertyInfo* cp, ExprContext& ec)
{
    if(cp->getIdx != SymInfo::invalidIndexValue)
    {
        return OpArg(atMember, cp->getIdx);
    } else if(cp->getMethod)
    {
        if(si->currentScope->st != sytMethod)
        {
            abort();
        }
        auto* mi = (MethodInfo*) si->currentScope;
        op += new OpCallMethod(0, OpArg(atLocal, mi->argsCount), cp->getMethod->localIndex, ec.mkTmpDst());
        return ec.dst.tmp();
    } else
    {
        ZTHROW(CGException, pos, "Property %{} do not have getter", cp->name);
    }
}

void CodeGenerator::genPropSetter(const FileLocation& pos, OpPair& op, ClassPropertyInfo* cp, OpArg src)
{
    if(cp->setIdx != SymInfo::invalidIndexValue)
    {
        op += new OpAssign(OpArg(atMember, cp->setIdx), src, OpArg());
    } else if(cp->getMethod)
    {
        if(si->currentScope->st != sytMethod)
        {
            abort();
        }
        auto* mi = (MethodInfo*) si->currentScope;
        op += new OpPush(src);
        op += new OpCallMethod(1, OpArg(atLocal, mi->argsCount), cp->setMethod->localIndex, OpArg());
    } else
    {
        ZTHROW(CGException, pos, "Property %{} do not have setter", cp->name);
    }
}

template<class OP>
CodeGenerator::OpPair CodeGenerator::genBinOp(Expr* expr, ExprContext& ec)
{
    OpArg left, right, dst = ec.dst;

    bool isSelfUpdate =
        expr->et == etSPlus || expr->et == etSMinus || expr->et == etSMul || expr->et == etSDiv || expr->et == etSMod;

    if(ec.ifContext && dst.at == atNul)
    {
        dst = ec.mkTmpDst();
    }

    OpPair op(expr->pos, vm);
    ExprContext ecl(si);
    SymInfo* sym;
    if(isSelfUpdate && currentClass && (sym = isSpecial(expr->e1)) != nullptr && sym->st == sytProperty)
    {
        auto* cp = (ClassPropertyInfo*) sym;
        if((cp->getIdx == SymInfo::invalidIndexValue && !cp->getMethod) ||
           (cp->setIdx == SymInfo::invalidIndexValue && !cp->setMethod))
        {
            ZTHROW(CGException, expr->pos, "Property must be both readable and writable to perform this operation");
        }
        if(cp->getIdx != SymInfo::invalidIndexValue && cp->getIdx == cp->setIdx)
        {
            left = OpArg(atMember, cp->getIdx);
        } else
        {
            left = genPropGetter(expr->pos, op, cp, ecl);
            ExprContext ec2(si);
            right = genArgExpr(op, expr->e2, ec2.setConst());
            op += (ec.lastBinOp = new OP(left, right, dst));
            genPropSetter(expr->pos, op, cp, left);
            if(ec.ifContext)
            {
                op += ec.addJump(new OpCondJump(dst));
            }
            return op;
        }
    } else
    {
        if(isSimple(expr->e1))
        {
            left = getArgType(expr->e1, isSelfUpdate);
        } else
        {
            left = ecl.mkTmpDst();
            if(isSelfUpdate)
            {
                ecl.setLvalue();
            }
            op += generateExpr(expr->e1, ecl);
            left.tmp();
        }
    }
    ExprContext ec2(si);
    right = genArgExpr(op, expr->e2, ec2.setConst());
    //if(dst.at!=atNul || isSelfUpdate)
    {
        op += (ec.lastBinOp = new OP(left, right, dst));
    }
    if(ec.ifContext)
    {
        op += ec.addJump(new OpCondJump(dst));
    }
    return op;
}

template<class OP>
CodeGenerator::OpPair CodeGenerator::genBoolOp(Expr* expr, ExprContext& ec)
{
    OpPair op(expr->pos, vm);
    ExprContext ec2(si);
    OpArg left = genArgExpr(op, expr->e1, ec2);
    OpArg right = genArgExpr(op, expr->e2, ec2.setConst());
    OpJumpBase* jmp;
    op += ec.addJump(jmp = new OP(left, right));
    jmp->fallback.pos = jmp->pos;
    if(!ec.ifContext)
    {
        if(ec.dst.at != atStack)
        {
            op += new OpAssign(ec.dst, OpArg(atGlobal, si->trueIdx), OpArg());
            jmp->elseOp = new OpAssign(ec.dst, OpArg(atGlobal, si->falseIdx), OpArg());
        } else
        {
            op += new OpPush(OpArg(atGlobal, si->trueIdx));
            jmp->elseOp = new OpPush(OpArg(atGlobal, si->falseIdx));
        }
        jmp->elseOp->pos = expr->pos;
        op.addFix(jmp->elseOp->next);
    } else
    {
        op.addFix(jmp->next);
    }
    return op;
}

template<class T1, class T2>
struct IsSameClass {
    enum {
        RET = false
    };
};

template<class T>
struct IsSameClass<T, T> {
    enum {
        RET = true
    };
};


template<class OP>
CodeGenerator::OpPair CodeGenerator::genUnOp(Expr* expr, CodeGenerator::ExprContext& ec, bool lvalue)
{
    OpArg dst = ec.dst;
    if(ec.ifContext && dst.at == atNul)
    {
        dst = ec.mkTmpDst();
    }
    if(isSimple(expr))
    {
        return fillDst(expr->pos, getArgType(expr, false), ec);
    }
    OpPair op(expr->pos, vm);
    SymInfo* sym;
    if(lvalue && currentClass && (sym = isSpecial(expr->e1)) != nullptr && sym->st == sytProperty)
    {
        auto* cp = (ClassPropertyInfo*) sym;
        ExprContext ec2(si);
        OpArg val = genPropGetter(expr->pos, op, cp, ec2);
        bool valTmp = val.isTemporal;
        val.isTemporal = false;
        op += new OP(val, ec.dst);
        val.isTemporal = valTmp;
        genPropSetter(expr->pos, op, cp, val);
        return op;
    }
    ExprContext ec2(ec);
    OpArg val = genArgExpr(op, expr->e1, lvalue ? ec2.setLvalue() : ec2);
    op += new OP(val, ec.dst);
    if(ec.ifContext)
    {
        op += ec.addJump(new OpCondJump(dst));
    }
    return op;
}


struct ArrayInitPair {
    size_t index;
    Expr* expr;

    ArrayInitPair(size_t argIndex, Expr* argExpr) : index(argIndex), expr(argExpr)
    {
    }
};

struct MapInitPair {
    Expr* key;
    Expr* item;

    MapInitPair(Expr* argKey, Expr* argItem) : key(argKey), item(argItem)
    {
    }
};

bool CodeGenerator::fillConstant(Expr* ex, Value& val)
{
    switch(ex->et)
    {
        case etNil:
            val = NilValue;
            break;
        case etTrue:
            val = BoolValue(true);
            break;
        case etFalse:
            val = BoolValue(false);
            break;
        case etInt:
            val = IntValue(ZString::parseInt(ex->val.c_str()));
            break;
        case etDouble:
            val = DoubleValue(ZString::parseDouble(ex->val.c_str()));
            break;
        case etString:
            val = StringValue(ex->val, true);
            break;
        case etNeg:
        {
            if(!fillConstant(ex->e1, val))
            {
                return false;
            }
            if(val.vt == vtInt)
            {
                val.iValue = -val.iValue;
            } else if(val.vt == vtDouble)
            {
                val.dValue = -val.dValue;
            } else
            {
                throw CGException("Unary minus can only be appied to integer or double", ex->e1->pos);
            }
        }
            break;
        case etArray:
        {
            val = ArrayValue(vm->allocZArray());
            ZArray& za = *val.arr;
            for(auto& it : *ex->lst)
            {
                Value arrVal;
                if(fillConstant(it, arrVal))
                {
                    za.pushAndRef(arrVal);
                }
                else {
                    za.pushAndRef(NilValue);
                }
            }
        }
            break;
        case etVar:
        {
            SymInfo* ptr = si->getSymbol(ex->getSymbol());
            if(ptr && (ptr->st == sytConstant || ptr->st == sytClass || ptr->st == sytFunction))
            {
                val = si->globals[ptr->index];
                return true;
            }
            return false;
        }
        default:
            return false;
    }
    return true;
}


OpArg CodeGenerator::genArgExpr(OpPair& op, Expr* expr, ExprContext& ec)
{
    if(isSimple(expr))
    {
        return getArgType(expr, ec.lvalue);
    }
    if(expr->et == etFunc)
    {

    }
    OpArg dst = ec.mkTmpDst();
    op += generateExpr(expr, ec);
    return dst == ec.dst ? dst.tmp() : ec.dst;
}

CodeGenerator::OpPair CodeGenerator::fillDst(const FileLocation& pos, const OpArg& src, ExprContext& ec)
{
    if(ec.ifContext)
    {
        return OpPair(pos, vm, ec.addJump(new OpCondJump(src)));
    }
    if(ec.dst.at == atStack)
    {
        return OpPair(pos, vm, new OpPush(src));
    } else
    {
        if(ec.dst.at != atNul)
        {
            return OpPair(pos, vm, new OpAssign(ec.dst, src, atNul));
        } else
        {
            if(src.isTemporal)
            {
                return OpPair(pos, vm, new OpAssign(src, nil, atNul));
            } else
            {
                return OpPair(pos, vm);
            }
        }
    }
}

void CodeGenerator::gatherNumArgs(Expr* expr, std::vector<size_t>& args)
{
    if(expr->et == etNumArg)
    {
        size_t index = static_cast<size_t>(ZString::parseInt(expr->val.c_str() + 1));
        auto it = std::find(args.begin(), args.end(), index);
        if(it == args.end())
        {
            args.push_back(index);
        }
    } else
    {
        if(expr->e1)
        {
            gatherNumArgs(expr->e1, args);
        }
        if(expr->e2)
        {
            gatherNumArgs(expr->e2, args);
        }
        if(expr->e3)
        {
            gatherNumArgs(expr->e3, args);
        }
        if(expr->lst)
        {
            for(auto& it : *expr->lst)
            {
                gatherNumArgs(it, args);
            }
        }
    }
}

struct NamedArgInfo {
    OpArg nameDst;
    OpArg valDst;

    NamedArgInfo(OpArg argName, OpArg argVal) : nameDst(argName), valDst(argVal)
    {
    }
};

CodeGenerator::OpPair CodeGenerator::generateExpr(Expr* expr, ExprContext& ec)
{
    switch(expr->et)
    {
        case etPlus:
            return genBinOp<OpAdd>(expr, ec);
        case etSPlus:
            return genBinOp<OpSAdd>(expr, ec);
        case etMinus:
            return genBinOp<OpSub>(expr, ec);
        case etSMinus:
            return genBinOp<OpSSub>(expr, ec);
        case etMul:
            return genBinOp<OpMul>(expr, ec);
        case etSMul:
            return genBinOp<OpSMul>(expr, ec);
        case etDiv:
            return genBinOp<OpDiv>(expr, ec);
        case etSDiv:
            return genBinOp<OpSDiv>(expr, ec);
        case etMod:
            return genBinOp<OpMod>(expr, ec);
        case etSMod:
            return genBinOp<OpSMod>(expr, ec);
        case etBitOr:
            return genBinOp<OpBitOr>(expr, ec);
        case etBitAnd:
            return genBinOp<OpBitAnd>(expr, ec);
        case etPreInc:
            return genUnOp<OpPreInc>(expr, ec, true);
        case etPostInc:
            return genUnOp<OpPostInc>(expr, ec, true);
        case etPreDec:
            return genUnOp<OpPreDec>(expr, ec, true);
        case etPostDec:
            return genUnOp<OpPostDec>(expr, ec, true);
        case etNeg:
            return genUnOp<OpNeg>(expr, ec);
        case etGetType:
            return genUnOp<OpGetType>(expr, ec);
        case etCopy:
            return genUnOp<OpCopy>(expr, ec);
        case etRef:
            return genUnOp<OpMakeRef>(expr, ec, true);
        case etWeakRef:
            return genUnOp<OpMakeWeakRef>(expr, ec, false);
        case etCor:
            return genUnOp<OpMakeCor>(expr, ec, false);
        case etMatch:
        {
            OpPair op = genBinOp<OpMatch>(expr, ec);
            if(expr->e2->et == etRegExp)
            {
                auto* mop = (OpMatch*) ec.lastBinOp;
                RegExpVal* rxv = si->globals[mop->right.idx].regexp;
                if(rxv->narrSize)
                {
                    size_t cnt = rxv->val->getNamedBracketsCount();
                    mop->vars = new OpArg[cnt];
                    for(size_t i = 0; i < cnt; ++i)
                    {
                        Expr evar(etVar, vm->mkZString(rxv->val->getName(i)));
                        mop->vars[i] = getArgType(&evar, true);
                    }
                }
            }
            return op;
        }
            break;
        case etAssign:
        {
            OpPair op(expr->pos, vm);
            if(expr->e1->et == etIndex || expr->e1->et == etProp || expr->e1->et == etKey)
            {
                Expr* earr = expr->e1->e1;
                Expr* eidx = expr->e1->e2;
                Expr* eitem = expr->e2;
                ExprContext ec2(si);
                OpArg arr = genArgExpr(op, earr, ec2);
                OpArg idx = genArgExpr(op, eidx, ec2);
                OpArg item = genArgExpr(op, eitem, ec2);
                if(ec.ifContext)
                {
                    ec.mkTmpDst();
                }
                switch(expr->e1->et)
                {
                    case etIndex:
                        op += new OpSetArrayItem(arr, idx, item, ec.dst);
                        break;
                    case etProp:
                        op += new OpSetProp(arr, idx, item, ec.dst);
                        break;
                    case etKey:
                        op += new OpSetKey(arr, idx, item, ec.dst);
                        break;
                    default:
                        break;
                }
                if(ec.ifContext)
                {
                    op += OpPair(expr->pos, vm, ec.addJump(new OpCondJump(ec.dst)));
                }
                return op;
            }
            if(!ec.ifContext)
            {
                ExprContext ecd(si);
                OpArg dst;
                SymInfo* sym = currentClass ? isSpecial(expr->e1) : nullptr;
                if(sym && sym->st == sytProperty)
                {
                    auto* cp = (ClassPropertyInfo*) sym;
                    if(cp->setIdx != SymInfo::invalidIndexValue)
                    {
                        dst = OpArg(atMember, cp->setIdx);
                    } else if(cp->setMethod != nullptr)
                    {
                        if(si->currentScope->st != sytMethod)
                        {
                            abort();
                        }
                        auto* mi = (MethodInfo*) si->currentScope;
                        ecd.dst = OpArg(atStack);
                        op += generateExpr(expr->e2, ecd);
                        op += new OpCallMethod(1, OpArg(atLocal, mi->argsCount), cp->setMethod->localIndex, ec.dst);
                        return op;
                    } else
                    {
                        ZTHROW(CGException, expr->pos, "Property %s do not have setter", expr->e1->getSymbol());
                    }
                } else
                {
                    dst = genArgExpr(op, expr->e1, ecd.setLvalue());
                }
                if(isSimple(expr->e2))
                {
                    OpArg src = getArgType(expr->e2);
                    op += new OpAssign(dst, src, ec.dst);
                } else
                {
                    dst.isTemporal = false;
                    ExprContext ec2(si, dst);
                    op += generateExpr(expr->e2, ec2);
                    if(ec.dst.at != atNul)
                    {
                        op += new OpAssign(ec.dst, dst, atNul);
                    }
                }
            } else
            {
                /*if(expr->e1->et==etIndex || expr->e1->et==etProp || expr->e1->et==etKey)
        {
          ZTHROW(CGException,expr->pos,"Can't assign to index, key or property in bool context");
        }*/
                ExprContext ec2(si);
                OpArg tmp = ec2.mkTmpDst();
                op += generateExpr(expr->e2, ec2);
                OpArg left;
                if(isSimple(expr->e1))
                {
                    left = getArgType(expr->e1, true);
                } else
                {
                    left = OpArg(atLocal, si->acquireTemp());
                    ExprContext ec3(si);
                    ec3.dst = left;
                    ec2.addTemp(left);
                    op += generateExpr(expr->e1, ec3.setLvalue());
                }
                if(ec.ifContext)
                {
                    ec.mkTmpDst();
                }
                op += new OpAssign(left, tmp.tmp(), ec.dst);
                if(ec.ifContext)
                {
                    op += OpPair(expr->pos, vm, ec.addJump(new OpCondJump(ec.dst)));
                }
            }
            return op;
        }
        case etIn:
            return genBoolOp<OpJumpIfIn>(expr, ec);
        case etIs:
            return genBoolOp<OpJumpIfIs>(expr, ec);
        case etLess:
            return genBoolOp<OpJumpIfLess>(expr, ec);
        case etGreater:
            return genBoolOp<OpJumpIfGreater>(expr, ec);
        case etLessEq:
            return genBoolOp<OpJumpIfLessEq>(expr, ec);
        case etGreaterEq:
            return genBoolOp<OpJumpIfGreaterEq>(expr, ec);
        case etEqual:
            return genBoolOp<OpJumpIfEqual>(expr, ec);
        case etNotEqual:
            return genBoolOp<OpJumpIfNotEqual>(expr, ec);
        case etNot:
        {
            OpPair op(expr->pos, vm);
            ExprContext ec2(si);
            OpArg arg = genArgExpr(op, expr->e1, ec2);
            if(!ec.ifContext)
            {
                op += new OpNot(arg, ec.dst);
            } else
            {
                OpJumpBase* jmp;
                op += ec.addJump(jmp = new OpJumpIfNot(arg, OpArg()));
                jmp->fallback.pos = jmp->pos;
                //op.addFix(jmp->next);
            }
            return op;
        }
            break;
            //return genBoolOp<OpJumpIfNot>(expr,ec);
        case etOr:
        {
            OpPair op(expr->pos, vm);
            if(ec.ifContext)
            {
                if(isBoolOp(expr->e1))
                {
                    op += generateExpr(expr->e1, ec);
                } else
                {
                    ExprContext ec2(si);
                    op += ec.addJump(new OpCondJump(genArgExpr(op, expr->e1, ec2)));
                }
                OpPair op2(expr->pos, vm);
                ExprContext ec2(si);
                if(isBoolOp(expr->e2))
                {
                    op2 = generateExpr(expr->e2, ec2.setIf());
                } else
                {
                    OpCondJump* j = new OpCondJump(genArgExpr(op2, expr->e2, ec2));
                    op2 += ec2.addJump(j);
                    op2.addFix(j->next);
                }
                for(auto& jump : ec.jumps)
                {
                    if(!jump->elseOp)
                    {
                        jump->elseOp = *op2.first;
                    }
                }
                op2.release();
                op.chainFixes(op2);
                //ec.addJumps(ec2.jumps);
                ec.jumps = ec2.jumps;
            } else
            {
                OpCondJump* j;
                if(ec.dst.at == atStack)
                {
                    ExprContext ec2(si);
                    OpArg src = genArgExpr(op, expr->e1, ec2);
                    op += j = new OpCondJump(src, nullptr);
                    op += new OpPush(src);
                    OpPair op2 = generateExpr(expr->e2, ec);
                    j->elseOp = *op2.first;
                    op2.release();
                    op.addFix(op2.last->next);
                    op.chainFixes(op2);
                } else
                {
                    op += generateExpr(expr->e1, ec);
                    op += j = new OpCondJump(ec.dst, nullptr);
                    OpPair op2 = generateExpr(expr->e2, ec);
                    j->elseOp = *op2.first;
                    op += op2;
                    op.addFix(j->next);
                }
            }
            return op;
        }
        case etAnd:
        {
            OpPair op(expr->pos, vm);
            if(ec.ifContext)
            {
                if(isBoolOp(expr->e1))
                {
                    op += generateExpr(expr->e1, ec);
                } else
                {
                    ExprContext ec2(si);
                    op += ec.addJump(new OpCondJump(genArgExpr(op, expr->e1, ec2)));
                }
                ExprContext ec2(si);
                if(isBoolOp(expr->e2))
                {
                    op += generateExpr(expr->e2, ec2.setIf());
                } else
                {
                    OpCondJump* j = new OpCondJump(genArgExpr(op, expr->e2, ec2));
                    op += ec2.addJump(j);
                    op.addFix(j->next);
                }
                ec.addJumps(ec2.jumps);
            } else
            {
                OpCondJump* j;
                if(ec.dst.at == atStack)
                {
                    ExprContext ec2(si);
                    OpArg src = genArgExpr(op, expr->e1, ec2);
                    op += j = new OpCondJump(src, nullptr);
                    op += generateExpr(expr->e2, ec);
                    j->elseOp = new OpPush(src);
                    op.addFix(j->elseOp->next);
                } else
                {
                    op += generateExpr(expr->e1, ec);
                    op += j = new OpCondJump(ec.dst, nullptr);
                    op += generateExpr(expr->e2, ec);
                    op.addFix(j->elseOp);
                }
            }
            return op;
        }
        case etNil:
            return fillDst(expr->pos, nil, ec);
        case etTrue:
            return fillDst(expr->pos, OpArg(atGlobal, si->trueIdx), ec);
        case etFalse:
            return fillDst(expr->pos, OpArg(atGlobal, si->falseIdx), ec);
        case etInt:
            return fillDst(expr->pos, OpArg(atGlobal, si->getIntConst(expr->val)), ec);
        case etDouble:
            return fillDst(expr->pos, OpArg(atGlobal, si->getDoubleConst(expr->val)), ec);
        case etString:
            return fillDst(expr->pos, OpArg(atGlobal, si->getStringConst(expr->val)), ec);
        case etVar:
        {
            SymInfo* sym = si->getSymbol(expr->getSymbol());
            if(!ec.lvalue && !sym)
            {
                throw UndefinedSymbolException(expr->getSymbol());
            }
            OpArg src;
            if(sym)
            {
                switch(sym->st)
                {
                    case sytLocalVar:
                        src.at = atLocal;
                        break;
                    case sytClosedVar:
                        src.at = atClosed;
                        break;
                    case sytConstant:
                        if(ec.lvalue)
                        {
                            ZTHROW(CGException, expr->pos, "Lvalue access to constant:%{}", expr->getSymbol());
                        }
                    case sytClass:
                    case sytGlobalVar:
                        src.at = atGlobal;
                        break;
                    case sytClassMember:
                        src.at = atMember;
                        break;
                    case sytFunction:
                    {
                        auto* f = (FuncInfo*) sym;
                        if(!f->isClosure())
                        {
                            src.at = atGlobal;
                        } else
                        {
                            OpPair op(expr->pos, vm);
                            OpArg self;
                            if(f->selfClosed)
                            {
                                self = OpArg(atLocal, f->argsCount);
                            }
                            src = OpArg(atGlobal, f->index);
                            size_t cnt = f->closedVars.size();
                            OpArg* arr = cnt ? &f->closedVars[0] : nullptr;
                            for(auto& cf:f->closedFuncs)
                            {
                                FuncInfo* cfi = si->globals[cf.src.idx].func;
                                OpArg cself;
                                if(cfi->selfClosed)
                                {
                                    cself = OpArg(atLocal, f->argsCount);
                                }
                                size_t ccnt = cfi->closedVars.size();
                                OpArg* carr = ccnt ? &cfi->closedVars[0] : nullptr;
                                op += new OpMakeClosure(cf.src, cf.dst, cself, ccnt, carr);
                            }
                            op += new OpMakeClosure(src, ec.dst, self, cnt, arr);
                            return op;
                        }
                    }
                        break;
                    case sytMethod:
                        src.at = atGlobal;
                        break;
                    case sytProperty:
                    {
                        auto* cp = (ClassPropertyInfo*) sym;
                        if(cp->getIdx != SymInfo::invalidIndexValue)
                        {
                            src = OpArg(atMember, cp->getIdx);
                        } else if(cp->getMethod)
                        {
                            //src=OpArg(atGlobal,cp->getMethod->index);
                            if(si->currentScope->st != sytMethod)
                            {
                                abort();
                            }
                            auto* curMethod = (MethodInfo*) si->currentScope;
                            OpPair op(expr->pos, vm);
                            op += new OpCallMethod(0, OpArg(atLocal, curMethod->argsCount), cp->getMethod->localIndex,
                                                   ec.dst);
                            return op;
                        } else
                        {
                            ZTHROW(CGException, expr->pos, "Property %s cannot be read", sym->name);
                        }
                    }
                        break;
                    default:
                        abort();
                        break;
                }
                src.idx = sym->index;
            } else
            {
                bool isLocal;
                src.idx = si->registerVar(expr->val, isLocal);
                src.at = isLocal ? atLocal : atGlobal;
            }
            return fillDst(expr->pos, src, ec);
        }
        case etCall:
        {
            OpPair op(expr->pos, vm);
            OpArg self;
            OpArg func;
            size_t cnt = 0;
            ExprContext ec2(si);
            bool methodCall = false;
            size_t methodIndex = SymInfo::invalidIndexValue;
            if(si->currentClass)
            {
                Expr* mname = expr->e1;
                if(mname->et == etVar && mname->ns && mname->ns->values.size() == 1 &&
                   mname->ns->values.begin()->val->equalsTo("super", 5))
                {
                    SymInfo* sym = si->currentClass->symMap.findSymbol(mname->val);
                    if(!sym)
                    {
                        throw UndefinedSymbolException(mname->getSymbol());
                    }
                    if(sym->st != sytMethod)
                    {
                        ZTHROW(CGException, expr->pos, "Symbol %{} is not method", mname->getSymbol());
                    }
                    auto* mi = (MethodInfo*) sym;
                    if(mi->overIndex == SymInfo::invalidIndexValue)
                    {
                        ZTHROW(CGException, expr->pos, "Method %{} wasn't overriden", mname->getSymbol());
                    }
                    methodCall = true;
                    methodIndex = mi->overIndex;
                } else
                {
                    SymInfo* sym = isSpecial(expr->e1);
                    methodCall = sym && sym->st == sytMethod;
                    if(methodCall)
                    {
                        methodIndex = ((MethodInfo*) sym)->localIndex;
                    }
                }
            }
            if(methodCall)
            {
                size_t selfIdx = si->getSymbol(Symbol(selfName, nullptr))->index;
                DPRINT("self idx=%u\n", static_cast<unsigned int>(selfIdx));
                self = OpArg(atLocal, selfIdx);
                //func=OpArg(atGlobal,fidx);
            }/*else
      if(expr->e1->et==etProp)
      {
        methodCall=true;
        self=genArgExpr(op,expr->e1->e1,ec2);
        func=genArgExpr(op,expr->e1->e2,ec2);
        OpArg dst=ec2.mkTmpDst();
        op+=new OpGetProp(self,func,dst);
        dst.tmp();
        func=dst;
      }*/
            else
            {
                func = genArgExpr(op, expr->e1, ec2);
            }
            std::vector<NamedArgInfo> namedArgs;
            if(expr->lst)
            {
                for(auto arg : *expr->lst)
                {
                    if(arg->et == etPair)
                    {
                        OpArg nameDst;
                        OpArg valDst;
                        if(isSimple(arg->e1))
                        {
                            nameDst = getArgType(arg->e1, false);
                        } else
                        {
                            nameDst = OpArg(atLocal, si->acquireTemp(), true);
                            ExprContext ec3(si, nameDst);
                            op += generateExpr(arg->e1, ec3);
                        }
                        if(isSimple(arg->e2))
                        {
                            valDst = getArgType(arg->e2, false);
                        } else
                        {
                            valDst = OpArg(atLocal, si->acquireTemp(), true);
                            ExprContext ec3(si, valDst);
                            op += generateExpr(arg->e2, ec3);
                        }
                        namedArgs.emplace_back(nameDst, valDst);
                    } else
                    {
                        ExprContext ec3(si, atStack);
                        op += generateExpr(arg, ec3);
                        ++cnt;
                    }
                }
            }
            if(!namedArgs.empty())
            {
                ExprContext ec3(si);
                OpArg mapArg = ec3.mkTmpDst();
                op += new OpMakeMap(OpArg(), mapArg);

                for(auto& namedArg : namedArgs)
                {
                    op += new OpSetKey(mapArg, namedArg.nameDst, namedArg.valDst, OpArg());
                }
                op += new OpPush(mapArg);
                ++cnt;
            }
            if(ec.dst.at == atNul && ec.ifContext)
            {
                ec.mkTmpDst();
            }
            if(methodCall)
            {
                op += OpPair(expr->pos, vm, new OpCallMethod(cnt, self, methodIndex, ec.dst, !namedArgs.empty()));
            } else
            {
                bool isTemp = func.isTemporal;
                func.isTemporal = false;
                if(namedArgs.empty())
                {
                    op += OpPair(expr->pos, vm, new OpCall(cnt, func, ec.dst));
                } else
                {
                    op += OpPair(expr->pos, vm, new OpNamedArgsCall(cnt, func, ec.dst));
                }
                if(isTemp)
                {
                    op += new OpAssign(func, nil, OpArg());
                }
            }
            if(ec.ifContext)
            {
                OpJumpBase* jmp;
                op += ec.addJump(jmp = new OpCondJump(ec.dst));
                op.addFix(jmp->next);
            }
            if(!namedArgs.empty())
            {
                for(auto& namedArg : namedArgs)
                {
                    if(namedArg.nameDst.isTemporal)
                    {
                        si->releaseTemp(namedArg.nameDst.idx);
                    }
                    if(namedArg.valDst.isTemporal)
                    {
                        si->releaseTemp(namedArg.valDst.idx);
                    }
                }
            }
            return op;
        }
        case etProp:
        {
            /*OpPair op(expr->pos,vm);
      ExprContext ec1(si);
      OpArg obj=genArgExpr(op,expr->e1,ec1);
      OpArg prop=genArgExpr(op,expr->e2,ec1);
      if(ec.lvalue)
      {
        op+=new OpMakeMemberRef(obj,prop,ec.dst);
      }else
      {
        op+=new OpGetProp(obj,prop,ec.dst);
      }
      return op;*/
            if(ec.lvalue)
            {
                ec.lvalue = false;
                return genBinOp<OpMakeMemberRef>(expr, ec);
            } else
            {
                return genBinOp<OpGetProp>(expr, ec);
            }
        }
        case etPropOpt:
        {
            if(ec.lvalue)
            {
                ZTHROW(CGException, expr->pos, "Optional property cannot be assigned to.");
            }
            return genBinOp<OpGetPropOpt>(expr, ec);
        }
        case etKey:
        {
            /*
      OpPair op(expr->pos,vm);
      OpArg obj,prop;
      ExprContext ec2(si);
      obj=genArgExpr(op,expr->e1,ec2);
      prop=genArgExpr(op,expr->e2,ec2);
      if(ec.lvalue)
      {
        op+=new OpMakeKey(obj,prop,ec.dst);
      }else
      {
        op+=new OpGetKey(obj,prop,ec.dst);
      }
      return op;*/
            if(ec.lvalue)
            {
                ec.lvalue = false;
                return genBinOp<OpMakeKey>(expr, ec);
            } else
            {
                return genBinOp<OpGetKey>(expr, ec);
            }

        }
        case etPair:
            return OpPair(expr->pos, nullptr);//never happen
        case etArray:
        {
            OpArg arr;
            std::vector<ArrayInitPair> init;
            bool haveConst = false;
            bool allConst = false;
            if(expr->lst)
            {
                for(auto& it : *expr->lst)
                {
                    if(isConstant(it))
                    {
                        haveConst = true;
                    } else
                    {
                        allConst = false;
                    }
                }
                if(haveConst)
                {
                    arr.at = atGlobal;
                    arr.idx = si->registerArray();
                    ZArray* za = si->globals[arr.idx].arr;
                    size_t cnt = expr->lst ? expr->lst->size() : 0;
                    za->resize(cnt);
                    size_t i = 0;
                    for(auto it = expr->lst->begin(), end = expr->lst->end(); it != end; ++it, ++i)
                    {
                        Value& item = za->getItemRef(i);
                        if(fillConstant(*it, item))
                        {
                            if(ZISREFTYPE(&item))
                            {
                                item.refBase->ref();
                                za->isSimpleContent = false;
                            }
                        } else
                        {
                            allConst = false;
                            init.emplace_back(i, *it);
                        }
                    }
                } else
                {
                    size_t i = 0;
                    for(auto it = expr->lst->begin(), end = expr->lst->end(); it != end; ++it, ++i)
                    {
                        init.emplace_back(i, *it);
                    }
                }
            }
            if(allConst && ec.constAccess)
            {
                return fillDst(expr->pos, arr, ec);
            }
            OpArg dst = init.empty() ? ec.dst : OpArg(atLocal, si->acquireTemp()).tmp();
            OpPair op = OpPair(expr->pos, vm, new OpMakeArray(arr, dst));
            if(!init.empty())
            {
                for(auto& it : init)
                {
                    ExprContext ec2(si);
                    OpArg item = genArgExpr(op, it.expr, ec2);
                    op += new OpInitArrayItem(dst, item, it.index);
                }
                //op+=new OpAssign(ec.dst,dst,atNul);
                op += fillDst(expr->pos, dst, ec);
                si->releaseTemp(dst.idx);
            }
            return op;
        }
        case etMap:
        {
            OpArg map;
            std::vector<MapInitPair> init;
            bool haveNonConst = false;
            if(expr->lst)
            {
                for(auto& it : *expr->lst)
                {
                    if(!isConstant(it))
                    {
                        haveNonConst = true;
                        break;
                    }
                }
                if(haveNonConst)
                {
                    for(auto& it : *expr->lst)
                    {
                        init.push_back(MapInitPair(it->e1, it->e2));
                    }
                } else
                {
                    map.at = atGlobal;
                    map.idx = si->registerMap();
                    ZMap* zm = si->globals[map.idx].map;
                    for(auto& it : *expr->lst)
                    {
                        Expr* key = it->e1;
                        Expr* item = it->e2;
                        Value keyVal = NilValue;
                        Value itemVal = NilValue;
                        fillConstant(key, keyVal);
                        fillConstant(item, itemVal);
                        zm->insert(keyVal, itemVal);
                    }
                }
            }
            if(!haveNonConst && ec.constAccess)
            {
                return fillDst(expr->pos, map, ec);
            }
            OpArg dst = init.empty() ? ec.dst : OpArg(atLocal, si->acquireTemp()).tmp();
            OpPair op = OpPair(expr->pos, vm, new OpMakeMap(map, dst));
            if(!init.empty())
            {
                for(auto& it : init)
                {
                    OpArg key, item;
                    ExprContext ec2(si);
                    key = genArgExpr(op, it.key, ec2);
                    item = genArgExpr(op, it.item, ec2);
                    op += new OpInitMapItem(dst, key, item);
                }
                //op+=new OpAssign(ec.dst,dst,atNul);
                op += fillDst(expr->pos, dst, ec);
                si->releaseTemp(dst.idx);
            }
            return op;
        }
        case etSet:
        {
            OpArg set;
            bool haveAllConst = true;
            if(expr->lst)
            {
                for(auto& it : *expr->lst)
                {
                    if(!isConstant(it))
                    {
                        haveAllConst = false;
                        break;
                    }
                }
                if(haveAllConst)
                {
                    set.at = atGlobal;
                    set.idx = si->registerSet();
                    ZSet* zs = si->globals[set.idx].set;
                    for(auto& it : *expr->lst)
                    {
                        Value val = NilValue;
                        fillConstant(it, val);
                        zs->insert(val);
                    }
                }
            }
            if(haveAllConst && ec.constAccess)
            {
                if(!ec.tmps.empty() && ec.dst.at == atLocal && ec.dst.idx == ec.tmps.back())
                {
                    ec.dst = set;
                    return OpPair(expr->pos, vm);
                }
                return fillDst(expr->pos, set, ec);
            }
            OpArg dst = ec.dst;
            OpPair op = OpPair(expr->pos, vm, new OpMakeSet(set, dst));
            if(expr->lst && !haveAllConst)
            {
                for(auto& it : *expr->lst)
                {
                    OpArg item;
                    ExprContext ec2(si);
                    item = genArgExpr(op, it, ec2);
                    op += new OpInitSetItem(dst, item);
                }
            }
            return op;
        }
        case etRange:
        {
            /*if(isConstant(expr->e1) && isConstant(expr->e2) && (!expr->e3 || isConstant(expr->e3)))
      {
        int idx=si->registerRange();
        Value& r=vm->getGlobal(idx);
        if(expr->e1->et!=etInt || expr->e2->et!=etInt || (expr->e3 && expr->e3->et!=etInt))
        {
          throw SyntaxError("Range can only have int arguments",expr->pos);
        }
        r.range->start=ZString::parseInt(expr->e1->val.c_str());
        r.range->end=ZString::parseInt(expr->e2->val.c_str());
        if(!expr->rangeIncl)
        {
          --r.range->end;
        }
        if(expr->e3)
        {
          r.range->step=ZString::parseInt(expr->e3->val.c_str());
        }else
        {
          r.range->step=1;
        }
        if(ec.dst.at==atStack)
        {
          return OpPair(expr->pos,new OpPush(OpArg(atGlobal,idx)));
        }else
        {
          return OpPair(expr->pos,new OpLoadReg(OpArg(atGlobal,idx)));
        }
      }*/
            OpPair op(expr->pos, vm);
            OpArg start, end, step = atNul;
            ExprContext ec2(si);
            if(isSimple(expr->e1))
            {
                start = getArgType(expr->e1);
            } else
            {
                start = ec2.mkTmpDst();
                op += generateExpr(expr->e1, ec2);
            }
            if(isSimple(expr->e2))
            {
                end = getArgType(expr->e2);
            } else
            {
                end = ec2.mkTmpDst();
                op += generateExpr(expr->e2, ec2);
            }
            if(expr->e3)
            {
                if(isSimple(expr->e3))
                {
                    step = getArgType(expr->e3);
                } else
                {
                    step = ec2.mkTmpDst();
                    op += generateExpr(expr->e3, ec2);
                }
            }

            op += new OpMakeRange(start, end, step, ec.dst, expr->rangeIncl);

            return op;
        }
        case etIndex:
            if(ec.lvalue)
            {
                ec.lvalue = false;
                return genBinOp<OpMakeIndex>(expr, ec);
            } else
            {
                return genBinOp<OpGetIndex>(expr, ec);
            }
        case etFunc:
        {
            char name[32];
            snprintf(name, sizeof(name), "anonymous-%p", expr);
            Name nm(vm->mkZString(name), expr->pos);
            FuncDeclStatement& fds = *expr->func;
            fds.name = nm;
            FuncInfo* fi;
            size_t argsCount = fds.argsCount();
            bool numArgs = false;
            if(expr->exprFunc)
            {
                std::vector<size_t> argIndexes;
                auto& rs = fds.body->front()->as<ReturnStatement>();
                gatherNumArgs(rs.expr, argIndexes);
                if(!argIndexes.empty())
                {
                    if(fds.args)
                    {
                        ZTHROW(CGException, expr->pos, "Either numeric or named arguments can be used.");
                    }
                    std::sort(argIndexes.begin(), argIndexes.end());
                    if(argIndexes.front() != 1)
                    {
                        ZTHROW(CGException, expr->pos, "Numeric arguments must start with 1.");
                    }
                    for(size_t i = 1; i < argIndexes.size(); ++i)
                    {
                        if(argIndexes[i - 1] + 1 != argIndexes[i])
                        {
                            ZTHROW(CGException, expr->pos, "Numeric arguments can't have gaps.");
                        }
                    }
                    argsCount = argIndexes.size();
                    numArgs = true;
                }
            }
//      if(currentClass)
//      {
//        fi=si->registerMethod(nm,argsCount);
//      }else
//      {
            fi = si->registerFunc(nm, argsCount);
//      }
            ScopeGuard sg(si);
            OpPair fp(expr->pos, vm);
            index_type selfIdx;
            OpPair argOps = genArgs(fp, fds.args, false, selfIdx);
            if(numArgs)
            {
                for(size_t idx = 1; idx <= argsCount; ++idx)
                {
                    si->registerArg(vm->mkZString(FORMAT("\\%{}", idx)));
                }
            }
            try
            {
                fp += generateStmtList(fds.body);
            }
            catch(...)
            {
                fi->defValEntries.clear();
                throw;
            }

            if(!fp.isLastReturn())
            {
                fp += new OpReturn();
            }
            if(fi->isClosure())
            {
                si->registerLocalVar(vm->mkZString("closure-storage"));
            }
            size_t ccount = si->currentScope->closedVars.size();
            OpArg* idxPtr = ccount ? &si->currentScope->closedVars[0] : nullptr;
            finArgs(fp, argOps);
            sg.release();

            //si->globals[idx].func->entry->ref();
            if(ccount == 0 && !fi->selfClosed)
            {
                return fillDst(expr->pos, OpArg(atGlobal, fi->index), ec);
            } else
            {
                /*bool isMethod=false;
        for(OpArg* it=idxPtr,*end=idxPtr+ccount;it!=end;++it)
        {
          if(it->at==atMember)
          {
            isMethod=true;
          }
        }*/
                OpArg self = fi->selfClosed ? OpArg(atLocal, ((FuncInfo*) si->currentScope)->argsCount) : OpArg();
                return OpPair(expr->pos, vm,
                              new OpMakeClosure(OpArg(atGlobal, fi->index), ec.dst, self, ccount, idxPtr));
            }
        }
        case etFormat:
        {
            ExprContext ec2(si);
            int w = -1, p = -1;
            OpArg src, width, prec, flags, extra;
            OpPair op(expr->pos, vm);
            bool haveWidth = false;
            bool havePrec = false;
            if(expr->val)
            {
                CStringWrap wrap(nullptr, 0, 0);
                const char* fmt = expr->val.c_str(wrap) + 1;
                int n;
                if(isdigit(*fmt))
                {
                    sscanf(fmt, "%d%n", &w, &n);
                    fmt += n;
                } else if(*fmt == '*')
                {
                    haveWidth = true;
                    fmt++;
                }
                if(*fmt == '.')
                {
                    fmt++;
                }
                if(isdigit(*fmt))
                {
                    sscanf(fmt, "%d%n", &p, &n);
                    fmt += n;
                } else if(*fmt == '*')
                {
                    havePrec = true;
                }
                if(*fmt == 'l' || *fmt == 'r' || *fmt == 'z' || *fmt == 'x' || *fmt == 'X')
                {
                    flags = OpArg(atGlobal, si->getStringConst(si->getStringConstVal(fmt)));
                }
            }
            if((haveWidth || havePrec) && expr->e1)
            {
                throw CGException("format expression with variable width/precision requires additional arguments",
                                  expr->pos);
            }
            if(!expr->e1)
            {
                ExprList& lst = *expr->lst;
                if((int) lst.size() < haveWidth + havePrec + 1)
                {
                    throw CGException("not enough arguments for format expression", expr->pos);
                }
                auto it = lst.begin(), end = lst.end();
                if(haveWidth)
                {
                    width = genArgExpr(op, *it, ec2);
                    ++it;
                }
                if(havePrec)
                {
                    prec = genArgExpr(op, *it, ec2);
                    ++it;
                }
                if(++it != end)
                {
                    --it;
                    extra = genArgExpr(op, *it, ec2);
                    ++it;
                } else
                {
                    --it;
                }
                src = genArgExpr(op, *it, ec2);
            } else
            {
                src = genArgExpr(op, expr->e1, ec2);
            }
            op += new OpFormat(w, p, src, ec.dst, width, prec, flags, extra);
            return op;
        }
            break;
        case etCombine:
        {
            ExprList& lst = *expr->lst;
            size_t count = lst.size();
            auto* args = new OpArg[count];
            int idx = 0;
            ExprContext ec2(si);
            OpPair op(expr->pos, vm);
            for(auto& it : lst)
            {
                args[idx++] = genArgExpr(op, it, ec2);
            }
            op += new OpCombine(args, count, ec.dst);
            return op;
        }
            break;
        case etCount:
        {
            OpPair op(expr->pos, vm);
            ExprContext ec2(si);
            OpArg src = genArgExpr(op, expr->e1, ec2);
            if(ec.ifContext && ec.dst.at == atNul)
            {
                ec.mkTmpDst();
            }
            op += new OpCount(src, ec.dst);
            if(ec.ifContext)
            {
                op += ec.addJump(new OpCondJump(ec.dst));
            }
            return op;
        }
            break;
        case etTernary:
        {
            OpPair op(expr->pos, vm);
            ExprContext ec2(si);
            op += generateExpr(expr->e1, ec2.setIf());
            op += generateExpr(expr->e2, ec);
            OpPair falseOp = generateExpr(expr->e3, ec);
            if(!*falseOp.first)
            {
                for(auto& jump : ec2.jumps)
                {
                    if(!jump->elseOp)
                    {
                        op.addFix(jump->elseOp);
                    }
                }
            } else
            {
                for(auto& jump : ec2.jumps)
                {
                    if(!jump->elseOp)
                    {
                        jump->elseOp = *falseOp.first;
                    }
                }
            }
            falseOp.release();
            op.chainFixes(falseOp);
            return op;
        }
            break;
        case etGetAttr:
        {
            OpPair op(expr->pos, vm);
            ExprContext ec2(si);
            OpArg obj = genArgExpr(op, expr->e1, ec2);
            OpArg memb = expr->e2 ? genArgExpr(op, expr->e2, ec2) : OpArg();
            OpArg att;
            att.at = atGlobal;
            AttrSym* attr = si->getAttr(expr->e3->getSymbol());
            if(!attr)
            {
                throw CGException("attr not found", expr->e3->pos);
            }
            att.idx = attr->index;
            if(ec.ifContext && ec.dst.at == atNul)
            {
                ec.mkTmpDst();
            }
            op += new OpGetAttr(obj, memb, att, ec.dst);
            if(ec.ifContext)
            {
                op += ec.addJump(new OpCondJump(ec.dst));
            }
            return op;
        }
            break;
        case etNumArg:
        {
            size_t index = static_cast<size_t>(ZString::parseInt(expr->val.c_str() + 1) - 1);
            return fillDst(expr->pos, OpArg(atLocal, index), ec);
        }
            break;
        case etMapOp:
            break;
        case etGrepOp:
            break;
            //case etSumOp:break;
        case etSeqOps:
        {
            OpPair op(expr->pos, vm);
            ExprContext ec2(si);
            OpArg dst = ec2.mkTmpDst();
            if(expr->e1->et != etAssign)
            {
                op += new OpMakeArray(OpArg(), dst);
            } else
            {
                ExprContext ec4(si, dst);
                op += generateExpr(expr->e1, ec4);
            }
            Expr* varExpr = expr->e1->et == etVar ? expr->e1 : expr->e1->e1;
            OpArg var = getArgType(varExpr, true);
            OpArg tmp = ec2.mkTmpDst();
            ExprContext ec3(si);
            OpArg src = genArgExpr(op, expr->e2, ec3);
            auto* forInit = new OpForInit(var, src, tmp);
            auto* forCor = new OpForCheckCoroutine(tmp);
            forCor->pos = expr->pos;
            forInit->corOp = forCor;
            op += forInit;
            auto* forStep = new OpForStep(var, tmp);
            forStep->corOp = forCor;
            op += forStep;
            //si->enterBlock(Name(),forStep);
            OpPair opBody(expr->pos, vm);
            for(auto opExpr : *expr->lst)
            {
                switch(opExpr->et)
                {
                    case etMapOp:
                    {
                        ExprContext ec4(si, var);
                        opBody += generateExpr(opExpr->e1, ec4);
                    }
                        break;
                    case etGrepOp:
                    {
                        ExprContext ec4(si);
                        opBody += generateExpr(opExpr->e1, ec4.setIf());
                        for(auto& jump : ec4.jumps)
                        {
                            jump->elseOp = forStep;
                        }
                    }
                        break;
                    default:
                        abort();
                        break;
                }
            }
            opBody += new OpSAdd(dst, var, atNul);
            forInit->next = *opBody.first;
            forCor->next = *opBody.first;
            op += opBody;
            op.fixLast(forStep);
            op.setLast(forCor->endOp = forInit->endOp = forStep->endOp = new OpAssign(tmp, nil, atNul));
            if(src.isTemporal)
            {
                op += new OpAssign(src, nil, atNul);
            }
            //si->leaveBlock();
            op += fillDst(expr->pos, dst, ec);
            op += new OpAssign(dst, nil, atNul);
            return op;
        }
            break;
        case etRegExp:
            return fillDst(expr->pos, getArgType(expr), ec);
        case etLiteral:
        {
            LiterInfo* li = si->getLiter(expr->lst);
            if(!li)
            {
                throw CGException("Unrecognized literal", expr->pos);
            }
            ExprContext ec2(si, atStack);
            OpPair op(expr->pos, vm);
            auto ait = li->markers.begin();
            if(ait->lt == ltString)
            {
                op += generateExpr(*++(expr->lst->begin()), ec2);
            } else
            {
                for(auto it = expr->lst->begin(), end = expr->lst->end(); it != end; ++it)
                {
                    Expr* e1 = *it;
                    ++it;
                    Expr* e2 = it != end ? *it : 0;
                    if(e2)
                    {
                        while(ait->marker.val != e2->val)
                        {
                            op += new OpPush(nil);
                            ++ait;
                        }
                        ++ait;
                    } else
                    {
                        if(!ait->marker.val)
                        {
                            ++ait;
                        }
                    }
                    op += generateExpr(e1, ec2);
                    if(!e2)
                    {
                        --it;
                    }
                }
                for(; ait != li->markers.end(); ++ait)
                {
                    op += new OpPush(nil);
                }
            }
            op += new OpCall(li->argsCount, OpArg(atGlobal, li->index), ec.dst);
            return op;
        }
            break;
    }
    return OpPair(expr->pos, nullptr);
}

}
