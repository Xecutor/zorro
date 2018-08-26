#ifndef __ZORRO_LEXERBASE_HPP__
#define __ZORRO_LEXERBASE_HPP__

#include "FileReader.hpp"
#include "Exceptions.hpp"
#include <string>

namespace zorro {

struct Term {
    int tt;
    unsigned int length;
    FileLocation pos;

    Term() : tt(-1), length(0u)
    {
    }

    Term(int argTt) : tt(argTt), length(0u)
    {
    }

    Term(int argTt, const FileLocation& argPos) : tt(argTt), length(0u), pos(argPos)
    {
    }

    bool operator<(const Term& argOther) const
    {
        return tt < argOther.tt;
    }

    bool operator==(int argTt) const
    {
        return tt == argTt;
    }

    bool operator!=(int argTt) const
    {
        return tt != argTt;
    }

    bool operator==(const Term& argOther) const
    {
        return tt == argOther.tt;
    }

    bool operator!=(const Term& argOther) const
    {
        return tt != argOther.tt;
    }

    const char* getValue(unsigned int& len) const
    {
        if(pos.fileRd)
        {
            len = length;
            return pos.fileRd->getBufferAt(pos.offset);
        }
        return nullptr;
    }

    void getValue(std::string& val) const
    {
        if(pos.fileRd)
        {
            val.assign(pos.fileRd->getBufferAt(pos.offset), length);
        }
    }

    std::string getValue() const
    {
        std::string rv;
        getValue(rv);
        return rv;
    }
};


template<class LexerImpl, typename TermType>
struct LexerBase {

    FileReader* fr;
    Term next, last;
    bool haveNext;

    bool termOnUnknown;
    bool errorDetected;


    typedef void (LexerImpl::*MatchMethod)();

    struct LexInfo {
        TermType tt;
        LexInfo* next;
        MatchMethod mm;
        bool ignore;
        bool used;

        LexInfo(MatchMethod argMm = 0) : tt(LexerImpl::EofTerm), next(0), mm(argMm), ignore(true), used(false)
        {
        }

        LexInfo(TermType argTt) : tt(argTt), next(0), mm(0), ignore(false), used(true)
        {
        }

        LexInfo(TermType argTt, MatchMethod argMm) : tt(argTt), next(0), mm(argMm), ignore(false), used(true)
        {
        }

        ~LexInfo()
        {
            if(next)
            {
                delete[] next;
            }
        }
    };


    LexInfo info[256];


    void reg(const char* termStr, TermType tt, MatchMethod mm = 0)
    {
        unsigned char* t = (unsigned char*) termStr;
        LexInfo* li = info + *t++;
        while(*t)
        {
            if(!li->next)
            {
                li->next = new LexInfo[256];
            }
            li = li->next + *t++;
        }
        li->tt = tt;
        li->mm = mm;
        li->ignore = false;
        li->used = true;
    }

    void reg(const char* termStr, MatchMethod mm = 0)
    {
        unsigned char* t = (unsigned char*) termStr;
        LexInfo* li = info + *t++;
        while(*t)
        {
            if(!li->next)
            {
                li->next = new LexInfo[256];
            }
            li = li->next + *t++;
        }
        li->mm = mm;
        li->ignore = true;
        li->used = true;
    }

    void reg(char termChar, TermType tt, MatchMethod mm = 0)
    {
        LexInfo* li = info + (unsigned char) termChar;
        li->tt = tt;
        li->mm = mm;
        li->ignore = false;
        li->used = true;
    }

    void reg(char termChar, MatchMethod mm = 0)
    {
        LexInfo* li = info + (unsigned char) termChar;
        li->mm = mm;
        li->ignore = true;
        li->used = true;
    }

    LexerBase() : fr(0), haveNext(false), termOnUnknown(false), errorDetected(false)
    {
    }

    void pushReader(FileReader* argFr, FileLocation loc)
    {
        if(haveNext)
        {
            fr->setLoc(next.pos);
            haveNext = false;
        }
        argFr->setParent(loc);
        fr = argFr;
    }

    void popReader()
    {
        fr = fr->getParent().fileRd;
        haveNext = false;
    }

    void reset()
    {
        haveNext = false;
    }

    Term& getNext()
    {
        if(haveNext)
        {
            haveNext = false;
            return last = next;
        }
        char c;
        for(;;)
        {
            if(fr->isEof())
            {
                if(fr->getParent().fileRd)
                {
                    popReader();
                    continue;
                }
                if(last.tt != LexerImpl::EolTerm && last.tt != LexerImpl::EofTerm)
                {
                    last = Term(LexerImpl::EolTerm, fr->getLoc());
                    return last;
                }
                last = Term(LexerImpl::EofTerm, fr->getLoc());
                return last;
            }
            last.pos = fr->getLoc();
            last.length = 1;
            LexInfo* li = info + (unsigned char) (c = fr->getNextChar());
            if(!li->used)
            {
                if(termOnUnknown)
                {
                    last.tt = LexerImpl::UnknownTerm;
                    return last;
                }
                throw UnexpectedSymbol(c, last.pos);
            }
            while(li->next && li->next[(unsigned char) fr->peek()].used)
            {
                li = li->next + (unsigned char) fr->getNextChar();
                last.length++;
            }
            if(li->ignore)
            {
                if(li->mm)
                {
                    (((LexerImpl*) this)->*(li->mm))();
                    if(last.tt == LexerImpl::UnknownTerm || errorDetected)
                    {
                        return last;
                    }
                }
                continue;
            }
            last.tt = li->tt;
            if(li->mm)
            {
                (((LexerImpl*) this)->*(li->mm))();
            }
            return last;
        }
//    return last;
    }

    Term& peekNext()
    {
        if(haveNext)
        {
            return next;
        }
        next = getNext();
        haveNext = true;
        return next;
    }

    FileLocation getLoc()
    {
        if(haveNext)
        {
            return next.pos;
        }
        return fr->getLoc();
    }

    void setLoc(FileLocation loc)
    {
        if(fr != loc.fileRd)
        {
            fr->setLoc(loc.fileRd->getParent());
            DPRINT("setLoc:%s\n", fr->getLoc().backTrace().c_str());
            fr = loc.fileRd;
        }
        fr->setLoc(loc);
        haveNext = false;
        last.tt = LexerImpl::MaxTermId;
    }
};

}

#endif
