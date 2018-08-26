#ifndef __ZORRO_ZORROLEXER_HPP__
#define __ZORRO_ZORROLEXER_HPP__

#include "LexerBase.hpp"
#include "ZHash.hpp"
#include "MacroExpander.hpp"

namespace zorro {

#ifndef ZORRO_USER_TERMS_COUNT
#define ZORRO_USER_TERMS_COUNT 0
#endif

enum TermType {
    FirstTerm,
    tInt = FirstTerm,
    tDouble,
    tString,
    tRawString,
    tNil,
    tTrue,
    tFalse,
    tIdent,
    tPlus,
    tPlusPlus,
    tPlusEq,
    tMinus,
    tMinusMinus,
    tMinusEq,
    tMul,
    tMulEq,
    tDiv,
    tDivEq,
    tAmp,
    tDoubleAmp,
    tMod,
    tModEq,
    tLess,
    tGreater,
    tLessEq,
    tGreaterEq,
    tEqual,
    tNotEqual,
    tMatch,
    tAnd,
    tOr,
    tNot,
    tORBr,
    tCRBr,
    tOSBr,
    tCSBr,
    tOCBr,
    tCCBr,
    tDot,
    tDotQMark,
    tDotStar,
    tDoubleDot,
    tDoubleDotLess,
    tColon,
    tDoubleColon,
    tStarColon,
    tComma,
    tArrow,
    tThinArrow,
    tEq,
    tAt,
    tFormat,
    tExclSign,
    tQuestMark,
    tHash,
    tPipe,
    tEol,
    tEof,
    tFunc,
    tEnd,
    tReturn,
    tReturnIf,
    tYield,
    tIf,
    tElse,
    tElsif,
    tWhile,
    tFor,
    tIn,
    tIs,
    tOn,
    tClass,
    tNamespace,
    tTry,
    tCatch,
    tThrow,
    tNext,
    tBreak,
    tRedo,
    tSwitch,
    tEnum,
    tBitEnum,
    tInclude,
    tProp,
    tAttr,
    tAttrGet,
    tUse,
    tNumArg,
    tCarret,
    tMapOp,
    tGrepOp,
    tRegExp,
    tLiter,
    tLiteral,
    tMacro,
    tMacroExpand,
    tComment,
    UserTermStart,
    TermsCount = UserTermStart + ZORRO_USER_TERMS_COUNT
};


struct ZLexer : LexerBase<ZLexer, TermType> {

    static const TermType MaxTermId = TermsCount;
    static const TermType EofTerm = tEof;
    static const TermType EolTerm = tEol;
    static const TermType UnknownTerm = tEol;

    static const char* names[TermsCount];

    static ZHash<TermType> idents;

    typedef TermType TypeOfTerm;

    static const char* getTermName(Term t);

    ZMacroExpander* macroExpander;

    ZLexer();


    void matchNumber();

    void matchIdent();

    void matchUIdent();

    void matchLineComment();

    void matchMLComment();

    void matchString();

    void matchRawString();

    void matchDollar();

    void matchNumArg();

    void matchRegExp();

    void matchMacroExpand();

    bool cmp(Term& t, const char* str)
    {
        unsigned int len = 0;
        const char* val = t.getValue(len);
        unsigned i = 0;
        while(i < len && val[i] == str[i])
        {
            i++;
        }
        return i == len;
    }
};

}

#endif
