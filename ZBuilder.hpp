#ifndef __ZORRO_ZBUILDER_HPP__
#define __ZORRO_ZBUILDER_HPP__

#include "ZorroVM.hpp"

namespace zorro {

class ZBuilder {
public:
    ZBuilder(ZorroVM* argVm) : vm(argVm)
    {
    }

    struct NSGuard {
        ZBuilder& b;

        NSGuard(ZBuilder& argB, const char* name) : b(argB)
        {
            b.enterNamespace(name);
        }

        ~NSGuard()
        {
            b.leaveNamespace();
        }

    protected:
        NSGuard(const NSGuard&);

        void operator=(const NSGuard&);
    };


    void enterNamespace(const char* name)
    {
        vm->symbols.enterNamespace(vm->mkZString(name));
    }

    void leaveNamespace()
    {
        vm->symbols.returnScope();
    }

    void registerCFunc(const char* name, ZorroCFunc func)
    {
        vm->symbols.registerCFunc(vm->mkZString(name), func);
    }

    void registerCFuncEx(const char* name, ZorroCFunc func, ClassInfo* rvType)
    {
        FuncInfo* fi = (FuncInfo*) vm->symbols.info[vm->symbols.registerCFunc(vm->mkZString(name), func)];
        fi->rvtype.ts = tsDefined;
        fi->rvtype.vt = rvType->nativeClass ? vtNativeObject : vtObject;
        fi->rvtype.symRef = rvType;
    }


    void registerConst(const char* name, int64_t value)
    {
        size_t idx = vm->symbols.registerScopedGlobal(new SymInfo(vm->mkZString(name), sytConstant));
        vm->symbols.globals[idx] = IntValue(value, true);
    }

    void registerConst(const char* name, double value)
    {
        size_t idx = vm->symbols.registerScopedGlobal(new SymInfo(vm->mkZString(name), sytConstant));
        vm->symbols.globals[idx] = DoubleValue(value, true);
    }

    void registerConst(const char* name, const char* value)
    {
        size_t idx = vm->symbols.registerScopedGlobal(new SymInfo(vm->mkZString(name), sytConstant));
        auto str = vm->mkZString(value);
        vm->symbols.globals[idx] = StringValue(str, true);
    }

    void registerGlobal(const char* name, const Value& val)
    {
        ZStringRef zname = vm->mkZString(name);
        SymInfo* si = new SymInfo(zname, sytGlobalVar);
        size_t idx = vm->symbols.registerGlobalSymbol(zname, si);
        Value* gv = vm->symbols.globals + idx;
        ZASSIGN(vm, gv, &val);
    }

    ClassInfo* enterClass(const char* name/*,const char* parent=0*/)
    {
        /*Symbol parentSym;
        if(parent)
        {
          parentSym.name=vm->mkZString(parent);
        }*/
        return vm->symbols.registerClass(vm->mkZString(name)/*,parent?&parentSym:0*/);
    }

    ClassInfo* enterNClass(const char* name, ZorroCMethod ctor, ZorroCMethod dtor)
    {
        return vm->symbols.registerNativeClass(vm->mkZString(name), ctor, dtor);
    }

    ClassInfo* enterNClass(const char* name)
    {
        return vm->symbols.enterClass(vm->mkZString(name));
    }

    void setNClassParent(ClassInfo* parent)
    {
        vm->symbols.currentClass->parentClass = parent;
    }


    MethodInfo* registerCMethod(const char* name, ZorroCMethod meth)
    {
        return vm->symbols.registerCMethod(vm->mkZString(name), meth);
    }

    void registerCSpecialMethod(ClassSpecialMethod csm, ZorroCMethod meth)
    {
        return vm->symbols.registerNativeClassSpecialMethod(csm, meth);
    }

    ClassPropertyInfo* registerCProperty(const char* name, ZorroCMethod getter, ZorroCMethod setter)
    {
        std::string sname = name;
        MethodInfo* gm = 0, * sm = 0;
        if(getter)
        {
            sname += "-get";
            gm = registerCMethod(sname.c_str(), getter);
            gm->specialMethod = true;
        }
        if(setter)
        {
            sname = name;
            sname += "-set";
            sm = registerCMethod(sname.c_str(), setter);
            sm->specialMethod = true;
        }
        ClassPropertyInfo* cp = vm->symbols.registerProperty(vm->mkZString(name));
        cp->getMethod = gm;
        cp->setMethod = sm;
        return cp;
    }

    size_t registerClassMember(const char* name)
    {
        return vm->symbols.registerMember(vm->mkZString(name))->index;
    }

    void leaveClass()
    {
        vm->symbols.returnScope();
    }

protected:
    ZorroVM* vm;
};

}

#endif
