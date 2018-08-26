#ifndef __ZORRO_DEBUGGER_HPP__
#define __ZORRO_DEBUGGER_HPP__

#include "ZorroVM.hpp"
#include "SynTree.hpp"

namespace zorro {


class Debugger {
public:
    Debugger(ZorroVM* argVm, StmtList* argTree) : vm(argVm), tree(argTree)
    {
    }

    void start();

    void stepOver();

    void stepInto();

    std::string getCurrentLine();

protected:
    ZorroVM* vm;
    StmtList* tree;

    struct StepCondition {
        FileLocation start, end;
        size_t depthLevel;
        bool stepInto;

        bool operator()(ZorroVM* vm);
    };


    Statement* findStmt(FileLocation pos);

    Statement* findStmtRec(StmtList* subtree, FileLocation pos);

    StepCondition makeStepCondition(FileLocation pos, bool stepInto);
};

}

#endif
