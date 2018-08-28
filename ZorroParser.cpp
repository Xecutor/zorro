#include "ZorroParser.hpp"
#include "ZorroVM.hpp"
#include <stdexcept>

namespace zorro {


static ExprType binopMap[TermsCount];
static ExprType valMap[TermsCount];

ZParser::ZParser(ZMemory* argMem) : mem(argMem), result(0)
{
    for(int i = FirstTerm; i < TermsCount; i++)
    {
        termMap[ZLexer::getTermName((TermType) i)] = (TermType) i;
        binopMap[i] = (ExprType) -1;
        valMap[i] = (ExprType) -1;
    }

    binopMap[tPlus] = etPlus;
    binopMap[tPlusEq] = etSPlus;
    binopMap[tMinus] = etMinus;
    binopMap[tMinusEq] = etSMinus;
    binopMap[tMul] = etMul;
    binopMap[tMulEq] = etSMul;
    binopMap[tDiv] = etDiv;
    binopMap[tDivEq] = etSDiv;
    binopMap[tEq] = etAssign;
    binopMap[tEqual] = etEqual;
    binopMap[tNotEqual] = etNotEqual;
    binopMap[tMatch] = etMatch;
    binopMap[tLess] = etLess;
    binopMap[tGreater] = etGreater;
    binopMap[tLessEq] = etLessEq;
    binopMap[tGreaterEq] = etGreaterEq;
    binopMap[tOr] = etOr;
    binopMap[tAnd] = etAnd;
    binopMap[tIn] = etIn;
    binopMap[tIs] = etIs;
    binopMap[tPipe] = etBitOr;
    binopMap[tAmp] = etBitAnd;
    binopMap[tMod] = etMod;
    binopMap[tModEq] = etSMod;

    valMap[tInt] = etInt;
    valMap[tDouble] = etDouble;
    valMap[tString] = etString;
    valMap[tRawString] = etString;
    valMap[tNil] = etNil;
    valMap[tTrue] = etTrue;
    valMap[tFalse] = etFalse;
    valMap[tRegExp] = etRegExp;

#define ADDRULE(n, rule, hnd) add##n(SeqInfo rule,&ZParser::hnd)

    /*
   ? - optional
   - not pushed
   . return on error
   ! priority reset
   ^ point of no return
   */

    ADDRULE(1, ("brk", "expr: tORBr- tEol? expr tEol? tCRBr-", 0), handleBr);
    ADDRULE(3, ("assign", "expr: expr tEq tEol? expr", 5, false), handleBinOp);
    ADDRULE(3, ("sadd", "expr: expr tPlusEq tEol? expr", 6, false), handleBinOp);
    ADDRULE(3, ("ssub", "expr: expr tMinusEq tEol? expr", 6, false), handleBinOp);
    ADDRULE(3, ("smul", "expr: expr tMulEq tEol? expr", 6, false), handleBinOp);
    ADDRULE(3, ("sdiv", "expr: expr tDivEq tEol? expr", 6, false), handleBinOp);
    ADDRULE(3, ("smod", "expr: expr tModEq tEol? expr", 6, false), handleBinOp);
    ADDRULE(3, ("ternary", "expr: expr tQuestMark- tEol? expr! tColon- tEol? expr!", 9), handleTernary);
    ADDRULE(3, ("or", "expr: expr tOr tEol? expr", 10, false), handleBinOp);
    ADDRULE(3, ("and", "expr: expr tAnd tEol? expr", 15, false), handleBinOp);
    ADDRULE(1, ("not", "expr: tNot- tEol? expr", 16), handleNot);
    ADDRULE(3, ("greater", "expr: expr tGreater tEol? expr", 20), handleBinOp);
    ADDRULE(3, ("less", "expr: expr tLess tEol? expr", 20), handleBinOp);
    ADDRULE(3, ("equal", "expr: expr tEqual tEol? expr", 20), handleBinOp);
    ADDRULE(3, ("notequal", "expr: expr tNotEqual tEol? expr", 20), handleBinOp);
    ADDRULE(3, ("match", "expr: expr tMatch tEol? expr", 20, false), handleBinOp);
    ADDRULE(3, ("lesseq", "expr: expr tLessEq tEol? expr", 20), handleBinOp);
    ADDRULE(3, ("greatereq", "expr: expr tGreaterEq tEol? expr", 20), handleBinOp);
    ADDRULE(3, ("in", "expr: expr tIn tEol? expr", 21), handleBinOp);
    ADDRULE(3, ("is", "expr: expr tIs tEol? expr", 21), handleBinOp);
    ADDRULE(3, ("bitor", "expr: expr tPipe tEol? expr", 22), handleBinOp);
    ADDRULE(3, ("bitand", "expr: expr tAmp tEol? expr", 22), handleBinOp);
    ADDRULE(4, ("expr range st", "expr: expr tDoubleDot tEol? expr rangeTail", 30), handleRangeStep);
    ADDRULE(4, ("expr range st", "expr: expr tDoubleDotLess tEol? expr rangeTail", 30), handleRangeStep);
    ADDRULE(3, ("add", "expr: expr tPlus tEol? expr", 50), handleBinOp);
    ADDRULE(3, ("minus", "expr: expr tMinus tEol? expr", 50), handleBinOp);
    ADDRULE(3, ("mul", "expr: expr tMul tEol? expr", 100), handleBinOp);
    ADDRULE(3, ("div", "expr: expr tDiv tEol? expr", 100), handleBinOp);
    ADDRULE(3, ("mod", "expr: expr tMod tEol? expr", 100), handleBinOp);
    ADDRULE(3, ("seqop", "expr: tCarret- expr tColon- expr seqopLst"), handleSeqOp);
    ADDRULE(1, ("array", "expr: tOSBr-^ tEol? exprList tEol? tCSBr-", 0), handleArray);
    ADDRULE(1, ("count", "expr: tHash- expr", 150), handleCount);
    ADDRULE(1, ("fmt expr", "expr: fmt", 150), handleAtom);
    ADDRULE(2, ("fmt", "fmt: tFormat expr.", 150), handleFormat);
    ADDRULE(2, ("fmt args", "fmt: tFormat tORBr-^ tEol? exprList tEol? tCRBr-", 150), handleFormatArgs);
    ADDRULE(1, ("pre inc", "expr: tPlusPlus- expr", 175), handlePreInc);
    ADDRULE(1, ("post inc", "expr: expr tPlusPlus-", 175), handlePostInc);
    ADDRULE(1, ("pre dec", "expr: tMinusMinus- expr", 175), handlePreDec);
    ADDRULE(1, ("post dec", "expr: expr tMinusMinus-", 175), handlePostDec);
    ADDRULE(2, ("array index", "expr: expr tOSBr- tEol? expr! tEol? tCSBr-", 180), handleArrayIndex);
    ADDRULE(2, ("map index", "expr: expr tOCBr- tEol? expr! tEol? tCCBr-", 180), handleMapIndex);
    ADDRULE(2, ("map name index", "expr: expr tThinArrow-^ tIdent", 180), handleNameMapIndex);
    ADDRULE(1, ("val int", "expr: tInt", 0), handleVal);
    ADDRULE(1, ("val dbl", "expr: tDouble", 0), handleVal);
    ADDRULE(1, ("val str", "expr: tString", 0), handleStr);
    ADDRULE(1, ("val rawstr", "expr: tRawString", 0), handleRawStr);
    ADDRULE(1, ("val nil", "expr: tNil", 0), handleVal);
    ADDRULE(1, ("val tru", "expr: tTrue", 0), handleVal);
    ADDRULE(1, ("val fal", "expr: tFalse", 0), handleVal);
    ADDRULE(1, ("val rx", "expr: tRegExp", 0), handleVal);
    ADDRULE(1, ("var", "expr: var", 400), handleVar);
    ADDRULE(1, ("liter", "expr: tLiteral", 400), handleLiteral);
    ADDRULE(1, ("numarg", "expr: tNumArg", 400), handleNumArg);
    ADDRULE(1, ("neg", "expr: tMinus- expr", 150), handleNeg);
    ADDRULE(1, ("copy", "expr: tMul- expr", 150), handleCopy);
    ADDRULE(1, ("ref", "expr: tAmp- expr", 150), handleRef);
    ADDRULE(1, ("wref", "expr: tDoubleAmp- expr", 150), handleWeakRef);
    ADDRULE(1, ("cor", "expr: tMod- expr", 150), handleCor);
    ADDRULE(2, ("expr get attr", "expr: expr tAttrGet- tEol? exprList tEol? tCCBr-", 180), handleGetAttr);
    ADDRULE(2, ("prop", "expr: expr tDot- tEol? tIdent", 200), handleProp);
    ADDRULE(2, ("propopt", "expr: expr tDotQMark- tEol? tIdent", 200), handlePropOpt);
    ADDRULE(2, ("expr call", "expr: expr tORBr-^ tEol? argList tEol? tCRBr-", 300), handleCall);
    ADDRULE(2, ("prop", "expr: expr tDotStar- tEol? expr", 400), handlePropExpr);
    ADDRULE(1, ("type", "expr: tQuestMark- expr", 500), handleGetType);
    ADDRULE(2, ("expr func", "expr: tFunc- tORBr-^ tEol? paramList tEol? tCRBr- stmtList tEnd-", 500, true),
            handleFuncExpr);
    ADDRULE(2, ("expr func!", "expr: tExclSign- tLess- paramList tGreater- expr", 0, true), handleShortFuncExpr);
    ADDRULE(1, ("expr func!noarg", "expr: tExclSign- expr", 0, true), handleShortFuncNoArgExpr);
    ADDRULE(1, ("expr map", "expr: tOCBr- tEol? pairList tEol? tCCBr-", 0), handleMap);
    ADDRULE(0, ("expr empty map", "expr: tOCBr- tArrow- tCCBr-"), handleEmptyMap);
    ADDRULE(1, ("expr set", "expr: tOCBr- tEol? exprList tEol? tCCBr-"), handleSet);
    ADDRULE(1, ("expr var", "var: tIdent", 1000, false), handleVarIdent);
    ADDRULE(2, ("expr ns", "var: tIdent tDoubleColon- var", 1000, false), handleNamespaceExpr);
    ADDRULE(1, ("expr ns global", "var: tDoubleColon- var", 1000, false), handleNamespaceExprGlobal);
//  ADDRULE(3,("expr range","expr: expr tDoubleDot expr",30),handleRange);
//  ADDRULE(3,("expr range","expr: expr tDotLess expr",30),handleRange);

    ADDRULE(0, ("range tail empty", "rangeTail:"), handleEmptyExpr);
    ADDRULE(1, ("range tail empty", "rangeTail: tAt- expr"), handleRangeTail);

    ADDRULE(1, ("seqopLst first", "seqopLst: seqop"), handleSeqOpLstFirst);
    ADDRULE(2, ("seqopLst first", "seqopLst: seqopLst seqop"), handleSeqOpLstNext);
    getRule("seqopLst").listMode = lmOneOrMore;

    ADDRULE(1, ("seqop map", "seqop: tMapOp- expr"), handleSeqOpMap);
    ADDRULE(1, ("seqop grp", "seqop: tGrepOp- expr"), handleSeqOpGrep);


    ADDRULE(0, ("pairList empty", "pairList:"), handleExprListEmpty);
    ADDRULE(1, ("pairList first", "pairList: pair"), handleExprListOne);
    ADDRULE(2, ("pairList next", "pairList: pairList tComma- tEol? pair"), handleExprListNext);
    ADDRULE(2, ("pairList next2", "pairList: pairList tEol- tEol? pair"), handleExprListNext);
    //ADDRULE(1,("pairList skip","pairList: pairList tEol-"),handleExprListSkip);
    //ADDRULE(2,("pairList next2","pairList: pairList pair"),handleExprListNext);
    getRule("pairList").listMode = lmZeroOrMoreTrail;

    ADDRULE(2, ("pair", "pair: expr tArrow- tEol? expr"), handlePair);

    ADDRULE(1, ("pairOrExpr expr", "pairOrExpr: expr"), handleAtom);
    ADDRULE(2, ("pairOrExpr pair", "pairOrExpr: expr tArrow- tEol? expr"), handlePair);

    ADDRULE(0, ("argList empty", "argList:"), handleExprListEmpty);
    ADDRULE(1, ("argList first", "argList: pairOrExpr"), handleExprListOne);
    //ADDRULE(1,("argList first pair","argList: pair"),handleExprListOne);
    //ADDRULE(2,("argList first pair","argList: expr tArrow- tEol? expr"),handleExprListOnePair);
    ADDRULE(2, ("argList next", "argList: argList tComma- tEol? pairOrExpr"), handleExprListNext);
    ADDRULE(2, ("argList next", "argList: argList tEol- tEol? pairOrExpr"), handleExprListNext);
    //ADDRULE(3,("argList next pair","argList: argList tComma- tEol? expr tArrow- tEol? expr"),handleExprListNextPair);
    //ADDRULE(3,("argList next pair","argList: argList tEol- tEol? expr tArrow- tEol? expr"),handleExprListNextPair);
    getRule("argList").listMode = lmZeroOrMoreTrail;

    ADDRULE(0, ("exprList empty", "exprList:"), handleExprListEmpty);
    ADDRULE(1, ("exprList first", "exprList: expr"), handleExprListOne);
    ADDRULE(2, ("exprList next", "exprList: exprList tComma- tEol? expr"), handleExprListNext);
    ADDRULE(2, ("exprList next", "exprList: exprList tEol- tEol? expr"), handleExprListNext);
    //ADDRULE(2,("exprList next","exprList: exprList expr"),handleExprListNext);
    //ADDRULE(1,("exprList skip","exprList: exprList tEol-"),handleExprListSkip);
    getRule("exprList").listMode = lmZeroOrMoreTrail;

    ADDRULE(1, ("func param simple", "funcParam: tIdent"), handleFuncParamSimple);
    ADDRULE(2, ("func param defVal", "funcParam: tIdent tEq- tEol? expr", 21), handleFuncParamDefVal);
    ADDRULE(1, ("func param arr", "funcParam: tIdent tOSBr- tCSBr-"), handleFuncParamArr);
    ADDRULE(1, ("func param arr", "funcParam: tIdent tOCBr- tCCBr-"), handleFuncParamMap);

    ADDRULE(0, ("param lst empty", "paramList:"), handleParamListEmpty);
    ADDRULE(1, ("param lst first", "paramList: funcParam"), handleParamListOne);
    ADDRULE(2, ("param lst next", "paramList: paramList tComma- tEol? funcParam"), handleParamListNext);
    ADDRULE(2, ("param lst next", "paramList: paramList tEol- tEol? funcParam"), handleParamListNext);
    getRule("paramList").listMode = lmZeroOrMoreTrail;

    ADDRULE(1, ("nameList1 first", "nameList1: tIdent"), handleNameListOne);
    ADDRULE(2, ("nameList1 next", "nameList1: nameList1 tComma- tEol? tIdent"), handleNameList);
    getRule("nameList1").listMode = lmOneOrMore;

    ADDRULE(2, ("stmt enum", "stmt: tEnum- tIdent tEol- exprList tEnd- tEol-"), handleEnum);
    ADDRULE(2, ("stmt bitenum", "stmt: tBitEnum- tIdent tEol- exprList tEnd- tEol-"), handleBitEnum);

    ADDRULE(1, ("stmt use", "stmt: tUse- var tEol-"), handleUse);

    ADDRULE(0, ("stmtList empty", "stmtList:"), handleStmtListEmpty);
    ADDRULE(1, ("stmtList first", "stmtList: stmt"), handleStmtListOne);
    ADDRULE(2, ("stmtList next", "stmtList: stmtList stmt"), handleStmtList);
    ADDRULE(2, ("include", "stmtList: stmtList tInclude- expr tEol-"), handleInclude);
    ADDRULE(1, ("include", "stmtList: tInclude- expr tEol-"), handleInclude1);
    getRule("stmtList").listMode = lmZeroOrMoreTrail;

    ADDRULE(2, ("exprList2 pair", "exprList2: expr tComma- expr", 30), handleExprListTwo);
    ADDRULE(2, ("exprList2 next", "exprList2: exprList2 tComma- expr", 30), handleExprListNext);
    //ADDRULE(2,("exprList2 next","exprList2: exprList2 tEol- tEol? expr",30),handleExprListNext);
    getRule("exprList2").listMode = lmOneOrMore;

    ADDRULE(0, ("eslif list empty", "elsiflist:"), handleElsifEmpty);
    ADDRULE(2, ("eslif list empty", "elsiflist: tElsif- expr tEol- stmtList"), handleElsifFirst);
    ADDRULE(3, ("eslif list first", "elsiflist: elsiflist tElsif- expr tEol- stmtList"), handleElsifNext);
    getRule("elsiflist").listMode = lmZeroOrMore;

    ADDRULE(0, ("else empty", "else: "), handleElseEmpty);
    ADDRULE(1, ("else empty", "else: tElse- stmtList"), handleElse);


    ADDRULE(1, ("simple stmt", "stmt: simpleStmt tEol-"), handleSimpleStmt);

    ADDRULE(1, ("stmt expr", "simpleStmt: expr"), handleExprStmt);
    ADDRULE(0, ("stmt return nil", "simpleStmt: tReturn-"), handleReturnNil);
    ADDRULE(1, ("stmt return", "simpleStmt: tReturn- expr"), handleReturn);
    ADDRULE(1, ("stmt returnif", "simpleStmt: tReturnIf- expr"), handleReturnIf);
    ADDRULE(2, ("stmt lstassign", "simpleStmt: exprList2 tEq- expr"), handleListAssign1);
    ADDRULE(2, ("stmt lstassign", "simpleStmt: exprList2 tEq- exprList2"), handleListAssign);

    ADDRULE(0, ("opt name empty", "optname:"), handleOptNameEmpty);
    ADDRULE(1, ("opt name", "optname: tColon- tIdent"), handleOptName);

    ADDRULE(4, ("stmt for", "stmt: tFor- optname nameList1 tIn- expr tEol- stmtList tEnd-"), handleFor);
    ADDRULE(4, ("stmt short for", "stmt: simpleStmt tFor- optname nameList1 tIn- expr tEol-"), handleShortFor);

    ADDRULE(1, ("stmt break nil", "simpleStmt: tBreak"), handleBreakNil);
    ADDRULE(2, ("stmt break id", "simpleStmt: tBreak tIdent"), handleBreakId);

    ADDRULE(1, ("stmt next nil", "simpleStmt: tNext"), handleNextNil);
    ADDRULE(2, ("stmt next id", "simpleStmt: tNext tIdent"), handleNextId);

    ADDRULE(1, ("stmt redo nil", "simpleStmt: tRedo"), handleRedoNil);
    ADDRULE(2, ("stmt redo id", "simpleStmt: tRedo tIdent"), handleRedoId);

    ADDRULE(3, ("stmt while", "stmt: tWhile- optname expr tEol- stmtList tEnd-"), handleWhile);
    ADDRULE(3, ("stmt short while", "stmt: simpleStmt tWhile- optname expr tEol-"), handleShortWhile);
    ADDRULE(4, ("stmt if", "stmt: tIf- expr tEol- stmtList elsiflist else tEnd- tEol-"), handleIf);
    ADDRULE(2, ("stmt shortif", "stmt: simpleStmt tIf- expr tEol-"), handleShortIf);

    ADDRULE(4, ("stmt func", "func: tFunc- tIdent tORBr-^ tEol? paramList tEol? tCRBr- tEol- stmtList tEnd tEol-"),
            handleFuncDecl);
    ADDRULE(1, ("stmt func", "stmt: func"), handleStmt);

    ADDRULE(3, ("stmt macro", "stmt: tMacro- tIdent tORBr-^ tEol? paramList tEol? tCRBr- tEol- stmtList tEnd- tEol-"),
            handleMacroDecl);

    ADDRULE(1, ("stmt attr", "stmt: tAttr- tIdent tEol-"), handleAttrDefSimple);
    ADDRULE(2, ("stmt attr", "stmt: tAttr- tIdent tEq- tEol? expr tEol-"), handleAttrDefValue);

    ADDRULE(2, ("liter arg num", "literArg: tPlus- tIdent tColon- tIdent"), handleLiterArgNum);
    ADDRULE(2, ("liter arg num opt", "literArg: tPlus- tIdent tColon- tIdent tQuestMark-"), handleLiterArgNumOpt);
    ADDRULE(2, ("liter arg int", "literArg: tMod- tIdent tColon- tIdent"), handleLiterArgInt);
    ADDRULE(2, ("liter arg int opt", "literArg: tMod- tIdent tColon- tIdent tQuestMark-"), handleLiterArgIntOpt);
    ADDRULE(2, ("liter arg int", "literArg: tDiv- tIdent tColon- tIdent"), handleLiterArgDbl);
    ADDRULE(2, ("liter arg int opt", "literArg: tDiv- tIdent tColon- tIdent tQuestMark-"), handleLiterArgDblOpt);

    ADDRULE(2, ("liter arg list first str", "literArgList: tIdent tFormat- tIdent"), handleLiterArgListStr);
    ADDRULE(1, ("liter arg list", "literArgList: literArgList2"), handleLiterArgList);
    ADDRULE(2, ("liter arg list last int", "literArgList: literArgList2 tMod- tIdent"), handleLiterArgListLastInt);
    ADDRULE(2, ("liter arg list last dbl", "literArgList: literArgList2 tDiv- tIdent"), handleLiterArgListLastDbl);
    ADDRULE(2, ("liter arg list last dbl", "literArgList: literArgList2 tPlus- tIdent"), handleLiterArgListLastNum);
    ADDRULE(2, ("liter arg list last int", "literArgList: tIdent tFormat- tIdent"), handleLiterArgListStr);
    ADDRULE(1, ("liter arg list first str", "literArgList2: literArg"), handleLiterArgListFirst);
    ADDRULE(2, ("liter arg list first str", "literArgList2: literArgList2 literArg"), handleLiterArgListNext);
    getRule("literArgList2").listMode = lmOneOrMoreTrail;
    ADDRULE(3, ("stmt liter", "stmt: tLiter- tIdent literArgList tEol- stmtList tEnd- tEol-"), handleLiterDecl);

    ADDRULE(0, ("casesList empty", "casesList:"), handleCasesListEmpty);
    ADDRULE(1, ("casesList empty", "casesList: tEol- casesList"), handleCasesListEmptyLine);
    ADDRULE(2, ("casesList first", "casesList: tStarColon^ stmtList"), handleCasesListFirstWild);
    ADDRULE(2, ("casesList first", "casesList: exprList tColon-^ stmtList"), handleCasesListFirst);
    ADDRULE(3, ("casesList next", "casesList: casesList exprList tColon-^ stmtList"), handleCasesListNext);
    ADDRULE(3, ("casesList next", "casesList: casesList tStarColon^ stmtList"), handleCasesListNextWild);
    getRule("casesList").listMode = lmZeroOrMoreTrail;

    ADDRULE(0, ("opt expr empty", "optexpr:"), handleEmptyExpr);
    ADDRULE(1, ("opt expr", "optexpr: expr"), handleAtom);
    ADDRULE(3, ("stmt switch", "stmt: tSwitch optexpr tEol- casesList tEnd- tEol-"), handleSwitch);

    ADDRULE(5, ("stmt class", "stmt: tClass-^ tIdent optargs parent tEol- classStmtList tEnd tEol-"), handleClass);
    ADDRULE(0, ("class parent", "parent:"), handleNoParent);
    ADDRULE(1, ("class parent", "parent: tColon- expr"), handleParent);

    ADDRULE(0, ("classStmtList empty", "classStmtList:"), handleStmtListEmpty);
    ADDRULE(1, ("classStmtList first", "classStmtList: classStmt"), handleStmtListOne);
    ADDRULE(2, ("class stmtList next", "classStmtList: classStmtList classStmt"), handleStmtList);
    getRule("classStmtList").listMode = lmZeroOrMoreTrail;

    ADDRULE(0, ("calssstmt empty", "classStmt: tEol-"), handleEmptyLine);
    ADDRULE(1, ("classStmt func", "classStmt: func^"), handleStmt);
    ADDRULE(0, ("optional argList", "optargs:"), handleOptArgNone);
    ADDRULE(1, ("optional argList", "optargs: tORBr- paramList tCRBr-"), handleOptArg);
    ADDRULE(3, ("classStmt func", "classStmt: tOn-^ tIdent optargs tEol- stmtList tEnd- tEol-"), handleOn);
    ADDRULE(1, ("class attrlist", "classStmt: tAt- tOCBr-^ tEol? attrList tEol? tCCBr- tEol-"), handleClassAttr);
    //ADDRULE(2,("classStmt lstassign","classStmt: exprList2 tEq- exprList2 tEol-"),handleListAssign);
    ADDRULE(3, ("classStmt assign", "classStmt: tIdent attrDecl tEq- expr tEol-"), handleMemberInit);
    ADDRULE(2, ("classStmt def", "classStmt: tIdent attrDecl tEol-"), handleMemberDef);
    //ADDRULE(1,("classStmt member","classStmt: nameList1 tEol-"),handleAttrList);

    ADDRULE(1, ("attr simple", "attr: var"), handleAttrSimple);
    ADDRULE(2, ("attr assign", "attr: var tEq- expr"), handleAttrAssign);

    ADDRULE(0, ("attr empty", "attrDecl: "), handleAttrDeclEmpty);
    ADDRULE(1, ("attr list", "attrDecl: tOCBr- attrList tCCBr-"), handleAttrDecl);

    ADDRULE(0, ("attr list empty", "attrList: "), handleAttrListEmpty);
    ADDRULE(1, ("attr list first", "attrList: attr"), handleAttrListFirst);
    ADDRULE(2, ("attr list next", "attrList: attrList tComma- tEol? attr"), handleAttrListNext);
    ADDRULE(2, ("attr list next", "attrList: attrList tEol- tEol? attr"), handleAttrListNext);
    getRule("attrList").listMode = lmZeroOrMoreTrail;

    ADDRULE(0, ("propAccDecl empty", "propAccDecl: tEol-"), handleEmptyPropAcc);
    ADDRULE(2, ("propAccDecl name", "propAccDecl: tIdent tEq- tIdent tEol-"), handlePropAccName);
    ADDRULE(2, ("propAccDecl func", "propAccDecl: tIdent tEol- stmtList tEnd-"), handlePropAccNoArg);
    ADDRULE(3, ("propAccDecl func", "propAccDecl: tIdent tORBr- tIdent tCRBr- tEol- stmtList tEnd-"), handlePropAccArg);

    ADDRULE(0, ("propAccDeclList empty", "propAccDeclList:"), handlePropAccListEmpty);
    ADDRULE(1, ("propAccDeclList first", "propAccDeclList: propAccDecl"), handlePropAccListFirst);
    ADDRULE(2, ("propAccDeclList next", "propAccDeclList: propAccDeclList^ propAccDecl"), handlePropAccListNext);
    getRule("propAccDeclList").listMode = lmZeroOrMore;

    ADDRULE(2, ("classStmt prop", "classStmt: tProp- tIdent tEol-^ propAccDeclList tEnd- tEol-"), handlePropDecl);

    ADDRULE(0, ("namespace empty", "nsdecl:"), handleNsEmpty);
    ADDRULE(1, ("namespace first", "nsdecl: tIdent"), handleNsFirst);
    ADDRULE(2, ("namespace next", "nsdecl: nsdecl tDoubleColon- tIdent"), handleNsNext);
    getRule("nsdecl").listMode = lmZeroOrMore;

    ADDRULE(2, ("namespace stmt", "stmt: tNamespace- nsdecl tEol- stmtList tEnd- tEol-"), handleNamespace);

    ADDRULE(0, ("nm list empty", "nmlist:"), handleExprListEmpty);
    ADDRULE(1, ("nm list one", "nmlist: nsdecl"), handleNmListOne);
    ADDRULE(2, ("nm list next", "nmlist: nmlist tComma- tEol? nsdecl"), handleNmListNext);
    getRule("nmlist").listMode = lmZeroOrMore;

    ADDRULE(4, ("try catch stmt", "stmt: tTry- tEol- stmtList tCatch- nmlist tIn- tIdent tEol- stmtList tEnd- tEol-"),
            handleTryCatch);

    ADDRULE(1, ("throw stmt", "simpleStmt: tThrow- expr"), handleThrow);
    ADDRULE(1, ("yield nil stmt", "simpleStmt: tYield"), handleYieldNil);
    ADDRULE(1, ("yield stmt", "simpleStmt: tYield- expr"), handleYield);


    ADDRULE(0, ("stmt empty", "stmt: tEol-"), handleEmptyLine);
    ADDRULE(0, ("class stmt empty", "classStmt: tEol-"), handleEmptyLine);

    ADDRULE(1, ("goal", "goal: stmtList tEof-"), handleGoal);

/*
  add1(SeqInfo("brk","expr: tORBr- expr tCRBr-",0),&ZParser::handleBr);
  add3(SeqInfo("assign","expr: expr tEq expr",5,false),&ZParser::handleBinOp);
  add3(SeqInfo("sadd","expr: expr tPlusEq expr",6),&ZParser::handleBinOp);
  add3(SeqInfo("or","expr: expr tOr expr",10),&ZParser::handleBinOp);
  add3(SeqInfo("and","expr: expr tAnd expr",15),&ZParser::handleBinOp);
  add3(SeqInfo("greater","expr: expr tGreater expr",20),&ZParser::handleBinOp);
  add3(SeqInfo("less","expr: expr tLess expr",20),&ZParser::handleBinOp);
  add3(SeqInfo("equal","expr: expr tEqual expr",20),&ZParser::handleBinOp);
  add3(SeqInfo("in","expr: expr tIn expr",21),&ZParser::handleBinOp);
  add4(SeqInfo("expr range st","expr: expr tDoubleDot expr rangeTail",30),&ZParser::handleRangeStep);
  add4(SeqInfo("expr range st","expr: expr tDoubleDotLess expr rangeTail",30),&ZParser::handleRangeStep);
  add3(SeqInfo("add","expr: expr tPlus expr",50),&ZParser::handleBinOp);
  add3(SeqInfo("minus","expr: expr tMinus expr",50),&ZParser::handleBinOp);
  add3(SeqInfo("mul","expr: expr tMul expr",100),&ZParser::handleBinOp);
  add3(SeqInfo("div","expr: expr tDiv expr",100),&ZParser::handleBinOp);
  add3(SeqInfo("array","expr: tOSBr exprList tCSBr"),&ZParser::handleArray);
  add1(SeqInfo("pre inc","expr: tPlusPlus- expr",175),&ZParser::handlePreInc);
  add1(SeqInfo("post inc","expr: expr tPlusPlus-",175),&ZParser::handlePostInc);
  add2(SeqInfo("array index","expr: expr tOSBr- expr! tCSBr-",180),&ZParser::handleArrayIndex);
  add2(SeqInfo("map index","expr: expr tOCBr- expr! tCCBr-",180),&ZParser::handleMapIndex);
  add1(SeqInfo("val","expr: tInt",400),&ZParser::handleVal);
  add1(SeqInfo("val","expr: tDouble",400),&ZParser::handleVal);
  add1(SeqInfo("val","expr: tString",400),&ZParser::handleStr);
  add1(SeqInfo("val","expr: tNil",400),&ZParser::handleVal);
  add1(SeqInfo("val","expr: tTrue",400),&ZParser::handleVal);
  add1(SeqInfo("val","expr: tFalse",400),&ZParser::handleVal);
  add1(SeqInfo("var","expr: tIdent",400),&ZParser::handleVar);
  add1(SeqInfo("neg","expr: tMinus- expr",150),&ZParser::handleNeg);
  add1(SeqInfo("ref","expr: tAmp- expr",150),&ZParser::handleRef);
  add2(SeqInfo("prop","expr: expr tDot- tIdent",200),&ZParser::handleProp);
  add2(SeqInfo("expr call","expr: expr tORBr- argList tCRBr-",300),&ZParser::handleCall);
  add2(SeqInfo("expr func","expr: tFunc- tORBr- nameList tCRBr- stmtList tEnd-",500,true),&ZParser::handleFuncExpr);
  add1(SeqInfo("expr map","expr: tOCBr- pairlist tCCBr-"),&ZParser::handleMap);
  add0(SeqInfo("expr empty map","expr: tOCBr- tArrow- tCCBr-"),&ZParser::handleEmptyMap);
  add1(SeqInfo("expr set","expr: tOCBr- exprList tCCBr-"),&ZParser::handleSet);
  add2(SeqInfo("expr ns","expr: tIdent tDoubleColon- expr",1000,false),&ZParser::handleNamespaceExpr);
//  add3(SeqInfo("expr range","expr: expr tDoubleDot expr",30),&ZParser::handleRange);
//  add3(SeqInfo("expr range","expr: expr tDotLess expr",30),&ZParser::handleRange);

  add0(SeqInfo("range tail empty","rangeTail:"),&ZParser::handleRangeTailEmpty);
  add1(SeqInfo("range tail empty","rangeTail: tColon- expr"),&ZParser::handleRangeTail);

  add0(SeqInfo("pairlist empty","pairlist:"),&ZParser::handleExprListEmpty);
  add1(SeqInfo("pairlist first","pairlist: pair"),&ZParser::handleExprListOne);
  add2(SeqInfo("pairlist next","pairlist: pairlist tComma- pair"),&ZParser::handleExprListNext);
  //add2(SeqInfo("pairlist next2","pairlist: pairlist tEol- pair"),&ZParser::handlePairListNext);
  add1(SeqInfo("pairlist skip","pairlist: pairlist tEol-"),&ZParser::handleExprListSkip);
  add2(SeqInfo("pairlist next2","pairlist: pairlist pair"),&ZParser::handleExprListNext);
  getRule("pairlist").listMode=lmZeroOrMoreTrail;

  add2(SeqInfo("pair","pair: expr tArrow- expr"),&ZParser::handlePair);

  add0(SeqInfo("argList empty","argList:"),&ZParser::handleExprListEmpty);
  add1(SeqInfo("argList first","argList: expr"),&ZParser::handleExprListOne);
  add2(SeqInfo("argList next","argList: argList tComma- expr"),&ZParser::handleExprListNext);
  getRule("argList").listMode=lmZeroOrMore;

  add0(SeqInfo("exprList empty","exprList:"),&ZParser::handleExprListEmpty);
  add1(SeqInfo("exprList first","exprList: expr"),&ZParser::handleExprListOne);
  add2(SeqInfo("exprList next","exprList: exprList tComma- expr"),&ZParser::handleExprListNext);
  add2(SeqInfo("exprList next","exprList: exprList expr"),&ZParser::handleExprListNext);
  add1(SeqInfo("exprList skip","exprList: exprList tEol-"),&ZParser::handleExprListSkip);
  getRule("exprList").listMode=lmZeroOrMoreTrail;

  add0(SeqInfo("name empty","nameList:"),&ZParser::handleNameListEmpty);
  add1(SeqInfo("name first","nameList: tIdent"),&ZParser::handleNameListOne);
  add2(SeqInfo("name next","nameList: nameList tComma- tIdent"),&ZParser::handleNameList);
  getRule("nameList").listMode=lmZeroOrMore;

  add1(SeqInfo("nameList1 first","nameList1: tIdent"),&ZParser::handleNameListOne);
  add2(SeqInfo("nameList1 next","nameList1: nameList1 tComma- tIdent"),&ZParser::handleNameList);
  getRule("nameList1").listMode=lmOneOrMore;


  add0(SeqInfo("stmtList empty","stmtList:"),&ZParser::handleStmtListEmpty);
  add1(SeqInfo("stmtList first","stmtList: stmt"),&ZParser::handleStmtListOne);
  add2(SeqInfo("stmtList next","stmtList: stmtList stmt"),&ZParser::handleStmtList);
  getRule("stmtList").listMode=lmZeroOrMore;

  add2(SeqInfo("exprList2 pair","exprList2: expr tComma- expr",30),&ZParser::handleExprListTwo);
  add2(SeqInfo("exprList2 next","exprList2: exprList2 tComma- expr",30),&ZParser::handleExprListNext);
  getRule("exprList2").listMode=lmOneOrMore;

  add0(SeqInfo("eslif list empty","elsiflist:"),&ZParser::handleElsifEmpty);
  add2(SeqInfo("eslif list empty","elsiflist: tElsif- expr tEol- stmtList"),&ZParser::handleElsifFirst);
  add3(SeqInfo("eslif list first","elsiflist: elsiflist tElsif- expr tEol- stmtList"),&ZParser::handleElsifNext);
  getRule("elsiflist").listMode=lmZeroOrMore;

  add3(SeqInfo("stmt func","func: tFunc- tIdent tORBr- nameList tCRBr- tEol- stmtList tEnd- tEol-"),&ZParser::handleFuncDecl);
  add1(SeqInfo("stmt func","stmt: func"),&ZParser::handleStmt);

  add0(SeqInfo("else empty","else: "),&ZParser::handleElseEmpty);
  add1(SeqInfo("else empty","else: tElse- stmtList"),&ZParser::handleElse);

  add4(SeqInfo("stmt if","stmt: tIf- expr tEol- stmtList elsiflist else tEnd- tEol-"),&ZParser::handleIf);

  add1(SeqInfo("simple stmt","stmt: simpleStmt"),&ZParser::handleSimpleStmt);

  add1(SeqInfo("stmt return","simpleStmt: tReturn- expr tEol-"),&ZParser::handleReturn);
  add0(SeqInfo("stmt return nil","simpleStmt: tReturn- tEol-"),&ZParser::handleReturnNil);

  add3(SeqInfo("stmt for","stmt: tFor- nameList1 tIn- expr tEol- stmtList tEnd-"),&ZParser::handleFor);
  add3(SeqInfo("stmt short for","stmt: simpleStmt tFor- nameList1 tIn- expr tEol-"),&ZParser::handleShortFor);

  add2(SeqInfo("stmt shortif","stmt: simpleStmt tIf- expr tEol-"),&ZParser::handleShortIf);
  add1(SeqInfo("stmt expr","simpleStmt: expr tEol-"),&ZParser::handleExprStmt);
  add2(SeqInfo("stmt while","stmt: tWhile- expr tEol- stmtList tEnd-"),&ZParser::handleWhile);
  add2(SeqInfo("stmt lstassign","simpleStmt: exprList2 tEq- exprList2 tEol-"),&ZParser::handleListAssign);
  add2(SeqInfo("stmt lstassign","simpleStmt: exprList2 tEq- expr tEol-"),&ZParser::handleListAssign1);

  add4(SeqInfo("stmt class","stmt: tClass- tIdent optargs parent tEol- classStmtList tEnd- tEol-"),&ZParser::handleClass);
  add0(SeqInfo("class parent","parent:"),&ZParser::handleNoParent);
  add2(SeqInfo("class parent","parent: tColon- nsdecl tORBr- argList tCRBr-"),&ZParser::handleParent);

  add0(SeqInfo("classStmtList empty","classStmtList:"),&ZParser::handleStmtListEmpty);
  add1(SeqInfo("classStmtList first","classStmtList: classStmt"),&ZParser::handleStmtListOne);
  add2(SeqInfo("class stmtList next","classStmtList: classStmtList classStmt"),&ZParser::handleStmtList);
  getRule("classStmtList").listMode=lmZeroOrMore;

  add0(SeqInfo("calssstmt empty","classStmt: tEol-"),&ZParser::handleEmptyLine);
  add1(SeqInfo("classStmt func","classStmt: func"),&ZParser::handleStmt);
  add0(SeqInfo("optional argList","optargs:"),&ZParser::handleOptArgNone);
  add1(SeqInfo("optional argList","optargs: tORBr- nameList tCRBr-"),&ZParser::handleOptArg);
  add3(SeqInfo("classStmt func","classStmt: tOn- tIdent optargs tEol- stmtList tEnd- tEol-"),&ZParser::handleOn);
  add2(SeqInfo("classStmt lstassign","classStmt: exprList2 tEq- exprList2 tEol-"),&ZParser::handleListAssign);
  add2(SeqInfo("classStmt assign","classStmt: tIdent tEq- expr tEol-"),&ZParser::handleMemberInit);
  add1(SeqInfo("classStmt attr","classStmt: nameList1 tEol-"),&ZParser::handleAttrList);

  add0(SeqInfo("namespace empty","nsdecl:"),&ZParser::handleNsEmpty);
  add1(SeqInfo("namespace first","nsdecl: tIdent"),&ZParser::handleNsFirst);
  add2(SeqInfo("namespace next","nsdecl: nsdecl tDoubleColon- tIdent"),&ZParser::handleNsNext);
  getRule("nsdecl").listMode=lmZeroOrMore;

  add2(SeqInfo("namespace stmt","stmt: tNamespace- nsdecl tEol- stmtList tEnd- tEol-"),&ZParser::handleNamespace);

  add0(SeqInfo("nm list empty","nmlist:"),&ZParser::handleExprListEmpty);
  add1(SeqInfo("nm list one","nmlist: nsdecl"),&ZParser::handleNmListOne);
  add2(SeqInfo("nm list next","nmlist: nmlist tComma- nsdecl"),&ZParser::handleNmListNext);
  getRule("nmlist").listMode=lmZeroOrMore;

  add4(SeqInfo("try catch stmt","stmt: tTry- tEol- stmtList tCatch- nmlist tIn- tIdent tEol- stmtList tEnd- tEol-"),&ZParser::handleTryCatch);

  add1(SeqInfo("throw stmt","stmt: tThrow- expr tEol-"),&ZParser::handleThrow);


  add0(SeqInfo("stmt empty","stmt: tEol-"),&ZParser::handleEmptyLine);
  add0(SeqInfo("class stmt empty","classStmt: tEol-"),&ZParser::handleEmptyLine);

  add1(SeqInfo("goal","goal: stmtList tEof-"),&ZParser::handleGoal);
*/


    prepare("goal");

}


Name ZParser::getValue(const Term &t)
{
    /*ZString* rv=mem->allocZString();
  int len;
  const char* str=t.getValue(len);
  rv->assign(mem,str,len);
  return Name(ZStringRef(mem,rv),t.pos);*/
    unsigned int len = 0;
    const char* str = t.getValue(len);
    return Name(mem->mkZString(str, len), t.pos);
}


Expr* ZParser::handlePreInc(Expr* expr)
{
    return new Expr(etPreInc, expr);
}

Expr* ZParser::handlePostInc(Expr* expr)
{
    return new Expr(etPostInc, expr);
}

Expr* ZParser::handlePreDec(Expr* expr)
{
    return new Expr(etPreDec, expr);
}

Expr* ZParser::handlePostDec(Expr* expr)
{
    return new Expr(etPostDec, expr);
}

Statement* ZParser::handleEmptyLine()
{
    return 0;
}

Statement* ZParser::handleIf(Expr* cond, StmtList* thenList, StmtList* elsifList, StmtList* elseList)
{
    return fillPos(new IfStatement(cond, thenList, elsifList, elseList));
}


Statement* ZParser::handleReturn(Expr* expr)
{
    Statement* rv = fillPos(new ReturnStatement(expr));
    if(expr)
    {
        rv->end = expr->end;
    }
    return rv;
}

Statement* ZParser::handleReturnIf(Expr* expr)
{
    Statement* rv = fillPos(new ReturnIfStatement(expr));
    if(expr)
    {
        rv->end = expr->end;
    }
    return rv;
}

Statement* ZParser::handleReturnNil()
{
    return fillPos(new ReturnStatement(0));
}

Statement* ZParser::handleFuncDecl(Term name, FuncParamList* args, StmtList* stmts, Term /*end*/)
{
    return fillPos(new FuncDeclStatement(getValue(name), args, stmts));
}

Statement* ZParser::handleMacroDecl(Term name, FuncParamList* args, StmtList* stmts)
{
    l.macroExpander->registerMacro(name, args, stmts);
    return nullptr;
}

StmtList* ZParser::handleGoal(StmtList* st)
{
    result = st;
    return result;
}

StmtList* ZParser::handleStmtListEmpty()
{
    return nullptr;
}

Statement* ZParser::handleListAssign(ExprList* e1, ExprList* e2)
{
    return fillPos(new ListAssignStatement(e1, e2));
}

Statement* ZParser::handleListAssign1(ExprList* e1, Expr* e2)
{
    ExprList* lst = new ExprList;
    lst->values.emplace_back(e2);
    return fillPos(new ListAssignStatement(e1, lst));
}


StmtList* ZParser::handleStmtListOne(Statement* stmt)
{
    if(!stmt)
    {
        return 0;
    }
    StmtList* rv = new StmtList;
    if(stmt->st == stNone)
    {
        delete stmt;
    }
    else
    {
        rv->values.emplace_back(stmt);
    }
    return rv;
}

StmtList* ZParser::handleStmtList(StmtList* lst, Statement* stmt)
{
    if(!stmt)
    {
        return lst;
    }
    if(!lst)
    {
        lst = new StmtList;
    }
    lst->values.emplace_back(stmt);
    return lst;
}


Expr* ZParser::handleFuncExpr(FuncParamList* lst, StmtList* stmt)
{
    Expr* rv = new Expr((FuncDeclStatement*) fillPos(new FuncDeclStatement(Name(), lst, stmt)));
    //rv->pos=callStack.last->callLoc;//lst?lst->front()->pos:stmt?stmt->front()->pos:lastOkMatch.pos;
    return rv;
}

Expr* ZParser::handleShortFuncExpr(FuncParamList* lst, Expr* expr)
{
    Expr* rv = new Expr((FuncDeclStatement*) fillPos(new FuncDeclStatement(Name(), lst, 0)));
    rv->exprFunc = true;
    rv->func->body = new StmtList;
    Statement* st = new ReturnStatement(expr);
    st->pos = expr->pos;
    st->end = expr->end;
    rv->func->body->values.emplace_back(st);
    return rv;
}

Expr* ZParser::handleShortFuncNoArgExpr(Expr* expr)
{
    Expr* rv = new Expr((FuncDeclStatement*) fillPos(new FuncDeclStatement(Name(), 0, 0)));
    rv->pos = expr->pos;
    rv->end = expr->end;
    rv->exprFunc = true;
    rv->func->body = new StmtList;
    Statement* st = new ReturnStatement(expr);
    st->pos = expr->pos;
    st->end = expr->end;
    rv->func->body->values.emplace_back(st);
    return rv;
}


Expr* ZParser::handleArray(ExprList* lst)
{
    Expr* rv = new Expr(etArray);
    rv->pos = callStack.last->start;
    rv->lst = lst;
    rv->end = callStack.last->end;
    return rv;
}

Expr* ZParser::handleArrayIndex(Expr* arr, Expr* idx)
{
    Expr* rv = new Expr(etIndex, arr, idx);
    return rv;
}

NameList* ZParser::handleNameListEmpty()
{
    return nullptr;
}

NameList* ZParser::handleNameListOne(Term nm)
{
    NameList* rv = new NameList();
    rv->values.push_back(getValue(nm));
    return rv;
}

NameList* ZParser::handleNameList(NameList* lst, Term nm)
{
    lst->values.push_back(getValue(nm));
    return lst;
}

ExprList* ZParser::handleExprListEmpty()
{
    return 0;
}

ExprList* ZParser::handleExprListOne(Expr* ex)
{
    ExprList* rv = new ExprList;
    rv->pos = ex->pos;
    rv->values.emplace_back(ex);
    return rv;
}

ExprList* ZParser::handleExprListOnePair(Expr* ex1, Expr* ex2)
{
    ExprList* rv = new ExprList;
    rv->pos = ex1->pos;
    if(ex1->et == etVar && !ex1->inBrackets && !ex1->ns)
    {
        ex1->et = etString;
    }
    rv->values.emplace_back(new Expr(etPair, ex1, ex2));
    return rv;
}

ExprList* ZParser::handleExprListTwo(Expr* ex1, Expr* ex2)
{
    ExprList* rv = new ExprList;
    rv->pos = ex1->pos;
    rv->values.emplace_back(ex1);
    rv->values.emplace_back(ex2);
    return rv;
}

ExprList* ZParser::handleExprListNext(ExprList* lst, Expr* e2)
{
    lst->values.emplace_back(e2);
    return lst;
}

ExprList* ZParser::handleExprListNextPair(ExprList* lst, Expr* e1, Expr* e2)
{
    if(e1->et == etVar && !e1->inBrackets && !e1->ns)
    {
        e1->et = etString;
    }
    lst->values.emplace_back(new Expr(etPair, e1, e2));
    return lst;
}

ExprList* ZParser::handleExprListSkip(ExprList* lst)
{
    return lst;
}


Expr* ZParser::handleCall(Expr* var, ExprList* argList)
{
    Expr* rv = new Expr(etCall, var);
    rv->lst = argList;
    rv->end = callStack.last->end;
    return rv;
}

Expr* ZParser::handleProp(Expr* var, Term ident)
{
    Expr* prop = new Expr(etString, getValue(ident));
    return new Expr(etProp, var, prop);
}

Expr* ZParser::handlePropOpt(Expr* var, Term ident)
{
    Expr* prop = new Expr(etString, getValue(ident));
    return new Expr(etPropOpt, var, prop);
}


Expr* ZParser::handlePropExpr(Expr* var, Expr* val)
{
    return new Expr(etProp, var, val);
}

Expr* ZParser::handleNot(Expr* expr)
{
    return new Expr(etNot, expr);
}


Expr* ZParser::handleNeg(Expr* expr)
{
    return new Expr(etNeg, expr);
}

Expr* ZParser::handleRef(Expr* expr)
{
    return new Expr(etRef, expr);
}

Expr* ZParser::handleWeakRef(Expr* expr)
{
    return new Expr(etWeakRef, expr);
}


Expr* ZParser::handleCor(Expr* expr)
{
    return new Expr(etCor, expr);
}

Expr* ZParser::handleBr(Expr* expr)
{
    expr->inBrackets = true;
    return expr;
}

Expr* ZParser::handleAtom(Expr* ex)
{
    return ex;
}

Expr* ZParser::handleBinOp(Expr* a, Term t, Expr* b)
{
    if(binopMap[t.tt] == (ExprType) -1)
    {
        std::string msg = "unexpected bin op %s";
        msg += ZLexer::getTermName(t);
        throw std::runtime_error(msg);
    }
    else
    {
        return new Expr(binopMap[t.tt], a, b);
    }
//  return 0;
}

Expr* ZParser::handleVal(Term t)
{
    Expr* rv = new Expr(valMap[t.tt], getValue(t));
    rv->pos = t.pos;
    rv->end = t.pos;
    rv->end.col += rv->val->getLength();
    return rv;
}

Expr* ZParser::handleVar(Expr* var)
{
    //Expr* rv=new Expr(etVar,getValue(var));
    //rv->pos=var.pos;
    return var;
}

Expr* ZParser::handleStr(Term t)
{
    unsigned len;
    const char* cstr = t.getValue(len);
    const char* begin = cstr + 1;
    len -= 2;
    const char* ptr = begin;
    const char* end = begin + len;
    std::vector<char> out;
    Expr* cmb = 0;
    while(ptr != end)
    {
        if(*ptr == '\\')
        {
            out.insert(out.end(), begin, ptr);
            ptr++;
            switch(*ptr)
            {
                case 'n':
                    out.push_back('\n');
                    break;
                case 't':
                    out.push_back('\t');
                    break;
                default:
                    out.push_back(*ptr);
                    break;
            }
            ptr++;
            begin = ptr;
        }
        else if(*ptr == '$')
        {
            out.insert(out.end(), begin, ptr);

            //FileRegistry::Entry* e=t.pos.fileRd->getOwner()->addEntry("string",ptr,end-ptr);
            //FileReader* fr=new FileReader(t.pos.fileRd->getOwner(),e);
            //t.pos.fileRd->getOwner()->addReader(fr);
            //l.pushReader(fr,t.pos);
            FileLocation save = l.getLoc();
            FileLocation strLoc = t.pos;
            strLoc.offset += ptr - cstr;
            strLoc.col += ptr - cstr;
            l.setLoc(strLoc);
            l.fr->setSize(static_cast<size_t>(strLoc.offset + end - ptr));
            l.termOnUnknown = true;
            Expr* res = parseRule<Expr*>(&getRule("fmt"));
            dumpstack();
            //l.popReader();
            l.setLoc(save);
            l.fr->restoreSize();
            l.termOnUnknown = false;
            std::string dump;
            res->dump(dump);
            DPRINT("expr dump:%s\n", dump.c_str());
            //return res;
            if(!cmb)
            {
                cmb = new Expr(etCombine);
                cmb->pos = t.pos;
                cmb->lst = new ExprList;
            }
            if(!out.empty())
            {
                cmb->lst->values.emplace_back(new Expr(etString, mem->mkZString(&out[0], out.size())));
                out.clear();
            }
            cmb->lst->values.emplace_back(res);
            ptr = ptr + lastOkMatch.pos.offset - strLoc.offset + lastOkMatch.length;
            begin = ptr;
        }
        else
        {
            ptr++;
        }
    }

    if(!out.empty() || cmb)
    {
        out.insert(out.end(), begin, end);
    }
    if(cmb && !out.empty())
    {
        cmb->lst->values.emplace_back(new Expr(etString, mem->mkZString(&out[0], out.size())));
    }
    if(!cmb && out.empty())
    {
        Expr* rv = new Expr(etString, mem->mkZString(begin, len));
        rv->pos = t.pos;
        rv->end = t.pos;
        rv->end.col += t.length;
        return rv;
    }
    else if(!cmb)
    {
        Expr* rv = new Expr(etString, mem->mkZString(&out[0], out.size()));
        rv->pos = t.pos;
        rv->end = t.pos;
        rv->end.col += t.length;
        return rv;
    }
    return cmb;
}

Expr* ZParser::handleRawStr(Term t)
{
    unsigned int len = 0;
    const char* cstr = t.getValue(len);
    const char* begin = cstr + 1;
    len -= 2;
    const char* ptr = begin;
    const char* end = begin + len;
    std::vector<char> out;
    while(ptr != end)
    {
        if(*ptr == '\\')
        {
            out.insert(out.end(), begin, ptr);
            ++ptr;
            switch(*ptr)
            {
                case '\'':
                    out.push_back('\'');
                    break;
                case '\\':
                    out.push_back('\\');
                    break;
                default:
                    out.push_back('\\');
                    out.push_back(*ptr);
                    break;
            }
            begin = ++ptr;
        }
        else
        {
            ++ptr;
        }
    }
    if(!out.empty())
    {
        out.insert(out.end(), begin, end);
    }
    if(out.empty())
    {
        Expr* rv = new Expr(etString, mem->mkZString(begin, len));
        rv->pos = t.pos;
        rv->end = t.pos;
        rv->end.col += t.length;
        return rv;
    }
    else
    {
        Expr* rv = new Expr(etString, mem->mkZString(&out[0], out.size()));
        rv->pos = t.pos;
        rv->end = t.pos;
        rv->end.col += t.length;
        return rv;
    }
}

Statement* ZParser::handleExprStmt(Expr* ex)
{
    return new ExprStatement(ex);
}

Statement* ZParser::handleShortIf(Statement* stmt, Expr* cnd)
{
    return fillPos(new IfStatement(cnd, handleStmtListOne(stmt)));
}

StmtList* ZParser::handleElsifEmpty()
{
    return 0;
}

StmtList* ZParser::handleElsifFirst(Expr* cnd, StmtList* stmt)
{
    StmtList* rv = new StmtList;
    rv->values.emplace_back(fillPos(new IfStatement(cnd, stmt)));
    return rv;
}

StmtList* ZParser::handleElsifNext(StmtList* lst, Expr* cnd, StmtList* stmt)
{
    lst->values.emplace_back(fillPos(new IfStatement(cnd, stmt)));
    return lst;
}

StmtList* ZParser::handleElseEmpty()
{
    return 0;
}

StmtList* ZParser::handleElse(StmtList* lst)
{
    return lst;
}

Statement* ZParser::handleWhile(Name name, Expr* cnd, StmtList* body)
{
    return fillPos(new WhileStatement(name, cnd, body));
}

Statement* ZParser::handleShortWhile(Statement* body, Name name, Expr* cnd)
{
    return fillPos(new WhileStatement(name, cnd, handleStmtListOne(body)));
}

Expr* ZParser::handleMap(ExprList* lst)
{
    Expr* rv = new Expr(etMap);
    rv->lst = lst;
    rv->pos = callStack.last->start;
    rv->end = callStack.last->end;
    return rv;
}

Expr* ZParser::handlePair(Expr* nm, Expr* vl)
{
    if(nm->et == etVar && !nm->inBrackets && !nm->ns)
    {
        nm->et = etString;
    }
    Expr* rv = new Expr(etPair, nm, vl);
    return rv;
}


Expr* ZParser::handleSet(ExprList* lst)
{
    Expr* rv = new Expr(etSet);
    rv->lst = lst;
    rv->pos = callStack.last->start;
    rv->end = callStack.last->end;
    return rv;
}

Expr* ZParser::handleMapIndex(Expr* mp, Expr* idx)
{
    Expr* rv = new Expr(etKey, mp, idx);
    return rv;
}

Expr* ZParser::handleNameMapIndex(Expr* map, Term name)
{
    Expr* str = new Expr(etString, getValue(name));
    return new Expr(etKey, map, str);
}

Expr* ZParser::handleRange(Expr* start, Term t, Expr* end)
{
    Expr* rv = new Expr(etRange, start, end);
    rv->rangeIncl = t.tt == tDoubleDot;
    return rv;
}

Expr* ZParser::handleRangeStep(Expr* start, Term t, Expr* end, Expr* step)
{
    Expr* rv = new Expr(etRange, start, end, step);
    rv->rangeIncl = t.tt == tDoubleDot;
    return rv;
}

Expr* ZParser::handleEmptyExpr()
{
    return 0;
}

Expr* ZParser::handleRangeTail(Expr* st)
{
    return st;
}

Statement* ZParser::handleFor(Name name, NameList* lst, Expr* expr, StmtList* body)
{
    return fillPos(new ForLoopStatement(name, lst, expr, body));
    //if(body && !body->empty())rv->end=body->back()->end;
    //else rv->end=expr->end;
    //return rv;
}

Statement* ZParser::handleShortFor(Statement* body, Name name, NameList* lst, Expr* expr)
{
    StmtList* bodyList = new StmtList;
    bodyList->values.emplace_back(body);
    return fillPos(new ForLoopStatement(name, lst, expr, bodyList));
    //ForLoopStatement* rv=new ForLoopStatement(name,lst,expr,bodyList);
    //rv->pos=body->pos;
    //rv->end=expr->end;
    //return rv;
}

Expr* ZParser::handleEmptyMap()
{
    Expr* rv = new Expr(etMap);
    return rv;
}

Statement* ZParser::handleClass(Term name, FuncParamList* args, ClassParent* parent, StmtList* body, Term /*end*/)
{
    return fillPos(new ClassDefStatement(getValue(name), args, parent, body));
    //ClassDefStatement* rv=new ClassDefStatement(getValue(name),args,parent,body);
    //rv->pos=name.pos;
    //rv->end=end.pos;
    //return rv;
}

ClassParent* ZParser::handleNoParent()
{
    return 0;
}

ClassParent* ZParser::handleParent(Expr* parent)
{
    if(parent->et == etVar)
    {
        auto rv = new ClassParent(parent->getSymbol(), nullptr);
        delete parent;
        return rv;
    }
    if(parent->et == etCall && parent->e1->et == etVar)
    {
        auto rv = new ClassParent(parent->e1->getSymbol(), parent->lst);
        parent->lst = nullptr;
        delete parent;
        return rv;
    }
    throw SyntaxErrorException("Expected parent class", parent->pos);
}

/*ClassParent* ZParser::handleParentNoArgs(NameList* name)
{
  return handleParent(name,0);
}*/


Statement* ZParser::handleStmt(Statement* stmt)
{
    return stmt;
}

Statement* ZParser::handleAttrList(NameList* lst)
{
    return fillPos(new VarListStatement(lst));
    //VarListStatement* rv=new VarListStatement(lst);
    // todo:pos?
    //return rv;
}

Statement* ZParser::handleOn(Term name, FuncParamList* argList, StmtList* body)
{
    return fillPos(new FuncDeclStatement(getValue(name), argList, body, true));
//  FuncDeclStatement* rv=new FuncDeclStatement(getValue(name),argList,body,true);
//  rv->pos=name.pos;
//  return rv;
}

FuncParamList* ZParser::handleOptArg(FuncParamList* argList)
{
    return argList;
}

FuncParamList* ZParser::handleOptArgNone()
{
    return 0;
}

Statement* ZParser::handleMemberInit(Term name, AttrInstList* attr, Expr* expr)
{
    return fillPos(new ClassMemberDef(getValue(name), attr, expr));
//  ClassMemberDef* rv=new ClassMemberDef(getValue(name),attr,expr);
//  return rv;
}

Expr* ZParser::handleNamespaceExpr(Term name, Expr* expr)
{
    if(expr->et != etVar)
    {
        throw SyntaxErrorException("Namespace is only valid for identifiers", expr->pos);
    }
    if(!expr->ns)
    {
        expr->ns = new NameList();
    }
    expr->pos = name.pos;
    expr->ns->values.push_front(getValue(name));
    return expr;
}

Statement* ZParser::handleNamespace(NameList* ns, StmtList* body)
{
    return fillPos(new NamespaceStatement(ns, body));
    /*
  rv->pos=ns && !ns->empty()?ns->front().pos:body?body->front()->pos:FileLocation();
  if(body && !body->empty())
  {
    rv->end=body->back()->end;
  }*/
//  rv->pos=callStack.last->start;
//  rv->end=callStack.last->end;
//  return rv;
}

NameList* ZParser::handleNsEmpty()
{
    return 0;
}

NameList* ZParser::handleNsFirst(Term name)
{
    NameList* rv = new NameList();
    rv->values.push_back(getValue(name));
    return rv;
}

NameList* ZParser::handleNsNext(NameList* ns, Term name)
{
    ns->values.push_back(getValue(name));
    return ns;
}

Statement* ZParser::handleTryCatch(StmtList* tryBody, ExprList* exList, Term var, StmtList* catchBody)
{
    return fillPos(new TryCatchStatement(tryBody, exList, getValue(var), catchBody));
}

Statement* ZParser::handleThrow(Expr* ex)
{
    return fillPos(new ThrowStatement(ex));
}

ExprList* ZParser::handleNmListOne(NameList* nm)
{
    Expr* expr = new Expr(etVar);
    expr->ns = nm;
    expr->val = nm->values.back().val;
    expr->pos = nm->values.back().pos;
    nm->values.pop_back();
    if(expr->ns->values.empty())
    {
        delete expr->ns;
        expr->ns = 0;
    }
    ExprList* rv = new ExprList;
    rv->pos = expr->pos;
    rv->values.emplace_back(expr);
    return rv;
}

ExprList* ZParser::handleNmListNext(ExprList* lst, NameList* nm)
{
    Expr* expr = new Expr(etVar);
    expr->ns = nm;
    expr->val = nm->values.back().val;
    expr->pos = nm->values.back().pos;
    nm->values.pop_back();
    if(expr->ns->values.empty())
    {
        delete expr->ns;
        expr->ns = 0;
    }
    lst->values.emplace_back(expr);
    return lst;
}

Statement* ZParser::handleSimpleStmt(Statement* stmt)
{
    return stmt;
}

Statement* ZParser::handleYieldNil(Term /*t*/)
{
    return fillPos(new YieldStatement(nullptr));
//  Statement* rv=new YieldStatement(0);
//  rv->pos=t.pos;
//  return rv;
}

Statement* ZParser::handleYield(Expr* ex)
{
    Statement* rv = fillPos(new YieldStatement(ex));
    if(ex)
    {
        rv->end = ex->end;
    }
    return rv;
//  Statement* rv=new YieldStatement(ex);
//  rv->pos=ex->pos;
//  return rv;
}

Statement* ZParser::handleBreakNil(Term /*brk*/)
{
    return fillPos(new BreakStatement());
//  Statement* rv=new BreakStatement();
//  rv->pos=brk.pos;
//  return rv;
}

Statement* ZParser::handleBreakId(Term /*brk*/, Term id)
{
    return fillPos(new BreakStatement(getValue(id)));
//  Statement* rv=new BreakStatement(getValue(id));
//  rv->pos=brk.pos;
//  return rv;
}

Statement* ZParser::handleNextNil(Term /*nxt*/)
{
    return fillPos(new NextStatement());
//  Statement* rv=new NextStatement();
//  rv->pos=nxt.pos;
//  return rv;
}

Statement* ZParser::handleNextId(Term /*nxt*/, Term id)
{
    return fillPos(new NextStatement(getValue(id)));
//  Statement* rv=new NextStatement(getValue(id));
//  rv->pos=nxt.pos;
//  return rv;
}

Statement* ZParser::handleRedoNil(Term /*rd*/)
{
    return fillPos(new RedoStatement());
//  Statement* rv=new RedoStatement();
//  rv->pos=rd.pos;
//  return rv;
}

Statement* ZParser::handleRedoId(Term /*rd*/, Term id)
{
    return fillPos(new RedoStatement(getValue(id)));
//  Statement* rv=new RedoStatement(getValue(id));
//  rv->pos=rd.pos;
//  return rv;
}

Name ZParser::handleOptNameEmpty()
{
    return Name();
}

Name ZParser::handleOptName(Term name)
{
    return Name(getValue(name));
}

Statement* ZParser::handleSwitch(Term /*sw*/, Expr* expr, SwitchCasesList* lst)
{
    return fillPos(new SwitchStatement(expr, lst));
//  SwitchStatement* rv=new SwitchStatement(expr,lst);
//  rv->pos=sw.pos;
//  return rv;
}

SwitchCasesList* ZParser::handleCasesListEmpty()
{
    return 0;
}

SwitchCasesList* ZParser::handleCasesListEmptyLine(SwitchCasesList* lst)
{
    return lst;
}

SwitchCasesList* ZParser::handleCasesListFirst(ExprList* caseExpr, StmtList* caseBody)
{
    SwitchCasesList* lst = new SwitchCasesList;
    lst->push_back(SwitchCase(caseExpr->pos, caseExpr, caseBody));
    return lst;
}

SwitchCasesList* ZParser::handleCasesListNext(SwitchCasesList* lst, ExprList* caseExpr, StmtList* caseBody)
{
    lst->push_back(SwitchCase(caseExpr->pos, caseExpr, caseBody));
    return lst;
}

SwitchCasesList* ZParser::handleCasesListFirstWild(Term star, StmtList* caseBody)
{
    SwitchCasesList* lst = new SwitchCasesList;
    lst->push_back(SwitchCase(star.pos, 0, caseBody));
    return lst;
}

SwitchCasesList* ZParser::handleCasesListNextWild(SwitchCasesList* lst, Term star, StmtList* caseBody)
{
    lst->push_back(SwitchCase(star.pos, 0, caseBody));
    return lst;
}


Statement* ZParser::handleEnum(Term name, ExprList* items)
{
    return fillPos(new EnumStatement(false, getValue(name), items));
}

Statement* ZParser::handleBitEnum(Term name, ExprList* items)
{
    return fillPos(new EnumStatement(true, getValue(name), items));
}

Expr* ZParser::handleFormat(Term fmt, Expr* expr)
{
    Expr* rv = new Expr(etFormat, getValue(fmt));
    rv->e1 = expr;
    return rv;
}

Expr* ZParser::handleFormatArgs(Term fmt, ExprList* lst)
{
    Expr* rv = new Expr(etFormat, getValue(fmt));
    rv->lst = lst;
    return rv;

}

StmtList* ZParser::handleInclude(StmtList* lst, Expr* fileName)
{
    if(fileName->et != etString)
    {
        throw std::runtime_error("include expected string");
    }
    FileRegistry::Entry* e = l.fr->getOwner()->openFile(fileName->val.c_str());
    if(!e)
    {
        throw std::runtime_error(std::string("file not found:") + fileName->val.c_str());
    }
    FileReader* fr = l.fr->getOwner()->newReader(e);
    pushReader(fr, fileName->pos);
    StmtList* lst2 = parseRule<StmtList*>(&getRule("goal"));
    delete fileName;
    //l.popReader();
    if(!lst)
    {
        lst = new StmtList;
    }
    lst->values.splice(lst->values.end(), lst2->values);
    delete lst2;
    result = nullptr;
    return lst;
}

StmtList* ZParser::handleInclude1(Expr* fileName)
{
    if(fileName->et != etString)
    {
        throw std::runtime_error("include expected string");
    }
    FileRegistry::Entry* e = l.fr->getOwner()->openFile(fileName->val.c_str());
    if(!e)
    {
        throw std::runtime_error(std::string("file not found:") + fileName->val.c_str());
    }
    FileReader* fr = l.fr->getOwner()->newReader(e);
    pushReader(fr, fileName->pos);
    StmtList* lst2 = parseRule<StmtList*>(&getRule("goal"));
    delete fileName;
    //l.popReader();
    //StmtList* rv=new StmtList;
    //rv->splice(rv->end(),*lst2);
    //delete lst2;
    //result=0;
    return lst2;
}

Expr* ZParser::handleCount(Expr* expr)
{
    return new Expr(etCount, expr);
}

Expr* ZParser::handleTernary(Expr* cnd, Expr* trueExpr, Expr* falseExpr)
{
    return new Expr(etTernary, cnd, trueExpr, falseExpr);
}

Statement* ZParser::handlePropDecl(Term name, PropAccessorList* accList)
{
    PropStatement* rv = new PropStatement(getValue(name));
    if(accList)
    {
        for(PropAccessorList::iterator it = accList->begin(), end = accList->end(); it != end; ++it)
        {
            PropAccessor* acc = *it;
            if(acc)
            {
                if(acc->isGet)
                {
                    if(rv->getName.val.get() || rv->getFunc)
                    {
                        throw SyntaxErrorException(FORMAT("duplicate getter for property %{}", rv->name), acc->pos);
                    }
                    if(acc->name.val.get())
                    {
                        rv->getName = acc->name;
                    }
                    else
                    {
                        Name getName = rv->name.val + ".get";
                        getName.pos = acc->pos;
                        rv->getFunc = new FuncDeclStatement(getName, 0, acc->func);
                        rv->getFunc->pos = acc->pos;
                        rv->getFunc->end = acc->end;
                    }
                }
                else
                {
                    if(rv->setName.val.get() || rv->setFunc)
                    {
                        throw SyntaxErrorException(FORMAT("duplicate setter for property %{}", rv->name), acc->pos);
                    }
                    if(acc->name.val.get())
                    {
                        rv->setName = acc->name;
                    }
                    else
                    {
                        Name getName = rv->name.val + ".set";
                        getName.pos = acc->pos;
                        FuncParamList* args = new FuncParamList;
                        args->values.emplace_back(new FuncParam(acc->arg));
                        rv->setFunc = new FuncDeclStatement(getName, args, acc->func);
                        rv->setFunc->pos = acc->pos;
                        rv->setFunc->end = acc->end;
                    }
                }
                acc->func = 0;
            }
        }
        delete accList;
    }
    return fillPos(rv);
}

PropAccessor* ZParser::handleEmptyPropAcc()
{
    return 0;
}

PropAccessor* ZParser::handlePropAccName(Term acc, Term name)
{
    bool isGet = acc.getValue() == "get";
    if(!isGet && acc.getValue() != "set")
    {
        throw SyntaxErrorException("expected get or set", acc.pos);
    }
    PropAccessor* rv = new PropAccessor(getValue(name));
    rv->pos = callStack.last->start;
    rv->end = callStack.last->end;
    rv->isGet = isGet;
    return rv;
}

PropAccessor* ZParser::handlePropAccNoArg(Term acc, StmtList* func)
{
    bool isGet = acc.getValue() == "get";
    if(!isGet && acc.getValue() != "set")
    {
        throw SyntaxErrorException("expected get or set", acc.pos);
    }
    if(!isGet)
    {
        throw SyntaxErrorException("expected argument for property setter", acc.pos);
    }
    PropAccessor* rv = new PropAccessor(func);
    rv->pos = callStack.last->start;
    rv->end = callStack.last->end;
    rv->isGet = isGet;
    return rv;
}

PropAccessor* ZParser::handlePropAccArg(Term acc, Term arg, StmtList* func)
{
    bool isGet = acc.getValue() == "get";
    if(!isGet && acc.getValue() != "set")
    {
        throw SyntaxErrorException("expected get or set", acc.pos);
    }
    if(isGet)
    {
        throw SyntaxErrorException("argument unexpected for property getter", acc.pos);
    }
    PropAccessor* rv = new PropAccessor(func, getValue(arg));
    rv->pos = callStack.last->start;
    rv->end = callStack.last->end;
    rv->isGet = isGet;
    return rv;
}


PropAccessorList* ZParser::handlePropAccListEmpty()
{
    return 0;
}

PropAccessorList* ZParser::handlePropAccListFirst(PropAccessor* acc)
{
    if(acc)
    {
        PropAccessorList* rv = new PropAccessorList;
        rv->push_back(acc);
        return rv;
    }
    else
    {
        return 0;
    }
}

PropAccessorList* ZParser::handlePropAccListNext(PropAccessorList* lst, PropAccessor* acc)
{
    if(!lst)
    {
        if(!acc)
        {
            return 0;
        }
        lst = new PropAccessorList;
    }
    if(acc)
    {
        if(lst->size() == 2)
        {
            throw SyntaxErrorException("there can be only one setter and one getter in property declaration",
                    lastOkMatch.pos);
        }
        lst->push_back(acc);
    }
    return lst;
}

Expr* ZParser::handleVarIdent(Term name)
{
    return new Expr(etVar, getValue(name));
}

AttrInstance* ZParser::handleAttrSimple(Expr* name)
{
    return new AttrInstance(name);
}

AttrInstance* ZParser::handleAttrAssign(Expr* name, Expr* value)
{
    return new AttrInstance(name, value);
}

AttrInstList* ZParser::handleAttrDeclEmpty()
{
    return 0;
}

AttrInstList* ZParser::handleAttrDecl(AttrInstList* lst)
{
    return lst;
}

AttrInstList* ZParser::handleAttrListEmpty()
{
    return 0;
}

AttrInstList* ZParser::handleAttrListFirst(AttrInstance* attr)
{
    AttrInstList* rv = new AttrInstList;
    rv->push_back(attr);
    return rv;
}

AttrInstList* ZParser::handleAttrListNext(AttrInstList* lst, AttrInstance* attr)
{
    lst->push_back(attr);
    return lst;
}

AttrDeclStatement* ZParser::handleAttrDefSimple(Term name)
{
    AttrDeclStatement* rv = new AttrDeclStatement(getValue(name));
    rv->pos = callStack.last->start;
    rv->end = callStack.last->end;
    return rv;
}

AttrDeclStatement* ZParser::handleAttrDefValue(Term name, Expr* value)
{
    AttrDeclStatement* rv = new AttrDeclStatement(getValue(name), value);
    rv->pos = callStack.last->start;
    rv->end = callStack.last->end;
    return rv;
}

Expr* ZParser::handleGetAttr(Expr* expr, ExprList* lst)
{
    if(lst->values.empty())
    {
        throw SyntaxErrorException("property name and/or attribute expected", expr->pos);
    }
    if(lst->values.size() > 2)
    {
        throw SyntaxErrorException("unexpected number of parameters for attribute getter", expr->pos);
    }
    Expr* member = lst->values.size() == 2 ? lst->values.front().release() : nullptr;
    Expr* attr = lst->values.back().release();
    lst->values.clear();
    delete lst;
    return new Expr(etGetAttr, expr, member, attr);
}


Statement* ZParser::handleClassAttr(AttrInstList* lst)
{
    return fillPos(new ClassAttrDef(lst));
}

Statement* ZParser::handleMemberDef(Term name, AttrInstList* attrs)
{
    return fillPos(new ClassMemberDef(getValue(name), attrs));
}

Expr* ZParser::handleGetType(Expr* expr)
{
    return new Expr(etGetType, expr);
}

Expr* ZParser::handleCopy(Expr* expr)
{
    return new Expr(etCopy, expr);
}

Statement* ZParser::handleUse(Expr* expr)
{
    return fillPos(new UseStatement(expr));
}

FuncParamList* ZParser::handleParamListEmpty()
{
    return 0;
}

FuncParamList* ZParser::handleParamListOne(FuncParam* param)
{
    FuncParamList* rv = new FuncParamList;
    rv->values.emplace_back(param);
    return rv;
}

FuncParamList* ZParser::handleParamListNext(FuncParamList* lst, FuncParam* param)
{
    if(!lst)
    {
        lst = new FuncParamList;
    }
    lst->values.emplace_back(param);
    return lst;
}


FuncParam* ZParser::handleFuncParamSimple(Term ident)
{
    return new FuncParam(getValue(ident));
}

FuncParam* ZParser::handleFuncParamDefVal(Term ident, Expr* expr)
{
    return new FuncParam(getValue(ident), expr);
}

FuncParam* ZParser::handleFuncParamArr(Term ident)
{
    return new FuncParam(getValue(ident), FuncParam::ptVarArgs);
}

FuncParam* ZParser::handleFuncParamMap(Term ident)
{
    return new FuncParam(getValue(ident), FuncParam::ptNamedArgs);
}

Expr* ZParser::handleNamespaceExprGlobal(Expr* expr)
{
    if(!expr->ns)
    {
        expr->ns = new NameList;
    }
    expr->global = true;
    return expr;
}

Expr* ZParser::handleNumArg(Term arg)
{
    return new Expr(etNumArg, getValue(arg));
}

Expr* ZParser::handleSeqOp(Expr* cnt, Expr* var, ExprList* lst)
{
    if(!(var->et == etVar || (var->et == etAssign && var->e1->et == etVar)))
    {
        throw SyntaxErrorException("Expected variable with optional assignment", var->pos);
    }
    Expr* rv = new Expr(etSeqOps, var, cnt);
    rv->lst = lst;
    return rv;
}

ExprList* ZParser::handleSeqOpLstFirst(Expr* expr)
{
    ExprList* rv = new ExprList;
    rv->values.emplace_back(expr);
    return rv;
}

ExprList* ZParser::handleSeqOpLstNext(ExprList* lst, Expr* expr)
{
    lst->values.emplace_back(expr);
    return lst;
}

Expr* ZParser::handleSeqOpMap(Expr* expr)
{
    return new Expr(etMapOp, expr);
}

Expr* ZParser::handleSeqOpGrep(Expr* expr)
{
    return new Expr(etGrepOp, expr);
}

/*Expr* ZParser::handleSeqOpSum(Expr* expr)
{
  return new Expr(etSumOp,expr);
}*/

Expr* ZParser::handleLiteral(Term t)
{
    Expr* rv = new Expr(etLiteral);
    rv->pos = t.pos;
    ExprList* lst = new ExprList();
    unsigned int len = 0;
    typedef unsigned char uchar;
    const uchar* str = (const uchar*) t.getValue(len);
    const uchar* end = str + len;
    const uchar* strStart = str;
    while(str < end)
    {
        if(isdigit(*str))
        {
            const uchar* start = str;
            ExprType et = etInt;
            while(isdigit(*str))
            {
                ++str;
            }
            if(*str == '.')
            {
                et = etDouble;
                ++str;
                while(isdigit(*str))
                {
                    ++str;
                }
            }
            lst->values.emplace_back(new Expr(et, mem->mkZString((const char*) start, static_cast<size_t>(str - start))));
        }
        else if(*str == '\'' || *str == '"')
        {
            const uchar* ptr = str;
            uchar qchar = *ptr;
            ++ptr;
            while(*ptr != qchar)
            {
                if(*ptr == '\\')
                {
                    ++ptr;
                }
                ++ptr;
            }
            Term strTerm = t;
            strTerm.pos.offset += str - strStart;
            strTerm.pos.col += str - strStart;
            strTerm.length = static_cast<size_t>(ptr - str + 1);
            if(qchar == '\'')
            {
                lst->values.emplace_back(handleRawStr(strTerm));
            }
            else
            {
                lst->values.emplace_back(handleStr(strTerm));
            }
            str = ptr + 1;
        }
        else //ident
        {
            const uchar* start = str;
            while(*str > 127 || isalpha(*str) || *str == '_')
            {
                ++str;
            }
            lst->values.emplace_back(new Expr(etVar, mem->mkZString((const char*) start, static_cast<size_t>(str - start))));
        }
    }

    rv->lst = lst;
    rv->pos = t.pos;
    return rv;
}

Statement* ZParser::handleLiterDecl(Term name, LiterArgsList* args, StmtList* body)
{
    return fillPos(new LiterStatement(getValue(name), args, body));
//  LiterStatement* rv=new LiterStatement(getValue(name),args,body);
//  rv->pos=callStack.last->callLoc;
//  return rv;
}

LiterArgsList* ZParser::handleLiterArgListStr(Term ident, Term arg)
{
    LiterArgsList* rv = new LiterArgsList;
    rv->push_back(std::make_unique<LiterArg>(ltString, getValue(arg), getValue(ident)));
    return rv;
}

LiterArgsList* ZParser::handleLiterArgListFirst(LiterArg* arg)
{
    LiterArgsList* rv = new LiterArgsList;
    rv->emplace_back(arg);
    return rv;
}

LiterArgsList* ZParser::handleLiterArgListNext(LiterArgsList* lst, LiterArg* arg)
{
    lst->emplace_back(arg);
    return lst;
}

LiterArg* ZParser::handleLiterArgNum(Term arg, Term ident)
{
    return new LiterArg(ltNumber, getValue(arg), getValue(ident));
}

LiterArg* ZParser::handleLiterArgNumOpt(Term arg, Term ident)
{
    return new LiterArg(ltNumber, getValue(arg), getValue(ident), true);
}

LiterArg* ZParser::handleLiterArgInt(Term arg, Term ident)
{
    return new LiterArg(ltInt, getValue(arg), getValue(ident));
}

LiterArg* ZParser::handleLiterArgIntOpt(Term arg, Term ident)
{
    return new LiterArg(ltInt, getValue(arg), getValue(ident), true);
}

LiterArg* ZParser::handleLiterArgDbl(Term arg, Term ident)
{
    return new LiterArg(ltDouble, getValue(arg), getValue(ident));
}

LiterArg* ZParser::handleLiterArgDblOpt(Term arg, Term ident)
{
    return new LiterArg(ltDouble, getValue(arg), getValue(ident), true);
}

LiterArgsList* ZParser::handleLiterArgList(LiterArgsList* lst)
{
    return lst;
}

LiterArgsList* ZParser::handleLiterArgListLastInt(LiterArgsList* lst, Term ident)
{
    lst->push_back(std::make_unique<LiterArg>(ltInt, getValue(ident), Name()));
    return lst;
}

LiterArgsList* ZParser::handleLiterArgListLastDbl(LiterArgsList* lst, Term ident)
{
    lst->push_back(std::make_unique<LiterArg>(ltDouble, getValue(ident), Name()));
    return lst;
}

LiterArgsList* ZParser::handleLiterArgListLastNum(LiterArgsList* lst, Term ident)
{
    lst->push_back(std::make_unique<LiterArg>(ltNumber, getValue(ident), Name()));
    return lst;
}

}
