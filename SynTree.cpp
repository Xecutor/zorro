#include "SynTree.hpp"

namespace zorro {

Expr::Expr(FuncDeclStatement* argFunc) : et(etFunc), e1(0), e2(0), e3(0), lst(0), func(argFunc), ns(0),
    global(false), rangeIncl(true), inBrackets(false), exprFunc(false)
{
    pos = func->pos;
    end = func->end;
}


Expr::~Expr()
{
    /*std::string out;
    dump(out);
    printf("deleting expr %p(%s)\n",this,out.c_str());
    fflush(stdout);*/

    if(e1)
    {
        delete e1;
    }
    if(e2)
    {
        delete e2;
    }
    if(e3)
    {
        delete e3;
    }
    if(lst)
    {
        delete lst;
    }
    if(func)
    {
        delete func;
    }
    if(ns)
    {
        delete ns;
    }
}


void Expr::dump(std::string& out)
{
    {
        switch(et)
        {
            case etPlus:
                out += FORMAT("(%{}+%{})", e1, e2);
                break;
            case etSPlus:
                out += FORMAT("(%{}+=%{})", e1, e2);
                break;
            case etPreInc:
                out += FORMAT("(++%{})", e1);
                break;
            case etPostInc:
                out += FORMAT("(%{}++)", e1);
                break;
            case etPreDec:
                out += FORMAT("(--%{})", e1);
                break;
            case etPostDec:
                out += FORMAT("(%{}--)", e1);
                break;
            case etMinus:
                out += FORMAT("(%{}-%{})", e1, e2);
                break;
            case etSMinus:
                out += FORMAT("(%{}-=%{})", e1, e2);
                break;
            case etMul:
                out += FORMAT("(%{}*%{})", e1, e2);
                break;
            case etSMul:
                out += FORMAT("(%{}*=%{})", e1, e2);
                break;
            case etDiv:
                out += FORMAT("(%{}/%{})", e1, e2);
                break;
            case etSDiv:
                out += FORMAT("(%{}/=%{})", e1, e2);
                break;
            case etMod:
                out += FORMAT("(%{}%%%{})", e1, e2);
                break;
            case etSMod:
                out += FORMAT("(%{}%%=%{})", e1, e2);
                break;
            case etBitOr:
                out += FORMAT("(%{}|%{})", e1, e2);
                break;
            case etBitAnd:
                out += FORMAT("(%{}&%{})", e1, e2);
                break;
            case etRef:
                out += FORMAT("&(%{})", e1);
                break;
            case etWeakRef:
                out += FORMAT("&&(%{})", e1);
                break;
            case etCor:
                out += FORMAT("%(%{})", e1);
                break;
            case etCopy:
                out += FORMAT("*(%{})", e1);
                break;
            case etLess:
                out += FORMAT("(%{}<%{})", e1, e2);
                break;
            case etGreater:
                out += FORMAT("(%{}>%{})", e1, e2);
                break;
            case etLessEq:
                out += FORMAT("(%{}<=%{})", e1, e2);
                break;
            case etGreaterEq:
                out += FORMAT("(%{}>=%{})", e1, e2);
                break;
            case etEqual:
                out += FORMAT("(%{}==%{})", e1, e2);
                break;
            case etNotEqual:
                out += FORMAT("(%{}!=%{})", e1, e2);
                break;
            case etMatch:
                out += FORMAT("(%{}=~%{})", e1, e2);
                break;
            case etIn:
                out += FORMAT("(%{} in %{})", e1, e2);
                break;
            case etIs:
                out += FORMAT("(%{} is %{})", e1, e2);
                break;
            case etOr:
                out += FORMAT("(%{} or %{})", e1, e2);
                break;
            case etAnd:
                out += FORMAT("(%{} and %{})", e1, e2);
                break;
            case etNot:
                out += FORMAT("(not %{})", e1);
                break;
            case etNeg:
                out += FORMAT("(-%{})", e1);
                break;
            case etAssign:
                out += FORMAT("(%{}=%{})", e1, e2);
                break;
            case etTernary:
                out += FORMAT("(%{}?%{}:%{})", e1, e2, e3);
                break;
            case etInt:
            case etDouble:
                out += val.c_str();
                break;
            case etString:
                out += '"';
                out += val.c_str();
                out += '"';
                break;
            case etRange:
                out += FORMAT("(%{}..%{}%{}%{}%{})", e1, rangeIncl ? "" : "<", e2, e3 ? ":" : "", e3);
                break;
            case etNil:
                out += "nil";
                break;
            case etTrue:
                out += "true";
                break;
            case etFalse:
                out += "false";
                break;
            case etVar:
                if(ns)
                {
                    ns->dump(out, "::");
                    out += "::";
                }
                out += val.c_str();
                break;
            case etPair:
                out += FORMAT("%{}=>%{}", e1, e2);
                break;
            case etCall:
                out += FORMAT("%{}(%{})", e1, lst);
                break;
            case etProp:
                out += FORMAT("(%{}.%{})", e1, e2);
                break;
            case etPropOpt:
                out += FORMAT("(%{}.?%{})", e1, e2);
                break;
            case etKey:
                out += FORMAT("(%{}{%{}})", e1, e2);
                break;
            case etGetAttr:
                out += FORMAT("(%{}.{%{}%{}%{}%{}})", e1, e2, e2 ? "," : "", e3);
                break;
            case etIndex:
                out += FORMAT("(%{}[%{}])", e1, e2);
                break;
            case etArray:
                out += FORMAT("[%{}]", lst);
                break;
            case etSet:
            case etMap:
                out += FORMAT("{%{}}", lst);
                break;
            case etFunc:
                out += "func(";
                if(func->args)
                {
                    func->args->dump(out);
                }
                out += ")\n";
                if(func->body)
                {
                    func->body->dump(out);
                }
                out += "end";
                break;
            case etFormat:
                out += val.c_str();
                out += ":";
                if(e1)
                {
                    e1->dump(out);
                } else
                {
                    if(lst)
                    {
                        lst->dump(out);
                    }
                }
                break;
            case etCombine:
                out += FORMAT("combine(%{})", lst);
                break;
            case etCount:
                out += FORMAT("#(%{})", e1);
                break;
            case etGetType:
                out += FORMAT("?(%{})", e1);
                break;
            case etNumArg:
                out += val.c_str();
                break;
            case etSeqOps:
                out += FORMAT("^%{}:%{}%{}", e2, e1, lst);
                break;
            case etMapOp:
                out += FORMAT(":>%{}", e1);
                break;
            case etGrepOp:
                out += FORMAT(":?%{}", e1);
                break;
                //case etSumOp:out+=FORMAT(":+%{}",e1);break;
            case etRegExp:
                out += val.c_str();
                break;
            case etLiteral:
                lst->dump(out);
                break;
        }
    }
}

bool Expr::isConst() const
{
    switch(et)
    {
        case etNil:
        case etTrue:
        case etFalse:
        case etInt:
        case etString:
            return true;
        case etNeg:
            return e1->isConst();
        default:
            return false;
    }
}

bool Expr::isDeepConst() const
{
    if(isConst())
    {
        return true;
    }
    switch(et)
    {
        case etPlus:
        case etMinus:
        case etMul:
        case etDiv:
        case etMod:
        case etBitOr:
        case etBitAnd:
        case etLess:
        case etGreater:
        case etLessEq:
        case etGreaterEq:
        case etEqual:
        case etNotEqual:
        case etOr:
        case etAnd:
        case etIn:
        case etPair:
            return e1->isConst() && e2->isConst();
        case etNot:
            return e1->isConst();
        case etRange:
            return e1->isConst() && e2->isConst() && (!e3 || e3->isConst());
        case etSet:
        case etArray:
        case etMap:
            if(lst)
            {
                for(auto* eptr:*lst)
                {
                    if(!eptr->isConst())
                    {
                        return false;
                    }
                }
            }
            return true;
        default:
            return false;
    }
}

bool Expr::isVal(ExprType et)
{
    switch(et)
    {
        case etNil:
        case etTrue:
        case etFalse:
//    case etArray:
//    case etMap:
//    case etSet:
        case etVar:
        case etNumArg:
//    case etCall:
//    case etFunc:
//    case etCombine:
//    case etSeqOps:
//    case etMapOp:
//    case etGrepOp:
//    case etSumOp:
        case etRegExp:
        case etLiteral:
            return true;
        default:
            return false;
    }
}

bool Expr::isUnary(ExprType et)
{
    switch(et)
    {
        case etPreInc:
        case etPostInc:
        case etPreDec:
        case etPostDec:
        case etNeg:
        case etCopy:
        case etRef:
        case etWeakRef:
        case etCor:
        case etNot:
        case etFormat:
        case etCount:
        case etGetType:
            return true;
        default:
            return false;
    }
}

bool Expr::isBinary(ExprType et)
{
    switch(et)
    {
        case etInt:
        case etDouble:
        case etString:
        case etPlus:
        case etSPlus:
        case etMinus:
        case etSMinus:
        case etMul:
        case etSMul:
        case etDiv:
        case etSDiv:
        case etMod:
        case etSMod:
        case etBitOr:
        case etBitAnd:
        case etAssign:
        case etLess:
        case etGreater:
        case etLessEq:
        case etGreaterEq:
        case etEqual:
        case etNotEqual:
        case etMatch:
        case etOr:
        case etAnd:
        case etIn:
        case etIs:
        case etPair:
        case etProp:
        case etPropOpt:
        case etKey:
        case etIndex:
            return true;
        default:
            return false;
    }
}

bool Expr::isTernary(ExprType et)
{
    switch(et)
    {
        case etTernary:
        case etRange:
        case etGetAttr:
            return true;
        default:
            return false;
    }

}


}
