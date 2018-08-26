#include "ZVMOps.hpp"
#include "ZorroVM.hpp"
#include <stdexcept>
#include <stdlib.h>

namespace zorro {


const char* getOpName(int ot)
{
    switch(ot)
    {
#define NCASE(id) case id:return #id
        NCASE(otPush);
        NCASE(otAssign);
        NCASE(otAdd);
        NCASE(otSAdd);
        NCASE(otSub);
        NCASE(otSSub);
        NCASE(otMul);
        NCASE(otSMul);
        NCASE(otDiv);
        NCASE(otSDiv);
        NCASE(otMod);
        NCASE(otSMod);
        NCASE(otMatch);
        NCASE(otPreInc);
        NCASE(otPostInc);
        NCASE(otPreDec);
        NCASE(otPostDec);
        NCASE(otEqual);
        NCASE(otNotEqual);
        NCASE(otLess);
        NCASE(otGreater);
        NCASE(otLessEq);
        NCASE(otGreaterEq);
        NCASE(otNot);
        NCASE(otNeg);
        NCASE(otCopy);
        NCASE(otIn);
        NCASE(otBitOr);
        NCASE(otBitAnd);
        NCASE(otMakeRef);
        NCASE(otMakeWeakRef);
        NCASE(otMakeCor);
        NCASE(otGetIndex);
        NCASE(otMakeIndex);
        NCASE(otMakeArray);
        NCASE(otInitArrayItem);
        NCASE(otSetArrayItem);
        NCASE(otGetKey);
        NCASE(otGetType);
        NCASE(otSetKey);
        NCASE(otMakeKey);
        NCASE(otMakeMap);
        NCASE(otInitMapItem);
        NCASE(otMakeSet);
        NCASE(otInitSetItem);
        NCASE(otGetProp);
        NCASE(otSetProp);
        NCASE(otMakeMemberRef);
        NCASE(otMakeRange);
        NCASE(otForInit);
        NCASE(otForStep);
        NCASE(otForInit2);
        NCASE(otForStep2);
        NCASE(otForCheckCoroutine);
        NCASE(otJump);
        NCASE(otJumpIfInited);
        NCASE(otJumpIfLess);
        NCASE(otJumpIfLessEq);
        NCASE(otJumpIfGreater);
        NCASE(otJumpIfGreaterEq);
        NCASE(otJumpIfEqual);
        NCASE(otJumpIfNotEqual);
        NCASE(otJumpIfNot);
        NCASE(otJumpIfIn);
        NCASE(otCondJump);
        NCASE(otJumpFallback);
        NCASE(otReturn);
        NCASE(otCall);
        NCASE(otNamedArgsCall);
        NCASE(otCallMethod);
        NCASE(otNamedArgsCallMethod);
        NCASE(otInitDtor);
        NCASE(otFinalDestroy);
        NCASE(otEnterTry);
        NCASE(otLeaveCatch);
        NCASE(otThrow);
        NCASE(otMakeClosure);
        NCASE(otMakeCoroutine);
        NCASE(otYield);
        NCASE(otEndCoroutine);
        NCASE(otMakeConst);
        NCASE(otRangeSwitch);
        NCASE(otHashSwitch);
        NCASE(otFormat);
        NCASE(otCombine);
        NCASE(otCount);
        NCASE(otGetAttr);
    }
    return "unknown";
}

const char* getArgTypeName(int at)
{
    switch(at)
    {
        NCASE(atNul);
        NCASE(atGlobal);
        NCASE(atLocal);
        NCASE(atMember);
        NCASE(atClosed);
        NCASE(atStack);
    }
    return "unknown";
}

#undef NCASE

/*
static void unimplemented(ZorroVM* vm,OpBase* op)
{
  DPRINT("unimplemented op");
  return 0;
}
*/

#define GETARG(oa) (vm->ctx.dataPtrs[(oa).at]+(oa).idx)
#define GETDST(isStack, da) (isStack?&(*vm->ctx.dataStack.push()=NilValue):(vm->ctx.dataPtrs[da.at]+da.idx))
#define INITCALL(cop, ret, argsCount, self) \
    CallFrame* cf=vm->ctx.callStack.push();\
    cf->callerOp=cop;\
    cf->retOp=ret;\
    cf->localBase=vm->ctx.dataStack.size()-argsCount;\
    cf->args=argsCount;\
    cf->funcIdx=0u;\
    cf->selfCall=self;\
    cf->closedCall=false;\
    cf->overload=false


static void Push(ZorroVM* vm, OpPush* op)
{
    //vm->ctx.dataStack.push(NilValue);
    Value* dst = vm->ctx.dataStack.push();
    Value* src = GETARG(op->src);
    ZASSIGN(vm, dst, src);
    if(op->src.isTemporal)
    {
        ZUNREF(vm, src);
    }

}


OpPush::OpPush(const OpArg& argSrc) : src(argSrc)
{
    ot = otPush;
    op = (OpFunc) Push;
}


template<OpType opType, bool isLeftTemp, bool isRightTemp, bool isDstStack>
static void binopFunc(ZorroVM* vm, OpBinOp* op)
{
    Value* d = GETDST(isDstStack, op->dst);
    Value* l = GETARG(op->left);
    Value* r = GETARG(op->right);
    switch(opType)
    {
        case otAssign:
        ZASSIGN(vm, l, r);
            if(d)
            {
                ZASSIGN(vm, d, r);
            }
            break;
        case otAdd:
            vm->addMatrix[l->vt][r->vt](vm, l, r, d);
            break;
        case otSAdd:
            vm->saddMatrix[l->vt][r->vt](vm, l, r, d);
            break;
        case otSub:
            vm->subMatrix[l->vt][r->vt](vm, l, r, d);
            break;
        case otSSub:
            vm->ssubMatrix[l->vt][r->vt](vm, l, r, d);
            break;
        case otMul:
            vm->mulMatrix[l->vt][r->vt](vm, l, r, d);
            break;
        case otSMul:
            vm->smulMatrix[l->vt][r->vt](vm, l, r, d);
            break;
        case otDiv:
            vm->divMatrix[l->vt][r->vt](vm, l, r, d);
            break;
        case otSDiv:
            vm->sdivMatrix[l->vt][r->vt](vm, l, r, d);
            break;
        case otMod:
            vm->modMatrix[l->vt][r->vt](vm, l, r, d);
            break;
        case otSMod:
            vm->smodMatrix[l->vt][r->vt](vm, l, r, d);
            break;
//    case otEqual:vm->eqMatrix[l->vt][r->vt](vm,l,r,d);break;
//    case otLess:vm->lessMatrix[l->vt][r->vt](vm,l,r,d);break;
//    case otIn:vm->inMatrix[l->vt][r->vt](vm,l,r,d);break;
        case otMakeIndex:
            vm->mkIndexMatrix[l->vt][r->vt](vm, l, r, d);
            break;
        case otGetIndex:
            vm->getIndexMatrix[l->vt][r->vt](vm, l, r, d);
            break;
            //case otSetArrayItem:vm->setIndexMatrix[l->vt][r->vt](vm,l,r,d);break;
        case otMakeKey:
            vm->mkKeyRefMatrix[l->vt][r->vt](vm, l, r, d);
            break;
        case otGetKey:
            vm->getKeyMatrix[l->vt][r->vt](vm, l, r, d);
            break;
            //case otSetKey:vm->setKeyMatrix[l->vt][r->vt](vm,l,r,d);break;
        case otGetProp:
            vm->getMemberMatrix[l->vt][r->vt](vm, l, r, d);
            break;
            //case otSetProp:vm->setMemberMatrix[l->vt][r->vt](vm,l,r,d);break;
        case otBitOr:
            vm->bitOrMatrix[l->vt][r->vt](vm, l, r, d);
            break;
        case otBitAnd:
            vm->bitAndMatrix[l->vt][r->vt](vm, l, r, d);
            break;
        default:
            abort();
            break;
    }

    if(isLeftTemp)
    {
        ZUNREF(vm, l);
    }
    if(isRightTemp)
    {
        ZUNREF(vm, r);
    }
#ifdef _MSC_VER
                                                                                                                            #pragma warning(push)
#pragma warning(disable: 4127)
#endif
    if((opType == otSetArrayItem || opType == otSetProp || opType == otSetKey) && op->dst.isTemporal)
    {
        ZUNREF(vm, d);
    }
#ifdef _MSC_VER
#pragma warning(pop)
#endif
}

template<OpType opType, bool isLeftTemp, bool isRightTemp, bool isDstStack>
static void teropFunc(ZorroVM* vm, OpTerOp* op)
{
    Value* d = GETDST(isDstStack, op->dst);
    Value* l = GETARG(op->left);
    Value* a = GETARG(op->arg);
    Value* r = GETARG(op->right);
    switch(opType)
    {
        case otSetArrayItem:
            vm->setIndexMatrix[l->vt][a->vt](vm, l, a, r, d);
            break;
        case otSetKey:
            vm->setKeyMatrix[l->vt][a->vt](vm, l, a, r, d);
            break;
        case otSetProp:
            vm->setMemberMatrix[l->vt][a->vt](vm, l, a, r, d);
            break;
        default:
            abort();
            break;
    }

    if(isLeftTemp)
    {
        ZUNREF(vm, l);
    }
    if(isRightTemp)
    {
        ZUNREF(vm, r);
    }
    if(op->dst.isTemporal)
    {
        ZUNREF(vm, d);
    }
}


#define OPCASE(name, tl, tr, ds) op=(OpFunc)(void(*)(ZorroVM*,OpBinOp*))(&binopFunc<ot##name,tl,tr,ds>)
#define INITBOP(name) Op##name::Op##name(const OpArg& argLeft,const OpArg& argRight,const OpArg& argDst): \
OpBinOp(argLeft,argRight,argDst) \
{\
  ot=ot##name;\
  if(dst.at==atStack){\
  if(left.isTemporal){\
    if(right.isTemporal){\
      OPCASE(name,true,true,true);\
    }else{\
      OPCASE(name,true,false,true);\
    }\
  }else{\
    if(right.isTemporal){\
      OPCASE(name,false,true,true);\
    }else{\
      OPCASE(name,false,false,true);\
    }\
  }\
  }else{\
    if(left.isTemporal){\
      if(right.isTemporal){\
        OPCASE(name,true,true,false);\
      }else{\
        OPCASE(name,true,false,false);\
      }\
    }else{\
      if(right.isTemporal){\
        OPCASE(name,false,true,false);\
      }else{\
        OPCASE(name,false,false,false);\
      }\
    }\
  }\
}

#define TOPCASE(name, tl, tr, ds) op=(OpFunc)(void(*)(ZorroVM*,OpTerOp*))(&teropFunc<ot##name,tl,tr,ds>)
#define INITTOP(name) Op##name::Op##name(const OpArg& argLeft,const OpArg& argArg,const OpArg& argRight,const OpArg& argDst): \
OpTerOp(argLeft,argArg,argRight,argDst) \
{\
  ot=ot##name;\
  if(dst.at==atStack){\
  if(left.isTemporal){\
    if(right.isTemporal){\
      TOPCASE(name,true,true,true);\
    }else{\
      TOPCASE(name,true,false,true);\
    }\
  }else{\
    if(right.isTemporal){\
      TOPCASE(name,false,true,true);\
    }else{\
      TOPCASE(name,false,false,true);\
    }\
  }\
  }else{\
    if(left.isTemporal){\
      if(right.isTemporal){\
        TOPCASE(name,true,true,false);\
      }else{\
        TOPCASE(name,true,false,false);\
      }\
    }else{\
      if(right.isTemporal){\
        TOPCASE(name,false,true,false);\
      }else{\
        TOPCASE(name,false,false,false);\
      }\
    }\
  }\
}


INITBOP(Assign)

INITBOP(Add)

INITBOP(SAdd)

INITBOP(Sub)

INITBOP(SSub)

INITBOP(Mul)

INITBOP(SMul)

INITBOP(Div)

INITBOP(SDiv)

INITBOP(Mod)

INITBOP(SMod)
//INITBOP(Less)
//INITBOP(Equal)
//INITBOP(In)
INITBOP(GetIndex)

INITTOP(SetArrayItem)

INITBOP(MakeIndex)

INITBOP(GetKey)

INITTOP(SetKey)

INITBOP(MakeKey)

INITBOP(GetProp)

INITTOP(SetProp)

INITBOP(BitOr)

INITBOP(BitAnd)

static void GetPropOpt(ZorroVM* vm, OpGetPropOpt* op)
{
    Value* d = GETDST(op->dst.at == atStack, op->dst);
    Value* l = GETARG(op->left);
    Value* r = GETARG(op->right);

    if(l->vt == vtRef || l->vt == vtWeakRef)
    {
        l = &l->valueRef->value;
    }
    if(l->vt == vtObject)
    {
        ClassInfo* ci = l->obj->classInfo;
        SymInfo** infoPtr = ci->symMap.getPtr(r->str);
        if(infoPtr && (*infoPtr)->st == sytClassMember)
        {
            ZASSIGN(vm, d, &(l->obj->members[(*infoPtr)->index]));
        } else
        {
            ZASSIGN(vm, d, &NilValue);
        }
    } else if(l->vt == vtNil)
    {
        ZASSIGN(vm, d, &NilValue);
    } else
    {
        ZTHROWR(TypeException, vm, "Expected object for .? operator");
    }

    if(op->left.isTemporal)
    {
        ZUNREF(vm, l);
    }
    if(op->right.isTemporal)
    {
        ZUNREF(vm, r);
    }
}

OpGetPropOpt::OpGetPropOpt(const OpArg& argLeft, const OpArg& argRight, const OpArg& argDst) :
    OpBinOp(argLeft, argRight, argDst)
{
    ot = otGetPropOpt;
    op = (OpFunc) GetPropOpt;
}

static void Match(ZorroVM* vm, OpMatch* op)
{
    Value* d = GETDST(op->dst.at == atStack, op->dst);
    Value* l = GETARG(op->left);
    Value* r = GETARG(op->right);

    vm->rxMatch(l, r, d, op->vars);

    if(op->left.isTemporal)
    {
        ZUNREF(vm, l);
    }
    if(op->right.isTemporal)
    {
        ZUNREF(vm, r);
    }
}

OpMatch::OpMatch(const OpArg& argLeft, const OpArg& argRight, const OpArg& argDst) :
    OpBinOp(argLeft, argRight, argDst), vars(0)
{
    ot = otMatch;
    op = (OpFunc) Match;
}

static void MakeRef(ZorroVM* vm, OpMakeRef* op)
{
    Value* src;
    Value val;
    Value* dst = GETDST(op->dst.at == atStack, op->dst);
    if(op->src.isTemporal)
    {
        src = GETARG(op->src);
    } else
    {
        val = LvalueRefValue(op->src.idx, op->src.at);
        src = &val;
    }
    vm->refOps[src->vt](vm, src, dst);
    if(op->src.isTemporal)
    {
        ZUNREF(vm, src);
    }
    if(op->dst.at == atStack)
    {
        dst->flags |= ValFlagARef;
    }
}

static void MakeWeakRef(ZorroVM* vm, OpMakeRef* op)
{
    Value* src;
    Value* dst = GETDST(op->dst.at == atStack, op->dst);
    src = GETARG(op->src);

    if(!ZISREFTYPE(src))
    {
        ZTHROWR(TypeException, vm, "Invalid type for weak reference creation: %{}", getValueTypeName(src->vt));
    }
    if(!dst)
    {
        return;
    }

    Value val = vm->mkWeakRef(src);

    ZASSIGN(vm, dst, &val);

    if(op->src.isTemporal)
    {
        ZUNREF(vm, src);
    }
}


static void MakeCor(ZorroVM* vm, OpMakeCor* op)
{
    Value* dst = GETDST(op->dst.at == atStack, op->dst);
    Value* src = GETARG(op->src);
    vm->corOps[src->vt](vm, src, dst);
    if(op->src.isTemporal)
    {
        ZUNREF(vm, src);
    }
}


static void PreInc(ZorroVM* vm, OpPreInc* op)
{
    Value* dst = GETDST(op->dst.at == atStack, op->dst);
    Value* src = GETARG(op->src);
    if(src->vt == vtInt)
    {
        ++src->iValue;
    } else
    {
        vm->incOps[src->vt](vm, src);
    }
    if(dst)
    ZASSIGN(vm, dst, src);
    if(op->src.isTemporal)
    {
        ZUNREF(vm, src);
    }
}

template<bool isSrcEqDst>
static void PostInc(ZorroVM* vm, OpPostInc* op)
{
    Value* dst = GETDST(op->dst.at == atStack, op->dst);
    Value* src = GETARG(op->src);
    Value val;
    if(!dst)
    {
        if(src->vt == vtInt)
        {
            ++src->iValue;
        } else
        {
            vm->incOps[src->vt](vm, src);
        }
        if(op->src.isTemporal)
        {
            ZUNREF(vm, src);
        }
        return;
    }
    if(isSrcEqDst)
    {
        val = NilValue;
        Value* srcVal = &vm->getValue(*src);
        ZASSIGN(vm, &val, srcVal);
    } else
    {
        //unsigned char saveFl=src->flags;
        //src->flags=0;
        ZASSIGN(vm, dst, src);
        //src->flags=saveFl;
    }
    if(src->vt == vtInt)
    {
        ++src->iValue;
    } else
    {
        vm->incOps[src->vt](vm, src);
    }
    if(op->src.isTemporal)
    {
        ZUNREF(vm, src);
    }
    if(isSrcEqDst)
    {
        ZASSIGN(vm, dst, &val);
        ZUNREF(vm, &val);
    }
    return;
}

static void PreDec(ZorroVM* vm, OpPreInc* op)
{
    Value* dst = GETDST(op->dst.at == atStack, op->dst);
    Value* src = GETARG(op->src);
    if(src->vt == vtInt)
    {
        --src->iValue;
    } else
    {
        vm->decOps[src->vt](vm, src);
    }
    if(dst)
    {
        src->flags &= ~ValFlagARef;
        ZASSIGN(vm, dst, src);
    }
    if(op->src.isTemporal)
    {
        ZUNREF(vm, src);
    }
}

template<bool isSrcEqDst>
static void PostDec(ZorroVM* vm, OpPostDec* op)
{
    Value* dst = GETDST(op->dst.at == atStack, op->dst);
    Value* src = GETARG(op->src);
    Value val;
    if(!dst)
    {
        if(src->vt == vtInt)
        {
            --src->iValue;
        } else
        {
            vm->decOps[src->vt](vm, src);
        }
        if(op->src.isTemporal)
        {
            ZUNREF(vm, src);
        }
        return;
    }
    if(isSrcEqDst)
    {
        val = NilValue;
        Value* srcVal = &vm->getValue(*src);
        ZASSIGN(vm, &val, srcVal);
    } else
    {
        ZASSIGN(vm, dst, src);
    }
    if(src->vt == vtInt)
    {
        --src->iValue;
    } else
    {
        vm->decOps[src->vt](vm, src);
    }
    if(op->src.isTemporal)
    {
        ZUNREF(vm, src);
    }
    if(isSrcEqDst)
    {
        ZASSIGN(vm, dst, &val);
        ZUNREF(vm, &val);
    }
    return;
}


static void MakeArray(ZorroVM* vm, OpMakeArray* op)
{
    Value* dst = GETDST(op->dst.at == atStack, op->dst);
    if(!dst)
    {
        return;
    }
    Value* src = GETARG(op->src);
    Value res;
    res.vt = vtArray;
    res.flags = 0;
    res.arr = src ? src->arr->copy() : vm->allocZArray();
    ZASSIGN(vm, dst, &res);
}


static void InitArrayItem(ZorroVM* vm, OpInitArrayItem* op)
{
    Value* arr = GETARG(op->arr);
    Value* item = GETARG(op->item);
    if(arr->vt == vtRef)
    {
        arr = &arr->valueRef->value;
    }
    Value* dstItem = &arr->arr->getItemRef(op->index);
    ZASSIGN(vm, dstItem, item);
    if(ZISREFTYPE(item))
    {
        arr->arr->isSimpleContent = false;
    }
    if(op->item.isTemporal)
    {
        ZUNREF(vm, item);
    }
}


static void initMapItem(ZorroVM* vm, OpInitMapItem* op)
{
    Value* map = GETARG(op->map);
    Value* key = GETARG(op->key);
    Value* item = GETARG(op->item);
    if(map->vt == vtRef)
    {
        map = &map->valueRef->value;
    }
    map->map->insert(*key, *item);
    if(op->key.isTemporal)
    {
        ZUNREF(vm, key);
    }
    if(op->item.isTemporal)
    {
        ZUNREF(vm, item);
    }
}

OpInitMapItem::OpInitMapItem(const OpArg& argMap, const OpArg& argKey, const OpArg& argItem) :
    map(argMap), key(argKey), item(argItem)
{
    ot = otInitMapItem;
    op = (OpFunc) initMapItem;
}

static void initSetItem(ZorroVM* vm, OpInitSetItem* op)
{
    Value* set = GETARG(op->set);
    Value* item = GETARG(op->item);
    if(set->vt == vtRef)
    {
        set = &set->valueRef->value;
    }
    set->set->insert(*item);
    if(op->item.isTemporal)
    {
        ZUNREF(vm, item);
    }
}

OpInitSetItem::OpInitSetItem(const OpArg& argSet, const OpArg& argItem) : set(argSet), item(argItem)
{
    ot = otInitSetItem;
    op = (OpFunc) initSetItem;
}


static void MakeMap(ZorroVM* vm, OpMakeMap* op)
{
    Value* dst = GETDST(op->dst.at == atStack, op->dst);
    Value* src = GETARG(op->src);
    Value res;
    res.vt = vtMap;
    res.flags = 0;
    res.map = src ? src->map->copy() : vm->allocZMap();
    ZASSIGN(vm, dst, &res);
}

static void MakeSet(ZorroVM* vm, OpMakeSet* op)
{
    Value* dst = GETDST(op->dst.at == atStack, op->dst);
    Value* src = GETARG(op->src);
    Value res;
    res.vt = vtSet;
    res.flags = 0;
    res.set = src ? src->set->copy() : vm->allocZSet();
    ZASSIGN(vm, dst, &res);
}


static void Count(ZorroVM* vm, OpCount* op)
{
    Value* dst = GETDST(op->dst.at == atStack, op->dst);
    Value* src = GETARG(op->src);
    vm->countOps[src->vt](vm, src, dst);
    if(op->src.isTemporal)
    {
        ZUNREF(vm, src);
    }
}

static void Neg(ZorroVM* vm, OpNeg* op)
{
    Value* dst = GETDST(op->dst.at == atStack, op->dst);
    Value* src = GETARG(op->src);
    vm->negOps[src->vt](vm, src, dst);
    if(op->src.isTemporal)
    {
        ZUNREF(vm, src);
    }
}

static void Not(ZorroVM* vm, OpNot* op)
{
    Value* dst = GETDST(op->dst.at == atStack, op->dst);
    Value* src = GETARG(op->src);
    if(dst->vt == vtRef)
    {
        dst = &dst->valueRef->value;
    }
    bool val = !vm->boolOps[src->vt](vm, src);
    ZUNREF(vm, dst);
    dst->vt = vtBool;
    dst->flags = 0;
    dst->bValue = val;
    if(op->src.isTemporal)
    {
        ZUNREF(vm, src);
    }
}

static void GetType(ZorroVM* vm, OpGetType* op)
{
    Value* dst = GETDST(op->dst.at == atStack, op->dst);
    Value* src = GETARG(op->src);
    vm->getTypeOps[src->vt](vm, src, dst);
    if(op->src.isTemporal)
    {
        ZUNREF(vm, src);
    }
}

static void Copy(ZorroVM* vm, OpGetType* op)
{
    Value* dst = GETDST(op->dst.at == atStack, op->dst);
    Value* src = GETARG(op->src);
    vm->copyOps[src->vt](vm, src, dst);
    if(op->src.isTemporal)
    {
        ZUNREF(vm, src);
    }
}


/*
#define TCASE(src,dst,OPNAME) case dst:op=(OpFunc)(OpBase*(*)(ZorroVM*,Op##OPNAME*))(&OPNAME<src,dst>);break
#define TWOARGSWITCH(arg1,arg2,OP) \
    switch(arg1){\
      case atMember:\
        switch(arg2){\
          TCASE(atMember,atMember,OP);\
          TCASE(atMember,atLocal,OP);\
          TCASE(atMember,atGlobal,OP);\
          TCASE(atMember,atStack,OP);\
        }break;\
      case atLocal:\
        switch(arg2){\
          TCASE(atLocal,atMember,OP);\
          TCASE(atLocal,atLocal,OP);\
          TCASE(atLocal,atGlobal,OP);\
          TCASE(atLocal,atStack,OP);\
        }break;\
      case atGlobal:\
        switch(arg2){\
          TCASE(atGlobal,atMember,OP);\
          TCASE(atGlobal,atLocal,OP);\
          TCASE(atGlobal,atGlobal,OP);\
          TCASE(atGlobal,atStack,OP);\
        }break;\
      case atStack:\
        switch(arg2){\
          TCASE(atStack,atMember,OP);\
          TCASE(atStack,atLocal,OP);\
          TCASE(atStack,atGlobal,OP);\
          TCASE(atStack,atStack,OP);\
        }break;\
    }
*/

#define INITUOP(name) Op##name::Op##name(const OpArg& argSrc,const OpArg& argDst):\
    OpUnOp(argSrc,argDst)\
{\
  ot=ot##name;\
  op=(OpFunc)name;\
}

INITUOP(MakeRef)

INITUOP(MakeWeakRef)

INITUOP(MakeCor)

INITUOP(PreInc)

INITUOP(PreDec)

INITUOP(MakeArray)

INITUOP(MakeMap)

INITUOP(MakeSet)

INITUOP(Count)

INITUOP(Neg)

INITUOP(Not)

INITUOP(GetType)

INITUOP(Copy)

OpPostInc::OpPostInc(const OpArg& argSrc, const OpArg& argDst) :
    OpUnOp(argSrc, argDst)
{
    ot = otPostInc;
    if(src.at == dst.at && src.idx == dst.idx)
    {
        op = (OpFunc)(void(*)(ZorroVM * , OpPostInc * )) (&PostInc < true > );
    } else
    {
        op = (OpFunc)(void(*)(ZorroVM * , OpPostInc * )) (&PostInc < false > );
    }
}

OpPostDec::OpPostDec(const OpArg& argSrc, const OpArg& argDst) :
    OpUnOp(argSrc, argDst)
{
    ot = otPostDec;
    if(src.at == dst.at && src.idx == dst.idx)
    {
        op = (OpFunc)(void(*)(ZorroVM * , OpPostDec * )) (&PostDec < true > );
    } else
    {
        op = (OpFunc)(void(*)(ZorroVM * , OpPostDec * )) (&PostDec < false > );
    }
}


OpInitArrayItem::OpInitArrayItem(const OpArg& argArr, const OpArg& argItem, size_t argIndex) :
    arr(argArr), item(argItem), index(argIndex)
{
    ot = otInitArrayItem;
    op = (OpFunc) InitArrayItem;
    //TWOARGSWITCH(arr.at,item.at,InitArrayItem);
}


static void MakeMemberRef(ZorroVM* vm, OpMakeMemberRef* op)
{
    Value* dst = GETDST(op->dst.at == atStack, op->dst);
    Value* obj = GETARG(op->obj);
    Value* prop = GETARG(op->prop);
    vm->mkMemberMatrix[obj->vt][prop->vt](vm, obj, prop, dst);
    if(op->obj.isTemporal)
    {
        ZUNREF(vm, obj);
    }
}


OpMakeMemberRef::OpMakeMemberRef(const OpArg& argObj, const OpArg& argProp, const OpArg& argDst) :
    OpDstBase(argDst), obj(argObj), prop(argProp)
{
    ot = otMakeMemberRef;
    op = (OpFunc) MakeMemberRef;
}

static void makeRange(ZorroVM* vm, OpMakeRange* op)
{
    Value* dst = GETDST(op->dst.at == atStack, op->dst);
    Value* start = GETARG(op->start);
    Value* end = GETARG(op->end);
    Value* step = GETARG(op->step);
    if(start->vt == vtRef)
    {
        start = &start->valueRef->value;
    }
    if(end->vt == vtRef)
    {
        end = &end->valueRef->value;
    }
    if(step && step->vt == vtRef)
    {
        step = &step->valueRef->value;
    }
    if(start->vt != vtInt || end->vt != vtInt || (step && step->vt != vtInt))
    {
        if(step)
        {
            ZTHROWR(TypeException, vm, "Invalid types for range: %{}..%{}@%{}", getValueTypeName(start->vt),
                    getValueTypeName(end->vt), getValueTypeName(step->vt));
        } else
        {
            ZTHROWR(TypeException, vm, "Invalid types for range: %{}..%{}", getValueTypeName(start->vt),
                    getValueTypeName(end->vt));
        }
    }
    if(dst)
    {
        Range* r = vm->allocRange();
        r->start = start->iValue;
        r->end = end->iValue - (op->inclusive ? 0 : 1);
        r->step = step ? step->iValue : 1;
        Value res;
        res.vt = vtRange;
        res.flags = 0;
        res.range = r;

        if(op->start.isTemporal)
        {
            ZUNREF(vm, start);
        }
        if(op->end.isTemporal)
        {
            ZUNREF(vm, end);
        }
        if(op->step.isTemporal)
        {
            ZUNREF(vm, step);
        }

        ZASSIGN(vm, dst, &res);
    }

}

OpMakeRange::OpMakeRange(const OpArg& argStart, const OpArg& argEnd, const OpArg& argStep, const OpArg& argDst,
                         bool argInclusive) :
    start(argStart), end(argEnd), step(argStep), dst(argDst), inclusive(argInclusive)
{
    ot = otMakeRange;
    op = (OpFunc) makeRange;
}


static void OverloadJump(ZorroVM* vm, OpJumpOverloadFallback* op)
{
    Value src = *vm->ctx.dataStack.stackTop;
    vm->ctx.dataStack.pop();
    bool val;
    size_t callLevel = vm->ctx.callStack.size();
    if(src.vt != vtBool)
    {
        val = vm->boolOps[src.vt](vm, &src);
    } else
    {
        val = src.bValue;
    }
    if(op->owner->ot == otJumpIfNot)
    {
        val = !val;
    }
    ZUNREF(vm, &src);
    if(callLevel == vm->ctx.callStack.size())
    {
        if(val)
        {
            vm->ctx.nextOp = op->owner->next;
        } else
        {
            vm->ctx.nextOp = op->owner->elseOp;
        }
    }
}


OpJumpOverloadFallback::OpJumpOverloadFallback(OpJumpBase* argOwner) : OpDstBase(atStack), owner(argOwner)
{
    ot = otJumpFallback;
    op = (OpFunc) OverloadJump;
}

void OpJumpOverloadFallback::dump(std::string& out)
{
    owner->dump(out);
    out += " << JumpOverloadFallback";
}


static void CondJump(ZorroVM* vm, OpCondJump* op)
{
    Value* src = GETARG(op->src);
    bool val;

    if(src->vt != vtBool)
    {
        val = vm->boolOps[src->vt](vm, src);
    } else
    {
        val = src->bValue;
    }
    if(op->src.isTemporal)
    {
        ZUNREF(vm, src);
    }
    if(!val)
    {
        vm->ctx.nextOp = op->elseOp;
    }
}

OpCondJump::OpCondJump(OpArg argSrc, OpBase* argElseOp) : OpJumpBase(argElseOp), src(argSrc)
{
    ot = otCondJump;
    op = (OpFunc) CondJump;
}

static void JumpIfInited(ZorroVM* vm, OpCondJump* op)
{
    Value* src = GETARG(op->src);
    if(src->flags & ValFlagInited)
    {
        vm->ctx.nextOp = op->elseOp;
    }
}


OpJumpIfInited::OpJumpIfInited(OpArg argSrc, OpBase* argInitedOp) : OpCondJump(argSrc, argInitedOp)
{
    ot = otJumpIfInited;
    op = (OpFunc) JumpIfInited;
}

static void Jump(ZorroVM* vm, OpJump* op)
{
    size_t inc = vm->ctx.dataStack.size() - vm->ctx.callStack.stackTop->localBase;
    if(op->localSize > inc)
    {
        inc = op->localSize - inc;
        vm->ctx.dataStack.pushBulk(inc);
    }
}

OpJump::OpJump(OpBase* argNext, size_t argLocalSize) : localSize(argLocalSize)
{
    ot = otJump;
    op = (OpFunc) Jump;
    next = argNext;
}


template<OpType ot, bool leftTmp, bool rightTmp>
static void JumpIfBinOp(ZorroVM* vm, OpJumpIfBinOp* op)
{
    Value* l = GETARG(op->left);
    Value* r = GETARG(op->right);
    bool val;
    switch(ot)
    {
        case otLess:
            val = vm->lessMatrix[l->vt][r->vt](vm, l, r);
            break;
        case otGreater:
            val = vm->greaterMatrix[l->vt][r->vt](vm, l, r);
            break;
        case otLessEq:
            val = vm->lessEqMatrix[l->vt][r->vt](vm, l, r);
            break;
        case otGreaterEq:
            val = vm->greaterEqMatrix[l->vt][r->vt](vm, l, r);
            break;
        case otEqual:
            val = vm->eqMatrix[l->vt][r->vt](vm, l, r);
            break;
        case otNotEqual:
            val = !vm->eqMatrix[l->vt][r->vt](vm, l, r);
            break;//vm->neqMatrix[l->vt][r->vt](vm,l,r);break;
        case otIn:
            val = vm->inMatrix[l->vt][r->vt](vm, l, r);
            break;
        case otIs:
            val = vm->isMatrix[l->vt][r->vt](vm, l, r);
            break;
        case otNot:
            val = !(l->vt == vtBool ? l->bValue : vm->boolOps[l->vt](vm, l));
            break;
        default:
            abort();
            break;
    }
    if(leftTmp)
    {
        ZUNREF(vm, l);
    }
    if(rightTmp)
    {
        ZUNREF(vm, r);
    }
    if(!val)
    {
        vm->ctx.nextOp = op->elseOp;
    }
}

#define JUMPIFBINOP(opname) \
OpJumpIf##opname::OpJumpIf##opname(const OpArg& argLeft,const OpArg& argRight,OpBase* argElseOp):\
  OpJumpIfBinOp(argLeft,argRight,argElseOp)\
{\
  ot=otJumpIf##opname; \
  if(left.isTemporal){ \
    if(right.isTemporal) \
      op=(OpFunc)(void (*)(ZorroVM*,OpJumpIfBinOp*))JumpIfBinOp<ot##opname,true,true>;\
    else\
      op=(OpFunc)(void (*)(ZorroVM*,OpJumpIfBinOp*))JumpIfBinOp<ot##opname,true,false>;\
  }else{\
    if(right.isTemporal) \
      op=(OpFunc)(void (*)(ZorroVM*,OpJumpIfBinOp*))JumpIfBinOp<ot##opname,false,true>;\
    else\
      op=(OpFunc)(void (*)(ZorroVM*,OpJumpIfBinOp*))JumpIfBinOp<ot##opname,false,false>;\
   }\
}

JUMPIFBINOP(Less);
JUMPIFBINOP(Greater);
JUMPIFBINOP(LessEq);
JUMPIFBINOP(GreaterEq);
JUMPIFBINOP(Equal);
JUMPIFBINOP(NotEqual);
JUMPIFBINOP(In);
JUMPIFBINOP(Is);
JUMPIFBINOP(Not);

template<bool refReturn, bool noReturn>
static void Return(ZorroVM* vm, OpReturn* op)
{
    CallFrame* oldFrame = vm->ctx.callStack.stackTop;
    Value* result;
    Value* dst;
    size_t targetStack = oldFrame->localBase;
    vm->ctx.nextOp = oldFrame->retOp;
    OpArg oaRes = oldFrame->callerOp->dst;
    bool wasOverload = oldFrame->overload;
    if(!noReturn)
    {
        result = GETARG(op->result);
        if(refReturn)
        {
            result->flags |= ValFlagARef;
        }
    }
    vm->ctx.callStack.pop();
    CallFrame* newFrame = vm->ctx.callStack.stackTop;

    vm->ctx.dataPtrs[atLocal] = vm->ctx.dataStack.stack + newFrame->localBase;
    if(newFrame->selfCall)
    {
        vm->ctx.dataPtrs[atMember] = vm->ctx.dataPtrs[atLocal][newFrame->args].obj->members;
    }
#ifdef DEBUG
    size_t oldStack = vm->ctx.dataStack.size();
#endif

    Value* target = vm->ctx.dataStack.stack + targetStack - 1;
    Value* top = vm->ctx.dataStack.stackTop;
    vm->ctx.dataStack.stackTop = target;

    if(newFrame->closedCall)
    {
        Value* cls = vm->ctx.dataStack.stackTop;
        vm->ctx.dataPtrs[atClosed] = cls->cls->closedValues;
    }

    if(noReturn)
    {
        if(oaRes.at == atStack)
        {
            if(!wasOverload)
            {
                ++target;
                dst = vm->ctx.dataStack.push();
                ZASSIGN(vm, dst, &NilValue);
            }
        } else if(oaRes.at != atNul)
        {
            dst = GETARG(oaRes);
            ZASSIGN(vm, dst, &NilValue);
        }
    } else
    {
        if(oaRes.at == atStack)
        {
            if(wasOverload)
            {
                dst = target;
            } else
            {
                ++target;
                dst = vm->ctx.dataStack.push();
                if(dst != result)
                {
                    ZUNREF(vm, dst);
                }
            }
        } else
        {
            dst = GETARG(oaRes);
        }
        if(dst && dst != result)
        {
            ZASSIGN(vm, dst, result);
        }
    }
    while(target < top)
    {
        ++target;
        ZUNREF(vm, target);
    }
    DPRINT("return (%s->%s) stack %u->%u, lb=%u\n", op->result.toStr().c_str(), oaRes.toStr().c_str(),
           static_cast<unsigned int>(oldStack), static_cast<unsigned int>(targetStack), newFrame->localBase);
}

OpReturn::OpReturn(const OpArg& argResult, bool argRefReturn) : result(argResult), refReturn(argRefReturn)
{
    ot = otReturn;
    if(refReturn)
    {
        op = (OpFunc) (void (*)(ZorroVM*, OpReturn*)) Return<true, false>;
    } else
    {
        op = (OpFunc) (void (*)(ZorroVM*, OpReturn*)) Return<false, false>;
    }
}

OpReturn::OpReturn() : refReturn(false)
{
    ot = otReturn;
    op = (OpFunc) (void (*)(ZorroVM*, OpReturn*)) &Return<false, true>;
}


static void Call(ZorroVM* vm, OpCall* op)
{
    Value* func = GETARG(op->func);

    INITCALL(op, op->next, op->args, false);

    vm->callOps[func->vt](vm, func);

    // func is never temporal to avoid closure destruction

    /*if(op->func.isTemporal)
  {
    ZUNREF(vm,func);
  }*/


    /*if(fv->vt==vtFunc)
  {
    FuncInfo* f=fv->func;
    if(f->argsCount!=op->args)
    {
      throw std::runtime_error("invalid number of arguments");
    }
    if(f->localsCount)
    {
      vm->ctx.dataStack.pushBulk(f->localsCount);
    }
    vm->ctx.dataPtrs[atLocal]=vm->ctx.dataStack.stack+cf->localBase;
    vm->ctx.nextOp=f->entry;
  }else if(fv->vt==vtCFunc)
  {
    fv->cfunc(vm);
    vm->cleanStack(cf->localBase);
    vm->ctx.callStack.pop();

    if(op->dst.at!=atNul)
    {
      Value* dst=GETDST(op->dst.at==atStack,op->dst);
      ZASSIGN(vm,dst,&vm->result);
      vm->clearResult();
    }
  }else if(fv->vt==vtClass)
  {
    int ctorIdx=fv->classInfo->ctorIdx;
    Value* ctor=ctorIdx?&vm->symbols.globals[fv->classInfo->ctorIdx]:0;
    if(ctor && ctor->vt==vtCMethod)
    {
      ctor->cmethod(vm,fv);
      vm->cleanStack(cf->localBase);
      vm->ctx.callStack.pop();
      Value* dst=GETDST(op->dst.at==atStack,op->dst);
      ZASSIGN(vm,dst,&vm->result);
      vm->clearResult();
      return;
    }
    Value res;
    res.vt=vtObject;
    res.obj=vm->allocObj();
    if(fv->classInfo->membersCount)
    {
      res.obj->members=vm->allocVArray(fv->classInfo->membersCount);
      memset(res.obj->members,0,sizeof(Value)*fv->classInfo->membersCount);
    }else
    {
      res.obj->members=0;
    }
    res.obj->classInfo=fv->classInfo;
    fv->classInfo->ref();

    if(ctor)
    {
      vm->ctx.dataPtrs[atLocal]=vm->ctx.dataStack.stack+cf->localBase;
      vm->ctx.dataPtrs[atMember]=res.obj->members;
      cf->selfCall=true;
      FuncInfo* f=ctor->func;
      if(f->argsCount!=op->args)
      {
        throw std::runtime_error("invalid number of arguments");
      }
      //vm->ctx.callStack.push(cf);
      if(f->localsCount)
      {
        //for(int i=0;i<f->locals;i++)vm->ctx.dataStack.push(NilValue);
        vm->ctx.dataStack.pushBulk(f->localsCount);
      }
      Value* self=&ZLOCAL(vm,op->args);
      ZASSIGN(vm,self,&res);
      //vm->setLocal(op->args,res);
      vm->ctx.nextOp=f->entry;
    }else
    {
      vm->ctx.callStack.pop();
      Value* dst=GETDST(op->dst.at==atStack,op->dst);
      ZASSIGN(vm,dst,&res);
    }
  }else if(fv->vt==vtDelegate)
  {
    vm->ctx.dataPtrs[atLocal]=vm->ctx.dataStack.stack+cf->localBase;
    vm->ctx.dataPtrs[atMember]=fv->dlg->obj.obj->members;
    cf->selfCall=true;
    Delegate* dlg=fv->dlg;
    FuncInfo* f=dlg->method;
    if(f->argsCount!=op->args)
    {
      throw std::runtime_error("invalid number of arguments");
    }
    if(f->localsCount)
    {
      //for(int i=0;i<f->locals;i++)vm->ctx.dataStack.push(NilValue);
      vm->ctx.dataStack.pushBulk(f->localsCount);
    }
    Value* self=&ZLOCAL(vm,op->args);
    ZASSIGN(vm,self,&dlg->obj);
    vm->ctx.nextOp=f->entry;
  }else if(fv->vt==vtCDelegate)
  {
    fv->dlg->cmethod(vm,&fv->dlg->obj);
    vm->cleanStack(cf->localBase);
    vm->ctx.callStack.pop();

    if(op->dst.at!=atNul)
    {
      Value* dst=GETDST(op->dst.at==atStack,op->dst);
      ZASSIGN(vm,dst,&vm->result);
      vm->clearResult();
    }
  }else if(fv->vt==vtClosure)
  {
    FuncInfo* f=fv->cls->func;
    if(f->argsCount!=op->args)
    {
      throw std::runtime_error("invalid number of arguments");
    }
    //vm->ctx.callStack.push(cf);
    vm->ctx.dataPtrs[atLocal]=vm->ctx.dataStack.stack+cf->localBase;
    vm->ctx.dataPtrs[atClosed]=fv->cls->closedValues;
    if(f->localsCount)
    {
      //for(int i=0;i<f->locals;i++)vm->ctx.dataStack.push(NilValue);
      vm->ctx.dataStack.pushBulk(f->localsCount);
    }
    vm->ctx.nextOp=f->entry;
  }else if(fv->vt==vtMethod)
  {
    CallFrame* of=vm->ctx.callStack.stackTop-1;
    if(!of->selfCall)
    {
      throw std::runtime_error(std::string("cannot call ")+getValueTypeName(fv->vt));
    }
    Value* self=vm->ctx.dataStack.stack+of->localBase+of->args;
    MethodInfo* mi=(MethodInfo*)fv->func;
    if(self->obj->classInfo!=mi->owningClass && !self->obj->classInfo->isInParents(mi->owningClass))
    {
      throw std::runtime_error(std::string("incompatible class to call ")+getValueTypeName(fv->vt));
    }
    if(mi->argsCount!=op->args)
    {
      throw std::runtime_error("invalid number of arguments");
    }
    vm->ctx.dataPtrs[atLocal]=vm->ctx.dataStack.stack+cf->localBase;
    if(mi->localsCount)
    {
      //for(int i=0;i<f->locals;i++)vm->ctx.dataStack.push(NilValue);
      vm->ctx.dataStack.pushBulk(mi->localsCount);
    }
    vm->ctx.nextOp=mi->entry;
  }else
  {
    throw std::runtime_error(std::string("cannot call ")+getValueTypeName(fv->vt));
  }*/
}

OpCall::OpCall(index_type argArgs, const OpArg& argFunc, const OpArg& argResult) :
    OpCallBase(argArgs, argFunc, argResult)
{
    ot = otCall;
    op = (OpFunc) Call;
}

static void NamedArgsCall(ZorroVM* vm, OpCall* op)
{
    Value* func = GETARG(op->func);
    INITCALL(op, op->next, op->args, false);
    vm->ncallOps[func->vt](vm, func);
}

OpNamedArgsCall::OpNamedArgsCall(index_type argArgs, const OpArg& argFunc, const OpArg& argResult) :
    OpCallBase(argArgs, argFunc, argResult)
{
    ot = otNamedArgsCall;
    op = (OpFunc) NamedArgsCall;
}


static void CallMethod(ZorroVM* vm, OpCallMethod* op)
{
    Value* sv = GETARG(op->self);
    MethodInfo* f = sv->obj->classInfo->methodsTable[op->methodIdx];
    INITCALL(op, op->next, op->args, true);
    OpBase* entry = op->namedArgs ? vm->getFuncEntryNArgs(f, cf) : vm->getFuncEntry(f, cf);
    vm->ctx.dataPtrs[atLocal] = vm->ctx.dataStack.stack + cf->localBase;
    vm->ctx.dataStack.pushBulk(f->localsCount);
    Value* self = &ZLOCAL(vm, cf->args);
    ZASSIGN(vm, self, sv);
    vm->ctx.nextOp = entry;
}

OpCallMethod::OpCallMethod(index_type argArgs, const OpArg& argSelf, index_type argMethodIdx, const OpArg& argResult,
                           bool argNamedArgs) :
    OpDstBase(argResult), self(argSelf), methodIdx(argMethodIdx), args(argArgs), namedArgs(argNamedArgs)
{
    ot = otCallMethod;
    op = (OpFunc) CallMethod;
//  TWOARGSWITCH(self.at,meth.at,CallMethod);
}


static void ForInit(ZorroVM* vm, OpForInit* op)
{
    Value* val = GETARG(op->target);
    Value* var = GETARG(op->dst);
    Value* iter = GETARG(op->temp);
    if(!vm->initForOps[val->vt](vm, val, var, iter))
    {
        vm->ctx.nextOp = op->endOp;
    }
    /*
  Value* val=GETARG(op->target);
  Value* var=GETARG(op->var);
  Value* iter=GETARG(op->temp);
  if(val->vt==vtRange)
  {
    if(val->range->start>val->range->end)
    {
      vm->ctx.nextOp=op->endOp;
      return;
    }
    *var=IntValue(val->range->start);
    iter->vt=vtRange;
    iter->flags=0;
    iter->range=vm->allocRange();
    iter->range->ref();
    iter->range->start=val->range->start;
    iter->range->end=val->range->end;
    iter->range->step=val->range->step;
  }else if(val->vt==vtArray)
  {
    if(val->arr->getCount()==0)
    {
      vm->ctx.nextOp=op->endOp;
      return;
    }
    iter->vt=vtSegment;
    iter->flags=0;
    iter->seg=vm->allocSegment();
    iter->seg->ref();
    iter->seg->cont=*val;
    val->refBase->ref();
    //vm->assign(iter.seg->cont,*val);
    iter->seg->segStart=0;
    iter->seg->segLength=1;
    Value* itemPtr=&iter->seg->cont.arr->getItem(0);
    ZASSIGN(vm,var,itemPtr);
  }else if(val->vt==vtFunc)
  {
    iter->vt=vtCoroutine;
    iter->flags=0;
    iter->cor=vm->allocCoroutine();
    iter->cor->result=op->var;
    iter->cor->ref();
    ZVMContext& ctx=iter->cor->ctx;
    ctx.coroutine=iter->cor;
    ctx.dataPtrs[atNul]=0;
    ctx.dataPtrs[atGlobal]=vm->symbols.globals;
    ctx.dataPtrs[atLocal]=ctx.dataStack.stack;
    ctx.dataPtrs[atMember]=0;
    ctx.dataPtrs[atClosed]=0;
    CallFrame* cf=ctx.callStack.push();
    cf->callerOp=0;
    cf->retOp=0;
    cf->localBase=0;
    cf->args=0;
    cf->selfCall=false;
    cf->closedCall=false;
    cf=ctx.callStack.push();
    cf->callerOp=vm->dtorCallOp;
    cf->retOp=vm->corRetOp;
    cf->localBase=0;
    cf->args=0;
    cf->selfCall=false;
    cf->closedCall=false;
    ctx.nextOp=val->func->entry;
    ctx.dataStack.pushBulk(val->func->localsCount);
    vm->ctx.nextOp=op->corOp;
    vm->ctx.swap(ctx);
  }else if(val->vt==vtSet)
  {
    if(val->set->empty())
    {
      vm->ctx.nextOp=op->endOp;
      return;
    }
    iter->vt=vtForIterator;
    iter->flags=0;
    iter->iter=val->set->getForIter();
    iter->iter->ref();
    iter->iter->cont=*val;
    val->refBase->ref();

    Value* itemPtr=&*val->set->begin();
    ZASSIGN(vm,var,itemPtr);
  }else
  {
    throw std::runtime_error("invalid for type");
  }*/
}

static void ForInit2(ZorroVM* vm, OpForInit2* op)
{
    Value* val = GETARG(op->target);
    Value* var1 = GETARG(op->dst);
    Value* var2 = GETARG(op->var2);
    Value* iter = GETARG(op->temp);
    if(!vm->initFor2Ops[val->vt](vm, val, var1, var2, iter))
    {
        vm->ctx.nextOp = op->endOp;
    }
    /*if(val->vt==vtMap)
  {
    if(val->map->empty())
    {
      vm->ctx.nextOp=op->endOp;
      return;
    }
    iter->vt=vtForIterator;
    iter->flags=0;
    iter->iter=val->map->getForIter();
    iter->iter->ref();
    iter->iter->cont=*val;
    iter->iter->ptr=0;
    val->refBase->ref();

    ZMap::iterator it=val->map->begin();
    Value* keyPtr=&it->m_key;
    Value* valPtr=&it->m_value;
    ZASSIGN(vm,var1,keyPtr);
    ZASSIGN(vm,var2,valPtr);
  }else if(val->vt==vtArray)
  {
    if(val->arr->getCount()==0)
    {
      vm->ctx.nextOp=op->endOp;
      return;
    }
    iter->vt=vtSegment;
    iter->flags=0;
    iter->seg=vm->allocSegment();
    iter->seg->ref();
    iter->seg->cont=*val;
    val->refBase->ref();
    iter->seg->segStart=0;
    iter->seg->segLength=1;
    Value idx;
    idx.vt=vtInt;
    idx.flags=0;
    idx.iValue=0;
    Value* itemPtr=&iter->seg->cont.arr->getItem(0);
    ZASSIGN(vm,var1,&idx);
    ZASSIGN(vm,var2,itemPtr);
  }else
  {
    throw std::runtime_error("invalid for type");
  }*/
}

static void ForCheckCoroutine(ZorroVM* vm, OpForCheckCoroutine* op)
{
    Value* iter = GETARG(op->temp);
    if(iter->cor->ctx.nextOp == 0)
    {
        vm->ctx.nextOp = op->endOp;
    }
}

static void ForStep(ZorroVM* vm, OpForStep* op)
{
    Value* var = GETARG(op->var);
    Value* iter = GETARG(op->temp);
    if(!vm->stepForOps[iter->vt](vm, 0, var, iter))
    {
        vm->ctx.nextOp = op->endOp;
    }
    /*
  if(iter->vt==vtRange)
  {
    if(var->vt!=vtInt)
    {
      ZUNREF(vm,var);
      var->vt=vtInt;
      var->flags=0;
    }
    var->iValue=iter->range->start+=iter->range->step;
    //var->iValue+=iter.range->step;
    //iter.range->start;
    if(var->iValue>iter->range->end)
    {
      vm->ctx.nextOp=op->endOp;
      return;
    }
  }else if(iter->vt==vtSegment)
  {
    ZArray& arr=*iter->seg->cont.arr;
    if(++(iter->seg->segStart)>=arr.getCount())
    {
      vm->ctx.nextOp=op->endOp;
      return;
    }
    Value* itemPtr=&arr.getItem(iter->seg->segStart);
    ZASSIGN(vm,var,itemPtr);
  }else if(iter->vt==vtCoroutine)
  {
    if(iter->cor->ctx.nextOp)
    {
      vm->ctx.nextOp=op->corOp;
      iter->cor->ctx.swap(vm->ctx);
    }else
    {
      vm->ctx.nextOp=op->endOp;
    }
  }else if(iter->vt==vtForIterator)
  {
    if(iter->iter->cont.vt==vtSet)
    {
      if(!iter->iter->cont.set->nextForIter(iter->iter))
      {
        vm->ctx.nextOp=op->endOp;
        return;
      }
      iter->iter->cont.set->getForIterValue(iter->iter,var);
    }
  }
*/
}

static void ForStep2(ZorroVM* vm, OpForStep2* op)
{
    Value* var1 = GETARG(op->var);
    Value* var2 = GETARG(op->var2);
    Value* iter = GETARG(op->temp);
    if(!vm->stepFor2Ops[iter->vt](vm, 0, var1, var2, iter))
    {
        vm->ctx.nextOp = op->endOp;
    }
    /*
  if(iter->vt==vtSegment)
  {
    ZArray& arr=*iter->seg->cont.arr;
    if(++(iter->seg->segStart)>=arr.getCount())
    {
      vm->ctx.nextOp=op->endOp;
      return;
    }
    Value idx;
    idx.vt=vtInt;
    idx.flags=0;
    idx.iValue=iter->seg->segStart;
    Value* itemPtr=&arr.getItem(iter->seg->segStart);
    ZASSIGN(vm,var1,&idx);
    ZASSIGN(vm,var2,itemPtr);
  }else if(iter->vt==vtForIterator)
  {
    if(iter->iter->cont.vt==vtMap)
    {
      if(!iter->iter->cont.map->nextForIter(iter->iter))
      {
        vm->ctx.nextOp=op->endOp;
        return;
      }
      iter->iter->cont.map->getForIterValue(iter->iter,var1,var2);
    }
  }else
  {
    //wtf?
  }*/
}

OpForInit::OpForInit(const OpArg& argVar, const OpArg& argTarget, const OpArg& argTemp) :
    OpDstBase(argVar), target(argTarget), temp(argTemp), endOp(0), corOp(0)
{
    ot = otForInit;
    op = (OpFunc) ForInit;
//#define TCASE(t,v) case v:op=(OpFunc)(OpBase*(*)(ZorroVM*,OpForInit*))(&forInit<t,v>);break
    //TWOARGSWITCH(target.at,var.at,ForInit);
}

OpForInit2::OpForInit2(const OpArg& argVar1, const OpArg& argVar2, const OpArg& argTarget, const OpArg& argTemp) :
    OpForInit(argVar1, argTarget, argTemp), var2(argVar2)
{
    ot = otForInit2;
    op = (OpFunc) ForInit2;
}

OpForCheckCoroutine::OpForCheckCoroutine(OpArg argTemp) : temp(argTemp), endOp(0)
{
    ot = otForCheckCoroutine;
    op = (OpFunc) ForCheckCoroutine;
}

OpForStep::OpForStep(const OpArg& argVar, const OpArg& argTemp) :
    var(argVar), temp(argTemp), endOp(0), corOp(0)
{
    ot = otForStep;
    op = (OpFunc) ForStep;
}

OpForStep2::OpForStep2(const OpArg& argVar, const OpArg& argVar2, const OpArg& argTemp) :
    OpForStep(argVar, argTemp), var2(argVar2)
{
    ot = otForStep2;
    op = (OpFunc) ForStep2;
}


static void InitDtor(ZorroVM* vm, OpInitDtor* op)
{
    vm->ctx.dataPtrs[atLocal] = vm->ctx.dataStack.stack + vm->ctx.callStack.stackTop->localBase;
    vm->ctx.dataStack.pushBulk(op->locals);
    Value* self = &ZLOCAL(vm, 0);
    *self = *vm->ctx.destructorStack.stackTop;
    vm->ctx.dataPtrs[atMember] = self->obj->members;
    vm->ctx.destructorStack.pop();
}


OpInitDtor::OpInitDtor(size_t argLocals) : locals(argLocals)
{
    ot = otInitDtor;
    op = (OpFunc) InitDtor;
}


static void FinalDestroy(ZorroVM* vm, OpFinalDestroy* /*op*/)
{
    Value* obj = &vm->ctx.dataStack.stack[vm->ctx.callStack.stackTop->localBase];
    DPRINT("obj=%p, refCount=%d\n", obj->obj, obj->obj->refCount);
    if(obj->obj->refCount == 1)
    {
        Object& zo = *obj->obj;
        for(size_t i = 0; i < zo.classInfo->membersCount; i++)
        {
            ZUNREF(vm, &zo.members[i]);
        }
        if(zo.members)
        {
            vm->freeVArray(zo.members, zo.classInfo->membersCount);
        }
        if(zo.classInfo->unref())
        {
            delete zo.classInfo;
        }
        zo.classInfo = 0;
    }
}

OpFinalDestroy::OpFinalDestroy()
{
    ot = otFinalDestroy;
    op = (OpFunc) FinalDestroy;
}


static void EnterCatch(ZorroVM* vm, OpEnterTry* op)
{
    vm->ctx.catchStack.push(
        CatchInfo(op->exList, op->exCount, op->idx, vm->ctx.dataStack.size(), vm->ctx.callStack.size(), op->catchOp));
}

OpEnterTry::OpEnterTry(ClassInfo** argExList, index_type argExCount, index_type argIdx, OpBase* argCatchOp) :
    exList(argExList), exCount(argExCount), idx(argIdx), catchOp(argCatchOp)
{
    ot = otEnterTry;
    op = (OpFunc) EnterCatch;
}

OpEnterTry::~OpEnterTry()
{
    /*for(int i=0;i<exCount;++i)
  {
    if(exList[i]->unref())
    {
      delete exList[i];
    }
  }*/
    if(exList)
    {
        delete[] exList;
    }
}


static void LeaveCatch(ZorroVM* vm, OpLeaveCatch* /*op*/)
{
    vm->ctx.catchStack.pop();
}


OpLeaveCatch::OpLeaveCatch()
{
    ot = otLeaveCatch;
    op = (OpFunc) LeaveCatch;
}

static void Throw(ZorroVM* vm, OpThrow* op)
{
    Value* obj = GETARG(op->obj);
    vm->throwValue(obj);
    if(op->obj.isTemporal)
    {
        ZUNREF(vm, obj);
    }
}

OpThrow::OpThrow(OpArg argObj) : obj(argObj)
{
    ot = otThrow;
    op = (OpFunc) Throw;
}

static void MakeClosure(ZorroVM* vm, OpMakeClosure* op)
{
    Value* dst = GETDST(op->dst.at == atStack, op->dst);
    if(!dst)
    {
        return;
    }
    Value* src = GETARG(op->src);
    Closure* val = vm->allocClosure();
    val->func = src->func;
    val->closedValues = vm->allocVArray(op->closedCount);
    val->closedCount = op->closedCount;
    val->self = NilValue;
    if(op->self.at != atNul)
    {
        Value* self = GETARG(op->self);
        ZASSIGN(vm, &val->self, self);
    }
    memset(val->closedValues, 0, sizeof(Value) * op->closedCount);
    Value* dstVal = val->closedValues;
    for(OpArg* ptr = op->closedVars, * end = op->closedVars + op->closedCount; ptr != end; ++ptr, ++dstVal)
    {
        Value var;
        var.vt = vtLvalue;
        var.atLvalue = ptr->at;
        var.idx = ptr->idx;
        vm->refOps[vtLvalue](vm, &var, dstVal);

        //Value* srcVal=GETARG(*ptr);
        //ZASSIGN(vm,dstVal,srcVal);
    }
    Value res;
    res.vt = vtClosure;
    res.flags = 0;
    res.cls = val;
    ZASSIGN(vm, dst, &res);
}

OpMakeClosure::OpMakeClosure(OpArg argSrc, OpArg argDst, OpArg argSelf, index_type argClosedCount,
                             OpArg* argClosedVars) : OpDstBase(argDst),
    src(argSrc), self(argSelf), closedVars(argClosedVars), closedCount(argClosedCount)
{
    ot = otMakeClosure;
    op = (OpFunc) MakeClosure;
}

static void MakeCoroutine(ZorroVM* vm, OpMakeClosure* op)
{
    Value* dst = GETDST(op->dst.at == atStack, op->dst);
    Value* src = GETARG(op->src);
    Coroutine* cor = vm->allocCoroutine();
    cor->ctx.coroutine = cor;
    CallFrame* cf = cor->ctx.callStack.push();
    cf->callerOp = vm->dummyCallOp;
    cf->localBase = 0;
    cf->retOp = 0;
    cf->selfCall = 0;

    Value val;
    val.vt = vtCoroutine;
    val.flags = 0;
    val.cor = cor;
    ZASSIGN(vm, dst, &val);
    if(op->src.isTemporal)
    {
        ZUNREF(vm, src);
    }
}

OpMakeCoroutine::OpMakeCoroutine(OpArg argSrc, OpArg argDst) : OpDstBase(argDst), src(argSrc)
{
    ot = otMakeCoroutine;
    op = (OpFunc) MakeCoroutine;
}

template<bool noReturn>
static void Yield(ZorroVM* vm, OpYield* op)
{
    if(!vm->ctx.coroutine)
    {
        if(!noReturn)
        {
            Value* src = GETARG(op->result);
            src->flags |= ValFlagARef;
            vm->setResult(*src);
            vm->ctx.nextOp = 0;
            return;
        } else
        {
            vm->clearResult();
            vm->ctx.nextOp = 0;
            return;
        }
        //throw std::runtime_error("not in coroutine");
    }
    Coroutine* cor = vm->ctx.coroutine;
    if(noReturn)
    {
        cor->ctx.swap(vm->ctx);
    } else
    {
        Value* src = GETARG(op->result);
        if(op->refYield)
        {
            src->flags |= ValFlagARef;
        }
        cor->ctx.swap(vm->ctx);
        Value* dst = GETDST(cor->result.at == atStack, cor->result);
        ZASSIGN(vm, dst, src);
        if(op->result.isTemporal)
        {
            ZUNREF(vm, src);
        }
    }
    //vm->ctx.callStack.pop();
}

OpYield::OpYield(OpArg argResult, bool argRefYield) : result(argResult), refYield(argRefYield)
{
    ot = otYield;
    op = (OpFunc) (void (*)(ZorroVM*, OpYield*)) Yield<false>;
}

OpYield::OpYield() : refYield(false)
{
    ot = otYield;
    op = (OpFunc) (void (*)(ZorroVM*, OpYield*)) Yield<true>;
}


static void EndCoroutine(ZorroVM* vm, OpBase*)
{
    if(!vm->ctx.coroutine)
    {
        ZTHROWR(RuntimeException, vm, "Attempt to end coroutine when not in coroutine");
    }
    vm->cleanStack(0);
    Coroutine* cor = vm->ctx.coroutine;
    OpArg dstAt = cor->result;
    cor->ctx.swap(vm->ctx);
    if(dstAt.at != atNul)
    {
        Value* dst = GETDST(dstAt.at == atStack, dstAt);
        ZUNREF(vm, dst);
        dst->vt = vtNil;
        dst->flags = 0;
    }
//  cor->ctx.nextOp=0;
    cor->ctx.callStack.reset();
//  while(cor->ctx.destructorStack.size())
//  {
//    vm->ctx.destructorStack.push(*cor->ctx.destructorStack.stackTop);
//    cor->ctx.destructorStack.pop();
//  }
}

OpEndCoroutine::OpEndCoroutine()
{
    ot = otEndCoroutine;
    op = (OpFunc) EndCoroutine;
}

static void MakeConst(ZorroVM* vm, OpMakeConst* op)
{
    Value* var = GETARG(op->var);
    var->flags |= ValFlagConst;
}

OpMakeConst::OpMakeConst(OpArg argVar) : var(argVar)
{
    ot = otMakeConst;
    op = (OpFunc) MakeConst;
}

static void RangeSwitch(ZorroVM* vm, OpRangeSwitch* op)
{
    Value* src = GETARG(op->src);
    Value* val = src->vt == vtRef ? &src->valueRef->value : src;
    if(val->vt != vtInt || val->iValue < op->minValue || val->iValue > op->maxValue)
    {
        vm->ctx.nextOp = op->defaultCase;
    } else
    {
        vm->ctx.nextOp = op->cases[val->iValue - op->minValue];
    }
    if(op->src.isTemporal)
    {
        ZUNREF(vm, src);
    }
}


OpRangeSwitch::OpRangeSwitch(OpArg argSrc) : src(argSrc), minValue(0), maxValue(0), cases(0), defaultCase(0)
{
    ot = otRangeSwitch;
    op = (OpFunc) RangeSwitch;
}

static void HashSwitch(ZorroVM* vm, OpHashSwitch* op)
{
    Value* src = GETARG(op->src);
    Value* val = src->vt == vtRef ? &src->valueRef->value : src;
    if(val->vt != vtString)
    {
        vm->ctx.nextOp = op->defaultCase;
    } else
    {
        OpBase** ptr = op->cases->getPtr(val->str);
        if(ptr)
        {
            vm->ctx.nextOp = *ptr;
        } else
        {
            vm->ctx.nextOp = op->defaultCase;
        }
    }
}

OpHashSwitch::OpHashSwitch(OpArg argSrc, ZorroVM* argVm) : src(argSrc), cases(0), defaultCase(0), vm(argVm)
{
    ot = otHashSwitch;
    op = (OpFunc) HashSwitch;
}

OpHashSwitch::~OpHashSwitch()
{
    ZHash<OpBase*>::Iterator it(*cases);
    ZString* str;
    OpBase** val;
    while(it.getNext(str, val))
    {
        if(str->unref())
        {
            str->clear(vm);
            vm->freeZString(str);
        }
    }
    delete cases;
}

void OpHashSwitch::getBranches(std::vector<OpBase*>& branches)
{
    ZHash<OpBase*>::Iterator it(*cases);
    ZString* str;
    OpBase** val;
    while(it.getNext(str, val))
    {
        branches.push_back(*val);
    }
    branches.push_back(defaultCase);
}


static void Format(ZorroVM* vm, OpFormat* op)
{
    Value* dst = GETDST(op->dst.at == atStack, op->dst);
    Value* src = GETARG(op->src);
    Value* flags = GETARG(op->flags);
    Value* extra = GETARG(op->extra);
    Value* width;
    Value* prec;
    int w = op->w;
    int p = op->p;
    if(op->width.at != atNul)
    {
        width = GETARG(op->width);
        if(width->vt == vtRef)
        {
            width = &width->valueRef->value;
        }
        if(width->vt != vtInt)
        {
            ZTHROWR(TypeException, vm, "Invalid type format width (must be int): %{}", getValueTypeName(width->vt));
        }
        w = (int) width->iValue;
    }
    if(op->prec.at != atNul)
    {
        prec = GETARG(op->prec);
        if(prec->vt == vtRef)
        {
            prec = &prec->valueRef->value;
        }
        if(prec->vt != vtInt)
        {
            ZTHROWR(TypeException, vm, "Invalid type format precision (must be int): %{}", getValueTypeName(prec->vt));
        }
        p = (int) prec->iValue;
    }
    vm->fmtOps[src->vt](vm, src, w, p, flags ? flags->str : 0, extra, dst);
}

OpFormat::OpFormat(int argW, int argP, OpArg argSrc, OpArg argDst, OpArg argWidth, OpArg argPrec, OpArg argFlags,
                   OpArg argExtra) : OpDstBase(argDst),
    src(argSrc), width(argWidth), prec(argPrec), flags(argFlags), extra(argExtra), w(argW), p(argP)
{
    ot = otFormat;
    op = (OpFunc) Format;
}

static void Combine(ZorroVM* vm, OpCombine* op)
{
    if(op->dst.at == atNul)
    {
        return;
    }
    size_t len = 0;
    OpArg* arr = op->args;
    OpArg* end = arr + op->count;
    bool hasUnicode = false;
    for(; arr != end; ++arr)
    {
        Value* val = GETARG(*arr);
        if(val->vt == vtRef)
        {
            val = &val->valueRef->value;
        }
        len += val->str->getLength();
        if(val->str->getCharSize() == 2)
        {
            hasUnicode = true;
        }
    }
    ZString* str = vm->allocZString();
    char* strBuf = vm->allocStr(hasUnicode ? len * 2 : len + 1);
    char* ptr = strBuf;
    arr = op->args;
    for(; arr != end; ++arr)
    {
        Value* val = GETARG(*arr);
        if(val->vt == vtRef)
        {
            val = &val->valueRef->value;
        }
        ZString* s = val->str;
        if(hasUnicode)
        {
            s->uappend(ptr);
        } else
        {
            memcpy(ptr, s->getDataPtr(), s->getLength());
            ptr += s->getLength();
        }
        if(arr->isTemporal)
        {
            ZUNREF(vm, val);
        }
    }
    *ptr = 0;
    str->init(strBuf, hasUnicode ? len * 2 : len, hasUnicode);
    Value res;
    res.vt = vtString;
    res.flags = 0;
    res.str = str;
    Value* dst = GETDST(op->dst.at == atStack, op->dst);
    ZASSIGN(vm, dst, &res);
}


OpCombine::OpCombine(OpArg* argArgs, size_t argCount, OpArg argDst) : args(argArgs), count(argCount), dst(argDst)
{
    ot = otCombine;
    op = (OpFunc) Combine;
}

static void GetAttr(ZorroVM* vm, OpGetAttr* op)
{
    Value* dst = GETDST(op->dst.at == atStack, op->dst);
    Value* obj = GETARG(op->obj);
    Value* mem = GETARG(op->mem);
    Value* att = GETARG(op->att);
    ClassInfo* ci;
    if(obj->vt == vtClass)
    {
        ci = obj->classInfo;
    } else if(obj->vt == vtObject)
    {
        ci = obj->obj->classInfo;
    } else
    {
        ZTHROWR(TypeException, vm, "Attempt to get attribute of type %{}", getValueTypeName(obj->vt));
    }
    AttrInfo* ai;
    if(mem)
    {
        SymInfo* si = ci->symMap.findSymbol(mem->str);
        if(!si || si->st != sytClassMember)
        {
            ZTHROWR(TypeException, vm, "Class %{} do not have property or member %{}", ci->name,
                    ZStringRef(vm, mem->str));
        }
        ai = &((ClassMember*) si)->attrs;
    } else
    {
        ai = &ci->attrs;
    }
    index_type midx = ai->hasAttr(op->att.idx);
    if(!midx)
    {
        ZASSIGN(vm, dst, &NilValue);
    } else
    {
        if(midx == op->att.idx)
        {
            ZASSIGN(vm, dst, att);
        } else
        {
            Value* matt = vm->ctx.dataPtrs[atGlobal] + midx;
            ZASSIGN(vm, dst, matt);
        }
    }
}

OpGetAttr::OpGetAttr(OpArg argObj, OpArg argMem, OpArg argAtt, OpArg argDst) :
    OpDstBase(argDst), obj(argObj), mem(argMem), att(argAtt)
{
    ot = otGetAttr;
    op = (OpFunc) GetAttr;
}

void ZCode::getAll(OpsVector& allOps)
{
    if(!code)
    {
        return;
    }
    OpsVector opsCheck, ops;
    opsCheck.push_back(code);
    int seq = code->seq++;
    while(!opsCheck.empty())
    {
        OpBase* op = opsCheck.back();
        //DPRINT("check op %p[%d]\n",op,op->seq);
        opsCheck.pop_back();
        allOps.push_back(op);
        op->getBranches(ops);
        ops.push_back(op->next);
        while(!ops.empty())
        {
            op = ops.back();
            ops.pop_back();
            if(op)
            {
                DPRINT("op=%p(%s)\n", op, op->pos.backTrace().c_str());
            }
            if(op && op->seq == seq)
            {
                //DPRINT("add op %p[%d]\n",op,op->seq);
                ++op->seq;
                opsCheck.push_back(op);
            }
        }
    }

}

ZCode::~ZCode()
{
    if(!code)
    {
        return;
    }
    DPRINT("code=%p\n", code);
    OpsVector allOps;
    ArgsVector args;
    getAll(allOps);
    for(std::vector<OpBase*>::iterator it = allOps.begin(), end = allOps.end(); it != end; ++it)
    {
        DPRINT("delete op=%p %s@%s\n", *it, getOpName((*it)->ot), (*it)->pos.backTrace().c_str());
        /*args.clear();
    (*it)->getArgs(args);
    for(ArgsVector::iterator ait=args.begin(),aend=args.end();ait!=aend;++ait)
    {
      OpArg& a=**ait;
      if(a.at==atGlobal)
      {
        SymbolType st=vm->symbols.info[a.idx]->st;
        if(st==sytFunction || st==sytConstant || st==sytClass || st==sytMethod)
        {
          Value* v=&vm->symbols.globals[a.idx];
          if(ZISREFTYPE(v))
          {
            DPRINT("unref idx=%d, type=%s, name=%s(rc=%d)\n",a.idx,getValueTypeName(v->vt),vm->symbols.info[a.idx]->name.val->c_str(),v->refBase->refCount);
            if(v->refBase->unref())
            {
              vm->unrefOps[v->vt](vm,v);
              *v=NilValue;
            }
          }
        }
        //ZUNREF(vm,v);
      }
    }*/
        delete *it;
    }

}

}



