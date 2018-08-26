#ifndef __ZORRO_ZORROPARSER_HPP__
#define __ZORRO_ZORROPARSER_HPP__

#include "ParserBase.hpp"
#include "ZorroLexer.hpp"
#include "SynTree.hpp"
#include <string>
#include <list>

namespace zorro {

class ZParser : public ParserBase<ZParser, ZLexer> {
public:
    ZParser(ZMemory* argMem);

    void setMem(ZMemory* argMem)
    {
        mem = argMem;
    }

    static const size_t MaxDataSize = sizeof(Name);

    StmtList* getResult()
    {
        return result;
    }

    ~ZParser()
    {
        //delete result;
    }

    Name getValue(const Term& t);

protected:

    ZMemory* mem;
    StmtList* result;


    Expr* handlePreInc(Expr* expr);

    Expr* handlePostInc(Expr* expr);

    Expr* handlePreDec(Expr* expr);

    Expr* handlePostDec(Expr* expr);

    Statement* handleEmptyLine();

    Statement* handleIf(Expr* cond, StmtList* thenList, StmtList* elsifList, StmtList* elseList);

    Statement* handleReturn(Expr* expr);

    Statement* handleReturnIf(Expr* expr);

    Statement* handleReturnNil();

    Statement* handleFuncDecl(Term name, FuncParamList* args, StmtList* stmts, Term end);

    Statement* handleMacroDecl(Term name, FuncParamList* args, StmtList* stmts);

    StmtList* handleGoal(StmtList* st);

    StmtList* handleStmtListEmpty();

    Statement* handleListAssign(ExprList* e1, ExprList* e2);

    Statement* handleListAssign1(ExprList* e1, Expr* e2);

    StmtList* handleStmtListOne(Statement* stmt);

    StmtList* handleStmtList(StmtList* lst, Statement* stmt);

    Expr* handleFuncExpr(FuncParamList* lst, StmtList* stmt);

    Expr* handleShortFuncExpr(FuncParamList* lst, Expr* expr);

    Expr* handleShortFuncNoArgExpr(Expr* expr);

    Expr* handleArray(ExprList* lst);

    Expr* handleArrayIndex(Expr* arr, Expr* idx);

    Expr* handleMapIndex(Expr* mp, Expr* idx);

    NameList* handleNameListEmpty();

    NameList* handleNameListOne(Term nm);

    NameList* handleNameList(NameList* l, Term nm);

    ExprList* handleExprListEmpty();

    ExprList* handleExprListOne(Expr* ex);

    ExprList* handleExprListOnePair(Expr* ex1, Expr* ex2);

    ExprList* handleExprListTwo(Expr* ex1, Expr* ex2);

    ExprList* handleExprListNext(ExprList* l, Expr* e2);

    ExprList* handleExprListNextPair(ExprList* l, Expr* e1, Expr* e2);

    ExprList* handleExprListSkip(ExprList* lst);

    Expr* handleCall(Expr* var, ExprList* argList);

    Expr* handleProp(Expr* var, Term ident);

    Expr* handlePropOpt(Expr* var, Term ident);

    Expr* handlePropExpr(Expr* var, Expr* val);

    Expr* handleNot(Expr* expr);

    Expr* handleNeg(Expr* expr);

    Expr* handleCopy(Expr* expr);

    Expr* handleRef(Expr* expr);

    Expr* handleWeakRef(Expr* expr);

    Expr* handleCor(Expr* expr);

    Expr* handleBr(Expr* expr);

    Expr* handleAtom(Expr* ex);

    Expr* handleBinOp(Expr* a, Term t, Expr* b);

    Expr* handleVal(Term t);

    Expr* handleStr(Term t);

    Expr* handleLiteral(Term t);

    Expr* handleRawStr(Term t);

    Expr* handleVar(Expr* var);

    Expr* handleMap(ExprList* lst);

    Expr* handleEmptyMap();

    Expr* handleSet(ExprList* lst);

    Expr* handlePair(Expr* nm, Expr* vl);

    Expr* handleRange(Expr* start, Term t, Expr* end);

    Expr* handleRangeStep(Expr* start, Term t, Expr* end, Expr* step);

    Expr* handleEmptyExpr();

    Expr* handleRangeTail(Expr* st);

    Statement* handleExprStmt(Expr* ex);

    Statement* handleShortIf(Statement* stmt, Expr* cnd);

    StmtList* handleElsifEmpty();

    StmtList* handleElsifFirst(Expr* cnd, StmtList* stmt);

    StmtList* handleElsifNext(StmtList* lst, Expr* cnd, StmtList* stmt);

    StmtList* handleElseEmpty();

    StmtList* handleElse(StmtList* lst);

    Name handleOptNameEmpty();

    Name handleOptName(Term name);

    Statement* handleWhile(Name name, Expr* cnd, StmtList* body);

    Statement* handleShortWhile(Statement* body, Name name, Expr* cnd);

    Statement* handleFor(Name name, NameList* lst, Expr* expr, StmtList* body);

    Statement* handleShortFor(Statement* body, Name name, NameList* lst, Expr* expr);

    Statement* handleClass(Term name, FuncParamList* args, ClassParent* parent, StmtList* body, Term end);

    ClassParent* handleNoParent();

    ClassParent* handleParent(Expr* parent);

    //ClassParent* handleParentNoArgs(NameList* name);
    Statement* handleStmt(Statement* stmt);

    Statement* handleAttrList(NameList* lst);

    Statement* handleOn(Term name, FuncParamList* argList, StmtList* body);

    FuncParamList* handleOptArg(FuncParamList* argList);

    FuncParamList* handleOptArgNone();

    Statement* handleMemberInit(Term name, AttrInstList* attr, Expr* expr);

    Expr* handleNamespaceExpr(Term name, Expr* expr);

    Statement* handleNamespace(NameList* ns, StmtList* body);

    NameList* handleNsEmpty();

    NameList* handleNsFirst(Term name);

    NameList* handleNsNext(NameList* ns, Term name);

    Statement* handleTryCatch(StmtList* tryBody, ExprList* exList, Term var, StmtList* catchBody);

    Statement* handleThrow(Expr* ex);

    Statement* handleYieldNil(Term t);

    Statement* handleYield(Expr* ex);

    ExprList* handleNmListOne(NameList* nm);

    ExprList* handleNmListNext(ExprList* lst, NameList* nm);

    Statement* handleSimpleStmt(Statement* stmt);

    Statement* handleBreakNil(Term brk);

    Statement* handleBreakId(Term brk, Term id);

    Statement* handleNextNil(Term nxt);

    Statement* handleNextId(Term nxt, Term id);

    Statement* handleRedoNil(Term rd);

    Statement* handleRedoId(Term rd, Term id);

    Statement* handleSwitch(Term sw, Expr* expr, SwitchCasesList* lst);

    SwitchCasesList* handleCasesListEmpty();

    SwitchCasesList* handleCasesListEmptyLine(SwitchCasesList* lst);

    SwitchCasesList* handleCasesListFirst(ExprList* caseExpr, StmtList* caseBody);

    SwitchCasesList* handleCasesListNext(SwitchCasesList* lst, ExprList* caseExpr, StmtList* caseBody);

    SwitchCasesList* handleCasesListFirstWild(Term star, StmtList* caseBody);

    SwitchCasesList* handleCasesListNextWild(SwitchCasesList* lst, Term star, StmtList* caseBody);

    Statement* handleEnum(Term name, ExprList* items);

    Statement* handleBitEnum(Term name, ExprList* items);

    Expr* handleFormat(Term fmt, Expr* expr);

    Expr* handleCount(Expr* expr);

    Expr* handleFormatArgs(Term fmt, ExprList* lst);

    StmtList* handleInclude(StmtList* lst, Expr* file);

    StmtList* handleInclude1(Expr* file);

    Expr* handleTernary(Expr* cnd, Expr* trueExpr, Expr* falseExpr);

    Statement* handlePropDecl(Term name, PropAccessorList* accList);

    PropAccessor* handleEmptyPropAcc();

    PropAccessor* handlePropAccName(Term acc, Term name);

    PropAccessor* handlePropAccNoArg(Term acc, StmtList* func);

    PropAccessor* handlePropAccArg(Term acc, Term arg, StmtList* func);

    PropAccessorList* handlePropAccListEmpty();

    PropAccessorList* handlePropAccListFirst(PropAccessor* acc);

    PropAccessorList* handlePropAccListNext(PropAccessorList*, PropAccessor* acc);

    Expr* handleNameMapIndex(Expr* map, Term name);

    Expr* handleVarIdent(Term name);

    AttrInstance* handleAttrSimple(Expr* name);

    AttrInstance* handleAttrAssign(Expr* name, Expr* value);

    AttrInstList* handleAttrDeclEmpty();

    AttrInstList* handleAttrDecl(AttrInstList* lst);

    AttrInstList* handleAttrListEmpty();

    AttrInstList* handleAttrListFirst(AttrInstance* attr);

    AttrInstList* handleAttrListNext(AttrInstList* lst, AttrInstance* attr);

    AttrDeclStatement* handleAttrDefSimple(Term name);

    AttrDeclStatement* handleAttrDefValue(Term name, Expr* value);

    Expr* handleGetAttr(Expr* expr, ExprList* lst);

    Statement* handleClassAttr(AttrInstList* lst);

    Expr* handleGetClassAttr(Expr* expr, Expr* attr);

    Statement* handleMemberDef(Term name, AttrInstList* attrs);

    Expr* handleGetType(Expr* expr);

    Statement* handleUse(Expr* expr);

    FuncParam* handleFuncParamSimple(Term ident);

    FuncParam* handleFuncParamDefVal(Term ident, Expr* expr);

    FuncParam* handleFuncParamArr(Term ident);

    FuncParam* handleFuncParamMap(Term ident);

    FuncParamList* handleParamListEmpty();

    FuncParamList* handleParamListOne(FuncParam* param);

    FuncParamList* handleParamListNext(FuncParamList* lst, FuncParam* param);

    Expr* handleNamespaceExprGlobal(Expr* expr);

    Expr* handleNumArg(Term arg);

    Expr* handleSeqOp(Expr* var, Expr* cnt, ExprList* lst);

    ExprList* handleSeqOpLstFirst(Expr* expr);

    ExprList* handleSeqOpLstNext(ExprList* lst, Expr* expr);

    Expr* handleSeqOpMap(Expr* expr);

    Expr* handleSeqOpGrep(Expr* expr);

    //Expr* handleSeqOpSum(Expr* expr);
    Statement* handleLiterDecl(Term name, LiterArgsList* args, StmtList* body);

    LiterArgsList* handleLiterArgListStr(Term ident, Term arg);

    LiterArgsList* handleLiterArgListFirst(LiterArg* arg);

    LiterArgsList* handleLiterArgListNext(LiterArgsList* lst, LiterArg* arg);

    LiterArg* handleLiterArgInt(Term arg, Term ident);

    LiterArg* handleLiterArgIntOpt(Term arg, Term ident);

    LiterArg* handleLiterArgNum(Term arg, Term ident);

    LiterArg* handleLiterArgNumOpt(Term arg, Term ident);

    LiterArg* handleLiterArgDbl(Term arg, Term ident);

    LiterArg* handleLiterArgDblOpt(Term arg, Term ident);

    LiterArgsList* handleLiterArgList(LiterArgsList* lst);

    LiterArgsList* handleLiterArgListLastInt(LiterArgsList* lst, Term ident);

    LiterArgsList* handleLiterArgListLastDbl(LiterArgsList* lst, Term ident);

    LiterArgsList* handleLiterArgListLastNum(LiterArgsList* lst, Term ident);

    Statement* fillPos(Statement* st)
    {
        st->pos = callStack.last->start;
        st->end = callStack.last->end;
        return st;
    }
};

}

#endif
