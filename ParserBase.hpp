#ifndef __ZORRO_PARSERBASE_HPP__
#define __ZORRO_PARSERBASE_HPP__

#include <memory.h>
#include <string>
#include <vector>
#include <list>
#include <new>
#include <assert.h>
#include "LexerBase.hpp"
#include "Exceptions.hpp"
#include "Debug.hpp"

namespace zorro {


template<class ImplType, class Lexer>
class ParserBase {
public:
    struct Rule;

    struct RuleEntry {
        bool isTerm;
        bool isPushed;
        bool isReturnOnError;
        bool isPrioReset;
        bool isPointOfNoReturn;
        bool isOptional;
        union {
            int t;
            Rule* nt;
        };

        RuleEntry() :
                isTerm(true), isPushed(true), isReturnOnError(false), isPrioReset(false), isPointOfNoReturn(false),
                isOptional(false), t(0)
        {
        }

        RuleEntry(int argT) :
                isTerm(true), isPushed(true), isReturnOnError(false), isPrioReset(false), isPointOfNoReturn(false),
                isOptional(false), t(argT)
        {
        }

        RuleEntry(Rule& argNt) :
                isTerm(false), isPushed(true), isReturnOnError(false), isPrioReset(false), isPointOfNoReturn(false),
                isOptional(false), nt(&argNt)
        {
        }

        RuleEntry(Rule* argNt) :
                isTerm(false), isPushed(true), isReturnOnError(false), isPrioReset(false), isPointOfNoReturn(false),
                isOptional(false), nt(argNt)
        {
        }

        RuleEntry(const RuleEntry& argRe) : isTerm(argRe.isTerm), isPushed(argRe.isPushed),
                                            isReturnOnError(argRe.isReturnOnError), isPrioReset(argRe.isPrioReset),
                                            isPointOfNoReturn(argRe.isPointOfNoReturn), isOptional(argRe.isOptional)
        {
            if(isTerm)
            {
                t = argRe.t;
            }
            else
            {
                nt = argRe.nt;
            }
        }
    };


    struct StackItem {
        ~StackItem()
        {
            if(owned)
            {
                destroy();
            }
        }

        StackItem() : ptr(0), destructor(0), owned(false), isPtr(false)
        {
        }

        union {
            void* ptr;
            char buf[ImplType::MaxDataSize];
        };

        void (* destructor)(void*, char*);

        RuleEntry re;
        bool owned;
        bool isPtr;

        template<class T>
        struct DHelper {
            static void destroy(void* vptr, char* cptr)
            {
                ((T*) cptr)->~T();
                (void) vptr;
                (void) cptr;
            }
        };

        template<class T>
        struct DHelper<T*> {
            static void destroy(void* vptr, char* /*cptr*/)
            {
                delete (T*) vptr;
            }
        };

        template<class T>
        void assign(const T& t)
        {
            static_assert(sizeof(t) <= ImplType::MaxDataSize, "Data too big for a buffer");
            new(buf)T(t);
            destructor = DHelper<T>::destroy;
            owned = true;
            isPtr = false;
        }

        template<class T>
        void assign(T* t)
        {
            ptr = (void*) t;
            destructor = DHelper<T*>::destroy;
            owned = true;
            isPtr = true;
        }

        void release()
        {
            owned = false;
        }

        void destroy()
        {
            if(owned)
            {
                destructor(ptr, buf);
                owned = false;
            }
        }

        template<class T>
        struct GetHelper {
            static T& getVal(void* /*aptr*/, char* abuf)
            {
                return *((T*) abuf);
            }
        };

        template<class T>
        struct GetHelper<T*> {
            static T*& getVal(void*& aptr, char* /*abuf*/)
            {
                return (T*&) aptr;
            }
        };

        template<class T>
        T& as()
        {
            return GetHelper<T>::getVal(ptr, buf);
        }
    };

    struct DataStack {
        StackItem* data;
        StackItem* last;
        StackItem* dataEnd;

        DataStack()
        {
            data = (StackItem*) new char[sizeof(StackItem) * 32];
            memset(data, 0, sizeof(StackItem) * 32);
            dataEnd = data + 32;
            last = 0;
        }

        ~DataStack()
        {
            shrink(0);
            delete[] (char*) data;
        }

        template<class T>
        void push_back(T item, RuleEntry re)
        {
            if(!last)
            {
                last = data;
            }
            else
            {
                last++;
            }
            if(last == dataEnd)
            {
                size_t oldSz = static_cast<size_t>(dataEnd - data);
                StackItem* newData = (StackItem*) new char[sizeof(StackItem) * (oldSz + 32)];
                memmove(newData, data, oldSz * sizeof(StackItem));
                memset(newData + oldSz, 0, sizeof(StackItem) * 32);
                delete[] (char*) data;
                data = newData;
                dataEnd = data + oldSz + 32;
                last = data + oldSz;
            }
            last->assign(item);
            last->re = re;
        }

        template<class T>
        void replace(int count, T item, RuleEntry re)
        {
            for(int i = 0; i < count; i++)
            {
                last->release();
                last--;
            }
            last++;
            last->assign(item);
            last->re = re;
        }

        void shrink(size_t sz)
        {
            if(!last)
            {
                return;
            }
            size_t count = static_cast<size_t>(1 + (last - data) - sz);
            for(size_t i = 0; i < count; i++)
            {
                last->destroy();
                last--;
            }
            if(last < data)
            {
                last = 0;
            }
        }

        size_t size()
        {
            if(!last)
            {
                return 0;
            }
            return static_cast<size_t>(1 + (last - data));
        }
    };

    struct RuleHandlerBase {
        virtual ~RuleHandlerBase()
        {
        }

        virtual void handleRule(DataStack& stack, RuleEntry re) = 0;
    };

    enum ListMode {
        lmNone,
        lmZeroOrMore,
        lmOneOrMore,
        lmZeroOrMoreTrail,
        lmOneOrMoreTrail
    };

    struct Sequence {
        std::string name;
        int prio;
        bool left;
        bool binary;
        int alt;
        int compatibleAlt;
        std::vector<RuleEntry> seq;
        RuleHandlerBase* handler;

        Sequence(const std::string& argName) :
                name(argName), prio(0), left(true), binary(false), alt(-1), compatibleAlt(-1), handler(0)
        {
        }

        Sequence(const std::string& argName, int argPrio, bool argLeft = true) :
                name(argName), prio(argPrio), left(argLeft), binary(true), alt(-1), compatibleAlt(-1), handler(0)
        {
        }

        Sequence(const Sequence& argOther) :
                name(argOther.name), prio(argOther.prio), left(argOther.left), binary(argOther.binary),
                alt(argOther.alt), compatibleAlt(argOther.compatibleAlt), seq(argOther.seq), handler(argOther.handler)
        {
            const_cast<Sequence&>(argOther).handler = 0;
        }

        ~Sequence()
        {
            if(handler)
            {
                delete handler;
            }
        }

        RuleEntry& add(const RuleEntry& re)
        {
            seq.push_back(re);
            return seq.back();
        }
    };

    typedef std::map<Term, int> TermMap;
    typedef std::vector<Sequence> SeqVector;


    struct Rule {
        int id;
        std::string name;
        //TermMap startTerms;
        int startTerms[Lexer::MaxTermId];
        int binopSeq[Lexer::MaxTermId];
        SeqVector seqs;
        ListMode listMode;
        int emptySeq;
        bool defined;
        bool prepared;
        bool preparing;

        Rule() : id(-1), listMode(lmNone), emptySeq(-1), defined(false), prepared(false), preparing(false)
        {
            for(int i = 0; i < Lexer::MaxTermId; i++)
            {
                startTerms[i] = -1;
                binopSeq[i] = -1;
            }
        }

        Sequence* findSeq(const std::string& argName)
        {
            for(typename SeqVector::iterator it = seqs.begin(), end = seqs.end(); it != end; ++it)
            {
                if(it->name == argName)
                {
                    return &*it;
                }
            }
            return 0;
        }

        void setTermSeq(int t, int idx)
        {
            startTerms[t] = idx;
        }

        int getTermSeq(Term t)
        {
            return startTerms[t.tt];
        }
    };

#define ARG(n, arg_type) stack.last[n].template as<arg_type>()

    template<class P, class RV>
    struct RuleHandler0 : RuleHandlerBase {
        P* p;

        RV (P::*method)();

        RuleHandler0(P* argP, RV (P::*argMethod)()) : p(argP), method(argMethod)
        {
        }

        void handleRule(DataStack& stack, RuleEntry re)
        {
            stack.push_back((p->*method)(), re);
        }
    };

    template<class P, class RV, class A1>
    struct RuleHandler1 : RuleHandlerBase {
        P* p;

        RV (P::*method)(A1);

        RuleHandler1(P* argP, RV (P::*argMethod)(A1)) : p(argP), method(argMethod)
        {
        }

        void handleRule(DataStack& stack, RuleEntry re)
        {
            stack.replace(1, (p->*method)(ARG(0, A1)), re);
        }
    };

    template<class P, class RV, class A1, class A2>
    struct RuleHandler2 : RuleHandlerBase {
        P* p;

        RV (P::*method)(A1, A2);

        RuleHandler2(P* argP, RV (P::*argMethod)(A1, A2)) : p(argP), method(argMethod)
        {
        }

        void handleRule(DataStack& stack, RuleEntry re)
        {
            stack.replace(2, (p->*method)(ARG(-1, A1), ARG(0, A2)), re);
        }
    };

    template<class P, class RV, class A1, class A2, class A3>
    struct RuleHandler3 : RuleHandlerBase {
        P* p;

        RV (P::*method)(A1, A2, A3);

        RuleHandler3(P* argP, RV (P::*argMethod)(A1, A2, A3)) : p(argP), method(argMethod)
        {
        }

        void handleRule(DataStack& stack, RuleEntry re)
        {
            stack.replace(3, (p->*method)(ARG(-2, A1), ARG(-1, A2), ARG(0, A3)), re);
        }
    };

    template<class P, class RV, class A1, class A2, class A3, class A4>
    struct RuleHandler4 : RuleHandlerBase {
        P* p;

        RV (P::*method)(A1, A2, A3, A4);

        RuleHandler4(P* argP, RV (P::*argMethod)(A1, A2, A3, A4)) : p(argP), method(argMethod)
        {
        }

        void handleRule(DataStack& stack, RuleEntry re)
        {
            stack.replace(4, (p->*method)(ARG(-3, A1), ARG(-2, A2), ARG(-1, A3), ARG(0, A4)), re);
        }
    };

    template<class P, class RV, class A1, class A2, class A3, class A4, class A5>
    struct RuleHandler5 : RuleHandlerBase {
        P* p;

        RV (P::*method)(A1, A2, A3, A4, A5);

        RuleHandler5(P* argP, RV (P::*argMethod)(A1, A2, A3, A4, A5)) : p(argP), method(argMethod)
        {
        }

        void handleRule(DataStack& stack, RuleEntry re)
        {
            stack.replace(5, (p->*method)(ARG(-4, A1), ARG(-3, A2), ARG(-2, A3), ARG(-1, A4), ARG(0, A5)), re);
        }
    };

    template<class P, class RV, class A1, class A2, class A3, class A4, class A5, class A6>
    struct RuleHandler6 : RuleHandlerBase {
        P* p;

        RV (P::*method)(A1, A2, A3, A4, A5, A6);

        RuleHandler6(P* argP, RV (P::*argMethod)(A1, A2, A3, A4, A5, A6)) : p(argP), method(argMethod)
        {
        }

        void handleRule(DataStack& stack, RuleEntry re)
        {
            stack.replace(6, (p->*method)(ARG(-5, A1), ARG(-4, A2), ARG(-3, A3), ARG(-2, A4), ARG(-1, A5), ARG(0, A6)),
                    re);
        }
    };

    template<class P, class RV, class A1, class A2, class A3, class A4, class A5, class A6, class A7>
    struct RuleHandler7 : RuleHandlerBase {
        P* p;

        RV (P::*method)(A1, A2, A3, A4, A5, A6, A7);

        RuleHandler7(P* argP, RV (P::*argMethod)(A1, A2, A3, A4, A5, A6, A7)) : p(argP), method(argMethod)
        {
        }

        void handleRule(DataStack& stack, RuleEntry re)
        {
            stack.replace(7, (p->*method)(ARG(-6, A1), ARG(-5, A2), ARG(-4, A3), ARG(-3, A4), ARG(-2, A5), ARG(-1, A6),
                    ARG(0, A7)), re);
        }
    };

#undef ARG


    Lexer l;
    typedef std::map<std::string, int> TermByNameMap;
    TermByNameMap termMap;
    typedef std::map<std::string, Rule> RuleMap;
    RuleMap ruleMap;
    int ruleSeq;

    std::list<Rule*> rulesList;

    Rule& getRule(const std::string& name)
    {
        typename RuleMap::iterator it = ruleMap.find(name);
        if(it != ruleMap.end())
        {
            return it->second;
        }
        Rule& rv = ruleMap[name];
        rulesList.push_back(&rv);
        rv.name = name;
        return rv;
    }

    void pushReader(FileReader* fr, FileLocation parentLoc = FileLocation())
    {
        l.pushReader(fr, parentLoc);
    }

    struct SeqInfo {
        const char* name;
        const char* rule;
        int prio;
        bool isBinRule;
        bool isLeft;

        SeqInfo(const char* argName, const char* argRule) :
                name(argName), rule(argRule), prio(0), isBinRule(false), isLeft(false)
        {
        }

        SeqInfo(const char* argName, const char* argRule, int argPrio, bool argIsLeft = true) :
                name(argName), rule(argRule), prio(argPrio), isBinRule(true), isLeft(argIsLeft)
        {
        }
    };

    void add(const SeqInfo& rule)
    {
        parseRule(rule, 0);
    }

    template<class RV>
    void add0(const SeqInfo& rule, RV (ImplType::*method)())
    {
        parseRule(rule, 0).handler = new RuleHandler0<ImplType, RV>((ImplType*) this, method);
    }

    template<class RV, class A1>
    void add1(const SeqInfo& rule, RV (ImplType::*method)(A1))
    {
        parseRule(rule, 1).handler = new RuleHandler1<ImplType, RV, A1>((ImplType*) this, method);
    }

    template<class RV, class A1, class A2>
    void add2(const SeqInfo& rule, RV (ImplType::*method)(A1, A2))
    {
        parseRule(rule, 2).handler = new RuleHandler2<ImplType, RV, A1, A2>((ImplType*) this, method);
    }

    template<class RV, class A1, class A2, class A3>
    void add3(const SeqInfo& rule, RV (ImplType::*method)(A1, A2, A3))
    {
        parseRule(rule, 3).handler = new RuleHandler3<ImplType, RV, A1, A2, A3>((ImplType*) this, method);
    }

    template<class RV, class A1, class A2, class A3, class A4>
    void add4(const SeqInfo& rule, RV (ImplType::*method)(A1, A2, A3, A4))
    {
        parseRule(rule, 4).handler = new RuleHandler4<ImplType, RV, A1, A2, A3, A4>((ImplType*) this, method);
    }

    template<class RV, class A1, class A2, class A3, class A4, class A5>
    void add5(const SeqInfo& rule, RV (ImplType::*method)(A1, A2, A3, A4, A5))
    {
        parseRule(rule, 5).handler = new RuleHandler5<ImplType, RV, A1, A2, A3, A4, A5>((ImplType*) this, method);
    }

    template<class RV, class A1, class A2, class A3, class A4, class A5, class A6>
    void add6(const SeqInfo& rule, RV (ImplType::*method)(A1, A2, A3, A4, A5, A6))
    {
        parseRule(rule, 6).handler = new RuleHandler6<ImplType, RV, A1, A2, A3, A4, A5, A6>((ImplType*) this, method);
    }

    template<class RV, class A1, class A2, class A3, class A4, class A5, class A6, class A7>
    void add7(const SeqInfo& rule, RV (ImplType::*method)(A1, A2, A3, A4, A5, A6, A7))
    {
        parseRule(rule, 7).handler = new RuleHandler7<ImplType, RV, A1, A2, A3, A4, A5, A6, A7>((ImplType*) this,
                method);
    }

    bool getNext(const char*& rule, std::string& buf)
    {
        buf.clear();
        while(*rule && !isalnum(*rule))
        {
            rule++;
        }
        if(!*rule)
        {
            return false;
        }
        while(isalnum(*rule))
        {
            buf += *rule++;
        }
        return true;
    }

    bool checkFlag(const char*& str, char flag)
    {
        if(*str == flag)
        {
            str++;
            return true;
        }
        return false;
    }

    Sequence& parseRule(const SeqInfo& seq, int args)
    {
        const char* rule = seq.rule;
        std::string str;
        if(!getNext(rule, str))
        {
            //invalid rule;
            throw InvalidRule(seq.rule);
        }
        Rule& r = getRule(str);
        if(!r.defined)
        {
            r.name = str;
            r.defined = true;
            r.id = ruleSeq++;
        }
        if(seq.isBinRule)
        {
            r.seqs.push_back(Sequence(seq.name, seq.prio, seq.isLeft));
        }
        else
        {
            r.seqs.push_back(Sequence(seq.name));
        }
        Sequence& s = r.seqs.back();
        bool pushed = true;
        bool returnOnError = false;
        bool prioReset = false;
        bool pointOfNoReturn = false;
        bool optional = false;
        int acnt = 0;
        while(getNext(rule, str))
        {
            optional = checkFlag(rule, '?');
            pushed = !checkFlag(rule, '-') && !optional;
            returnOnError = checkFlag(rule, '.');
            prioReset = checkFlag(rule, '!');
            pointOfNoReturn = checkFlag(rule, '^');

            if(pushed)
            {
                acnt++;
            }

            TermByNameMap::iterator it = termMap.find(str);
            RuleEntry* e;
            if(it == termMap.end())
            {
                e = &s.add(RuleEntry(getRule(str)));
            }
            else
            {
                e = &s.add(RuleEntry(it->second));
            }
            e->isPushed = pushed;
            e->isReturnOnError = returnOnError;
            e->isPrioReset = prioReset;
            e->isPointOfNoReturn = pointOfNoReturn;
            e->isOptional = optional;

        }
        if(acnt != args)
        {
            throw std::runtime_error(std::string("arguments number mismatch for seq ") + seq.name);
        }
        return s;
    }

    ParserBase() : ruleSeq(0), startRule(0)
    {
    }

    void prepare(const char* startRuleName)
    {
        //for(RuleMap::iterator it=ruleMap.begin(),end=ruleMap.end();it!=end;++it)
        for(typename std::list<Rule*>::iterator it = rulesList.begin(), end = rulesList.end(); it != end; ++it)
        {
            Rule& r = **it;//it->second;
            if(!r.defined)
            {
                DPRINT("rule not defined:%s\n", r.name.c_str());
                throw RuleUndefined(r.name.c_str());
            }
            if(!r.prepared)
            {
                prepareRule(r);
            }
            //for(TermMap::iterator it=r.startTerms.begin(),end=r.startTerms.end();it!=end;++it)
            for(int i = 0; i < Lexer::MaxTermId; i++)
            {
                if(r.startTerms[i] != -1)
                {
                    DPRINT("start term for %s:%s\n", r.name.c_str(), Lexer::getTermName(i));
                }
            }
        }
        startRule = &getRule(startRuleName);
        if(!startRule->defined)
        {
            DPRINT("start rule not defined:%s\n", startRuleName);
            throw RuleUndefined(startRuleName);
        }
    }

    void prepareRule(Rule& r)
    {
        if(r.preparing)
        {
            return;
        }
        r.preparing = true;
        for(size_t idx = 0; idx < r.seqs.size(); ++idx)
        {
            Sequence& s = r.seqs[idx];
            DPRINT("preparing %s:%s\n", r.name.c_str(), s.name.c_str());
            if(s.seq.empty())
            {
                r.emptySeq = static_cast<int>(idx);
                continue;
            }
            if((s.binary || r.listMode != lmNone) && s.seq.size() >= 2 && s.seq[1].isTerm &&
               !s.seq.front().isTerm && s.seq.front().nt->id == r.id)
            {
                r.binopSeq[s.seq[1].t] = static_cast<int>(idx);
            }
            RuleEntry& e = s.seq.front();
            if(e.isTerm)
            {
                if(r.startTerms[e.t] == -1)
                {
                    r.startTerms[e.t] = static_cast<int>(idx);
                    DPRINT("%s is starting with %s\n", s.name.c_str(), Lexer::getTermName(e.t));
                }
                else
                {
                    int aidx = r.startTerms[e.t];
                    Sequence& as = r.seqs[static_cast<size_t>(aidx)];
                    s.alt = aidx;
                    s.compatibleAlt = -1;
                    r.startTerms[e.t] = static_cast<int>(idx);
                    for(size_t i = 0; i < as.seq.size(); ++i)
                    {
                        if(i >= s.seq.size())
                        {
                            DPRINT("fail T1 at %d\n", (int) i);
                            break;
                        }
                        if(s.seq[i].isTerm != as.seq[i].isTerm)
                        {
                            DPRINT("fail T2 at %d\n", (int) i);
                            break;
                        }
                        if(s.seq[i].isTerm)
                        {
                            if(s.seq[i].t != as.seq[i].t)
                            {
                                DPRINT("marked seq %s as compatible alt of %s at %d\n", s.name.c_str(), as.name.c_str(),
                                        (int) i);
                                s.compatibleAlt = static_cast<int>(i);
                                break;
                            }
                            continue;
                        }
                        if(s.seq[i].nt != as.seq[i].nt)
                        {
                            DPRINT("marked seq %s as compatible alt of %s at %d\n", s.name.c_str(), as.name.c_str(),
                                    (int) i);
                            s.compatibleAlt = static_cast<int>(i);
                            break;
                        }
                    }
                }
            }
            else
            {
                if(e.nt->id != r.id)
                {
                    if(!e.nt->prepared)
                    {
                        prepareRule(*e.nt);
                    }
                    for(int i = 0; i < Lexer::MaxTermId; i++)
                    {
                        if(e.nt->startTerms[i] != -1)
                        {
                            if(r.startTerms[i] == -1)
                            {
                                r.startTerms[i] = static_cast<int>(idx);
                            }
                            else
                            {
                                int aidx = r.startTerms[i];
                                Sequence& as = r.seqs[static_cast<size_t>(aidx)];
                                s.alt = aidx;
                                s.compatibleAlt = -1;
                                r.startTerms[i] = static_cast<int>(idx);
                                bool fail = false;
                                DPRINT("compare %s and %s\n", s.name.c_str(), as.name.c_str());
                                for(size_t j = 0; j < as.seq.size(); ++j)
                                {
                                    if(j >= s.seq.size())
                                    {
                                        //DPRINT("fail NT1 at %d\n",(int)j);
                                        DPRINT("alt NT1 at %d\n", (int) j);
                                        s.compatibleAlt = static_cast<int>(j);
                                        break;
                                    }
                                    if(s.seq[j].isTerm != as.seq[j].isTerm)
                                    {
                                        DPRINT("fail NT2 at %d\n", (int) j);
                                        fail = true;
                                        break;
                                    }
                                    if(s.seq[j].isTerm)
                                    {
                                        if(s.seq[j].t != as.seq[j].t)
                                        {
                                            DPRINT("marked seq %s as compatible alt of %s at %d\n", s.name.c_str(),
                                                    as.name.c_str(), (int) j);
                                            s.compatibleAlt = static_cast<int>(j);
                                            break;
                                        }
                                        continue;
                                    }
                                    if(s.seq[j].nt != as.seq[j].nt)
                                    {
                                        DPRINT("marked seq %s as compatible alt of %s at %d\n", s.name.c_str(),
                                                as.name.c_str(), (int) j);
                                        s.compatibleAlt = static_cast<int>(j);
                                        break;
                                    }
                                }
                                if(!fail && s.seq.size() > as.seq.size())
                                {
                                    s.compatibleAlt = static_cast<int>(s.seq.size());
                                    DPRINT("alt NT3 at end\n");
                                }
                            }
                        }
                    }
                }
            }
        }
        r.prepared = true;
    }

    void dumpstack()
    {
#ifdef DEBUG
        DPRINT("stack dump:\n");
        for(StackItem* i = stack.data; i <= stack.last; i++)
        {
            RuleEntry& re = i->re;
            if(re.isTerm)
            {
                DPRINT("t:%s\n", Lexer::getTermName(re.t));
            }
            else
            {
                DPRINT("nt:%s\n", re.nt->name.c_str());
            }
        }
#endif
    }


    struct CallFrame {
        Rule* r;
        int idx;
        int pos;
        int saveIdx, savePos;
        bool failed;
        bool resetPrio;
        bool canRecurse;
        bool returnOnError;
        bool pointOfNoReturn;
        Term saveLast;
        FileLocation callLoc, start, end;
        size_t dataStackSize;
        //CallFrame(){}
        //CallFrame(Rule* argR,size_t argDataStackSize,FileLocation argCallLoc,bool argResetPrio):r(argR),idx(-1),pos(0),failed(false),resetPrio(argResetPrio),canRecurse(true),callLoc(argCallLoc),dataStackSize(argDataStackSize){}
    };
    //typedef std::vector<CallFrame> CallStack;

    struct CallStack {
        CallFrame* data;
        CallFrame* last;
        CallFrame* dataEnd;

        CallStack()
        {
            data = new CallFrame[32];
            dataEnd = data + 32;
            last = 0;
        }

        ~CallStack()
        {
            delete[] data;
        }

        void push_back(Rule* argR, size_t argDataStackSize, FileLocation argCallLoc, bool resetPrio, bool returnOnError,
                       bool pointOfNoReturn)
        {
            if(!last)
            {
                last = data;
            }
            else
            {
                last++;
            }
            if(last == dataEnd)
            {
                size_t oldSz = static_cast<size_t>(dataEnd - data);
                CallFrame* newData = new CallFrame[oldSz + 32];
                memmove(newData, data, oldSz * sizeof(CallFrame));
                delete[] data;
                data = newData;
                dataEnd = data + oldSz + 32;
                last = data + oldSz;
            }
            last->r = argR;
            last->dataStackSize = argDataStackSize;
            last->callLoc = argCallLoc;
            last->resetPrio = resetPrio;
            last->failed = false;
            last->canRecurse = true;
            last->pointOfNoReturn = pointOfNoReturn;
            last->returnOnError = returnOnError;
            last->idx = -1;
            last->pos = 0;
        }

        void pop_back()
        {
            last--;
            if(last < data)
            {
                last = 0;
            }
        }

        size_t size()
        {
            if(last)
            {
                return static_cast<size_t>(last - data + 1);
            }
            return 0;
        }

        bool empty()
        {
            return !last;
        }

        void clear()
        {
            last = 0;
        }
    };

    Rule* startRule;
    DataStack stack;
    CallStack callStack;
    Term lastFailMatch;
    Term lastOkMatch;

    void mkCall(Rule* r, bool resetPrio, bool returnOnError, bool pointOfNoReturn)
    {
        DPRINT("mkcall %s\n", r->name.c_str());
        callStack.push_back(r, stack.size(), l.getLoc(), resetPrio, returnOnError, pointOfNoReturn);
    }

    void rollbackCall()
    {
        DPRINT("call rollback\n");
        CallFrame& cf = *callStack.last;
        stack.shrink(cf.dataStackSize);
        l.setLoc(cf.callLoc);
        callStack.pop_back();
    }

    void restartCall(int idx)
    {
        DPRINT("call restart\n");
        CallFrame& cf = *callStack.last;
        stack.shrink(cf.dataStackSize);
        l.setLoc(cf.callLoc);
        cf.idx = idx;
        cf.pos = 0;
        cf.failed = false;
    }

    void reset()
    {
        stack.shrink(0);
        callStack.clear();
        l.reset();
    }

    template<class RvType>
    RvType parseRule(Rule* r)
    {
        size_t callLevel = callStack.size();
        size_t dataLevel = stack.size();
        mkCall(r, true, false, false);

        Term t = l.peekNext();
        Term saveLastFailMatch = lastFailMatch;
        lastFailMatch = t;
        while(callStack.size() > callLevel)
        {
            if(parseStep(t))
            {
                t = l.peekNext();
            }
        }
        if(callStack.last->failed)
        {
            throw SyntaxErrorException("syntax error", lastFailMatch.pos);
        }
        lastFailMatch = saveLastFailMatch;
        RvType rv = stack.last->template as<RvType>();
        stack.last->owned = false;
        stack.shrink(dataLevel);
        return rv;

    }

    void parse()
    {
        callStack.clear();
        stack.shrink(0);
        mkCall(startRule, false, false, false);
        Term t = l.peekNext();
        while(t.tt != Lexer::EofTerm || !callStack.empty())
        {
            if(parseStep(t))
            {
                t = l.peekNext();
            }
        }
    }

    bool parseStep(Term t)
    {
        DPRINT("next term is %s(%s) at %s\n", Lexer::getTermName(t), t.getValue().c_str(), t.pos.backTrace().c_str());
        if(callStack.empty())
        {
            DPRINT("callstack empty, parsing failed at %s\n", t.pos.backTrace().c_str());
            if(lastFailMatch.pos.fileRd)
            {
                throw SyntaxErrorException("syntax error", lastFailMatch.pos);
            }
            else
            {
                throw SyntaxErrorException("syntax error", t.pos);
            }
        }
        CallFrame& cf = *callStack.last;
        Rule& r = *cf.r;
        if(lastFailMatch.pos.offset < t.pos.offset)
        {
            lastFailMatch = t;
        }
        if(cf.idx == -1)//new call
        {
            cf.idx = r.getTermSeq(t);
            if(cf.idx == -1)
            {
                if(r.emptySeq >= 0)
                {
                    DPRINT("match failed, but there is empty rule:%s\n",
                            r.seqs[static_cast<size_t>(r.emptySeq)].name.c_str());
                    rollbackCall();
                    if(r.seqs[static_cast<size_t>(r.emptySeq)].handler)
                    {
                        r.seqs[static_cast<size_t>(r.emptySeq)].handler->handleRule(stack, &r);
                    }
                    return true;
                }

                DPRINT("parsing failed, unexpected %s\n", Lexer::getTermName(t));
                if(callStack.size() > 1)
                {
                    DPRINT("returning failure\n");
                    rollbackCall();
                    if(!callStack.empty())
                    {
                        callStack.last->failed = true;
                    }
                    return true;
                }
                char msg[256];
                if(lastFailMatch.pos.fileRd)
                {
                    snprintf(msg, sizeof(msg), "%s unexpected", Lexer::getTermName(lastFailMatch));
                    throw SyntaxErrorException(msg, lastFailMatch.pos);
                }
                else
                {
                    snprintf(msg, sizeof(msg), "%s unexpected", Lexer::getTermName(t));
                    throw SyntaxErrorException(msg, t.pos);
                }
            }
            DPRINT("selected seq:%s\n", r.seqs[static_cast<size_t>(cf.idx)].name.c_str());
            if(r.seqs[static_cast<size_t>(cf.idx)].alt != -1)
            {
                DPRINT("seq have alt %s\n",
                        r.seqs[static_cast<size_t>(r.seqs[static_cast<size_t>(cf.idx)].alt)].name.c_str());
            }
        }
        else
        {
            Sequence& s = r.seqs[static_cast<size_t>(cf.idx)];
            //return from call, or advance on term rule
            if(cf.failed)
            {
                if(r.listMode == lmNone)
                {
                    if(s.alt != -1)
                    {
                        DPRINT("match of %s failed, switching to alt:%s\n", s.name.c_str(),
                                r.seqs[static_cast<size_t>(s.alt)].name.c_str());
                        restartCall(s.alt);
                        return true;
                    }
                    if(cf.returnOnError)
                    {
                        rollbackCall();
                        callStack.last->failed = true;
                        return true;
                    }
                    if(cf.pos < (int) s.seq.size())
                    {
                        RuleEntry& re = s.seq[static_cast<size_t>(cf.pos)];
                        if(re.isTerm)
                        {
                            DPRINT("parsing failed, expected %s\n", Lexer::getTermName(re.t));
                        }
                        else
                        {
                            DPRINT("parsing failed, expected %s\n", re.nt->name.c_str());
                        }
                    }
                    char msg[256];
                    snprintf(msg, sizeof(msg), "Parsing of %s failed", s.name.c_str());
                    throw SyntaxErrorException(msg, t.pos);
                }
                else
                {
                    DPRINT("list rule %s failed, returning\n", r.name.c_str());
                    if(r.listMode == lmZeroOrMoreTrail && r.emptySeq >= 0 && stack.last && !stack.last->re.isTerm &&
                       stack.last->re.nt != &r)
                    {
                        r.seqs[static_cast<size_t>(r.emptySeq)].handler->handleRule(stack, &r);
                    }
                    if(r.listMode == lmOneOrMoreTrail || r.listMode == lmZeroOrMoreTrail)
                    {
                        callStack.pop_back();
                    }
                    else
                    {
                        rollbackCall();
                    }
                    return true;
                }
            }
        }
        Sequence& s = r.seqs[static_cast<size_t>(cf.idx)];

        if((int) s.seq.size() == cf.pos)//sequence fully matched
        {
            DPRINT("seq matched:%s\n", s.name.c_str());
            //dumpstack();
            if(s.handler)
            {
                s.handler->handleRule(stack, &r);//reduce
            }
            //DPRINT("after handler:\n");
            //dumpstack();


            if(s.binary)//matching binary op rules
            {
                DPRINT("binary rule, checking recursion\n");
                int oldPrio = 0;
                bool recursion = false;
                int newPrio = 0;
                if(callStack.size() > 1 && !cf.resetPrio)
                {
                    CallFrame& pcf = callStack.last[-1];
                    Rule* pr = pcf.r;
                    //if(pr->id==r.id)
                    if(pr->seqs[static_cast<size_t>(pcf.idx)].binary)
                    {
                        oldPrio = pr->seqs[static_cast<size_t>(pcf.idx)].prio;
                        DPRINT("old prio=%d\n", oldPrio);
                    }
                }
                /*
                  size_t nidx;
                  for(nidx=0;nidx<r.seqs.size();++nidx)
                  {
                    RuleEntry& re=r.seqs[nidx].seq.front();
                    if(!re.isTerm && re.nt->id==r.id)
                    {
                      if(t==r.seqs[nidx].seq[1].t)
                      {
                        recursion=true;
                        newPrio=r.seqs[nidx].prio;
                        DPRINT("recursion possible, new prio=%d\n",newPrio);
                        break;
                      }
                    }
                  }
                 */
                int nidx = r.binopSeq[t.tt];
                if(nidx != -1)
                {

                    recursion = true;
                    newPrio = r.seqs[static_cast<size_t>(nidx)].prio;
                    DPRINT("recursion possible, new prio=%d\n", newPrio);
                }
                if(recursion && cf.canRecurse)
                {
                    if(!(newPrio < oldPrio || (newPrio == oldPrio && r.seqs[static_cast<size_t>(nidx)].left)))
                    {
                        DPRINT("recurse loc=%s\n", l.getLoc().backTrace().c_str());
                        cf.callLoc = l.getLoc();
                        cf.dataStackSize = stack.size();
                        cf.saveIdx = cf.idx;
                        cf.saveLast = lastOkMatch;
                        cf.savePos = cf.pos;
                        cf.idx = nidx;
                        cf.pos = 1;
                        return false;
                    }
                    else
                    {
                        DPRINT("old rule has higher prio\n");
                    }
                }
            }
            else if(r.listMode != lmNone)
            {
                DPRINT("checking list rule for recursion\n");
                bool found = false;
                if(r.binopSeq[t.tt] != -1)
                {
                    found = true;
                    cf.idx = r.binopSeq[t.tt];
                    cf.pos = 1;
                }
                else
                {
                    for(size_t idx = 0; idx < r.seqs.size(); ++idx)
                    {
                        Sequence& ss = r.seqs[idx];
                        if(ss.seq.size() < 2)
                        {
                            continue;
                        }
                        RuleEntry& re = ss.seq.front();
                        if(!re.isTerm && !stack.last[0].re.isTerm && re.nt == stack.last[0].re.nt)
                        {
                            if(ss.seq[1].isTerm)
                            {
                                if(t == ss.seq[1].t)
                                {
                                    DPRINT("found %s\n", ss.name.c_str());
                                    cf.idx = static_cast<int>(idx);
                                    cf.pos = 1;
                                    found = true;
                                    break;
                                }
                            }
                            else
                            {
                                if(ss.seq[1].nt->getTermSeq(t) != -1)
                                {
                                    DPRINT("found %s\n", ss.name.c_str());
                                    cf.idx = static_cast<int>(idx);
                                    cf.pos = 1;
                                    found = true;
                                    break;
                                }
                            }
                        }
                    }
                }
                if(found)
                {
                    return false;
                }
            }

            DPRINT("return from %s\n", r.name.c_str());
            callStack.pop_back();
            return false;//return to previous frame
        }

        if(s.seq[static_cast<size_t>(cf.pos)].isTerm)
        {
            if(t == s.seq[static_cast<size_t>(cf.pos)].t)
            {
                if(cf.pos == 0)
                {
                    cf.start = t.pos;
                }
                DPRINT("match term:%s\n", Lexer::getTermName(t));
                lastOkMatch = l.getNext();
                if(s.seq[static_cast<size_t>(cf.pos)].isOptional)
                {
                    DPRINT("skipped optional:%s\n", Lexer::getTermName(t));
                    return true;
                }
                if(s.seq[static_cast<size_t>(cf.pos)].isPushed)
                {
                    stack.push_back(t, s.seq[static_cast<size_t>(cf.pos)]);
                }
                cf.pointOfNoReturn = cf.pointOfNoReturn || s.seq[static_cast<size_t>(cf.pos)].isPointOfNoReturn;
                cf.pos++;
                if(cf.pos == (int) s.seq.size())
                {
                    cf.end = t.pos;
                }
                return true;
            }
            else
            {
                if(s.seq[static_cast<size_t>(cf.pos)].isOptional)
                {
                    cf.pos++;
                    return false;
                }
                if(cf.pointOfNoReturn)
                {
                    throw SyntaxErrorException("syntax error", t.pos);
                }
                if(s.alt != -1)
                {
                    DPRINT("match of %s failed, switching to alt:%s\n", s.name.c_str(),
                            r.seqs[static_cast<size_t>(s.alt)].name.c_str());
                    if(s.compatibleAlt == cf.pos)
                    {
                        DPRINT("alt is at compatible position, switching without frame restart\n");
                        cf.idx = s.alt;
                    }
                    else
                    {
                        restartCall(s.alt);
                    }
                    return true;
                }
                DPRINT("term %s match failed pos %d of %s\n", Lexer::getTermName(t), cf.pos, s.name.c_str());
                //abort();
                if(cf.returnOnError)
                {
                    DPRINT("return on error\n");
                    cf.idx = cf.saveIdx;
                    cf.pos = cf.savePos;
                    cf.canRecurse = false;
                    lastOkMatch = cf.saveLast;
                    l.setLoc(cf.callLoc);
                    stack.shrink(cf.dataStackSize);
                    callStack.pop_back();
                    dumpstack();
                    return true;
                }

                rollbackCall();
                if(!callStack.empty())
                {
                    callStack.last->failed = true;
                }
                return true;
            }
        }
        else
        {

            int pos = cf.pos++;
            mkCall(s.seq[static_cast<size_t>(pos)].nt, s.seq[static_cast<size_t>(pos)].isPrioReset,
                    cf.returnOnError || s.seq[static_cast<size_t>(pos)].isReturnOnError,
                    false/*cf.pointOfNoReturn || s.seq[pos].isPointOfNoReturn*/);
        }
        return false;
    }
};

}

#endif
