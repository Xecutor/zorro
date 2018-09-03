#ifndef __ZORRO_SYNTREE_HPP__
#define __ZORRO_SYNTREE_HPP__

#include <list>
#include <string>
#include <memory>
#include "FileReader.hpp"
#include "ZString.hpp"
#include "NameList.hpp"
#include <kst/Format.hpp>

namespace zorro {


enum ExprType {
    etPlus,
    etSPlus,
    etMinus,
    etSMinus,
    etMul,
    etSMul,
    etDiv,
    etSDiv,
    etMod,
    etSMod,
    etPreInc,
    etPostInc,
    etPreDec,
    etPostDec,
    etNeg,
    etCopy,
    etBitOr,
    etBitAnd,
    etRef,
    etWeakRef,
    etCor,
    etAssign,
    etLess,
    etGreater,
    etLessEq,
    etGreaterEq,
    etEqual,
    etNotEqual,
    etMatch,
    etOr,
    etAnd,
    etNot,
    etIn,
    etIs,
    etInt,
    etDouble,
    etString,
    etPair,
    etRange,
    etNil,
    etTrue,
    etFalse,
    etArray,
    etMap,
    etSet,
    etVar,
    etNumArg,
    etCall,
    etProp,
    etPropOpt,
    etKey,
    etIndex,
    etFunc,
    etFormat,
    etCount,
    etCombine,
    etTernary,
    etGetAttr,
    etGetType,
    etSeqOps,
    etMapOp,
    etGrepOp,
//  etSumOp,
            etRegExp,
    etLiteral
};

class Expr;

struct FuncDeclStatement;

class ExprList {
public:
    FileLocation pos;
    std::list<std::unique_ptr<Expr>> values;

    Expr* getSecond()const
    {
        auto it = values.begin();
        ++it;
        return it->get();
    }

    inline void dump(std::string& out, char delim = ',');
};

enum StatementType {
    stNone,
    stExpr,
    stListAssign,
    stFuncDecl,
    stIf,
    stWhile,
    stReturn,
    stReturnIf,
    stYield,
    stForLoop,
    stClass,
    stClassMemberDef,
    stClassAttrDef,
    stVarList,
    stNamespace,
    stTryCatch,
    stThrow,
    stBreak,
    stNext,
    stRedo,
    stSwitch,
    stEnum,
    stProp,
    stAttr,
    stUse,
    stLiter
};

class StmtList;

class Statement {
public:
    StatementType st;
    FileLocation pos;
    FileLocation end;

    Statement() : st(stNone)
    {
    }

    virtual ~Statement() = default;

    virtual void dump(std::string& /*out*/)
    {
    };

    virtual void getChildData(std::vector<Expr*>& subExpr, std::vector<StmtList*>& subStmt) = 0;

    template<class T>
    T& as()
    {
        return *((T*) this);
    }
};

class StmtList {
public:
    std::list<std::unique_ptr<Statement>> values;

    void forgetValues()
    {
        for(auto& it: values)
        {
            (void)it.release();
        }
    }

    void dump(std::string& out)
    {
        for(auto& it : values)
        {
            it->dump(out);
        }
    }
};

inline void customformat(kst::FormatBuffer& buf, Expr* expr, int w, int p);

inline void customformat(kst::FormatBuffer& buf, ExprList* lst, int w, int p);

struct FuncParam : Name {
    std::unique_ptr<Expr> defValue;
    enum ParamType {
        ptNormal,
        ptVarArgs,
        ptNamedArgs
    };
    ParamType pt;

    explicit FuncParam(const Name& argName, ParamType argPT = ptNormal) : Name(argName), pt(argPT)
    {
    }

    FuncParam(const Name& argName, Expr* argDefValue) : Name(argName), defValue(argDefValue), pt(ptNormal)
    {
    }

    FuncParam(const FuncParam&) = delete;
    FuncParam( FuncParam &&) = delete;
    FuncParam& operator=(const FuncParam&) = delete;
    FuncParam& operator=(FuncParam&&) = delete;


    void dump(std::string& out);
};

class FuncParamList {
public:
    std::list<std::unique_ptr<FuncParam>> values;

    size_t size()const
    {
        return values.size();
    }

    void dump(std::string& out)
    {
        bool first = true;
        for(auto& it:values)
        {
            if(first)
            {
                first = false;
            }
            else
            {
                out += ",";
            }
            it->dump(out);
        }
    }
};

class Expr {
public:
    ExprType et;
    Expr* e1;
    Expr* e2;
    Expr* e3;
    ExprList* lst;
    FuncDeclStatement* func;

    NameList* ns;

    ZStringRef val;
    FileLocation pos;
    FileLocation end;
    bool global;
    bool rangeIncl;
    bool inBrackets;
    bool exprFunc;

    Expr(ExprType argEt, ZStringRef argVal) : et(argEt), e1(0), e2(0), e3(0), lst(0), func(0), ns(0), val(argVal),
                                              global(false), rangeIncl(true), inBrackets(false), exprFunc(false)
    {
    }

    Expr(ExprType argEt, Name argNm) : et(argEt), e1(0), e2(0), e3(0), lst(0), func(0), ns(0), val(argNm.val),
                                       pos(argNm.pos),
                                       global(false), rangeIncl(true), inBrackets(false), exprFunc(false)
    {
        end = pos;
        end.col += val->getLength();
    }

    Expr(ExprType argEt, Expr* a1 = 0, Expr* a2 = 0, Expr* a3 = 0) : et(argEt), e1(a1), e2(a2), e3(a3), lst(0), func(0),
                                                                     ns(0), val(),
                                                                     global(false), inBrackets(false), exprFunc(false)
    {
        if(a1)
        {
            pos = a1->pos;
            end = a1->end;
        }
        if(a2)
        {
            end = a2->end;
        }
        if(a3)
        {
            end = a3->end;
        }
        rangeIncl = true;
    }

    explicit Expr(FuncDeclStatement* argFunc);

    ~Expr();

    Symbol getSymbol()
    {
        return Symbol(Name(val, pos), ns, global);
    }

    void dump(std::string& out);

    bool isConst() const;

    bool isDeepConst() const;

    FileLocation getEnd();

    static bool isVal(ExprType et);

    static bool isUnary(ExprType et);

    static bool isBinary(ExprType et);

    static bool isTernary(ExprType et);
};

inline void FuncParam::dump(std::string& out)
{
    out += FORMAT("%{}%{}%{}", val.c_str(), defValue ? "=" : pt == ptVarArgs ? "[]" : pt == ptNamedArgs ? "{}" : "",
            defValue.get());
}


inline void customformat(kst::FormatBuffer& buf, Expr* expr, int, int)
{
    if(expr)
    {
        std::string out;
        expr->dump(out);
        buf.Append(out.c_str(), out.length());
    }
}

inline void customformat(kst::FormatBuffer& buf, ExprList* lst, int, int)
{
    if(lst)
    {
        std::string out;
        lst->dump(out);
        buf.Append(out.c_str(), out.length());
    }
}


void ExprList::dump(std::string& out, char delim)
{
    for(auto it = values.begin(), end = values.end(); it != end; ++it)
    {
        if(it != values.begin())
        {
            out += delim;
        }
        (*it)->dump(out);
    }
}


struct ExprStatement : Statement {
    std::unique_ptr<Expr> expr;

    explicit ExprStatement(Expr* argExpr) : expr(argExpr)
    {
        st = stExpr;
        if(expr)
        {
            pos = expr->pos;
            if(expr->ns)
            {
                pos = expr->ns->values.front().pos;
            }
            end = expr->end;
        }
    }

    ExprStatement(const ExprStatement&) = delete;
    ExprStatement( ExprStatement &&) = delete;
    ExprStatement& operator=(const ExprStatement&) = delete;
    ExprStatement& operator=(ExprStatement&&) = delete;

    void dump(std::string& out) override
    {
        expr->dump(out);
        out += "\n";
    }

    void getChildData(std::vector<Expr*>& subExpr, std::vector<StmtList*>& /*subStmt*/) override
    {
        subExpr.push_back(expr.get());
    }
};

struct ListAssignStatement : Statement {
    ExprList* lst1;
    ExprList* lst2;

    ListAssignStatement(ExprList* argLst1, ExprList* argLst2) : lst1(argLst1), lst2(argLst2)
    {
        st = stListAssign;
//    if(lst1 && !lst1->empty())
//    {
//      pos=lst1->front()->pos;
//      end=lst1->front()->end;
//    }else if(lst2 && !lst2->empty())
//    {
//      pos=lst2->front()->pos;
//      end=lst1->front()->end;
//    }
    }

    ~ListAssignStatement()
    {
        if(lst1)
        {
            delete lst1;
        }
        if(lst2)
        {
            delete lst2;
        }
    }

    void dump(std::string& out)
    {
        lst1->dump(out);
        out += "=";
        lst2->dump(out);
        out += "\n";
    }

    void getChildData(std::vector<Expr*>& subExpr, std::vector<StmtList*>& /*subStmt*/)
    {
        for(auto& it:lst1->values)
        {
            subExpr.push_back(it.get());
        }
        for(auto& it:lst2->values)
        {
            subExpr.push_back(it.get());
        }
    }
};


struct IfStatement : Statement {
    Expr* cond;
    StmtList* thenList;
    StmtList* elsifList;
    StmtList* elseList;

    IfStatement(Expr* argCond, StmtList* argThenList, StmtList* argElsifList = 0, StmtList* argElseList = 0) :
            cond(argCond), thenList(argThenList), elsifList(argElsifList), elseList(argElseList)
    {
        st = stIf;
//    pos=cond->pos;
//    end=cond->end;
//    if(thenList && !thenList->empty())end=thenList->back()->end;
//    if(elsifList && !elsifList->empty())end=elsifList->back()->end;
//    if(elseList && !elseList->empty())end=elseList->back()->end;
    }

    ~IfStatement()
    {
        if(cond)
        {
            delete cond;
        }
        if(thenList)
        {
            delete thenList;
        }
        if(elsifList)
        {
            delete elsifList;
        }
        if(elseList)
        {
            delete elseList;
        }
    }

    void dump(std::string& out)
    {
        out += "if ";
        cond->dump(out);
        out += "\n";
        if(thenList)
        {
            thenList->dump(out);
        }
        if(elsifList)
        {
            for(auto& stptr:elsifList->values)
            {
                IfStatement* ist = (IfStatement*) stptr.get();
                out += "elsif ";
                ist->cond->dump(out);
                out += "\n";
                ist->thenList->dump(out);
            }
        }

        if(elseList)
        {
            out += "else\n";
            elseList->dump(out);
        }
        out += "end\n";
    }

    void getChildData(std::vector<Expr*>& subExpr, std::vector<StmtList*>& subStmt)
    {
        subExpr.push_back(cond);
        if(thenList)
        {
            subStmt.push_back(thenList);
        }
        if(elsifList)
        {
            subStmt.push_back(elsifList);
        }
        if(elseList)
        {
            subStmt.push_back(elseList);
        }
    }
};

struct WhileStatement : Statement {
    Name name;
    Expr* cond;
    StmtList* body;

    WhileStatement(Name argName, Expr* argCond, StmtList* argBody) : name(argName), cond(argCond), body(argBody)
    {
        st = stWhile;
//    pos=cond->pos;
//    end=cond->end;
//    if(body && !body->empty())end=body->back()->end;
    }

    ~WhileStatement()
    {
        if(cond)
        {
            delete cond;
        }
        if(body)
        {
            delete body;
        }
    }

    void dump(std::string& out)
    {
        out += "while";
        if(name.val)
        {
            out += ":";
            out += name.val.c_str();
        }
        out += " ";
        cond->dump(out);
        out += "\n";
        if(body)
        {
            body->dump(out);
        }
        out += "end\n";
    }

    void getChildData(std::vector<Expr*>& subExpr, std::vector<StmtList*>& subStmt)
    {
        subExpr.push_back(cond);
        if(body)
        {
            subStmt.push_back(body);
        }
    }
};

struct ReturnStatement : ExprStatement {
    explicit ReturnStatement(Expr* argExpr) : ExprStatement(argExpr)
    {
        st = stReturn;
//    if(expr)
//    {
//      pos=expr->pos;
//      end=expr->end;
//    }
    }

    ReturnStatement(const ReturnStatement&) = delete;
    ReturnStatement( ReturnStatement &&) = delete;
    ReturnStatement& operator=(const ReturnStatement&) = delete;
    ReturnStatement& operator=(ReturnStatement&&) = delete;

    void dump(std::string& out) override
    {
        out += "return ";
        if(expr)
        {
            expr->dump(out);
        }
        out += "\n";
    }
};

struct ReturnIfStatement : ReturnStatement {
    explicit ReturnIfStatement(Expr* argExpr) : ReturnStatement(argExpr)
    {
        st = stReturnIf;
    }

    ReturnIfStatement(const ReturnIfStatement&) = delete;
    ReturnIfStatement( ReturnIfStatement &&) = delete;
    ReturnIfStatement& operator=(const ReturnIfStatement&) = delete;
    ReturnIfStatement& operator=(ReturnIfStatement&&) = delete;

    void dump(std::string& out) override
    {
        out += "returnif ";
        if(expr)
        {
            expr->dump(out);
        }
        out += "\n";
    }
};


struct YieldStatement : ReturnStatement {
    explicit YieldStatement(Expr* argExpr) : ReturnStatement(argExpr)
    {
        st = stYield;
    }

    YieldStatement(const YieldStatement&) = delete;
    YieldStatement( YieldStatement &&) = delete;
    YieldStatement& operator=(const YieldStatement&) = delete;
    YieldStatement& operator=(YieldStatement&&) = delete;

    void dump(std::string& out) override
    {
        out += "yield ";
        if(expr)
        {
            expr->dump(out);
        }
        out += "\n";
    }
};


struct FuncDeclStatement : Statement {
    Name name;
    FuncParamList* args;
    StmtList* body;
    bool isOnFunc;

    FuncDeclStatement(Name argName, FuncParamList* argArgs, StmtList* argBody, bool argIsOnFunc = false) : name(
            argName), args(argArgs), body(argBody), isOnFunc(argIsOnFunc)
    {
        st = stFuncDecl;
    }

    ~FuncDeclStatement()
    {
        if(args)
        {
            delete args;
        }
        if(body)
        {
            delete body;
        }
    }

    size_t argsCount()
    {
        return args ? args->size() : 0;
    }

    void dump(std::string& out)
    {
        out += "func ";
        out += name.val.c_str();
        out += "(";
        if(args)
        {
            args->dump(out);
        }
        out += ")\n";
        if(body)
        {
            body->dump(out);
        }
        out += "end\n";
    }

    void getChildData(std::vector<Expr*>& subExpr, std::vector<StmtList*>& subStmt)
    {
        if(args)
        {
            for(auto& fpp:args->values)
            {
                FuncParam& fp = *fpp;
                if(fp.defValue)
                {
                    subExpr.push_back(fp.defValue.get());
                }
            }
        }
        if(body)
        {
            subStmt.push_back(body);
        }
    }
};

struct ForLoopStatement : Statement {
    Name name;
    NameList* vars;
    Expr* expr;
    StmtList* body;

    ForLoopStatement(Name argName, NameList* argVars, Expr* argExpr, StmtList* argBody) : name(argName), vars(argVars),
                                                                                          expr(argExpr), body(argBody)
    {
        st = stForLoop;
//    pos=expr->pos;
    }

    ~ForLoopStatement()
    {
        if(vars)
        {
            delete vars;
        }
        if(expr)
        {
            delete expr;
        }
        if(body)
        {
            delete body;
        }
    }

    void dump(std::string& out)
    {
        out += "for";
        if(name.val)
        {
            out += ":";
            out += name.val.c_str();
        }
        out += " ";
        vars->dump(out);
        out += " in ";
        expr->dump(out);
        out += "\n";
        if(body)
        {
            body->dump(out);
        }
        out += "end\n";
    }

    void getChildData(std::vector<Expr*>& subExpr, std::vector<StmtList*>& subStmt)
    {
        subExpr.push_back(expr);
        if(body)
        {
            subStmt.push_back(body);
        }
    }
};

struct VarListStatement : Statement {
    NameList* vars;

    VarListStatement(NameList* argVars) : vars(argVars)
    {
        st = stVarList;
    }

    ~VarListStatement()
    {
        if(vars)
        {
            delete vars;
        }
    }

    void dump(std::string& out)
    {
        vars->dump(out);
        out += "\n";
    }

    void getChildData(std::vector<Expr*>& /*subExpr*/, std::vector<StmtList*>& /*subStmt*/)
    {

    }
};

struct ClassParent {
    Symbol name;
    ExprList* args;

    ClassParent(Symbol argName, ExprList* argArgs) : name(argName), args(argArgs)
    {
    }

    ~ClassParent()
    {
        if(args)
        {
            delete args;
        }
        if(name.ns)
        {
            delete name.ns;
        }
    }

    void dump(std::string& out)
    {
        out += name.toString();
        if(args)
        {
            out += "(";
            args->dump(out);
            out += ")";
        }
    }
};

struct AttrInstance {
    Expr* name;
    Expr* value;

    AttrInstance(Expr* argName, Expr* argValue = 0) : name(argName), value(argValue)
    {
    }

    ~AttrInstance()
    {
        delete name;
        if(value)
        {
            delete value;
        }
    }
};

struct AttrInstList : std::list<AttrInstance*> {
    ~AttrInstList()
    {
        for(iterator it = begin(), ed = end(); it != ed; ++it)
        {
            delete *it;
        }
    }

    void dump(std::string& out)
    {
        out += "{";
        bool first = true;
        for(iterator it = begin(), ed = end(); it != ed; ++it)
        {
            if(first)
            {
                first = false;
            }
            else
            {
                out += ",";
            }
            (*it)->name->dump(out);
            if((*it)->value)
            {
                out += "=";
                (*it)->value->dump(out);
            }
        }
        out += "}";
    }
};

struct ClassMemberDef : Statement {
    Name name;
    AttrInstList* attrs;
    Expr* value;

    ClassMemberDef(const Name& argName, AttrInstList* argAttrs, Expr* argValue = 0) : name(argName), attrs(argAttrs),
                                                                                      value(argValue)
    {
        st = stClassMemberDef;
//    pos=name.pos;
//    if(argValue)end=argValue->end;
    }

    ~ClassMemberDef()
    {
        if(attrs)
        {
            delete attrs;
        }
        if(value)
        {
            delete value;
        }
    }

    void dump(std::string& out)
    {
        out += name.val.c_str();
        if(attrs)
        {
            attrs->dump(out);
        }
        if(value)
        {
            out += "=";
            value->dump(out);
        }
        out += "\n";
    }

    void getChildData(std::vector<Expr*>& subExpr, std::vector<StmtList*>& /*subStmt*/)
    {
        if(value)
        {
            subExpr.push_back(value);
        }
    }
};

struct ClassAttrDef : Statement {
    AttrInstList* lst;

    ClassAttrDef(AttrInstList* argLst) : lst(argLst)
    {
        st = stClassAttrDef;
    }

    ~ClassAttrDef()
    {
        if(lst)
        {
            delete lst;
        }
    }

    void dump(std::string& out)
    {
        out += "@";
        lst->dump(out);
    }

    void getChildData(std::vector<Expr*>& subExpr, std::vector<StmtList*>& /*subStmt*/)
    {
        if(lst)
        {
            for(auto& ai:*lst)
            {
                if(ai->value)
                {
                    subExpr.push_back(ai->value);
                }
            }
        }
    }
};

struct ClassDefStatement : Statement {
    Name name;
    FuncParamList* args;
    ClassParent* parent;
    StmtList* body;

    ClassDefStatement(Name argName, FuncParamList* argArgs, ClassParent* argParent, StmtList* argBody) : name(argName),
                                                                                                         args(argArgs),
                                                                                                         parent(argParent),
                                                                                                         body(argBody)
    {
        st = stClass;
    }

    ~ClassDefStatement()
    {
        if(args)
        {
            delete args;
        }
        if(parent)
        {
            delete parent;
        }
        if(body)
        {
            delete body;
        }
    }

    void dump(std::string& out)
    {
        out += "class ";
        out += name.val.c_str();
        if(args)
        {
            out += "(";
            args->dump(out);
            out += ")";
        }
        if(parent)
        {
            out += ":";
            parent->dump(out);
        }
        out += "\n";
        if(body)
        {
            body->dump(out);
        }
        out += "end\n";
    }

    void getChildData(std::vector<Expr*>& subExpr, std::vector<StmtList*>& subStmt)
    {
        if(args)
        {
            for(auto& fp:args->values)
            {
                if(fp->defValue)
                {
                    subExpr.push_back(fp->defValue.get());
                }
            }
        }
        if(parent && parent->args)
        {
            for(auto& it:parent->args->values)
            {
                subExpr.push_back(it.get());
            }
        }
        if(body)
        {
            subStmt.push_back(body);
        }
    }
};

struct NamespaceStatement : Statement {
    NameList* ns;
    StmtList* body;
    Name name;

    NamespaceStatement(NameList* argNs, StmtList* argBody) : ns(argNs), body(argBody)
    {
        st = stNamespace;
    }

    ~NamespaceStatement()
    {
        if(ns)
        {
            delete ns;
        }
        if(body)
        {
            delete body;
        }
    }

    void dump(std::string& out)
    {
        out += "namespace ";
        if(ns)
        {
            ns->dump(out, "::");
        }
        out += "\n";
        if(body)
        {
            body->dump(out);
        }
        out += "end\n";
    }

    void getChildData(std::vector<Expr*>& /*subExpr*/, std::vector<StmtList*>& subStmt)
    {
        if(body)
        {
            subStmt.push_back(body);
        }
    }
};

struct TryCatchStatement : Statement {
    StmtList* tryBody;
    ExprList* exList;
    Name var;
    StmtList* catchBody;

    TryCatchStatement(StmtList* argTryBody, ExprList* argExList, Name argVar, StmtList* argCatchBody) : tryBody(
            argTryBody), exList(argExList), var(argVar), catchBody(argCatchBody)
    {
        st = stTryCatch;
//    if(tryBody)
//    {
//      pos=tryBody->front()->pos;
//      end=tryBody->back()->end;
//    }
//    if(catchBody)
//    {
//      end=catchBody->back()->end;
//    }
    }

    ~TryCatchStatement()
    {
        if(tryBody)
        {
            delete tryBody;
        }
        if(exList)
        {
            delete exList;
        }
        if(catchBody)
        {
            delete catchBody;
        }
    }

    void dump(std::string& out)
    {
        out += "try\n";
        tryBody->dump(out);
        out += "catch ";
        if(exList)
        {
            exList->dump(out);
        }
        out += " in ";
        out += var.val.c_str();
        out += "\n";
        if(catchBody)
        {
            catchBody->dump(out);
        }
        out += "end\n";
    }

    void getChildData(std::vector<Expr*>& subExpr, std::vector<StmtList*>& subStmt)
    {
        if(exList)
        {
            for(auto& it:exList->values)
            {
                subExpr.push_back(it.get());
            }
        }
        if(tryBody)
        {
            subStmt.push_back(tryBody);
        }
        if(catchBody)
        {
            subStmt.push_back(catchBody);
        }
    }
};

struct ThrowStatement : Statement {
    Expr* ex;

    ThrowStatement(Expr* argEx) : ex(argEx)
    {
        st = stThrow;
//    pos=ex->pos;
    }

    ~ThrowStatement()
    {
        if(ex)
        {
            delete ex;
        }
    }

    void dump(std::string& out)
    {
        out += "throw ";
        ex->dump(out);
        out += "\n";
    }

    void getChildData(std::vector<Expr*>& subExpr, std::vector<StmtList*>& /*subStmt*/)
    {
        subExpr.push_back(ex);
    }
};

struct NextStatement : Statement {
    Name id;

    NextStatement(Name argId = Name()) : id(argId)
    {
        st = stNext;
    }

    void dump(std::string& out)
    {
        out += "next ";
        if(id.val)
        {
            out += id.val.c_str();
        }
        out += "\n";
    }

    void getChildData(std::vector<Expr*>& /*subExpr*/, std::vector<StmtList*>& /*subStmt*/)
    {
    }

};

struct BreakStatement : Statement {
    Name id;

    BreakStatement(Name argId = Name()) : id(argId)
    {
        st = stBreak;
    }

    void dump(std::string& out)
    {
        out += "break ";
        if(id.val)
        {
            out += id.val.c_str();
        }
        out += "\n";
    }

    void getChildData(std::vector<Expr*>& /*subExpr*/, std::vector<StmtList*>& /*subStmt*/)
    {

    }
};

struct RedoStatement : Statement {
    Name id;

    RedoStatement(Name argId = Name()) : id(argId)
    {
        st = stRedo;
    }

    void dump(std::string& out)
    {
        out += "redo ";
        if(id.val)
        {
            out += id.val.c_str();
        }
        out += "\n";
    }

    void getChildData(std::vector<Expr*>& /*subExpr*/, std::vector<StmtList*>& /*subStmt*/)
    {

    }
};

struct SwitchCase {
    FileLocation pos;
    ExprList* caseExpr;
    StmtList* caseBody;

    SwitchCase(FileLocation argPos, ExprList* argExpr, StmtList* argBody) : pos(argPos), caseExpr(argExpr),
                                                                            caseBody(argBody)
    {
    }

    SwitchCase(const SwitchCase& argOther) : pos(argOther.pos), caseExpr(argOther.caseExpr), caseBody(argOther.caseBody)
    {
        const_cast<SwitchCase&>(argOther).caseExpr = 0;
        const_cast<SwitchCase&>(argOther).caseBody = 0;
    }

    ~SwitchCase()
    {
        if(caseExpr)
        {
            delete caseExpr;
        }
        if(caseBody)
        {
            delete caseBody;
        }
    }

    void dump(std::string& out)
    {
        if(caseExpr)
        {
            caseExpr->dump(out);
        }
        else
        {
            out += "*";
        }
        out += ":";
        if(caseBody)
        {
            caseBody->dump(out);
        }
        else
        {
            out += "\n";
        }
    }
};

struct SwitchCasesList : std::list<SwitchCase> {
    void dump(std::string& out)
    {
        for(iterator it = begin(), e = end(); it != e; ++it)
        {
            it->dump(out);
        }
    }
};

struct SwitchStatement : Statement {
    Expr* expr;
    SwitchCasesList* casesList;

    SwitchStatement(Expr* argExpr, SwitchCasesList* argCasesList) : expr(argExpr), casesList(argCasesList)
    {
        st = stSwitch;
    }

    ~SwitchStatement()
    {
        if(expr)
        {
            delete expr;
        }
        if(casesList)
        {
            delete casesList;
        }
    }

    void dump(std::string& out)
    {
        out += "switch";
        if(expr)
        {
            out += " ";
            expr->dump(out);
        }
        out += "\n";
        if(casesList)
        {
            casesList->dump(out);
        }
        out += "end\n";
    }

    void getChildData(std::vector<Expr*>& subExpr, std::vector<StmtList*>& subStmt)
    {
        if(expr)
        {
            subExpr.push_back(expr);
        }
        if(casesList)
        {
            for(auto& sc:*casesList)
            {
                if(sc.caseExpr)
                {
                    for(auto& it:sc.caseExpr->values)
                    {
                        subExpr.push_back(it.get());
                    }
                }
                if(sc.caseBody)
                {
                    subStmt.push_back(sc.caseBody);
                }
            }
        }
    }

};

struct EnumStatement : Statement {
    Name name;
    ExprList* items;
    bool bitEnum;

    EnumStatement(bool argBitEnum, Name argName, ExprList* argItems) : name(argName), items(argItems),
                                                                       bitEnum(argBitEnum)
    {
        st = stEnum;
//    pos=name.pos;
    }

    ~EnumStatement()
    {
        if(items)
        {
            delete items;
        }
    }

    void dump(std::string& out)
    {
        if(bitEnum)
        {
            out += "bitenum ";
        }
        else
        {
            out += "enum ";
        }
        out += name.val.c_str();
        out += "\n";
        if(items)
        {
            items->dump(out);
        }
        out += "\nend\n";
    }

    void getChildData(std::vector<Expr*>& subExpr, std::vector<StmtList*>& /*subStmt*/)
    {
        if(items)
        {
            for(auto& it:items->values)
            {
                subExpr.push_back(it.get());
            }
        }
    }

};

struct PropAccessor {
    Name name;
    Name arg;
    FileLocation pos;
    FileLocation end;
    StmtList* func;
    bool isGet;

    PropAccessor(const Name& argName) : name(argName), func(0), isGet(false)
    {
    }

    PropAccessor(StmtList* argFunc, const Name& argArg = Name()) : arg(argArg), func(argFunc), isGet(false)
    {
    }

    ~PropAccessor()
    {
        if(func)
        {
            delete func;
        }
    }
};

struct PropAccessorList : std::list<PropAccessor*> {
    ~PropAccessorList()
    {
        for(iterator it = begin(), ed = end(); it != ed; ++it)
        {
            delete *it;
        }
    }
};

struct PropStatement : Statement {
    Name name;
    Name getName;
    FuncDeclStatement* getFunc;
//  FileLocation getPos,getEnd;
    Name setName;
//  Name setArgName;
    FuncDeclStatement* setFunc;

//  FileLocation setPos,setEnd;
//  bool haveGetter,haveSetter;
    PropStatement(Name argName) : name(argName), getFunc(0), setFunc(0)
    {
        st = stProp;
//    pos=name.pos;
    }

    ~PropStatement()
    {
        if(getFunc)
        {
            delete getFunc;
        }
        if(setFunc)
        {
            delete setFunc;
        }
    }

    void dump(std::string& out)
    {
        out += "prop ";
        out += name.val.c_str();
        out += "\n";
        if(getName.val.get())
        {
            out += "get=";
            out += getName.val.c_str();
        }
        else if(getFunc)
        {
            out += "get()\n";
            if(getFunc->body)
            {
                getFunc->body->dump(out);
            }
            out += "end\n";
        }

        if(setName.val.get())
        {
            out += "set=";
            out += setName.val.c_str();
        }
        else if(setFunc)
        {
            out += "set(";
            out += setFunc->args->values.front()->val.c_str();
            out += ")\n";
            if(setFunc->body)
            {
                setFunc->body->dump(out);
            }
            out += "end\n";
        }
    }

    void getChildData(std::vector<Expr*>& /*subExpr*/, std::vector<StmtList*>& subStmt)
    {
        if(getFunc && getFunc->body)
        {
            subStmt.push_back(getFunc->body);
        }
        if(setFunc && setFunc->body)
        {
            subStmt.push_back(setFunc->body);
        }
    }
};

struct AttrDeclStatement : Statement {
    Name name;
    Expr* value;

    AttrDeclStatement(const Name& argName, Expr* argValue = 0) : name(argName), value(argValue)
    {
        st = stAttr;
//    pos=name.pos;
    }

    ~AttrDeclStatement()
    {
        if(value)
        {
            delete value;
        }
    }

    void dump(std::string& out)
    {
        out += "attr ";
        out += name.val.c_str();
        if(value)
        {
            out += "=";
            value->dump(out);
        }
        out += "\n";
    }

    void getChildData(std::vector<Expr*>& subExpr, std::vector<StmtList*>& /*subStmt*/)
    {
        if(value)
        {
            subExpr.push_back(value);
        }
    }
};

struct UseStatement : Statement {
    Expr* expr;

    UseStatement(Expr* argExpr) : expr(argExpr)
    {
//    pos=expr->pos;
//    end=expr->end;
        st = stUse;
    }

    ~UseStatement()
    {
        if(expr)
        {
            delete expr;
        }
    }

    void dump(std::string& out)
    {
        out += "use ";
        expr->dump(out);
        out += "\n";
    }

    void getChildData(std::vector<Expr*>& subExpr, std::vector<StmtList*>& /*subStmt*/)
    {
        subExpr.push_back(expr);
    }
};

enum LiterType {
    ltInt,
    ltDouble,
    ltNumber,
    ltString
};

struct LiterArg {
    LiterType lt;
    Name name;
    Name marker;
    bool optional;

    LiterArg(LiterType argLt, const Name& argName, const Name& argMarker, bool argOpt = false) :
            lt(argLt), name(argName), marker(argMarker), optional(argOpt)
    {
    }

    void dump(std::string& out)
    {
        switch(lt)
        {
            case ltInt:
                out += '%';
                break;
            case ltDouble:
                out += '/';
                break;
            case ltNumber:
                out += '+';
                break;
            case ltString:
                out += '$';
                break;
        }
        out += name.val.c_str();
        if(marker.val.get())
        {
            out += ':';
            out += marker.val.c_str();
        }
        if(optional)
        {
            out += '?';
        }
    }
};

typedef std::list<std::unique_ptr<LiterArg>> LiterArgsList;

struct LiterStatement : Statement {
    Name name;
    LiterArgsList* args;
    StmtList* body;


    LiterStatement(Name argName, LiterArgsList* argArgs, StmtList* argBody) : name(argName), args(argArgs),
                                                                              body(argBody)
    {
        st = stLiter;
    }

    ~LiterStatement()
    {
//        for(auto ptr:*args)
//        {
//            delete ptr;
//        }
        delete args;
        delete body;
    }

    void dump(std::string& out)
    {
        out += "liter ";
        for(auto& ptr:*args)
        {
            ptr->dump(out);
        }
        out += "\n";
        if(body)
        {
            body->dump(out);
        }
        out += "end\n";
    }

    void getChildData(std::vector<Expr*>& /*subExpr*/, std::vector<StmtList*>& subStmt)
    {
        /*if(args)
        {
          for(LiterArgsList::iterator it=args->begin(),end=args->end();it!=end;++it)
          {
            LiterArg& la=**it;
          }
        }*/
        if(body)
        {
            subStmt.push_back(body);
        }
    }
};

}

#endif
