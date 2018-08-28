#include <utility>

#ifndef __ZORRO_CODE_GENERATOR_HPP__
#define __ZORRO_CODE_GENERATOR_HPP__

#include "ZVMOpsDefs.hpp"
#include "Symbolic.hpp"
#include "Exceptions.hpp"
#include "FileReader.hpp"

namespace zorro {

class ZorroVM;

class Expr;

class StmtList;

class Statement;

class FuncParamList;

struct ClassDefStatement;
struct OpJumpBase;

struct CGWarning {
    std::string msg;
    FileLocation pos;

    CGWarning(std::string argMsg, FileLocation argPos) : msg(std::move(argMsg)), pos(argPos)
    {
    }
};

class CodeGenerator {
public:
    CodeGenerator(ZorroVM* argVm);

    void generate(StmtList* sl);

    void generateMacro(const Name& name, FuncParamList* args, StmtList* sl);

    typedef std::vector<CGWarning> WarnVector;

    WarnVector warnings;

    ZorroVM* vm;
    SymbolsInfo* si;
    typedef std::vector<OpBase * *> FixVector;

    struct OpPair {
        ZorroVM* vm;
        ZCodeRef first;
        OpBase* last;
        OpBase* link;
        FileLocation pos;
        FixVector efixes;
        bool skipNext;
        mutable bool owned;

        OpPair(FileLocation argPos, ZorroVM* argVm, OpBase* argFirst = 0) :
            vm(argVm), first(new ZCode(vm, argFirst)), last(argFirst), link(0), pos(argPos), skipNext(false),
            owned(true)
        {
            if(*first)
            {
                first->pos = pos;
            }
        }

        OpPair(FileLocation argPos, ZorroVM* argVm, OpBase* argFirst, OpBase* argLast) :
            vm(argVm), first(new ZCode(vm, argFirst)), last(argLast), link(nullptr), pos(argPos), skipNext(false), owned(true)
        {
        }
        //merge two pairs

        ~OpPair()
        {
            if(!owned)
            {
                if(*first)
                {
                    first.release();
                }
            }
        }

        void release() const
        {
            owned = false;
        }

        void addFix(OpBase*& op)
        {
            efixes.push_back(&op);
        }

        void chainFixes(OpPair& op)
        {
            if(!op.skipNext && op.last)
            {
                addFix(op.last->next);
            }
            efixes.insert(efixes.end(), op.efixes.begin(), op.efixes.end());
        }

        void addFixes(const FixVector& v)
        {
            efixes.insert(efixes.end(), v.begin(), v.end());
        }

        void fixLast(ZCodeRef& next)
        {
            fixLast(*next);
        }

        bool cannotBeFixed()const
        {
            return (skipNext || !last) && efixes.empty();
        }

        void fixLast(OpBase* next)
        {
            if(!skipNext)
            {
                if(last)
                {
                    last->next = next;
                }
            }
            for(auto& efix : efixes)
            {
                *efix = next;
            }
            efixes.clear();
            skipNext = true;
        }

        void operator+=(const OpPair& argOther)
        {
            if(!*argOther.first)
            {
                if(argOther.link)
                {
                    fixLast(argOther.link);
                }
                return;
            }
            if(!*first)
            {
                first.get()->code = *argOther.first;
            }
            if(last)
            {
                if(efixes.empty() && skipNext)
                {
                    skipNext = false;
                    return;
                }
                for(auto& efix : efixes)
                {
                    *efix = *argOther.first;
                }
                efixes.clear();
                if(skipNext)
                {
                    skipNext = false;
                } else
                {
                    last->next = *argOther.first;
                }
            }
            last = argOther.last;
            efixes = argOther.efixes;
            skipNext = argOther.skipNext;
            argOther.release();
        }

        void operator+=(OpBase* argOther)
        {
            if(argOther)
            {
                argOther->pos = pos;
            }
            if(!*first)
            {
                first.get()->code = argOther;
            }
            if(last)
            {
                if(efixes.empty() && skipNext)
                {
                    ZCode zc(vm, argOther);
                    //fprintf(stderr,"killed:%s\n",argOther->pos.backTrace().c_str());
                    skipNext = false;
                    return;
                }
                for(auto& efix : efixes)
                {
                    *efix = argOther;
                }
                efixes.clear();
                if(skipNext)
                {
                    skipNext = false;
                } else
                {
                    last->next = argOther;
                }
            }
            last = argOther;
        }

        void setLast(OpBase* argLast)
        {
            skipNext = false;
            last = argLast;
            if(last)
            {
                last->pos = pos;
            }
        }

        bool isLastReturn()
        {
            return last && last->ot == otReturn && efixes.empty();
        }
    };

    /*
    enum ExprFlags{
      efToStack=1,
      efLvalue=2,
      efTopLevel=4
    };
    */

    struct ExprContext {
        SymbolsInfo* si;
        std::vector<index_type> tmps;
        OpArg dst;
        bool lvalue;
        bool constAccess;
        bool ifContext;
        typedef std::vector<OpJumpBase*> JVector;
        JVector jumps;
        OpBase* lastBinOp;

        ExprContext(SymbolsInfo* argSi, const OpArg& argDst = OpArg()) :
            si(argSi), dst(argDst), lvalue(false), constAccess(false), ifContext(false), lastBinOp(nullptr)
        {
        }

        ExprContext(const ExprContext& argOther, const OpArg& argDst = OpArg()) :
            si(argOther.si), dst(argDst), lvalue(false), constAccess(argOther.constAccess),
            ifContext(argOther.ifContext), jumps(argOther.jumps), lastBinOp(argOther.lastBinOp)
        {
        }

        ~ExprContext()
        {
            release();
        }

        void release()
        {
            for(auto tmp : tmps)
            {
                si->releaseTemp(tmp);
            }
            tmps.clear();
        }

        ExprContext& setLvalue()
        {
            lvalue = true;
            return *this;
        }

        ExprContext& setConst()
        {
            constAccess = true;
            return *this;
        }

        ExprContext& setIf()
        {
            ifContext = true;
            return *this;
        }

        OpJumpBase* addJump(OpJumpBase* jmp)
        {
            jumps.push_back(jmp);
            return jmp;
        }

        void addJumps(const JVector& jmps)
        {
            jumps.insert(jumps.begin(), jmps.begin(), jmps.end());
        }

        void addTemp(const OpArg& oa)
        {
            tmps.push_back(oa.idx);
        }

        void setTmpDst(const OpArg& oa)
        {
            dst = oa;
            tmps.push_back(oa.idx);
        }

        OpArg mkTmpDst()
        {
            dst = OpArg(atLocal, si->acquireTemp());
            addTemp(dst);
            return dst;
        }
    };

    OpPair initOps;


    OpArg nil;
    ZStringRef selfName;

    struct ClassDefItem {
        ClassDefStatement* classPtr;
        bool classHasMemberInit;

        ClassDefItem(ClassDefStatement* argClassPtr, bool argClassHasMemberInit) :
            classPtr(argClassPtr), classHasMemberInit(argClassHasMemberInit)
        {
        }
    };

    std::list<ClassDefItem> classStack;
    ClassDefStatement* currentClass;
    bool classHasMemberInit;

    bool interruptedFlow;

    void fillNames(StmtList* sl, bool deep = false);

    void fillClassNames(StmtList* sl);

    void checkDuplicate(const Name& nm, const char* type);

    void getExprType(Expr* e, TypeInfo& info);

    void fillTypes(StmtList* sl);

    void fillTypes(Statement* st);

    OpPair generateExpr(Expr* expr, ExprContext& ec);

    OpArg genArgExpr(OpPair& op, Expr* expr, ExprContext& ec);

    OpPair generateStmtList(StmtList* sl);

    void generateStmt(OpPair& op, Statement& st);

    bool fillConstant(Expr* expr, Value& val);

    OpPair fillDst(const FileLocation& pos, const OpArg& src, ExprContext& ec);

    OpArg genPropGetter(const FileLocation& pos, OpPair& op, ClassPropertyInfo* cp, ExprContext& ec);

    void genPropSetter(const FileLocation& pos, OpPair& op, ClassPropertyInfo* cp, OpArg src);

    OpPair genArgs(OpPair& fp, FuncParamList* args, bool isCtor, index_type& selfIdx);

    void finArgs(OpPair& fp, OpPair& argOps);

    void gatherNumArgs(Expr* expr, std::vector<size_t>& args);

    OpArg getArgType(Expr* expr, bool lvalue = false);

    bool isSimple(Expr* expr);

    bool isConstant(Expr* expr);

    /* method or closure */
    SymInfo* isSpecial(Expr* expr);

    bool isIntConst(Expr* expr, int64_t& value);

    bool isIntRange(Expr* ex, int64_t& start, int64_t& end, int64_t& step);

    template<class OP>
    inline OpPair genBinOp(Expr* expr, ExprContext& ec);

    template<class OP>
    OpPair genBoolOp(Expr* expr, ExprContext& ec);

    template<class OP>
    inline OpPair genUnOp(Expr* expr, ExprContext& ec, bool lvalue = false);
};

}

#endif
