/*
  $Id: RegExp.hpp,v 1.6 2012/06/25 10:50:27 skv Exp $
  Copyright (c) Konstantin Stupnik (aka Xecutor) 2000-2001 <skv@eyeline.mobi>
  You can use, modify, distribute this code or any other part
  only with permissions of author or Eyeline Communications!

  Regular expressions support library.
  Syntax and semantics of regexps very close to
  syntax and semantics of perl regexps.
*/

#ifndef __KST_REGEXP_HPP__
#define __KST_REGEXP_HPP__

#ifndef __cplusplus
#error "RegExp.hpp is for C++ only"
#endif

#include <wchar.h>
#include <memory.h>
#include <stddef.h>

namespace kst{


//! Possible compile and runtime errors returned by LastError.
enum REError{
  //! No errors
  errNone=0,
  //! RegExp wasn't even tried to compile
  errNotCompiled,
  //! expression contain syntax error
  errSyntax,
  //! Unbalanced brackets
  errBrackets,
  //! Max recursive brackets level reached. Controled in compile time
  errMaxDepth,
  //! Invalid options combination
  errOptions,
  //! Reference to nonexistent bracket
  errInvalidBackRef,
  //! Invalid escape char
  errInvalidEscape,
  //! Invalid range value
  errInvalidRange,
  //! Quantifier applyed to invalid object. f.e. lookahed assertion
  errInvalidQuantifiersCombination,
  //! Size of match array isn't large enough.
  errNotEnoughMatches,
  //! Attempt to match RegExp with Named Brackets, and no storage class provided.
  errNoStorageForNB,
  //! Reference to undefined named bracket
  errReferenceToUndefinedNamedBracket,
  //! Only fixed length look behind assertions are supported
  errVariableLengthLookBehind
};

//! Used internally
struct REOpCode;
//! Used internally
typedef REOpCode *PREOpCode;

//! Max brackets depth can be redefined in compile time
#ifndef MAXDEPTH
static const int MAXDEPTH=256;
#endif

/**
  \defgroup options Regular expression compile time options
*/
/*@{*/
//! Match in a case insensetive manner
static const int OP_IGNORECASE   =0x0001;
//! Single line mode, dot metacharacter will match newline symbol
static const int OP_SINGLELINE   =0x0002;
//! Multiline mode, ^ and $ can match line start and line end
static const int OP_MULTILINE    =0x0004;
//! Extended syntax, spaces symbols are ignored unless escaped
static const int OP_XTENDEDSYNTAX=0x0008;
//! Perl style regexp provided. i.e. /expression/imsx
static const int OP_PERLSTYLE    =0x0010;
//! Optimize after compile
static const int OP_OPTIMIZE     =0x0020;
//! Strict escapes - only unrecognized escape will prodce errInvalidEscape error
static const int OP_STRICT       =0x0040;
//! Replace backslash with slash, used
//! when regexp source embeded in c++ sources
// useless
//static const int OP_CPPMODE      =0x0080;
/*@}*/


/**
 \defgroup localebits Locale Info bits

*/
/*@{*/
//! Digits
static const unsigned char TYPE_DIGITCHAR  =0x01;
//! space, newlines tab etc
static const unsigned char TYPE_SPACECHAR  =0x02;
//! alphanumeric and _
static const unsigned char TYPE_WORDCHAR   =0x03;
//! lowcase symbol
static const unsigned char TYPE_LOWCASE    =0x04;
//! upcase symbol
static const unsigned char TYPE_UPCASE     =0x05;
//! letter
static const unsigned char TYPE_ALPHACHAR  =0x06;
/*@}*/


//! default preallocated stack size, and stack page size
#ifndef STACK_PAGE_SIZE
static const int STACK_PAGE_SIZE=16;
#endif

//! Structure that contain single bracket info
struct SMatch{
  int start,end;
};

typedef unsigned short rechar_t;

struct NamedMatch{
  const rechar_t* name;
  SMatch info;
  NamedMatch(){}
  NamedMatch(const rechar_t* argName,SMatch argInfo):name(argName),info(argInfo){}
  template <typename INPUT_ITER>
  bool checkName(INPUT_ITER argName)
  {
    const rechar_t* ptr=name;
    while(*argName && *argName==*ptr)
    {
      ++argName;
      ++ptr;
    }
    return !*argName && !*ptr;
  }
};

struct MatchInfo{
  SMatch* br = nullptr;
  size_t brSize = 0;
  size_t brCount = 0;
  NamedMatch* nm = nullptr;
  size_t nmSize = 0;
  size_t nmCount = 0;
  MatchInfo()=default;
  MatchInfo(SMatch* argBr,size_t argBrSize,NamedMatch* argNm=0,size_t argNmSize=0):
    br(argBr),brSize(argBrSize),brCount(0), nm(argNm),nmSize(argNmSize),nmCount(0){}
  template <size_t N>
  MatchInfo(SMatch (&argBr)[N]):br(argBr),brSize(N){}
  template <size_t N,size_t M>
  MatchInfo(SMatch (&argBr)[N],NamedMatch (&argNm)[M]):br(argBr),brSize(N),nm(argNm),nmSize(M){}

  template <typename INPUT_ITER>
  SMatch* hasNamedMatch(INPUT_ITER argName)
  {
    for(NamedMatch* it=nm,*end=nm+nmCount;it!=end;++it)
    {
      if(it->checkName(argName))
      {
        return &it->info;
      }
    }
    return 0;
  }
};

class RegExp;

template <size_t B=10,size_t N=5>
struct MatchInfoN:MatchInfo{
  SMatch mArr[B];
  NamedMatch nmArr[N];
  MatchInfoN(const RegExp& re);
  ~MatchInfoN()
  {
    if(br!=mArr)delete [] br;
    if(nm!=nmArr)delete [] nm;
  }
protected:
  MatchInfoN(const MatchInfoN&);
  void operator=(const MatchInfo&);
};

//! Used internally
template <typename INPUT_ITER>
struct StateStackItem{
  int op;
  REOpCode* pos;
  INPUT_ITER savestr;
  INPUT_ITER startstr;
  int min;
  int cnt;
  int max;
  int forward;
};


template <typename INPUT_ITER>
struct StateStack{
  enum{PAGE_SIZE=128};
  struct StateStackPage{
    StateStackItem<INPUT_ITER> stack[PAGE_SIZE];
    StateStackPage* prev;
    StateStackPage* next;
    int count;
    StateStackPage()
    {
      prev=next=0;
      count=0;
    }
  };
  StateStackPage firstPage;
  StateStackPage* curPage;
  StateStack()
  {
    curPage=&firstPage;
  }
  ~StateStack()
  {
    StateStackPage* ptr=firstPage.next;
    while(ptr)
    {
      curPage=ptr->next;
      delete ptr;
      ptr=curPage;
    }
  }
  StateStackItem<INPUT_ITER>* pushState()
  {
    ++curPage->count;
    if(curPage->count==PAGE_SIZE)
    {
      if(curPage->next)
      {
        curPage=curPage->next;
      }else
      {
        StateStackPage* newPage=new StateStackPage;
        curPage->next=newPage;
        newPage->prev=curPage;
        curPage=newPage;
      }
    }
    return curPage->stack+curPage->count;
  }
  StateStackItem<INPUT_ITER>* popState()
  {
    if(curPage->count==0)
    {
      curPage=curPage->prev;
      if(!curPage)return 0;
    }
    return curPage->stack+--curPage->count;
  }
  StateStackItem<INPUT_ITER>* getTop()
  {
    StateStackPage* ptr=curPage;
    if(ptr->count==0)
    {
      ptr=ptr->prev;
      if(!ptr)return 0;
    }
    return ptr->stack+ptr->count-1;
  }
  inline StateStackItem<INPUT_ITER>* findByPos(PREOpCode pos,int op);
};

struct UniSet;

/*! Regular expressions support class.

Expressions must be Compile'ed first,
and than Match string or Search for matching fragment.
*/
class RegExp{
private:
  // code
  PREOpCode code = nullptr;

  UniSet *firstptr = nullptr;
  UniSet *lastptr = nullptr;

  PREOpCode firstStr = nullptr;

  bool haveFirst = false;
  bool haveLast = false;
  bool haveLookAhead = false;

  size_t minLength = 0;

  // error info
  int errorCode = 0;
  ptrdiff_t errorPos = 0;

  // options
  bool ignoreCase = false;

  size_t bracketsCount = 0;
  size_t maxBackRef = 0;
  size_t namedBracketsCount = 0;
  const rechar_t** names = nullptr;

  template <typename INPUT_ITER>
  inline size_t calcLength(INPUT_ITER src,INPUT_ITER end);
  template <typename INPUT_ITER>
  inline bool innerCompile(INPUT_ITER start,INPUT_ITER end,int options);

  template <typename INPUT_ITER>
  inline bool innerMatch(INPUT_ITER start,INPUT_ITER str,INPUT_ITER end,MatchInfo* mi);

  size_t SetErrorSz(int argCode,ptrdiff_t argPos)
  {
    errorCode=argCode;
    errorPos=argPos;
    return 0;
  }

  bool SetError(int argCode,ptrdiff_t argPos)
  {
    errorCode=argCode;
    errorPos=argPos;
    return false;
  }

  template <typename INPUT_ITER>
  static inline int parseNum(INPUT_ITER& src);

  //void PushState();
  //StateStackItem* GetState();
  //StateStackItem* FindStateByPos(PREOpCode pos,int op);
  //int PopState();


  template <typename INPUT_ITER>
  inline int strCmp(INPUT_ITER& str,INPUT_ITER strend,INPUT_ITER start,INPUT_ITER end);

  template <typename INPUT_ITER>
  inline void init(INPUT_ITER,int options);
  RegExp(const RegExp& re)=delete;

  static inline int CalcPatternLength(PREOpCode from,PREOpCode to);

  template <typename INPUT_ITER>
  static inline bool BMSearch(PREOpCode sstr,INPUT_ITER& str,INPUT_ITER end);

public:
  //! Default constructor.
  inline RegExp();
  /*! Create object with compiled expression

     \param expr - source of expression
     \param options - compilation options

     By default expression in perl style expected,
     and will be optimized after compilation.

     Compilation status can be verified with LastError method.
     \sa LastError
  */
  template <typename INPUT_ITER>
  inline RegExp(INPUT_ITER expr,int options=OP_PERLSTYLE|OP_OPTIMIZE);
  virtual ~RegExp();

  /*! Compile regular expression
      Generate internall op-codes of expression.

      \param src - source of expression
      \param options - compile options
      \return 1 on success, 0 otherwise

      If compilation fails error code can be obtained with LastError function,
      position of error in a expression can be obtained with ErrorPosition function.
      See error codes in REError enumeration.
      \sa LastError
      \sa REError
      \sa ErrorPosition
      \sa options
  */
  template <typename INPUT_ITER>
  inline bool Compile(INPUT_ITER src,int options=OP_PERLSTYLE|OP_OPTIMIZE);
  template <typename INPUT_ITER>
  inline bool CompileEx(INPUT_ITER src,INPUT_ITER end,int options=OP_PERLSTYLE|OP_OPTIMIZE,rechar_t delim='/');
  /*! Try to optimize regular expression
      Significally speedup Search mode in some cases.
      \return 1 on success, 0 if optimization failed.
  */
  inline bool Optimize();

  /*! Try to match string with regular expression
      \param textstart - start of string to match
      \param textend - point to symbol after last symbols of the string.
      \param match - array of SMatch structures that receive brackets positions.
      \param matchcount - in/out parameter that indicate number of items in
      match array on input, and number of brackets on output.
      \param hmatch - storage of named brackets if NAMEDBRACKETS feature enabled.
      \return 1 on success, 0 if match failed.
      \sa SMatch
  */
  template <typename INPUT_ITER>
  inline bool Match(INPUT_ITER textstart,INPUT_ITER textend,MatchInfo& mi);
  /*! Same as Match(const char* textstart,const char* textend,...), but for ASCIIZ string.
      textend calculated automatically.
  */
  template <typename INPUT_ITER>
  inline bool Match(INPUT_ITER textstart,MatchInfo& match);
  /*! Advanced version of match. Can be used for multiple matches
      on one string (to imitate /g modifier of perl regexp
  */
  template <typename INPUT_ITER>
  inline bool MatchEx(INPUT_ITER datastart,INPUT_ITER textstart,INPUT_ITER textend,MatchInfo& mi);
  /*! Try to find substring that will match regexp.
      Parameters and return value are the same as for Match.
      It is highly recommended to call Optimize before Search.
  */
  template <typename INPUT_ITER>
  inline bool Search(INPUT_ITER textstart,INPUT_ITER textend,MatchInfo& mi);
  /*! Same as Search with specified textend, but for ASCIIZ strings only.
      textend calculated automatically.
  */
  template <typename INPUT_ITER>
  inline bool Search(INPUT_ITER textstart,MatchInfo& mi);
  /*! Advanced version of search. Can be used for multiple searches
      on one string (to imitate /g modifier of perl regexp
  */
  template <typename INPUT_ITER>
  bool SearchEx(INPUT_ITER datastart,INPUT_ITER textstart,INPUT_ITER textend,MatchInfo& mi);

  /*! Get last error
      \return code of the last error
      Check REError for explanation
      \sa REError
      \sa ErrorPosition
  */
  int getLastError() const {return errorCode;}
  /*! Get last error position.
      \return position of the last error in the regexp source.
      \sa LastError
  */
  ptrdiff_t getErrorPosition() const {return errorPos;}
  /*! Get number of brackets in expression
      \return number of brackets, excluding brackets of type (:expr)
      and named brackets.
  */
  size_t getBracketsCount() const {return bracketsCount;}
  /*! Get number of named brackets in expression
      \return number of named brackets.
  */
  size_t getNamedBracketsCount() const {return namedBracketsCount;}
  const rechar_t* getName(size_t idx)const
  {
    if(idx>=namedBracketsCount)return 0;
    return names[idx];
  }
};


}

#include "RegExp_impl.hpp"


#endif
