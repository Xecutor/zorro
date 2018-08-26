#include "ZorroVM.hpp"
#include "ZBuilder.hpp"
#include "ZVMOps.hpp"

#include <wctype.h>

namespace zorro{

static void stringLength(ZorroVM* vm,Value* self)
{
  vm->setResult(IntValue(self->str->getLength()));
}

static void stringSubstr(ZorroVM* vm,Value* self)
{
  if(vm->getArgsCount()==0 || vm->getArgsCount()>2)
  {
    throw std::runtime_error("Unexpected number of arguments for String.substr");
  }
  Value startVal=vm->getLocalValue(0);
  if(startVal.vt!=vtInt)
  {
    throw std::runtime_error("Expected integer as first argument for String.substr");
  }
  uint32_t start=static_cast<uint32_t>(startVal.iValue);
  uint32_t len=self->str->getLength();
  if(vm->getArgsCount()==2)
  {
    Value lenVal=vm->getLocalValue(1);
    if(lenVal.vt!=vtInt)
    {
      throw std::runtime_error("Expected integer as second argument for String.substr");
    }
    len=static_cast<uint32_t>(lenVal.iValue);
  }
  vm->setResult(StringValue(self->str->substr(vm,start,len)));
}

static void stringErase(ZorroVM* vm,Value* self)
{
  if(vm->getArgsCount()==0 || vm->getArgsCount()>2)
  {
    throw std::runtime_error("Unexpected number of arguments for String.erase");
  }
  Value startVal=vm->getLocalValue(0);
  if(startVal.vt!=vtInt)
  {
    throw std::runtime_error("Expected integer as first argument for String.erase");
  }
  uint32_t start=static_cast<uint32_t>(startVal.iValue);
  uint32_t len=self->str->getLength();
  if(vm->getArgsCount()==2)
  {
    Value lenVal=vm->getLocalValue(1);
    if(lenVal.vt!=vtInt)
    {
      throw std::runtime_error("Expected integer as second argument for String.erase");
    }
    len=static_cast<uint32_t>(lenVal.iValue);
  }
  vm->setResult(StringValue(self->str->erase(vm,start,len)));
}

static void stringInsert(ZorroVM* vm,Value* self)
{
  if(vm->getArgsCount()!=2)
  {
    throw std::runtime_error("Unexpected number of arguments for String.insert");
  }
  Value startVal=vm->getLocalValue(0);
  if(startVal.vt!=vtInt)
  {
    throw std::runtime_error("Expected integer as first argument for String.insert");
  }
  uint32_t start=static_cast<uint32_t>(startVal.iValue);
  Value strVal=vm->getLocalValue(1);
  if(!(strVal.vt==vtString || (strVal.vt==vtSegment && strVal.seg->cont.vt==vtString)))
  {
    throw std::runtime_error("Expected string as second argument for String.insert");
  }
  ZStringRef str;
  if(strVal.vt==vtSegment)
  {
    str=ZStringRef(vm,strVal.seg->cont.str->substr(vm,static_cast<uint32_t>(strVal.seg->segStart),
                                                   static_cast<uint32_t>(strVal.seg->segEnd-strVal.seg->segStart)));
  }else
  {
    str=ZStringRef(vm,strVal.str);
  }
  vm->setResult(StringValue(self->str->insert(vm,start,str.get())));
}

static void stringFind(ZorroVM* vm,Value* self)
{
  uint32_t from=0;
  if(vm->getArgsCount()==0 || vm->getArgsCount()>2)
  {
    throw std::runtime_error("Unexpected number of arguments for String.find");
  }
  Value substr=vm->getLocalValue(0);
  if(substr.vt!=vtString)
  {
    throw std::runtime_error("Expected string as first argument for String.find");
  }
  if(vm->getArgsCount()==2)
  {
    Value idx=vm->getLocalValue(1);
    if(idx.vt!=vtInt)
    {
      throw std::runtime_error("Expected int as second argument for String.find");
    }
    from=static_cast<uint32_t>(idx.iValue);
  }
  vm->setResult(IntValue(self->str->find(*substr.str,from)));
}

static void stringToUpper(ZorroVM* vm,Value* self)
{
  ZString& rv=*self->str->copy(vm);
  if(rv.getCharSize()==1)
  {
    size_t len=rv.getLength();
    char* ptr=(char*)rv.getDataPtr();
    for(size_t i=0;i<len;++i)
    {
      ptr[i]=static_cast<char>(toupper(static_cast<unsigned char>(ptr[i])));
    }
  }else
  {
    size_t len=rv.getLength();
    uint16_t* ptr=(uint16_t*)rv.getDataPtr();
    for(size_t i=0;i<len;++i)
    {
      ptr[i]=towupper(ptr[i]);
    }
  }
  vm->setResult(StringValue(&rv));
}


static void classGetSubclasses(ZorroVM* vm,Value* self)
{
  ClassInfo::ChildrenVector& cv=self->classInfo->children;
  Value rv;
  rv.vt=vtArray;
  rv.flags=0;
  ZArray* za=rv.arr=vm->allocZArray();
  za->resize(self->classInfo->children.size());
  size_t idx=0;
  for(ClassInfo::ChildrenVector::iterator it=cv.begin(),end=cv.end();it!=end;++it,++idx)
  {
    Value& v=za->getItemRef(idx);
    v.vt=vtClass;
    v.flags=0;
    v.classInfo=*it;
  }
  vm->setResult(rv);
}

static void classGetDataMembers(ZorroVM* vm,Value* self)
{
  ClassInfo* ci=self->classInfo;
  Value rv;
  rv.vt=vtArray;
  rv.flags=0;
  ZArray* za=rv.arr=vm->allocZArray();
  za->resize(ci->membersCount);
  size_t idx=0;
  for(auto& cm:ci->members)
  {
    Value& val=za->getItemRef(idx++);
    val.vt=vtString;
    val.flags=0;
    val.str=cm->name.val.get();
    val.str->ref();
  }
  vm->setResult(rv);
}

static void classGetName(ZorroVM* vm,Value* self)
{
  Value rv;
  rv.vt=vtString;
  rv.flags=0;
  rv.str=self->classInfo->name.val.get();
  rv.str->ref();
  vm->setResult(rv);
}


static void coroutineIsRunning(ZorroVM* vm,Value* self)
{
  vm->setResult(BoolValue(self->cor->ctx.nextOp!=0));
}

static void NilCtor(ZorroVM* vm,Value*)
{
  if(vm->getArgsCount()!=0)
  {
    throw std::runtime_error("argument not expected for Nil constructor");
  }
  vm->setResult(NilValue);
}

static void BoolCtor(ZorroVM* vm,Value*)
{
  if(vm->getArgsCount()!=1)
  {
    throw std::runtime_error("Expected exactly 1 argument for Bool constructor");
  }
  Value* arg=&vm->getLocalValue(0);
  size_t callLevel=vm->ctx.callStack.size();
  bool rv=vm->boolOps[arg->vt](vm,arg);
  if(callLevel!=vm->ctx.callStack.size())
  {
    size_t savelb=vm->ctx.callStack.stackTop[-1].localBase;
    vm->ctx.callStack.stackTop[-1].localBase=vm->ctx.callStack.stackTop[-2].localBase;
    OpBase* saveRet=vm->ctx.callStack.stackTop->retOp;
    vm->ctx.callStack.stackTop->retOp=0;
    vm->resume();
    vm->ctx.nextOp=saveRet;
    vm->ctx.callStack.stackTop->localBase=savelb;
  }else
  {
    vm->setResult(BoolValue(rv));
  }
}

static void IntCtor(ZorroVM* vm,Value*)
{
  if(vm->getArgsCount()!=1)
  {
    throw std::runtime_error("Expected exactly 1 argument for Int constructor");
  }
  Value* arg=&vm->getLocalValue(0);
  int64_t val=0;
  if(arg->vt==vtBool)
  {
    val=arg->bValue?1:0;
  }else if(arg->vt==vtInt)
  {
    val=arg->iValue;
  }else if(arg->vt==vtDouble)
  {
    val=static_cast<int64_t>(arg->dValue);
  }else if(arg->vt==vtString)
  {
    val=ZString::parseInt(arg->str->c_str(vm));
  }
  vm->setResult(IntValue(val));
}

static void DoubleCtor(ZorroVM* vm,Value*)
{
  if(vm->getArgsCount()!=1)
  {
    throw std::runtime_error("Expected exactly 1 argument for Double constructor");
  }
  Value* arg=&vm->getLocalValue(0);
  double val=0;
  if(arg->vt==vtInt)
  {
    val=static_cast<double>(arg->iValue);
  }else if(arg->vt==vtDouble)
  {
    val=arg->dValue;
  }else if(arg->vt==vtString)
  {
    val=ZString::parseDouble(arg->str->c_str(vm));
  }else if(arg->vt==vtSegment && arg->seg->cont.vt==vtString)
  {
    val=ZString::parseDouble(arg->seg->cont.str->c_substr(vm,static_cast<uint32_t>(arg->seg->segStart),
                                                          static_cast<uint32_t>(arg->seg->segEnd-arg->seg->segStart)));
  }else
  {
    throw std::runtime_error("Unexpected type of argument for Double constructor");
  }
  vm->setResult(DoubleValue(val));
}

static Symbol parseSymbol(ZorroVM* vm,const char* name,NameList& ns)
{
  Name nm;
  const char* ptr=name;
  for(;;)
  {
    const char* col=strchr(ptr,':');
    if(col)
    {
      if(col[1]!=':')
      {
        ZTHROW(SyntaxErrorException,FileLocation(),"Invalid symbol %{}",name);
      }
      ZStringRef str=vm->mkZString(ptr,static_cast<size_t>(col-ptr));
      ns.values.push_back(str);
      ptr=col+2;
    }else
    {
      break;
    }
  }
  nm.val=vm->mkZString(ptr);
  return Symbol(nm,ns.values.empty()?0:&ns);
}

static Value mkObject(ZorroVM* vm,const char* name)
{
  NameList ns;
  Symbol nm=parseSymbol(vm,name,ns);
  ClassInfo* ci=(ClassInfo*)vm->symbols.getSymbol(nm);
  if(!ci)
  {
    throw UndefinedSymbolException(Symbol(nm));
  }
  if(ci->st!=sytClass)
  {
    ZTHROW0(ZorroException,"%{} is not class",name);
  }
  Value rv;
  rv.vt=vtObject;
  Object& obj=*(rv.obj=vm->allocObj());
  obj.classInfo=ci;
  ci->ref();
  obj.members=vm->allocVArray(ci->membersCount);
  for(size_t i=0;i<ci->membersCount;++i)
  {
    obj.members[i]=NilValue;
  }
  return rv;
}

void setMemberValue(ZorroVM* vm,Value obj,const char* field,const std::string& val)
{
  Value* members=obj.obj->members;
  ClassInfo* ci=obj.obj->classInfo;
  ZStringRef str = vm->mkZString(val.c_str(), val.length());
  vm->assign(members[(*ci->symMap.getPtr(field))->index], StringValue(str));
}

void setMemberValue(ZorroVM* vm,Value obj,const char* field,int64_t val)
{
  Value* members=obj.obj->members;
  ClassInfo* ci=obj.obj->classInfo;
  vm->assign(members[(*ci->symMap.getPtr(field))->index],IntValue(val));
}


static void getStackTraceFunc(ZorroVM* vm)
{
  Value rv;
  rv.vt=vtArray;
  rv.flags=0;
  std::vector<ZorroVM::StackTraceItem> trace;
  vm->getStackTrace(trace,true);
  ZArray& za=*(rv.arr=vm->allocZArray());
  for(std::vector<ZorroVM::StackTraceItem>::iterator it=trace.begin(),end=trace.end();it!=end;++it)
  {
    Value val=mkObject(vm,"sys::StackTraceItem");
    if(it->pos)
    {
      setMemberValue(vm,val,"line",it->pos->line);
      setMemberValue(vm,val,"col",it->pos->col);
    }
    setMemberValue(vm,val,"funcName",it->func);
    setMemberValue(vm,val,"fileName",it->file);
    setMemberValue(vm,val,"text",it->text);
    //vm->saddMatrix[rv.vt][val.vt](vm,&rv,&val,0);
    za.pushAndRef(val);
  }
  vm->setResult(rv);
}

static void arrayBack(ZorroVM* vm,Value* arr)
{
  if(vm->getArgsCount()>1)
  {
    throw std::runtime_error("Unexpected number of arguments for Array.back");
  }
  if(arr->arr->getCount()==0)
  {
    throw std::runtime_error("calling back on empty array");
  }
  int64_t off=0;
  if(vm->getArgsCount()>1)
  {
    Value offVal=vm->getLocalValue(0);
    if(offVal.vt!=vtInt)
    {
      throw std::runtime_error("Expected integer as first argument for Array.back");
    }
    off=offVal.iValue;
  }
  if((int64_t)arr->arr->getCount()-off-1<0)
  {
    throw std::runtime_error("Array index out of bounds in Array.back");
  }
  vm->setResult(arr->arr->getItem(static_cast<size_t>(arr->arr->getCount()-1-off)));
}

static void arrayPopBack(ZorroVM* vm,Value* arr)
{
  if(arr->arr->getCount()==0)
  {
    throw std::runtime_error("calling back on empty array");
  }
  Value v=arr->arr->getItem(arr->arr->getCount()-1);
  vm->setResult(v);
  arr->arr->pop();
  ZUNREF(vm,&v);
}

static void arrayErase(ZorroVM* vm,Value* arr)
{
  if(vm->getArgsCount()==0 || vm->getArgsCount()>2)
  {
    throw std::runtime_error("Unexpected number of arguments for Array.erase");
  }
  Value startVal=vm->getLocalValue(0);
  if(startVal.vt!=vtInt)
  {
    throw std::runtime_error("Expected integer as first argument for Array.erase");
  }
  int64_t start=startVal.iValue;
  int64_t count=arr->arr->getCount()-start;
  if(vm->getArgsCount()==2)
  {
    Value countVal=vm->getLocalValue(1);
    if(countVal.vt!=vtInt)
    {
      throw std::runtime_error("Expected integer as second argument for Array.erase");
    }
    count=countVal.iValue;
    if(start+count>(int64_t)arr->arr->getCount())
    {
      count=arr->arr->getCount()-start;
    }
  }
  arr->arr->erase(static_cast<size_t>(start),static_cast<size_t>(count));
}

static void arrayInsert(ZorroVM* vm,Value* arr)
{
  if(vm->getArgsCount()<2 || vm->getArgsCount()>4)
  {
    throw std::runtime_error("Unexpected number of arguments for Array.insert");
  }
  Value insIdx=vm->getLocalValue(0);
  if(insIdx.vt!=vtInt)
  {
    throw std::runtime_error("Expected integer as first argument for Array.insert");
  }
  int64_t index=insIdx.iValue;
  Value insArr=vm->getLocalValue(1);
  if(!(insArr.vt==vtArray || (insArr.vt==vtSegment && insArr.seg->cont.vt==vtArray) || (insArr.vt==vtSlice && insArr.slice->cont.vt==vtArray)))
  {
    throw std::runtime_error("Expected array as second argument for Array.insert");
  }
  int64_t start=0;
  int64_t count;
  if(insArr.vt==vtArray)count=insArr.arr->getCount();
  else if(insArr.vt==vtSegment)count=insArr.seg->segEnd-insArr.seg->segStart;
  else count=insArr.slice->indeces.arr->getCount();
  if(vm->getArgsCount()>2)
  {
    Value startVal=vm->getLocalValue(2);
    if(startVal.vt!=vtInt)
    {
      throw std::runtime_error("Expected integer as third argument for Array.insert");
    }
    start=startVal.iValue;
  }
  if(vm->getArgsCount()>3)
  {
    Value countVal=vm->getLocalValue(3);
    if(countVal.vt!=vtInt)
    {
      throw std::runtime_error("Expected integer as fourth argument for Array.insert");
    }
    count=countVal.iValue;
  }
  if(insArr.vt==vtArray)
  {
    arr->arr->insert(static_cast<size_t>(index),*insArr.arr,static_cast<size_t>(start),static_cast<size_t>(count));
  }else if(insArr.vt==vtSegment)
  {
    if(insArr.seg->segStart+start+count>insArr.seg->segEnd)
    {
      count=insArr.seg->segEnd-(insArr.seg->segStart+start);
    }
    start+=insArr.seg->segStart;
    arr->arr->insert(static_cast<size_t>(index),*insArr.seg->cont.arr,static_cast<size_t>(start),static_cast<size_t>(count));
  }else//slice
  {
    Value dst=NilValue;
    vm->copyOps[insArr.vt](vm,&insArr,&dst);
    arr->arr->insert(static_cast<size_t>(index),*dst.arr,static_cast<size_t>(start),static_cast<size_t>(count));
    ZUNREF(vm,&dst);
  }
}




static void arrayReverseView(ZorroVM* vm,Value* arr)
{
  Value rv;
  rv.vt=vtSegment;
  rv.flags=0;
  Segment& seg=*(rv.seg=vm->allocSegment());
  seg.segStart=0;
  seg.segEnd=arr->arr->getCount();
  seg.cont=*arr;
  seg.cont.arr->ref();
  seg.step=-1;
  vm->setResult(rv);
}


void ZorroVM::initStd()
{
  ZBuilder b(this);
  objectClass=b.enterNClass("Object",0,0);
  b.leaveClass();
  nilClass=b.enterNClass("Nil",NilCtor,0);
  b.leaveClass();
  boolClass=b.enterNClass("Bool",BoolCtor,0);
  b.leaveClass();
  intClass=b.enterNClass("Int",IntCtor,0);
  b.leaveClass();
  doubleClass=b.enterNClass("Double",DoubleCtor,0);
  b.leaveClass();
  classClass=b.enterNClass("Class",0,0);
  b.registerCMethod("getSubclasses",classGetSubclasses);
  b.registerCMethod("getName",classGetName);
  b.registerCMethod("getDataMembers",classGetDataMembers);
  b.leaveClass();
  stringClass=b.enterNClass("String",0,0);
  b.registerCMethod("count",stringLength);
  b.registerCMethod("length",stringLength);
  b.registerCMethod("substr",stringSubstr);
  b.registerCMethod("erase",stringErase);
  b.registerCMethod("insert",stringInsert);
  b.registerCMethod("find",stringFind);
  b.registerCMethod("toupper",stringToUpper);
  b.leaveClass();
  arrayClass=b.enterNClass("Array",0,0);
  b.registerCMethod("back",arrayBack);
  b.registerCMethod("popBack",arrayPopBack);
  b.registerCMethod("erase",arrayErase);
  b.registerCMethod("insert",arrayInsert);
  b.registerCMethod("reverseView",arrayReverseView);
  b.leaveClass();
  mapClass=b.enterNClass("Map",0,0);
  b.leaveClass();
  setClass=b.enterNClass("Set",0,0);
  b.leaveClass();
  rangeClass=b.enterNClass("Range",0,0);
  b.leaveClass();
  dlgClass=b.enterNClass("Delegate",0,0);
  b.leaveClass();
  corClass=b.enterNClass("Coroutine",0,0);
  b.registerCMethod("isRunning",coroutineIsRunning);
  b.leaveClass();
  funcClass=b.enterNClass("Func",0,0);
  b.leaveClass();
  clsClass=b.enterNClass("Closure",0,0);
  b.leaveClass();
  clsClass->parentClass=funcClass;
  b.enterNamespace("sys");
  b.enterClass("StackTraceItem");
  b.registerClassMember("funcName");
  b.registerClassMember("fileName");
  b.registerClassMember("line");
  b.registerClassMember("col");
  b.registerClassMember("text");
  b.leaveClass();
  b.registerCFunc("stacktrace",getStackTraceFunc);
  b.leaveNamespace();
  symbols.stdEnd=symbols.info.size();
}

}
