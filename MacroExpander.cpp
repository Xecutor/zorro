#include "MacroExpander.hpp"
#include "ZorroVM.hpp"
#include "ZorroParser.hpp"
#include "ZVMOps.hpp"
#include "CodeGenerator.hpp"
#include <memory>

namespace zorro {

void ZMacroExpander::expandMacro()
{
    Term fname = p->l.getNext();
    if(fname.tt != tIdent)
    {
        throw SyntaxErrorException("Expected macro name at", fname.pos);
    }
    if(p->l.getNext().tt != tORBr)
    {
        throw SyntaxErrorException("Expected '(' at", fname.pos);
    }

    ZParser::Rule* argsRule = &p->getRule("argList");
    std::unique_ptr<ExprList> ex (p->parseRule<ExprList*>(argsRule));
    if(p->l.getNext().tt != tCRBr)
    {
        throw SyntaxErrorException("Expected ')' at", fname.pos);
    }
    if(ex)
    {
        for(auto& it : ex->values)
        {
            if(!it->isDeepConst())
            {
                throw SyntaxErrorException("Only constant expressions are allowed as argument to macro", it->pos);
            }
        }
    }
    Symbol name(p->getValue(fname));
    auto ns = std::make_unique<NameList>();
    name.ns = ns.get();
    name.ns->values.emplace_back(vm->mkZString("MACRO"));
    SymInfo* macro = vm->symbols.getSymbol(name);
    if(!macro)
    {
        throw UndefinedSymbolException(name);
    }
    auto* fi = (FuncInfo*) macro;
    if(fi->argsCount != (ex ? ex->values.size() : 0))
    {
        throw SyntaxErrorException("Macro args count mismatch", ex->pos);
    }
    vm->init();
    if(ex)
    {
        CodeGenerator cg(vm);
        for(auto& it : ex->values)
        {
            Value* arg = vm->ctx.dataStack.push();
            Value argVal;
            cg.fillConstant(it.get(), argVal);
            //=StringValue((*it)->val);
            //printf("arg:%s\n",ValueToString(vm,argVal).c_str());
            ZASSIGN(vm, arg, &argVal);
        }
    }
    OpCall callOp(0, OpArg(), OpArg(atStack));
    callOp.pos = name.name.pos;
    vm->pushFrame(&callOp, nullptr, fi->argsCount);
    vm->ctx.dataStack.pushBulk(fi->localsCount);
    vm->ctx.nextOp = ((FuncInfo*) macro)->entry;
    std::string rv;
    while(vm->ctx.callStack.size() >= 2)
    {
        vm->resume();
        Value* res = vm->ctx.callStack.size() >= 2 ? &vm->result : vm->ctx.dataStack.stackTop;
        if(res->vt == vtString)
        {
            DPRINT("result:%s[%p->%p]\n", ValueToString(vm, *res).c_str(), res->str, res->str->getDataPtr());
            rv += res->str->c_str(vm);
        } else if(res->vt != vtNil)
        {
            vm->clearResult();
            throw SyntaxErrorException(
                FORMAT("Macro must yield values of type String, but %{} was yielded", getValueTypeName(res->vt)),
                vm->ctx.lastOp->pos);
        }
        vm->clearResult();
        if(vm->ctx.callStack.size() >= 2)
        {
            vm->ctx.nextOp = vm->ctx.lastOp->next;
        }
    }
    vm->cleanStack(0);
    FileRegistry::Entry* e = p->l.fr->getOwner()->addEntry(FORMAT("macro %s expansion", name.name.val.c_str()),
                                                           rv.c_str(), rv.length());
    FileReader* fr = p->l.fr->getOwner()->newReader(e);
    p->l.pushReader(fr, p->l.fr->getLoc());
    p->l.getNext();
    //p->l.last.tt=tEol;
}

void ZMacroExpander::registerMacro(const Term& name, FuncParamList* params, StmtList* body)
{
    CodeGenerator cg(vm);
    std::unique_ptr<FuncParamList> paramsGuard(params);
    std::unique_ptr<StmtList> bodyGuard(body);
    try
    {
        cg.generateMacro(p->getValue(name), params, body);
    }
    catch(...)
    {
        paramsGuard.release();
        bodyGuard.release();
        throw;
    }
}


}
