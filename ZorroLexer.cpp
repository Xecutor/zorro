#include "ZorroLexer.hpp"
#include <cctype>
#include "Exceptions.hpp"
#include "FileReader.hpp"
#include "ZString.hpp"

namespace zorro {

const char* ZLexer::names[TermsCount] = {
    "tInt",
    "tDouble",
    "tString",
    "tRawString",
    "tNil",
    "tTrue",
    "tFalse",
    "tIdent",
    "tPlus",
    "tPlusPlus",
    "tPlusEq",
    "tMinus",
    "tMinusMinus",
    "tMinusEq",
    "tMul",
    "tMulEq",
    "tDiv",
    "tDivEq",
    "tAmp",
    "tDoubleAmp",
    "tMod",
    "tModEq",
    "tLess",
    "tGreater",
    "tLessEq",
    "tGreaterEq",
    "tEqual",
    "tNotEqual",
    "tMatch",
    "tAnd",
    "tOr",
    "tNot",
    "tORBr",
    "tCRBr",
    "tOSBr",
    "tCSBr",
    "tOCBr",
    "tCCBr",
    "tDot",
    "tDotQMark",
    "tDotStar",
    "tDoubleDot",
    "tDoubleDotLess",
    "tColon",
    "tDoubleColon",
    "tStarColon",
    "tComma",
    "tArrow",
    "tThinArrow",
    "tEq",
    "tAt",
    "tFormat",
    "tExclSign",
    "tQuestMark",
    "tHash",
    "tPipe",
    "tEol",
    "tEof",
    "tFunc",
    "tEnd",
    "tReturn",
    "tReturnIf",
    "tYield",
    "tIf",
    "tElse",
    "tElsif",
    "tWhile",
    "tFor",
    "tIn",
    "tIs",
    "tOn",
    "tClass",
    "tNamespace",
    "tTry",
    "tCatch",
    "tThrow",
    "tNext",
    "tBreak",
    "tRedo",
    "tSwitch",
    "tEnum",
    "tBitEnum",
    "tInclude",
    "tProp",
    "tAttr",
    "tAttrGet",
    "tUse",
    "tNumArg",
    "tCarret",
    "tMapOp",
    "tGrepOp",
    "tRegExp",
    "tLiter",
    "tLiteral",
    "tMacro",
    "tMacroExpand",
    "tComment"
};

ZHash<TermType> ZLexer::idents;

struct IdentsInit {
    IdentsInit()
    {
#define ID(n, t) ZLexer::idents.insert(mkstr(n),t)
        ID("if", tIf);
        ID("or", tOr);
        ID("in", tIn);
        ID("is", tIs);
        ID("on", tOn);
        ID("end", tEnd);
        ID("and", tAnd);
        ID("nil", tNil);
        ID("for", tFor);
        ID("try", tTry);
        ID("not", tNot);
        ID("use", tUse);
        ID("func", tFunc);
        ID("else", tElse);
        ID("true", tTrue);
        ID("next", tNext);
        ID("redo", tRedo);
        ID("enum", tEnum);
        ID("prop", tProp);
        ID("attr", tAttr);
        ID("break", tBreak);
        ID("false", tFalse);
        ID("elsif", tElsif);
        ID("while", tWhile);
        ID("class", tClass);
        ID("catch", tCatch);
        ID("throw", tThrow);
        ID("yield", tYield);
        ID("liter", tLiter);
        ID("macro", tMacro);
        ID("return", tReturn);
        ID("returnif", tReturnIf);
        ID("switch", tSwitch);
        ID("bitenum", tBitEnum);
        ID("include", tInclude);
        ID("namespace", tNamespace);
    }

    ~IdentsInit()
    {
        ZHash<TermType>::Iterator it(ZLexer::idents);
        ZString* str;
        TermType* val;
        while(it.getNext(str, val))
        {
            delete str;
        }
    }

    ZString* mkstr(const char* str)
    {
        ZString* rv = new ZString;
        rv->assignConst(str);
        return rv;
    }
} zlexerIdentsInit;


const char* ZLexer::getTermName(Term t)
{
    if(t.tt < TermsCount && names[t.tt])
    {
        return names[t.tt];
    } else
    {
        return "unknown";
    }
}

ZLexer::ZLexer() : macroExpander(0)
{
    reg('-', tMinus);
    reg("--", tMinusMinus);
    reg("-=", tMinusEq);
    reg('+', tPlus);
    reg("++", tPlusPlus);
    reg("+=", tPlusEq);
    reg('*', tMul);
    reg("*=", tMulEq);
    reg('/', tDiv);
    reg("/=", tDivEq);
    reg('%', tMod);
    reg("%=", tModEq);
    reg('&', tAmp);
    reg("&&", tDoubleAmp);
    reg('|', tPipe);
    reg('.', tDot);
    reg(".?", tDotQMark);
    reg(".*", tDotStar);
    reg("..", tDoubleDot);
    reg("..<", tDoubleDotLess);
    reg(':', tColon);
    reg("::", tDoubleColon);
    reg("*:", tStarColon);
    reg(',', tComma);
    reg('(', tORBr);
    reg(')', tCRBr);
    reg('[', tOSBr);
    reg(']', tCSBr);
    reg('{', tOCBr);
    reg('}', tCCBr);
    reg('<', tLess);
    reg('>', tGreater);
    reg("<=", tLessEq);
    reg(">=", tGreaterEq);
    reg('=', tEq);
    reg("==", tEqual);
    reg("!=", tNotEqual);
    reg("=~", tMatch);
    reg(' ');
    reg('\t');
    reg('\n', tEol);
    reg(';', tEol);
    reg("=>", tArrow);
    reg("->", tThinArrow);
    reg('@', tAt);
    reg('!', tExclSign);
    reg('?', tQuestMark);
    reg('#', tHash);
    reg('$', tFormat, &ZLexer::matchDollar);
    reg('"', tString, &ZLexer::matchString);
    reg('\'', tRawString, &ZLexer::matchRawString);
    reg('`', tRegExp, &ZLexer::matchRegExp);
    reg(".{", tAttrGet);
    reg('\\', tNumArg, &ZLexer::matchNumArg);
    reg('^', tCarret);
    reg(":>", tMapOp);
    reg(":?", tGrepOp);
    reg("@@", tMacroExpand, &ZLexer::matchMacroExpand);
    for(char c = '0'; c <= '9'; c++)
    {
        reg(c, tInt, &ZLexer::matchNumber);
    }
    for(char c = 'a'; c <= 'z'; c++)
    {
        reg(c, tIdent, &ZLexer::matchIdent);
    }
    for(char c = 'A'; c <= 'Z'; c++)
    {
        reg(c, tIdent, &ZLexer::matchIdent);
    }
    for(unsigned int c = 128; c <= 255; c++)
    {
        reg(static_cast<char>(c), tIdent, &ZLexer::matchUIdent);
    }
    reg('_', tIdent, &ZLexer::matchIdent);
    reg("//", &ZLexer::matchLineComment);
    reg("/*", &ZLexer::matchMLComment);
}

void ZLexer::matchNumber()
{
    char c;
    while((c = fr->peek()) >= '0' && c <= '9')
    {
        fr->getNextChar();
        last.length++;
    }
    if(c == 'x' || c == 'X')
    {
        fr->getNextChar();
        last.length++;
        while(((c = fr->peek()) >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'))
        {
            fr->getNextChar();
            last.length++;
        }
        return;
    }
    FileLocation loc = fr->getLoc();
    if(c != '.')
    {
        while(isalpha(c) || c == '_')
        {
            fr->getNextChar();
            last.tt = tLiteral;
            last.length++;
            c = fr->peek();
        }
        if(isdigit(fr->peek()))
        {
            matchNumber();
        }
        return;
    }
    fr->getNextChar();
    if(fr->peek() == '.')
    {
        fr->setLoc(loc);
        return;
    }
    last.length++;
    if(last.tt != tLiteral)
    {
        last.tt = tDouble;
    }
    while((c = fr->peek()) >= '0' && c <= '9')
    {
        fr->getNextChar();
        last.length++;
    }
    if(c == 'e' || c == 'E')
    {
        fr->getNextChar();
        last.length++;
        if(fr->peek() == '-' || fr->peek() == '+')
        {
            fr->getNextChar();
            last.length++;
        }
        while((c = fr->peek()) >= '0' && c <= '9')
        {
            fr->getNextChar();
            last.length++;
        }
    }
    if(isalpha(c = fr->peek()))
    {
        while(isalpha(c) || c == '_')
        {
            fr->getNextChar();
            last.tt = tLiteral;
            last.length++;
            c = fr->peek();
        }
        if(isdigit(fr->peek()))
        {
            matchNumber();
        }
    }
}

void ZLexer::matchUIdent()
{
    char c;
    while(((unsigned char) (c = fr->peek())) >= 128 || isalnum(c) || c == '_')
    {
        fr->getNextChar();
        last.length++;
    }
    if((c = fr->peek()) == '\'' || c == '"')
    {
        errorDetected = false;
        FileLocation savePos = last.pos;
        uint32_t saveLength = last.length;
        fr->getNextChar();
        last.length++;
        if(c == '\'')
        {
            matchRawString();
        } else
        {
            matchString();
        }
        if(!errorDetected)
        {
            last.tt = tLiteral;
            return;
        } else
        {
            last.tt = tIdent;
            errorDetected = false;
            fr->setLoc(savePos);
            last.length = saveLength;
        }
    }
    unsigned int len;
    const char* ptr = last.getValue(len);
    TermType* tt = idents.getPtr(ptr, len);
    if(tt)
    {
        last.tt = *tt;
    }
}

void ZLexer::matchIdent()
{
    char c;
    while(isalnum(c = fr->peek()) || c == '_')
    {
        fr->getNextChar();
        last.length++;
    }
    if((c = fr->peek()) == '\'' || c == '"')
    {
        errorDetected = false;
        FileLocation savePos = last.pos;
        uint32_t saveLength = last.length;
        fr->getNextChar();
        last.length++;
        if(c == '\'')
        {
            matchRawString();
        } else
        {
            matchString();
        }
        if(!errorDetected)
        {
            last.tt = tLiteral;
            return;
        } else
        {
            last.tt = tIdent;
            errorDetected = false;
            fr->setLoc(savePos);
            last.length = saveLength;
        }
    }
    unsigned int len;
    const char* ptr = last.getValue(len);
    TermType* tt = idents.getPtr(ptr, len);
    if(tt)
    {
        last.tt = *tt;
    }
}

void ZLexer::matchDollar()
{
    char c = fr->peek();
    if(c == '*')
    {
        fr->getNextChar();
        last.length++;
        c = fr->peek();
    } else
    {
        while((c = fr->peek()) >= '0' && c <= '9')
        {
            fr->getNextChar();
            last.length++;
        }
    }
    if(c == '.')
    {
        fr->getNextChar();
        last.length++;
        c = fr->peek();
        if(c == '*')
        {
            fr->getNextChar();
            last.length++;
        } else
        {
            while((c = fr->peek()) >= '0' && c <= '9')
            {
                fr->getNextChar();
                last.length++;
            }
        }
    }
    FileLocation saved = fr->getLoc();
    unsigned int savedLength = last.length;
    while((c = fr->peek()) == 'l' || c == 'r' || c == 'x' || c == 'X' || c == 'z')
    {
        fr->getNextChar();
        last.length++;
    }
    if(c == ':')
    {
        fr->getNextChar();
        last.length++;
    } else
    {
        fr->setLoc(saved);
        last.length = savedLength;
    }
}

void ZLexer::matchLineComment()
{
    while(!fr->isEof() && fr->peek() != '\n')
    {
        fr->getNextChar();
        last.length++;
    }
}

void ZLexer::matchMLComment()
{
    for(;;)
    {
        if(fr->isEof())
        {
            if(termOnUnknown)
            {
                fr->setLoc(last.pos);
                last.tt = tEol;
                errorDetected = true;
                return;
            }
            throw UnexpectedEndOfLine("inside multiline comment", last.pos);
        }
        if(fr->getNextChar() == '*' && fr->peek() == '/')
        {
            fr->getNextChar();
            last.length += 2;
            break;
        }
        last.length++;
    }
}


void ZLexer::matchString()
{
    char c;
    uint32_t startOff = fr->getLoc().offset;
    while((c = fr->getNextChar()) != '"')
    {
        if(fr->isEof())
        {
            if(termOnUnknown)
            {
                fr->setLoc(last.pos);
                last.tt = tEol;
                errorDetected = true;
                return;
            }
            throw UnexpectedEndOfLine("while parsing string", last.pos);
        }
        if(c == '\\')
        {
            fr->getNextChar();
        }
    }
    last.length = fr->getLoc().offset - startOff + 1;
}

void ZLexer::matchRawString()
{
    char c;
    uint32_t startOff = fr->getLoc().offset;
    while((c = fr->getNextChar()) != '\'')
    {
        if(fr->isEof())
        {
            if(termOnUnknown)
            {
                fr->setLoc(last.pos);
                last.tt = tEol;
                errorDetected = true;
                return;
            }
            throw UnexpectedEndOfLine("while parsing string", last.pos);
        }
        if(c == '\\')
        {
            fr->getNextChar();
        }
    }
    last.length = fr->getLoc().offset - startOff + 1;
}

void ZLexer::matchNumArg()
{
    last.tt = tNumArg;
    char c;
    if(!((c = fr->peek()) >= '0' && c <= '9'))
    {
        if(termOnUnknown)
        {
            fr->setLoc(last.pos);
            last.tt = tEol;
            errorDetected = true;
            return;
        }
        throw UnexpectedSymbol(c, fr->getLoc());
    }
    while((c = fr->peek()) >= '0' && c <= '9')
    {
        fr->getNextChar();
        last.length++;
    }
}

void ZLexer::matchRegExp()
{
    char c;
    while((c = fr->getNextChar()) != '`')
    {
        last.length++;
        if(fr->isEof())
        {
            if(termOnUnknown)
            {
                fr->setLoc(last.pos);
                last.tt = tEol;
                errorDetected = true;
                return;
            }
            throw UnexpectedEndOfLine("while parsing regexp", last.pos);
        }
        if(c == '\\')
        {
            fr->getNextChar();
            last.length++;
        }
    }
    last.length++;
    while(isalnum(fr->peek()))
    {
        fr->getNextChar();
        last.length++;
    }
}

void ZLexer::matchMacroExpand()
{
    if(macroExpander)
    {
        macroExpander->expandMacro();
    }
}


}
