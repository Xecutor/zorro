#include "Debugger.hpp"

#include "ZorroVM.hpp"
#include "SynTree.hpp"

namespace zorro {

namespace {
bool isInside(uint32_t line, uint32_t col, const FileLocation& start, const FileLocation& end)
{
    return ((line > start.line) || (line == start.line && col >= start.col)) &&
           ((line < end.line) || (line == end.line && col <= end.col));
}
}

bool Debugger::StepCondition::operator()(ZorroVM* vm)
{
    if(!vm->ctx.nextOp)
    {
        return false;
    }
    if(vm->ctx.callStack.size() > depthLevel && !stepInto)
    {
        return true;
    }
    FileLocation pos = vm->ctx.nextOp->pos;
    return pos.fileRd == start.fileRd && (pos.line == start.line || isInside(pos.line, pos.col, start, end));
}

void Debugger::start()
{
    vm->ctx.nextOp = vm->entry.get()->code;
}

void Debugger::stepOver()
{
    if(!vm->ctx.nextOp)
    {
        return;
    }
    StepCondition cnd = makeStepCondition(vm->ctx.nextOp->pos, false);
    do
    {
        vm->step();
    } while(vm->ctx.nextOp && cnd(vm));
}

void Debugger::stepInto()
{
    if(!vm->ctx.nextOp)
    {
        return;
    }
    StepCondition cnd = makeStepCondition(vm->ctx.nextOp->pos, true);
    do
    {
        vm->step();
    } while(vm->ctx.nextOp && cnd(vm));
}

Debugger::StepCondition Debugger::makeStepCondition(FileLocation pos, bool stepInto)
{
    Statement* st = findStmt(pos);
    if(st)
    {
        std::vector<Expr*> subExpr;
        std::vector<StmtList*> subStmts;
        st->getChildData(subExpr, subStmts);
        if(subExpr.empty())
        {
            return {pos, pos, vm->ctx.callStack.size(), stepInto};
        } else
        {
            return {subExpr.front()->pos, subExpr.back()->end, vm->ctx.callStack.size(), stepInto};
        }
    } else
    {
        return {pos, pos, vm->ctx.callStack.size(), stepInto};
    }
}

Statement* Debugger::findStmt(FileLocation pos)
{
    return findStmtRec(tree, pos);
}

Statement* Debugger::findStmtRec(StmtList* subtree, FileLocation pos)
{
    std::vector<Expr*> subExpr;
    std::vector<StmtList*> subStmts;
    for(auto& st:*subtree)
    {
        if(st->pos.fileRd == pos.fileRd && isInside(pos.line, pos.col, st->pos, st->end))
        {
            st->getChildData(subExpr, subStmts);
            if(subStmts.empty())
            {
                return st;
            } else
            {
                for(auto& subSt:subStmts)
                {
                    if(!subSt->empty())
                    {
                        if(isInside(pos.line, pos.col, subSt->front()->pos, subSt->back()->end))
                        {
                            return findStmtRec(subSt, pos);
                        }
                    }
                }
                return 0;
            }
        }
    }
    return 0;
}

std::string Debugger::getCurrentLine()
{
    if(!vm->ctx.nextOp)
    {
        return std::string();
    }
    FileLocation pos = vm->ctx.nextOp->pos;
    FileLocation end = pos;
    Statement* stmt = findStmt(pos);
    if(stmt)
    {
        std::vector<Expr*> subExpr;
        std::vector<StmtList*> subStmts;
        stmt->getChildData(subExpr, subStmts);
        if(!subExpr.empty())
        {
            end = subExpr.front()->end;
        }
    }
    FileReader* rd = pos.fileRd;
    if(!rd)
    {
        return std::string();
    }
    pos.offset -= pos.col;
    pos.col = 0;
    rd->setLoc(pos);
    std::string rv, tmp;
    int eoln;
    while(rd->readLine(rv, eoln, true) && rd->getLoc() <= end)
    {
        rv += "\n";
    }
    if(!rv.empty())
    {
        return rv;
    } else
    {
        return std::string();
    }
}

}
