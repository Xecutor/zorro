#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "ZorroParser.hpp"
#include "ZorroVM.hpp"
#include "CodeGenerator.hpp"
#include "Debug.hpp"
#include "ZBuilder.hpp"
#include <clocale>

#include "Debugger.hpp"
#include "ZVMOps.hpp"

/*
#include "/Users/skv/smsc/src/util/leaktracing/heaptracer/HeapTracer.cpp"
#include <new>
void* operator new(size_t sz)throw(std::bad_alloc)
{
  void* rv=malloc(sz);
//  printf("new:%p[%lu]\n",rv,sz);
  return rv;
}

void operator delete(void* vptr)throw()
{
//  printf("delete:%p\n",vptr);
  free(vptr);
}

void* operator new[](size_t sz)throw(std::bad_alloc)
{
  void* rv=malloc(sz);
//  printf("new:%p[%lu]\n",rv,sz);
  return rv;
}

void operator delete[](void* vptr)throw()
{
//  printf("delete:%p\n",vptr);
  free(vptr);
}
*/

using namespace zorro;

static void z_sqrt(ZorroVM* vm)
{
    int ac = vm->getArgsCount();
    if(ac != 1)
    {
        throw std::runtime_error("sqrt:invalid number of arguments");
    }
    Value* v = &vm->getLocalValue(0);
    double val;

    if(v->vt == vtDouble)
    {
        val = v->dValue;
    } else if(v->vt == vtInt)
    {
        val = static_cast<double>(v->iValue);
    } else
    {
        throw std::runtime_error("sqrt:invalid argument type");
    }
    if(val < 0)
    {
        throw std::runtime_error("sqrt:negative argument value");
    }
    vm->setResult(DoubleValue(sqrt(val)));
}

double valueAsNumber(const Value& v)
{
    if(v.vt == vtDouble)
    {
        return v.dValue;
    } else if(v.vt == vtInt)
    {
        return static_cast<double>(v.iValue);
    } else
    {
        throw std::runtime_error("values is not numeric");
    }
}

static void z_random(ZorroVM* vm)
{
    int ac = vm->getArgsCount();
    double mn = 0.0;
    double mx = 1.0;
    if(ac > 2)
    {
        throw std::runtime_error("random:invalid number of arguments");
    }
    if(ac > 0)
    {
        mx = valueAsNumber(vm->getLocal(ac - 1));
    }
    if(ac == 2)
    {
        mn = valueAsNumber(vm->getLocal(0));
    }
    int r;
    do
    {
        r = rand();
    } while(r == RAND_MAX);
    double v = r;
    v /= RAND_MAX;
    vm->setResult(DoubleValue(mn + mx * v));
}

static void z_irandom(ZorroVM* vm)
{
    int ac = vm->getArgsCount();
    double mn = 0.0;
    double mx = 1.0;
    if(ac > 2)
    {
        throw std::runtime_error("random:invalid number of arguments");
    }
    if(ac > 0)
    {
        mx = valueAsNumber(vm->getLocal(ac - 1));
    }
    if(ac == 2)
    {
        mn = valueAsNumber(vm->getLocal(0));
    }
    int r;
    do
    {
        r = rand();
    } while(r == RAND_MAX);
    double v = r;
    v /= RAND_MAX;
    vm->setResult(IntValue(static_cast<int64_t>(mn + mx * v)));
}

static void print(ZorroVM* vm)
{
    int ac = vm->getArgsCount();
    for(int i = 0; i < ac; i++)
    {
        printf("%s", ValueToString(vm, vm->getLocal(i)).c_str());
    }
    printf("\n");
}

static void input(ZorroVM* vm)
{
    int ac = vm->getArgsCount();
    for(int i = 0; i < ac; i++)
    {
        printf("%s", ValueToString(vm, vm->getLocal(i)).c_str());
    }
    if(ac)
    {
        fflush(stdout);
    }
    char buf[256];
    if(!fgets(buf, sizeof(buf) - 1, stdin))
    {
        return;
    }
    int l = strlen(buf);
    while(l > 0 && (buf[l - 1] == 0x0a || buf[l - 1] == 0x0d))
    {
        --l;
    }
    buf[l] = 0;
    vm->setResult(StringValue(vm->allocZString(buf, l)));
}

static void memreport(ZorroVM* vm)
{
    printf("%s\n", vm->getUsageReport().c_str());
}

struct MyContext {
    int val;
    int sum;
    OpCall callOp;

    MyContext() : callOp(0, OpArg(), OpArg(atStack))
    {
        val = 0;
        sum = 0;
    }
};

template<class T>
Value mkNativeObject(ZorroVM* vm, ClassInfo* cls, T* obj)
{
    Value res;
    res.vt = vtNativeObject;
    res.nobj = vm->allocNObj();
    res.nobj->classInfo = cls;
    cls->ref();
    res.nobj->userData = obj;
    return res;
}

/*static void testCtor(ZorroVM* vm,Value* cls)
{
  Value res;
  res.vt=vtNativeObject;
  res.nobj=vm->allocNObj();
  res.nobj->classInfo=cls->classInfo;
  cls->classInfo->ref();
  res.nobj->userData=new Test;
  vm->setResult(res);
}*/
static void ctxDtor(ZorroVM* vm, Value* obj)
{
    printf("ctx.dtor\n");
    delete &obj->nobj->as<MyContext*>();
}

static ClassInfo* ctxClsInfo;

static void ctxtest(ZorroVM* vm)
{
    bool first = true;
    Value rv = NilValue;
    //printf("argsCount=%d, cf->args=%d\n",vm->getArgsCount(),vm->ctx.callStack.stackTop->args);
    if(vm->ctx.dataStack.size() < 2 ||
       vm->ctx.dataStack.stackTop[-1].vt != vtNativeObject ||
       vm->ctx.dataStack.stackTop[-1].nobj->classInfo != ctxClsInfo)
    {
        //initial call
        MyContext* nc = new MyContext;
        nc->callOp.pos = vm->ctx.lastOp->pos;
        vm->pushValue(mkNativeObject(vm, ctxClsInfo, nc));
    } else
    {
        //pop extra call from call stack
        vm->ctx.callStack.pop();
        first = false;
        ZASSIGN(vm, &rv, vm->ctx.dataStack.stackTop);
        vm->ctx.dataStack.pop();
    }
    MyContext& c = vm->ctx.dataStack.stackTop->nobj->as<MyContext>();
    if(!first)
    {
        c.sum += static_cast<int>(valueAsNumber(rv));
    }
    if(c.val == 10)
    {
        vm->setResult(IntValue(c.sum));
        return;
    }
    vm->pushValue(IntValue(c.val++));
    Value func = vm->getLocalValue(0);
    vm->ctx.nextOp = vm->ctx.lastOp;
    vm->ctx.lastOp = &c.callOp;
    vm->callSomething(&func, 1);
    ZUNREF(vm, &rv);
}


/*static void testGetIndex(ZorroVM* vm,Value* obj)
{
  Value& idx=vm->getLocalValue(0);
  vm->setResult(IntValue(obj->nobj->as<Test>()[idx.iValue]));
}

static void testSetIndex(ZorroVM* vm,Value* obj)
{
  Value& idx=vm->getLocalValue(0);
  obj->nobj->as<Test>()[idx.iValue]=valueAsNumber(vm->getLocalValue(1));
  //vm->setResult(IntValue(obj->nobj->as<Test>()[idx.iValue]));
}*/



class TestClass {
public:
    TestClass()
    {
        idx = testClassCnt++;
    }

    static int testClassCnt;
    int idx;
    int val = 0;

    int add(int x, int y)
    {
        return x + y;
    }
};

int TestClass::testClassCnt = 0;

static void TestClassCtor(ZorroVM* vm, Value* cls)
{
    Value rv;
    rv.vt = vtNativeObject;
    rv.flags = 0;
    rv.nobj = vm->allocNObj();
    rv.nobj->classInfo = cls->classInfo;
    TestClass* tc = new TestClass;
    rv.nobj->userData = tc;
    printf("tccreate:%d\n", tc->idx);
    vm->setResult(rv);
}

static void TestClassDtor(ZorroVM* vm, Value* obj)
{
    TestClass* tc = &obj->nobj->as<TestClass>();
    printf("destroy:%d\n", tc->idx);
    delete tc;
}

static void TestClassDiv(ZorroVM* vm, Value* obj)
{
    Value rv;
    rv.vt = vtNativeObject;
    rv.flags = 0;
    rv.nobj = vm->allocNObj();
    rv.nobj->classInfo = obj->nobj->classInfo;
    TestClass* tc = new TestClass;
    rv.nobj->userData = tc;
    printf("div:%d->%d\n", obj->nobj->as<TestClass>().idx, tc->idx);
    vm->setResult(rv);
}


static void TestClassGetVal(ZorroVM* vm, Value* obj)
{
    vm->setResult(IntValue(obj->nobj->as<TestClass>().val));
}

static void TestClassSetVal(ZorroVM* vm, Value* obj)
{
    if(vm->getArgsCount() != 1)
    {
        return;
    }
    Value& val = vm->getLocalValue(0);
    if(val.vt == vtInt)
    {
        obj->nobj->as<TestClass>().val = static_cast<int>(val.iValue);
    }
}

static void TestClassAdd(ZorroVM* vm, Value* obj)
{
    if(vm->getArgsCount() != 2)
    {
        return;
    }
    int rv = obj->nobj->as<TestClass>().add(
        static_cast<int>(valueAsNumber(vm->getLocalValue(0))),
        static_cast<int>(valueAsNumber(vm->getLocalValue(1))));
    vm->setResult(IntValue(rv));
}

std::string dumpTypeInfo(TypeInfo& ti)
{
    if(ti.ts == tsUnknown)
    {
        return "Unknown";
    } else if(ti.ts == tsDefined)
    {
        if(ti.vt == vtObject)
        {
            if(!ti.symRef)
            {
                return "object";
            } else
            {
                return FORMAT("object of type %s\n", ti.symRef.asClass()->name);
            }
        } else if(ti.vt == vtFunc)
        {
            return FORMAT("func %s\n", ti.symRef.asFunc()->name);
        } else if(ti.vt == vtArray || ti.vt == vtSet)
        {
            std::string rv = ti.vt == vtArray ? "Array[" : "Set[";
            for(auto& vv:ti.arr)
            {
                rv += dumpTypeInfo(vv);
                rv += ",";
            }
            if(*rv.rbegin() == ',')
            {
                rv.erase(rv.length() - 1);
            }
            rv += "]";
            return rv;
        } else if(ti.vt == vtClass)
        {
            if(!ti.symRef)
            {
                return "class";
            } else
            {
                return FORMAT("class %s", ti.symRef.asClass()->name);
            }
        } else
        {
            return getValueTypeName(ti.vt);
        }
    } else
    {
        std::string rv = "[";
        for(auto v:ti.arr)
        {
            rv += dumpTypeInfo(v);
            rv += ",";
        }
        if(*rv.rbegin() == ',')
        {
            rv.erase(rv.length() - 1);
        }
        rv += "]";
        return rv;
    }
}

static void ShowTypeInfo(ZorroVM* vm)
{
    for(size_t i = 0; i < vm->getArgsCount(); ++i)
    {
        Value& v = vm->getLocalValue(i);
        if(v.vt != vtString)
        {
            printf("showTypeInfo: only string arguments expected");
            continue;
        }
        SymInfo* sym = vm->symbols.getSymbol(Name(ZStringRef(vm, v.str)));
        if(sym)
        {
            TypeInfo& ti = sym->tinfo;
            printf("%s\n", dumpTypeInfo(ti).c_str());
            if(sym->st == sytFunction || sym->st == sytClass || sym->st == sytMethod)
            {
                vm->symbols.currentScope = (ScopeSym*) sym;
            }
        } else
        {
            printf("sym %s not found\n", v.str->c_str(vm));
        }
    }
}

int main(int argc, char* argv[])
{
    std::setlocale(LC_CTYPE, "");
#ifdef DEBUG
    dpFile = fopen("zorro.log", "wb+");
#endif
    FileRegistry freg;
    ZorroVM vm;
    //printf("sizeof=%d(sym=%d, ctx=%d)\n",(int)sizeof(vm),(int)sizeof(vm.symbols),(int)sizeof(vm.ctx));
    try
    {

        srand(static_cast<unsigned int>(time(0)));
        srand(rand());
        ZBuilder zb(&vm);
        zb.enterNamespace("math");
        zb.registerCFunc("sqrt", z_sqrt);
        zb.registerCFunc("rand", z_random);
        zb.registerCFunc("irand", z_irandom);
        zb.leaveNamespace();

        zb.enterNClass("TestClass", TestClassCtor, TestClassDtor);
        zb.registerCProperty("val", TestClassGetVal, TestClassSetVal);
        zb.registerCSpecialMethod(csmDiv, TestClassDiv);
        zb.registerCMethod("add", TestClassAdd);
        zb.leaveClass();

        ctxClsInfo = zb.enterNClass("MyContext", 0, ctxDtor);
        zb.leaveClass();
        zb.registerCFunc("cbtest", ctxtest);
        //tci->specialMethods[csmGetIndex]=zb.registerCMethod("on getIndex",testGetIndex);
        //tci->specialMethods[csmSetIndex]=zb.registerCMethod("on setIndex",testSetIndex);
        /*zb.registerCMethod("add",testAdd);
        zb.registerCMethod("get",testGet);*/

        zb.registerCFunc("print", print);
        zb.registerCFunc("input", input);
        zb.registerCFunc("memreport", memreport);
        zb.registerCFunc("showTypeInfo", ShowTypeInfo);

        ZParser p(&vm);
        ZMacroExpander mex;
        mex.init(&vm, &p);
        p.l.macroExpander = &mex;
        FileReader fr(&freg);
        const char* fileName = "test.zs";
        bool debugMode = false;
        for(int i = 1; i < argc; ++i)
        {
            if(argv[i][0] == '-')
            {
                if(argv[i][1] == 'd')
                {
                    debugMode = true;
                } else
                {
                    fprintf(stderr, "Unknown option %s", argv[i]);
                    return 1;
                }
            } else
            {
                fileName = argv[i];
            }
        }
        FileRegistry::Entry* e = freg.openFile(fileName);
        if(!e)
        {
            fprintf(stderr, "Failed to open file %s for reading:%s.\n", fileName, strerror(errno));
            return 1;
        }
        fr.assignEntry(e);
        p.pushReader(&fr);
        p.parse();
        CodeGenerator cg(&vm);
        cg.generate(p.getResult());
        cg.fillTypes(p.getResult());
        cg.fillTypes(p.getResult());
        for(CodeGenerator::WarnVector::iterator it = cg.warnings.begin(), end = cg.warnings.end(); it != end; ++it)
        {
            fprintf(stderr, "%s Warning: %s\n", it->pos.backTrace().c_str(), it->msg.c_str());
        }
        vm.init();
        //try{
        if(!debugMode)
        {
            vm.run();
        } else
        {
            Debugger dbg(&vm, p.getResult());
            char cmd[256];
            dbg.start();
            while(vm.ctx.nextOp)
            {
                printf("%s\n", dbg.getCurrentLine().c_str());
                printf(">");
                fflush(stdout);
                if(!fgets(cmd, sizeof(cmd), stdin))
                {
                    break;
                }
                if(cmd[0] == 'n')
                {
                    dbg.stepOver();
                }
                if(cmd[0] == 's')
                {
                    dbg.stepInto();
                }
                if(cmd[0] == 'q')
                {
                    break;
                }
            }
        }
        /*}catch(...)
        {
          ZorroVM::StackTraceVector trace;
          vm.getStackTrace(trace,false);
          for(ZorroVM::StackTraceVector::iterator it=trace.begin(),end=trace.end();it!=end;++it)
          {
            printf("%s\n",it->text.c_str());
          }
          throw;
        }*/
        //char buf[256];
        //fgets(buf,sizeof(buf),stdin);
    }
    catch(std::exception& e)
    {
        fprintf(stderr, "exception:\n%s\n", e.what());
        /*if(vm.ctx.lastOp)
        {
          printf("at %s\n",vm.ctx.lastOp->pos.backTrace().c_str());
        }*/
    }
    vm.deinit();
    //printf("%s\n",vm.getUsageReport().c_str());
    return 0;
}

