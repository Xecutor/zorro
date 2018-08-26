/*
  $Id: RegExp_impl.hpp,v 1.8 2013/08/12 11:22:30 skv Exp $
  Copyright (c) Konstantin Stupnik (aka Xecutor) 2001 <skv@eyeline.mobi>
  You can use, modify, distribute this code or any other part
  only with permissions of author or Eyeline Communications!

  Regular expressions support library.
  Syntax and semantics of regexps very close to
  syntax and semantics of perl regexps.

*/

#include <vector>
#include <string.h>

#ifdef __GNUC__
#include <wctype.h>
#else
#include <wchar.h>
#endif

namespace kst{


template <size_t B,size_t N>
MatchInfoN<B,N>::MatchInfoN(const RegExp& re):MatchInfo(mArr,nmArr)
{
  if(re.getBracketsCount()>B)
  {
    br=new SMatch[re.getBracketsCount()];
  }
  if(re.getNamedBracketsCount()>N)
  {
    nm=new NamedMatch[re.getNamedBracketsCount()];
  }
}

#define RE_ISDIGIT(c) (iswdigit(c) != 0)
#define RE_ISSPACE(c) (iswspace(c) != 0)
#define RE_ISWORD(c)  (iswalnum(c) != 0 || (c)=='_')
#define RE_ISLOWER(c) (iswlower(c) != 0)
#define RE_ISUPPER(c) (iswupper(c) != 0)
#define RE_ISALPHA(c) (iswalpha(c) != 0)
#define RE_TOUPPER(c) towupper(c)
#define RE_TOLOWER(c) towlower(c)

#define RE_ISTYPE(c,t) isType(c,t)

inline bool isType(rechar_t chr,int type)
{
  switch(type)
  {
    case TYPE_DIGITCHAR:return RE_ISDIGIT(chr);
    case TYPE_SPACECHAR:return RE_ISSPACE(chr);
    case TYPE_WORDCHAR: return RE_ISWORD(chr);
    case TYPE_LOWCASE:  return RE_ISLOWER(chr);
    case TYPE_UPCASE:   return RE_ISUPPER(chr);
    case TYPE_ALPHACHAR:return RE_ISALPHA(chr);
  }
  return false;
}

/*template <typename INPUT_ITER>
inline int reStrLen(INPUT_ITER str)
{
  INPUT_ITER end=str;
  while(*end)++end;
  return (int)(end-str);
}*/

template <typename INPUT_ITER>
inline INPUT_ITER reStrEnd(INPUT_ITER str)
{
  INPUT_ITER end=str;
  while(*end)++end;
  return end;
}

inline const char* reStrEnd(const char* str)
{
  return str+strlen(str);
}



/*inline int reStrLen(const char* str)
{
  return (int)strlen(str);
}*/


struct UniSet{
//protected:
  unsigned char* high[256];
  unsigned char types[6];
  unsigned char typesCount;
  unsigned char notTypes[6];
  unsigned char notTypesCount;
  bool negative;
public:
  UniSet()
  {
    memset(high,0,sizeof(high));
    typesCount=0;
    notTypesCount=0;
    negative=0;
  }
  UniSet(const UniSet& src)
  {
    for(int i=0;i<256;i++)
    {
      if(src.high[i])
      {
        high[i]=new unsigned char[32];
        memcpy(high[i],src.high[i],32);
      }else
      {
        high[i]=NULL;
      }
    }
    typesCount=src.typesCount;
    notTypesCount=src.notTypesCount;
    for(int i=0;i<typesCount;++i)types[i]=src.types[i];
    for(int i=0;i<notTypesCount;++i)notTypes[i]=src.notTypes[i];
    negative=src.negative;
  }
  UniSet& operator=(const UniSet& src)
  {
    for(int i=0;i<256;i++)
    {
      if(src.high[i])
      {
        if(!high[i])high[i]=new unsigned char[32];
        memcpy(high[i],src.high[i],32);
      }else
      {
        if(high[i])delete [] high[i];
        high[i]=NULL;
      }
    }
    typesCount=src.typesCount;
    notTypesCount=src.notTypesCount;
    for(int i=0;i<typesCount;++i)types[i]=src.types[i];
    for(int i=0;i<notTypesCount;++i)notTypes[i]=src.notTypes[i];
    negative=src.negative;
    return (*this);
  }

  void setNegative()
  {
    negative=true;
  }

  UniSet& operator+=(const UniSet& src)
  {
    if((src.negative && !negative) || (!src.negative && negative))
    {
      for(size_t i=0;i<256;i++)
      {
        if(src.high[i])
        {
          if(!high[i])
          {
            high[i]=new unsigned char[32];
            for(size_t j=0;j<32;j++)high[i][j]=static_cast<unsigned char>(~src.high[i][j]);
          }else
          {
            for(size_t j=0;j<32;j++)high[i][j]|=static_cast<unsigned char>(~src.high[i][j]);
          }
        }else
        {
          if(!high[i])high[i]=new unsigned char[32];
          memset(high[i],0xff,32);
        }
      }
      for(int i=0;i<src.typesCount;++i)addNotType(src.types[i]);
      for(int i=0;i<src.notTypesCount;++i)addType(src.notTypes[i]);
    }else
    {
      for(int i=0;i<256;i++)
      {
        if(src.high[i])
        {
          if(!high[i])
          {
            high[i]=new unsigned char[32];
            memcpy(high[i],src.high[i],32);
          }else
          {
            for(int j=0;j<32;j++)high[i][j]|=src.high[i][j];
          }
        }
      }
      for(int i=0;i<src.typesCount;++i)addType(src.types[i]);
      for(int i=0;i<src.notTypesCount;++i)addNotType(src.notTypes[i]);
    }
    return (*this);
  }


  void Reset()
  {
    for(int i=0;i<256;i++)
    {
      if(high[i])
      {
        delete [] high[i];
        high[i]=0;
      }
    }
    typesCount=0;
    notTypesCount=0;
    negative=false;
  }
/*
  struct Setter{
    UniSet& set;
    rechar_t idx;
    Setter(UniSet& s,rechar_t chr):set(s),idx(chr)
    {
    }
    void operator=(int val)
    {
      if(val)set.SetBit(idx);
      else set.ClearBit(idx);
    }
    bool operator!()const
    {
      return !set.GetBit(idx);
    }
  };

  const bool operator[](rechar_t idx)const
  {
    return GetBit(idx);
  }
  Setter operator[](rechar_t idx)
  {
    return Setter(*this,idx);
  }*/
  ~UniSet()
  {
    for(int i=0;i<256;i++)
    {
      if(high[i])delete [] high[i];
    }
  }
  void addType(uint8_t type)
  {
    for(int i=0;i<typesCount;++i)
    {
      if(types[i]==type)return;
    }
    types[typesCount++]=type;
  }
  void addNotType(uint8_t notType)
  {
    for(int i=0;i<notTypesCount;++i)
    {
      if(notTypes[i]==notType)return;
    }
    notTypes[notTypesCount++]=notType;
  }
  bool GetBit(rechar_t chr)const
  {
    for(size_t i=0;i<typesCount;++i)
    {
      if(isType(chr,types[i]))
      {
        return !negative;
      }
    }
    for(size_t i=0;i<notTypesCount;++i)
    {
      if(!isType(chr,notTypes[i]))
      {
        return !negative;
      }
    }
    unsigned char h=static_cast<unsigned char>((chr&0xff00)>>8);
    if(!high[h])return negative;
    if(high[h][(chr&0xff)>>3]&(1<<(chr&7)))
    {
      return !negative;
    }else
    {
      return negative;
    }
  }
  void SetBit(rechar_t  chr)
  {
    unsigned char h=static_cast<unsigned char>((chr&0xff00)>>8);
    if(!high[h])
    {
      high[h]=new unsigned char[32];
      memset(high[h],0,32);
    }
    high[h][(chr&0xff)>>3]|=1<<(chr&7);
  }
  void ClearBit(rechar_t  chr)
  {
    unsigned char h=static_cast<unsigned char>((chr&0xff00)>>8);
    if(!high[h])
    {
      high[h]=new unsigned char[32];
      memset(high[h],0,32);
    }
    high[h][(chr&0xff)>>3]&=~(1<<(chr&7));
  }

};


enum REOp{
  opLineStart=0x1,        // ^
  opLineEnd,              // $
  opDataStart,            // \A and ^ in single line mode
  opDataEnd,              // \Z and $ in signle line mode

  opWordBound,            // \b
  opNotWordBound,         // \B

  opType,                 // \d\s\w\l\u\e
  opNotType,              // \D\S\W\L\U\E

  opCharAny,              // .
  opCharAnyAll,           // . in single line mode

  opString,               // sequence of characters
  opStringIgnoreCase,     // sequence of characters

  opSymbol,               // single char
  opNotSymbol,            // [^c] negative charclass with one char
  opSymbolIgnoreCase,     // symbol with IGNORE_CASE turned on
  opNotSymbolIgnoreCase,  // [^c] with ignore case set.

  opSymbolClass,          // [chars]

  opOpenBracket,          // (

  opClosingBracket,       // )

  opAlternative,          // |

  opBackRef,              // \1

  opNamedBracket,         // (?{name}
  opNamedBackRef,         // \p{name}


  opRangesBegin,          // for op type check

  opRange,                // generic range
  opMinRange,             // generic minimizing range

  opSymbolRange,          // quantifier applied to single char
  opSymbolMinRange,       // minimizing quantifier

  opNotSymbolRange,       // [^x]
  opNotSymbolMinRange,

  opAnyRange,             // .
  opAnyMinRange,

  opTypeRange,            // \w, \d, \s
  opTypeMinRange,

  opNotTypeRange,         // \W, \D, \S
  opNotTypeMinRange,

  opClassRange,           // for char classes
  opClassMinRange,

  opBracketRange,         // for brackets
  opBracketMinRange,

  opBackRefRange,         // for backrefs
  opBackRefMinRange,

  opNamedRefRange,
  opNamedRefMinRange,

  opRangesEnd,            // end of ranges

  opAssertionsBegin,

  opLookAhead,
  opNotLookAhead,

  opLookBehind,
  opNotLookBehind,

  opAsserionsEnd,

  opNoReturn,

  opRegExpEnd
};

struct REOpCode{
  int op;
  REOpCode *next,*nextVal,*prev;
  REOpCode()
  {
    memset(this,0,sizeof(*this));
  }
  inline ~REOpCode();

  struct SBracket{
    REOpCode* nextalt;
    int index;
    REOpCode* pairindex;
  };

  struct SRange{
    union{
      SBracket bracket;
      int op;
      rechar_t symbol;
      UniSet *symbolclass;
      REOpCode* nextalt;
      int refindex;
      const rechar_t* refname;
      int type;
    };
    int min,max;
  };

  struct SNamedBracket{
    REOpCode* nextalt;
    const rechar_t* name;
    REOpCode* pairindex;
  };

  struct SAssert{
    REOpCode* nextalt;
    int length;
    REOpCode* pairindex;
  };

  struct SAlternative{
    REOpCode* nextalt;
    REOpCode* endindex;
  };

  struct String{
    rechar_t* str;
    size_t bmskip[128];
    size_t length;
  };

  union{
    SRange range;
    SBracket bracket;
    SNamedBracket nbracket;
    SAssert assert;
    SAlternative alternative;
    rechar_t symbol;
    UniSet *symbolclass;
    int refindex;
    const rechar_t* refname;
    String str;

    int type;
  };
};

inline REOpCode::~REOpCode()
{
  switch(op)
  {
    case opString:
    case opStringIgnoreCase:delete [] str.str;break;
    case opSymbolClass:delete symbolclass;break;
    case opClassRange:
    case opClassMinRange:delete range.symbolclass;break;
    case opNamedBracket:delete [] nbracket.name;break;
    case opNamedBackRef:delete [] refname;break;
  }
}


template <typename INPUT_ITER>
inline void RegExp::init(INPUT_ITER expr,int options)
{
  firstptr=new UniSet();
  lastptr=new UniSet();

  Compile(expr,options);
}

inline RegExp::RegExp()
{
  firstptr=new UniSet();
  lastptr=new UniSet();

  errorCode=errNotCompiled;
}

template <typename INPUT_ITER>
inline RegExp::RegExp(INPUT_ITER expr,int options)
{
  init(expr,options);
}

inline RegExp::~RegExp()
{
  if(code)
  {
    delete [] code;
  }
  if(names)
  {
    delete [] names;
  }
  delete firstptr;
  delete lastptr;
}

template <typename INPUT_ITER>
inline size_t RegExp::calcLength(INPUT_ITER src,INPUT_ITER end)
{
  size_t length=3;//global brackets
  INPUT_ITER brackets[MAXDEPTH];
  size_t count=0;
  INPUT_ITER save;
  bracketsCount=1;
  size_t inquote=0;
  INPUT_ITER start=src;

  for(;src!=end;++src,length++)
  {
    if(inquote && *src!='\\' && src[1]!='E')
    {
      continue;
    }
    if(*src=='\\')
    {
      ++src;
      if(*src=='Q')inquote=1;
      if(*src=='E')inquote=0;
      if(*src=='x')
      {
        ++src;
        int c=RE_TOLOWER(static_cast<rechar_t>(*src));
        if((c>='0' && c<='9')||(c>='a' && c<='f'))
        {
          int c2=RE_TOLOWER(static_cast<rechar_t>(src[1]));
          if((c2>='0' && c2<='9')||(c2>='a' && c2<='f'))++src;

        }else return SetErrorSz(errSyntax,src-start);
      }
      if(*src=='p')
      {
        ++src;
        if(*src!='{')
          return SetErrorSz(errSyntax,src-start);
        ++src;
        INPUT_ITER save2=src;
        while(src<end &&
            (RE_ISWORD(static_cast<rechar_t>(*src)) || RE_ISSPACE(static_cast<rechar_t>(*src))) && *src!='}')
        {
            ++src;
        }
        if(src>=end)
        {
            return SetErrorSz(errBrackets, save2 - start);
        }
        if(*src!='}' && !(RE_ISWORD(static_cast<rechar_t>(*src)) || RE_ISSPACE(static_cast<rechar_t>(*src))))
          return SetErrorSz(errSyntax,src-start);
      }
      continue;
    }
    switch(*src)
    {
      case '(':
      {
        brackets[count]=src;
        count++;
        if(count==MAXDEPTH)return SetErrorSz(errMaxDepth,src-start);
        if(src[1]=='?')
        {
          src+=2;
          if(*src=='{')
          {
            save=src;
            ++src;
            while(src<end && (RE_ISWORD(static_cast<rechar_t>(*src)) || RE_ISSPACE(static_cast<rechar_t>(*src))) &&
                  *src!=static_cast<rechar_t>('}'))++src;
            if(src>=end)
              return SetErrorSz(errBrackets,save-start);
            if(*src!=static_cast<rechar_t>('}') &&
                    (!(RE_ISWORD(static_cast<rechar_t>(*src))) || RE_ISSPACE(static_cast<rechar_t>(*src))))
            {
                return SetErrorSz(errSyntax, src - start);
            }
          }
        }else
        {
          bracketsCount++;
        }
        break;
      }
      case ')':
      {
        if(count==0)return SetErrorSz(errBrackets,src-start);
        count--;
        break;
      }
      case '{':
      case '*':
      case '+':
      case '?':
      {
        length--;
        if(*src=='{')
        {
          save=src;
          while(src<end && *src!='}')++src;
          if(src>=end)return SetErrorSz(errBrackets,save-start);
        }
        if(src[1]=='?')++src;
        break;
      }
      case '[':
      {
        save=src;
        while(src<end && *src!=']')++src;
        if(src>=end)return SetErrorSz(errBrackets,save-start);
        break;
      }
    }
  }
  if(count)
  {
    return SetErrorSz(errBrackets,brackets[0]-start);
  }
  return length;
}

template <typename INPUT_ITER>
inline bool RegExp::Compile(INPUT_ITER src,int options)
{
  INPUT_ITER end=reStrEnd(src);
  return CompileEx(src,end,options,'/');
}

template <typename INPUT_ITER>
inline bool RegExp::CompileEx(INPUT_ITER src,INPUT_ITER end,int options,rechar_t delim)
{
  INPUT_ITER srcstart=src,srcend=end;
  size_t relength;
  haveFirst=false;
  haveLast=false;
  firstStr=0;
  if(code)delete [] code;
  code=0;
  if(names)delete [] names;
  names=0;
  code=NULL;
  rechar_t c;
  if(options&OP_PERLSTYLE)
  {
    if(*src!=delim)return SetError(errSyntax,0);
    srcstart=src+1;
    srcend=srcstart;
    while((c=static_cast<rechar_t>(*srcend))!=0 && c!=delim)
    {
      if(*srcend=='\\')
      {
        ++srcend;
        if(*srcend!=0)++srcend;
      }else
      {
        ++srcend;
      }
    }
    if(!*srcend)
    {
      return SetError(errSyntax,srcend-src-1);
    }
    INPUT_ITER opt=srcend+1;
    while(opt!=end)
    {
      switch(*opt)
      {
        case 'i':options|=OP_IGNORECASE;break;
        case 's':options|=OP_SINGLELINE;break;
        case 'm':options|=OP_MULTILINE;break;
        case 'x':options|=OP_XTENDEDSYNTAX;break;
        case 'o':options|=OP_OPTIMIZE;break;
        default:return SetError(errOptions,opt-src);
      }
      ++opt;
    }
  }
  ignoreCase=options&OP_IGNORECASE?true:false;
  relength=calcLength(srcstart,srcend);
  if(relength==0)
  {
    return false;
  }
  code=new REOpCode[relength];
  memset(code,0,sizeof(REOpCode)*relength);
  for(size_t i=0;i<relength;i++)
  {
    code[i].next=i<relength-1?code+i+1:0;
    code[i].prev=i>0?code+i-1:0;
    code[i].nextVal=0;
  }
  bool result=innerCompile(srcstart,srcend,options);
  if(!result)
  {
    delete [] code;
    code=nullptr;
  }else
  {
    errorCode=errNone;
    minLength=0;
    if(options&OP_OPTIMIZE)Optimize();
  }
  return result;
}

template <typename INPUT_ITER>
inline int RegExp::parseNum(INPUT_ITER& src)
{
  int res=0;//atoi((const char*)src+i);
  while(RE_ISDIGIT(static_cast<rechar_t>(*src)))
  {
    res*=10;
    res+=*src-'0';
    ++src;
  }
  return res;
}

inline int RegExp::CalcPatternLength(PREOpCode from,PREOpCode to)
{
  int len=0;
  int altcnt=0;
  int altlen=-1;
  for(;from->prev!=to;from=from->next)
  {
    switch(from->op)
    {
      //zero width
      case opLineStart:
      case opLineEnd:
      case opDataStart:
      case opDataEnd:
      case opWordBound:
      case opNotWordBound:continue;

      case opType:
      case opNotType:

      case opCharAny:
      case opCharAnyAll:

      case opSymbol:
      case opNotSymbol:
      case opSymbolIgnoreCase:
      case opNotSymbolIgnoreCase:
      case opSymbolClass:
        len++;
        altcnt++;
        continue;

      case opString:
      case opStringIgnoreCase:
        len+=from->str.length;
        altlen+=from->str.length;
        continue;

      case opNamedBracket:
      case opOpenBracket:
      {
        int l=CalcPatternLength(from->next,from->bracket.pairindex->prev);
        if(l==-1)return -1;
        len+=l;
        altcnt+=l;
        from=from->bracket.pairindex;
        continue;
      }
      case opClosingBracket:
        break;

      case opAlternative:
        if(altlen!=-1 && altcnt!=altlen)return -1;
        altlen=altcnt;
        altcnt=0;
        continue;

      case opBackRef:
      case opNamedBackRef:
        return -1;



      case opRangesBegin:

      case opRange:
      case opMinRange:

      case opSymbolRange:
      case opSymbolMinRange:

      case opNotSymbolRange:
      case opNotSymbolMinRange:

      case opAnyRange:
      case opAnyMinRange:

      case opTypeRange:
      case opTypeMinRange:

      case opNotTypeRange:
      case opNotTypeMinRange:

      case opClassRange:
      case opClassMinRange:
        if(from->range.min!=from->range.max)return -1;
        len+=from->range.min;
        altcnt+=from->range.min;
        continue;

      case opBracketRange:
      case opBracketMinRange:
      {
        if(from->range.min!=from->range.max)return -1;
        int l=CalcPatternLength(from->next,from->bracket.pairindex->prev);
        if(l==-1)return -1;
        len+=from->range.min*l;
        altcnt+=from->range.min*l;
        from=from->bracket.pairindex;
        continue;
      }


      case opBackRefRange:
      case opBackRefMinRange:

      case opNamedRefRange:
      case opNamedRefMinRange:
        return -1;

      case opRangesEnd:

      case opAssertionsBegin:

      case opLookAhead:
      case opNotLookAhead:
      case opLookBehind:
      case opNotLookBehind:
        from=from->assert.pairindex;
        continue;

      case opAsserionsEnd:

      case opNoReturn:
        continue;

    }
  }
  if(altlen!=-1 && altlen!=altcnt)return -1;
  return altlen==-1?len:altlen;
}

template <typename INPUT_ITER>
inline bool RegExp::innerCompile(INPUT_ITER start,INPUT_ITER end,int options)
{
  int j;
  PREOpCode brackets[MAXDEPTH];
  // current brackets depth
  // one place reserved for surrounding 'main' brackets
  size_t brdepth=1;

  INPUT_ITER src=start;

  // compiling interior of lookbehind
  // used to apply restrictions of lookbehind
  size_t lookbehind=0;

  // counter of normal brackets
  size_t brcount=0;

  std::vector<NamedMatch> nm;

  // counter of closed brackets
  // used to check correctness of backreferences
  bool closedbrackets[MAXDEPTH];


  // quoting is active
  bool inquote=0;

  maxBackRef=0;

  UniSet *tmpclass;

  code->op=opOpenBracket;
  code->bracket.index=0;

  size_t pos=1;

  PREOpCode op;//=code;

  brackets[0]=code;

  haveLookAhead=0;
  for(;src<end;++src)
  {
    op=code+pos;
    pos++;
    if(inquote && *src!='\\')
    {
      op->op=ignoreCase?opSymbolIgnoreCase:opSymbol;
      op->symbol=static_cast<rechar_t>(ignoreCase?RE_TOLOWER(static_cast<rechar_t>(*src)):*src);
      if(ignoreCase && RE_TOUPPER(op->symbol)==op->symbol)op->op=opSymbol;
      continue;
    }
    if(*src=='\\')
    {
      ++src;
      if(inquote && *src!='E')
      {
        op->op=opSymbol;
        op->symbol='\\';
        op=code+pos;
        pos++;
        op->op=ignoreCase?opSymbolIgnoreCase:opSymbol;
        op->symbol=static_cast<rechar_t>(ignoreCase?RE_TOLOWER(static_cast<rechar_t>(*src)):*src);
        if(ignoreCase && RE_TOUPPER(op->symbol)==op->symbol)op->op=opSymbol;
        continue;
      }
      op->op=opType;
      switch(*src)
      {
        case 'Q':inquote=true;pos--;continue;
        case 'E':inquote=false;pos--;continue;

        case 'b':op->op=opWordBound;continue;
        case 'B':op->op=opNotWordBound;continue;
        case 'D':op->op=opNotType;
        /* no break */
        case 'd':op->type=TYPE_DIGITCHAR;continue;
        case 'S':op->op=opNotType;
        /* no break */
        case 's':op->type=TYPE_SPACECHAR;continue;
        case 'W':op->op=opNotType;
        /* no break */
        case 'w':op->type=TYPE_WORDCHAR;continue;
        case 'U':op->op=opNotType;
        /* no break */
        case 'u':op->type=TYPE_UPCASE;continue;
        case 'L':op->op=opNotType;
        /* no break */
        case 'l':op->type=TYPE_LOWCASE;continue;
        case 'I':op->op=opNotType;
        /* no break */
        case 'i':op->type=TYPE_ALPHACHAR;continue;
        case 'A':op->op=opDataStart;continue;
        case 'Z':op->op=opDataEnd;continue;
        case 'n':op->op=opSymbol;op->symbol='\n';continue;
        case 'r':op->op=opSymbol;op->symbol='\r';continue;
        case 't':op->op=opSymbol;op->symbol='\t';continue;
        case 'f':op->op=opSymbol;op->symbol='\f';continue;
        case 'e':op->op=opSymbol;op->symbol=27;continue;
        case 'O':op->op=opNoReturn;continue;
        case 'p':
        {
          op->op=opNamedBackRef;
          ++src;
          if(*src!='{')return SetError(errSyntax,src-start);
          size_t len=0;
          INPUT_ITER n=++src;
          while(*src!='}')++len,++src;
          if(len>0)
          {
            rechar_t* dst;
            op->refname=dst=new rechar_t[len+1];
            //memcpy(op->refname,src+i,len*sizeof(INPUT_ITER));

            for(INPUT_ITER it=n,nend=src;it!=nend;++it)
            {
              *dst++=static_cast<rechar_t>(*it);
            }
            *dst=0;
            bool found=false;
            for(auto& m :nm)
            {
              if(m.checkName(op->refname))
              {
                found=true;
                break;
              }
            }
            if(!found)
            {
              return SetError(errReferenceToUndefinedNamedBracket,src-start);
            }
          }else
          {
            return SetError(errSyntax,src-start);
          }
        }continue;
        case 'x':
        {
          int c=0;
          ++src;
          if(src>=end)return SetError(errSyntax,src-start-1);
          c=RE_TOLOWER(static_cast<rechar_t>(*src));
          if((c>='0' && c<='9') ||
             (c>='a' && c<='f'))
          {
            c-='0';
            if(c>9)c-='a'-'0'-10;
            op->op=ignoreCase?opSymbolIgnoreCase:opSymbol;
            op->symbol=static_cast<rechar_t>(c);
            if(src+1<end)
            {
              c=RE_TOLOWER(static_cast<rechar_t>(src[1]));
              if((c>='0' && c<='9') ||
                 (c>='a' && c<='f'))
              {
                ++src;
                c-='0';
                if(c>9)c-='a'-'0'-10;
                op->symbol<<=4;
                op->symbol|=c;
              }
              if(ignoreCase)
              {
                op->symbol=RE_TOLOWER(op->symbol);
                if(RE_TOUPPER(op->symbol)==RE_TOLOWER(op->symbol))
                {
                  op->op=opSymbol;
                }
              }

            }
          }else return SetError(errSyntax,src-start);
          continue;
        }
        default:
        {
          if(RE_ISDIGIT(static_cast<rechar_t>(*src)))
          {
            INPUT_ITER save=src;
            op->op=opBackRef;
            op->refindex=parseNum(src);--src;
            if(op->refindex<=0 || op->refindex>static_cast<int>(brcount) || !closedbrackets[op->refindex])
            {
              return SetError(errInvalidBackRef,save-start-1);
            }
            if(op->refindex>static_cast<int>(maxBackRef))maxBackRef=static_cast<size_t>(op->refindex);
          }else
          {
            if((options&OP_STRICT) && RE_ISALPHA(static_cast<rechar_t>(*src)))
            {
              return SetError(errInvalidEscape,src-start-1);
            }
            op->op=ignoreCase?opSymbolIgnoreCase:opSymbol;
            op->symbol=static_cast<rechar_t>(ignoreCase?RE_TOLOWER(static_cast<rechar_t>(*src)):*src);
            if(RE_TOLOWER(op->symbol)==RE_TOUPPER(op->symbol))
            {
              op->op=opSymbol;
            }
          }
        }break;
      }
      continue;
    }
    switch(*src)
    {
      case '.':
      {
        if(options&OP_SINGLELINE)
        {
          op->op=opCharAnyAll;
        }else
        {
          op->op=opCharAny;
        }
        continue;
      }
      case '^':
      {
        if(options&OP_MULTILINE)
        {
          op->op=opLineStart;
        }else
        {
          op->op=opDataStart;
        }
        continue;
      }
      case '$':
      {
        if(options&OP_MULTILINE)
        {
          op->op=opLineEnd;
        }else
        {
          op->op=opDataEnd;
        }
        continue;
      }
      case '|':
      {
        if(brackets[brdepth-1]->op==opAlternative)
        {
          brackets[brdepth-1]->alternative.nextalt=op;
        }else
        {
          if(brackets[brdepth-1]->op==opOpenBracket)
          {
            brackets[brdepth-1]->bracket.nextalt=op;
          }else
          {
            brackets[brdepth-1]->assert.nextalt=op;
          }
        }
        if(brdepth==MAXDEPTH)return SetError(errMaxDepth,src-start);
        brackets[brdepth++]=op;
        op->op=opAlternative;
        continue;
      }
      case '(':
      {
        op->op=opOpenBracket;
        if(src[1]=='?')
        {
          src+=2;
          switch(*src)
          {
            case ':':op->bracket.index=-1;break;
            case '=':op->op=opLookAhead;haveLookAhead=1;break;
            case '!':op->op=opNotLookAhead;haveLookAhead=1;break;
            case '<':
            {
              ++src;
              if(*src=='=')
              {
                op->op=opLookBehind;
              }else if(*src=='!')
              {
                op->op=opNotLookBehind;
              }else return SetError(errSyntax,src-start);
              lookbehind++;
            }break;
            case '{':
            {
              op->op=opNamedBracket;
              INPUT_ITER nstart=++src;
              size_t len=0;
              while(*src && *src!='}')++len,++src;
              if(len)
              {
                rechar_t* dst;
                op->nbracket.name=dst=new rechar_t[len+1];
                for(INPUT_ITER it=nstart,nend=src;it!=nend;++it)
                {
                  *dst++=static_cast<rechar_t>(*it);
                }
                *dst=0;
              }else
              {
                op->op=opOpenBracket;
                op->bracket.index=-1;
              }
            }break;
            default:
            {
              return SetError(errSyntax,src-start);
            }
          }

        }else
        {
          brcount++;
          closedbrackets[brcount]=false;
          op->bracket.index=static_cast<int>(brcount);
        }
        brackets[brdepth]=op;
        brdepth++;
        continue;
      }
      case ')':
      {
        op->op=opClosingBracket;
        brdepth--;
        while(brackets[brdepth]->op==opAlternative)
        {
          brackets[brdepth]->alternative.endindex=op;
          brdepth--;
        }
        switch(brackets[brdepth]->op)
        {
          case opOpenBracket:
          {
            op->bracket.pairindex=brackets[brdepth];
            brackets[brdepth]->bracket.pairindex=op;
            op->bracket.index=brackets[brdepth]->bracket.index;
            if(op->bracket.index!=-1)
            {
              closedbrackets[op->bracket.index]=true;
            }
            break;
          }
          case opNamedBracket:
          {
            op->nbracket.pairindex=brackets[brdepth];
            brackets[brdepth]->nbracket.pairindex=op;
            op->nbracket.name=brackets[brdepth]->nbracket.name;
            //h.SetItem((char*)op->nbracket.name,m);
            bool found=false;
            for(std::vector<NamedMatch>::iterator it=nm.begin(),nend=nm.end();it!=nend;++it)
            {
              if(it->checkName(op->nbracket.name))
              {
                found=true;
                break;
              }
            }
            if(!found)
            {
              NamedMatch m;
              m.name=op->nbracket.name;
              nm.push_back(m);
            }
            break;
          }
          case opLookBehind:
          case opNotLookBehind:
          {
            lookbehind--;
            int l=CalcPatternLength(brackets[brdepth]->next,op->prev);
            if(l==-1)return SetError(errVariableLengthLookBehind,src-start);
            brackets[brdepth]->assert.length=l;
          }// there is no break and this is correct!
          case opLookAhead:
          case opNotLookAhead:
          {
            op->assert.pairindex=brackets[brdepth];
            brackets[brdepth]->assert.pairindex=op;
            break;
          }
        }
        continue;
      }
      case '[':
      {
        ++src;
        bool negative=0;
        if(*src=='^')
        {
          negative=true;
          ++src;
        }
        int lastchar=-1;
        int classsize=0;
        op->op=opSymbolClass;
        op->symbolclass=new UniSet();
        tmpclass=op->symbolclass;
        for(;*src!=']';++src)
        {
          if(*src=='\\')
          {
            ++src;
            bool isnottype=false;
            uint8_t type=0;
            lastchar=0;
            switch(*src)
            {
              case 'D':isnottype=true;
              case 'd':type=TYPE_DIGITCHAR;break;
              case 'W':isnottype=true;
              case 'w':type=TYPE_WORDCHAR;break;
              case 'S':isnottype=true;
              case 's':type=TYPE_SPACECHAR;break;
              case 'L':isnottype=true;
              case 'l':type=TYPE_LOWCASE;break;
              case 'U':isnottype=true;
              case 'u':type=TYPE_UPCASE;break;
              case 'I':isnottype=true;
              case 'i':type=TYPE_ALPHACHAR;break;
              case 'n':lastchar='\n';break;
              case 'r':lastchar='\r';break;
              case 't':lastchar='\t';break;
              case 'f':lastchar='\f';break;
              case 'e':lastchar=27;break;
              case 'x':
              {
                int c=0;
                ++src;
                if(src>=end)return SetError(errSyntax,src-start-1);
                c=RE_TOLOWER(static_cast<rechar_t>(*src));
                if((c>='0' && c<='9') ||
                   (c>='a' && c<='f'))
                {
                  c-='0';
                  if(c>9)c-='a'-'0'-10;
                  lastchar=c;
                  if(src+1<end)
                  {
                    c=RE_TOLOWER(static_cast<rechar_t>(src[1]));
                    if((c>='0' && c<='9') ||
                       (c>='a' && c<='f'))
                    {
                      ++src;
                      c-='0';
                      if(c>9)c-='a'-'0'-10;
                      lastchar<<=4;
                      lastchar|=c;
                    }
                  }
                }else return SetError(errSyntax,src-start);
                break;
              }
              default:
              {
                if((options&OP_STRICT) && RE_ISALPHA(static_cast<rechar_t>(*src)))
                {
                  return SetError(errInvalidEscape,src-start-1);
                }
                lastchar=*src;
              }
            }
            if(type)
            {
              if(isnottype)
              {
                tmpclass->addNotType(type);
              }else
              {
                tmpclass->addType(type);
              }
              classsize=257;
            }else
            {
              if(options&OP_IGNORECASE)
              {
                tmpclass->SetBit(RE_TOLOWER(static_cast<rechar_t>(lastchar)));
                tmpclass->SetBit(RE_TOUPPER(static_cast<rechar_t>(lastchar)));
              }else
              {
                tmpclass->SetBit(static_cast<rechar_t>(lastchar));
              }
              classsize++;
            }
            continue;
          }
          if(*src=='-')
          {
            if(lastchar!=-1 && (*src+1)!=']')
            {
              int to=src[1];
              if(to=='\\')
              {
                to=src[2];
                if(to=='x')
                {
                  src+=2;
                  to=RE_TOLOWER(static_cast<rechar_t>(src[1]));
                  if((to>='0' && to<='9') ||
                     (to>='a' && to<='f'))
                  {
                    to-='0';
                    if(to>9)to-='a'-'0'-10;
                    if(src+1<end)
                    {
                      int c=RE_TOLOWER(static_cast<rechar_t>(src[2]));
                      if((c>='0' && c<='9') ||
                         (c>='a' && c<='f'))
                      {
                        ++src;
                        c-='0';
                        if(c>9)c-='a'-'0'-10;
                        to<<=4;
                        to|=c;
                      }
                    }
                  }else return SetError(errSyntax,src-start);
                }else
                {
                  tmpclass->SetBit('-');
                  classsize++;
                  continue;
                }
              }
              ++src;
              for(j=lastchar;j<=to;j++)
              {
                if(ignoreCase)
                {
                  tmpclass->SetBit(RE_TOLOWER(static_cast<rechar_t>(j)));
                  tmpclass->SetBit(RE_TOUPPER(static_cast<rechar_t>(j)));
                }else
                {
                  tmpclass->SetBit(static_cast<rechar_t>(j));
                }
                classsize++;
              }
              continue;
            }
          }
          lastchar=*src;
          if(ignoreCase)
          {
            tmpclass->SetBit(RE_TOLOWER(static_cast<rechar_t>(lastchar)));
            tmpclass->SetBit(RE_TOUPPER(static_cast<rechar_t>(lastchar)));
          }else
          {
            tmpclass->SetBit(static_cast<rechar_t>(lastchar));
          }
          classsize++;
        }
        if(negative && classsize>1)
        {
          tmpclass->negative=negative;
          //for(int j=0;j<32;j++)op->symbolclass[j]^=0xff;
        }
        if(classsize==1)
        {
          delete op->symbolclass;
          op->symbolclass=0;
          tmpclass=0;
          op->op=negative?opNotSymbol:opSymbol;
          if(ignoreCase)
          {
            op->op+=2;
            op->symbol=RE_TOLOWER(static_cast<rechar_t>(lastchar));
          }
          else
          {
            op->symbol=static_cast<rechar_t>(lastchar);
          }

        }
        if(tmpclass)tmpclass->negative=negative;
        continue;
      }
      case '+':
      case '*':
      case '?':
      case '{':
      {
        int min=0,max=0;
        switch(*src)
        {
          case '+':min=1;max=-2;break;
          case '*':min=0;max=-2;break;
          case '?':
          {
            //if(src[i+1]=='?') return SetError(errInvalidQuantifiersCombination,i);
            min=0;max=1;
            break;
          }
          case '{':
          {
            ++src;
            INPUT_ITER save=src;
            min=parseNum(src);
            max=min;
            if(min<0)return SetError(errInvalidRange,save-start);
//            i++;
            if(*src==',')
            {
              if(src[1]=='}')
              {
                ++src;
                max=-2;
              }else
              {
                ++src;
                max=parseNum(src);
//                i++;
                if(max<min)return SetError(errInvalidRange,save-start);
              }
            }
            if(*src!='}')return SetError(errInvalidRange,save-start);
          }
        }
        pos--;
        op=code+pos-1;
        if(min==1 && max==1)continue;
        op->range.min=min;
        op->range.max=max;
        switch(op->op)
        {
          case opLineStart:
          case opLineEnd:
          case opDataStart:
          case opDataEnd:
          case opWordBound:
          case opNotWordBound:
          {
            return SetError(errInvalidQuantifiersCombination,src-start);
//            op->range.op=op->op;
//            op->op=opRange;
//            continue;
          }
          case opCharAny:
          case opCharAnyAll:
          {
            op->range.op=op->op;
            op->op=opAnyRange;
            break;
          }
          case opType:
          {
            op->op=opTypeRange;
            break;
          }
          case opNotType:
          {
            op->op=opNotTypeRange;
            break;
          }
          case opSymbolIgnoreCase:
          case opSymbol:
          {
            op->op=opSymbolRange;
            break;
          }
          case opNotSymbol:
          case opNotSymbolIgnoreCase:
          {
            op->op=opNotSymbolRange;
            break;
          }
          case opSymbolClass:
          {
            op->op=opClassRange;
            break;
          }
          case opBackRef:
          {
            op->op=opBackRefRange;
            break;
          }
          case opNamedBackRef:
          {
            op->op=opNamedRefRange;
          }break;
          case opClosingBracket:
          {
            op=op->bracket.pairindex;
            if(op->op!=opOpenBracket)return SetError(errInvalidQuantifiersCombination,src-start);
            op->range.min=min;
            op->range.max=max;
            op->op=opBracketRange;
            break;
          }
          default:
          {
            return SetError(errInvalidQuantifiersCombination,src-start);
          }
        }//switch(code.op)
        if(src[1]=='?')
        {
           op->op++;
           ++src;
        }
        continue;
      }// case +*?{
      case ' ':
      case '\t':
      case '\n':
      case '\r':
      {
        if(options&OP_XTENDEDSYNTAX)
        {
          pos--;
          continue;
        }
      }
      /* no break */
      default:
      {
        op->op=options&OP_IGNORECASE?opSymbolIgnoreCase:opSymbol;
        if(ignoreCase)
        {
          op->symbol=RE_TOLOWER(static_cast<rechar_t>(*src));
        }else
        {
          op->symbol=static_cast<rechar_t>(*src);
        }
      }break;
    }//switch(src[i])
  }//for()

  namedBracketsCount=nm.size();
  if(namedBracketsCount)
  {
    names=new const rechar_t*[namedBracketsCount];
  }
  for(size_t i=0;i<nm.size();++i)
  {
    names[i]=nm[i].name;
  }
  op=code+pos;
  pos++;

  brdepth--;
  while(brdepth>=0 && brackets[brdepth]->op==opAlternative)
  {
    brackets[brdepth]->alternative.endindex=op;
    brdepth--;
  }

  op->op=opClosingBracket;
  op->bracket.pairindex=code;
  code->bracket.pairindex=op;

  op=code+pos;
  //pos++;
  op->op=opRegExpEnd;

  return 1;
}

template <typename INPUT_ITER>
inline StateStackItem<INPUT_ITER>* StateStack<INPUT_ITER>::findByPos(PREOpCode pos,int op)
{
  StateStackPage* ptr=curPage;
  int cnt=ptr->count;
  for(;;)
  {
    if(--cnt<0)
    {
      ptr=ptr->prev;
      if(!ptr)return 0;
      cnt=ptr->count-1;
    }
    StateStackItem<INPUT_ITER>* item=ptr->stack+cnt;
    if(item->pos==pos && item->op==op)
    {
      return item;
    }
  }
  //return 0;//avoid warning
}

template <typename INPUT_ITER>
inline int RegExp::strCmp(INPUT_ITER& str,INPUT_ITER strend,INPUT_ITER start,INPUT_ITER  end)
{
  INPUT_ITER save=str;
  if(ignoreCase)
  {
    while(start<end)
    {
      if(str==strend || RE_TOLOWER(static_cast<rechar_t>(*str))!=RE_TOLOWER(static_cast<rechar_t>(*start))){str=save;return 0;}
      ++str;
      ++start;
    }
  }else
  {
    while(start<end)
    {
      if(str==strend || *str!=*start){str=save;return 0;}
      ++str;
      ++start;
    }
  }
  return 1;
}

#define OP (*op)


#define MINSKIP(cmp) \
            { rechar_t jj; \
             PREOpCode next=op->nextVal;\
             if(next)\
            switch(next->op) \
            { \
              case opSymbol: \
              { \
                jj=next->symbol; \
                if(static_cast<rechar_t >(*str)!=jj) \
                while(str<strend && cmp && st->max--!=0)\
                {\
                  ++str;\
                  if(*str==jj)break;\
                } \
                break; \
              } \
              case opNotSymbol: \
              { \
                jj=next->symbol; \
                if(*str==jj) \
                while(str<strend && cmp && st->max--!=0)\
                {\
                  ++str;\
                  if(*str!=jj)break;\
                } \
                break; \
              } \
              case opSymbolIgnoreCase: \
              { \
                jj=next->symbol; \
                if(RE_TOLOWER(static_cast<rechar_t >(*str))!=jj) \
                while(str<strend && cmp && st->max--!=0)\
                {\
                  ++str;\
                  if(RE_TOLOWER(static_cast<rechar_t >(*str))==jj)break;\
                } \
                break; \
              } \
              case opNotSymbolIgnoreCase: \
              { \
                jj=next->symbol; \
                if(RE_TOLOWER(static_cast<rechar_t >(*str))==jj) \
                while(str<strend && cmp && st->max--!=0)\
                {\
                  ++str;\
                  if(RE_TOLOWER(static_cast<rechar_t >(*str))!=jj)break;\
                } \
                break; \
              } \
              case opString:\
              {\
                rechar_t* sstr=next->str.str;\
                size_t ii;\
                size_t l=next->str.length;\
                jj=*sstr;\
                while(str<strend && cmp && st->max--!=0)\
                {\
                  if(static_cast<rechar_t >(*str)!=jj)\
                  {\
                    ++str;\
                    continue;\
                  }\
                  for(ii=1;ii<l;++ii)\
                  {\
                    if(str[ii]!=sstr[ii])break;\
                  }\
                  if(ii==l)break;\
                  ++str;\
                }\
                break;\
              }\
              case opStringIgnoreCase:\
              {\
                rechar_t* sstr=next->str.str;\
                size_t ii;\
                size_t l=next->str.length;\
                jj=*sstr;\
                while(str<strend && cmp && st->max--!=0)\
                {\
                  if(RE_TOLOWER(static_cast<rechar_t >(*str))!=jj)\
                  {\
                    ++str;\
                    continue;\
                  }\
                  for(ii=1;ii<l;++ii)\
                  {\
                    if(RE_TOLOWER(static_cast<rechar_t >(str[ii]))!=static_cast<rechar_t >(sstr[ii]))break;\
                  }\
                  if(ii==l)break;\
                  ++str;\
                }\
                break;\
              }\
              case opType: \
              { \
                int type=next->type; \
                if(!(RE_ISTYPE(static_cast<rechar_t >(*str),type))) \
                while(str<strend && cmp && st->max--!=0)\
                {\
                  ++str;\
                  if(RE_ISTYPE(static_cast<rechar_t >(*str),type))break;\
                } \
                break; \
              } \
              case opNotType: \
              { \
                int type=next->type; \
                if((RE_ISTYPE(static_cast<rechar_t >(*str),type))) \
                while(str<strend && cmp && st->max--!=0)\
                {\
                  ++str;\
                  if(!(RE_ISTYPE(static_cast<rechar_t>(*str),type)))break;\
                } \
                break; \
              } \
              case opSymbolClass: \
              { \
                cl=next->symbolclass; \
                if(!cl->GetBit(static_cast<rechar_t >(*str))) \
                while(str<strend && cmp && st->max--!=0)\
                {\
                  ++str;\
                  if(cl->GetBit(static_cast<rechar_t >(*str)))break;\
                } \
                break; \
              } \
            } \
            }


template <typename INPUT_ITER>
inline bool RegExp::innerMatch(INPUT_ITER start,INPUT_ITER str,INPUT_ITER strend,MatchInfo* mi)
{
//  register prechar str=start;
  size_t i,j;
  int minimizing;
  PREOpCode op,tmp = nullptr;
  SMatch* m=mi->br;
  SMatch* match=m;
  UniSet *cl;
  //int inrangebracket=0;
  if(errorCode==errNotCompiled)return 0;
  if(mi->brSize<maxBackRef)return SetError(errNotEnoughMatches,0);
  if(namedBracketsCount>0 && (!mi->nm || mi->nmSize==0))return SetError(errNoStorageForNB,0);
  StateStack<INPUT_ITER> stack;
  StateStackItem<INPUT_ITER>* st=stack.firstPage.stack;
  StateStackItem<INPUT_ITER>* ps;
  errorCode=errNone;
  /*for(i=0;i<matchcount;i++)
  {
    match[i].start=-1;
    match[i].end=-1;
  }*/
  mi->brCount=bracketsCount;
  memset(mi->br,-1,sizeof(SMatch)*bracketsCount);

  for(op=code;op;op=op->next)
  {
    //dpf(("op:%s,\tpos:%d,\tstr:%d\n",ops[OP.op],pos,str-start));
    if(str<=strend)
    switch(OP.op)
    {
      case opLineStart:
      {
        if(str==start || str[-1]==0x0d || str[-1]==0x0a)continue;
        break;
      }
      case opLineEnd:
      {
        if(str==strend)continue;
        if(*str==0x0d || *str==0x0a)
        {
          if(*str==0x0d)++str;
          if(*str==0x0a)++str;
          continue;
        }
        break;
      }
      case opDataStart:
      {
        if(str==start)continue;
        break;
      }
      case opDataEnd:
      {
        if(str==strend)continue;
        break;
      }
      case opWordBound:
      {
        if((str==start && RE_ISWORD(static_cast<rechar_t >(*str))) || (str>start && (
           (!(RE_ISWORD(static_cast<rechar_t >(str[-1]))) && RE_ISWORD(static_cast<rechar_t >(*str))) ||
           (!(RE_ISWORD(static_cast<rechar_t >(*str))) && RE_ISWORD(static_cast<rechar_t >(str[-1]))) ||
           (str==strend && RE_ISWORD(static_cast<rechar_t >(str[-1]))))))continue;
        break;
      }
      case opNotWordBound:
      {
        if(!((str==start && RE_ISWORD(static_cast<rechar_t >(*str)))||
           (!(RE_ISWORD(static_cast<rechar_t >(str[-1]))) && RE_ISWORD(static_cast<rechar_t >(*str))) ||
           (!(RE_ISWORD(static_cast<rechar_t >(*str))) && RE_ISWORD(static_cast<rechar_t >(str[-1]))) ||
           (str==strend && RE_ISWORD(static_cast<rechar_t >(str[-1])))))continue;
        break;
      }
      case opType:
      {
        if(RE_ISTYPE(static_cast<rechar_t >(*str),OP.type))
        {
          ++str;
          continue;
        }
        break;
      }
      case opNotType:
      {
        if(!(RE_ISTYPE(static_cast<rechar_t >(*str),OP.type)))
        {
          ++str;
          continue;
        }
        break;
      }
      case opCharAny:
      {
        if(*str!=0x0d && *str!=0x0a)
        {
          ++str;
          continue;
        }
        break;
      }
      case opCharAnyAll:
      {
        ++str;
        continue;
      }
      case opSymbol:
      {
        if(*str==OP.symbol)
        {
          ++str;
          continue;
        }
        break;
      }
      case opNotSymbol:
      {
        if(*str!=OP.symbol)
        {
          ++str;
          continue;
        }
        break;
      }
      case opSymbolIgnoreCase:
      {
        if(RE_TOLOWER(static_cast<rechar_t >(*str))==OP.symbol)
        {
          ++str;
          continue;
        }
        break;
      }
      case opNotSymbolIgnoreCase:
      {
        if(RE_TOLOWER(static_cast<rechar_t >(*str))!=OP.symbol)
        {
          ++str;
          continue;
        }
        break;
      }
      case opString:
      {
        i=0;
        while(str<strend && *str==OP.str.str[i] && i<OP.str.length)++i,++str;
        if(i==OP.str.length)continue;
      }break;
      case opStringIgnoreCase:
      {
        i=0;
        while(str<strend && RE_TOLOWER(static_cast<rechar_t >(*str))==OP.str.str[i] && i<OP.str.length)++i,++str;
        if(i==OP.str.length)continue;
      }break;
      case opSymbolClass:
      {
        if(OP.symbolclass->GetBit(static_cast<rechar_t >(*str)))
        {
          ++str;
          continue;
        }
        break;
      }
      case opOpenBracket:
      {
        if(OP.bracket.index>=0 && static_cast<size_t>(OP.bracket.index)<mi->brSize)
        {
          //if(inrangebracket)
          {
            st->op=opOpenBracket;
            st->pos=op;
            st->min=match[OP.bracket.index].start;
            st->max=match[OP.bracket.index].end;
            st=stack.pushState();
          }
          match[OP.bracket.index].start=(int)(str-start);
        }
        if(OP.bracket.nextalt)
        {
          st->op=opAlternative;
          st->pos=OP.bracket.nextalt;
          st->savestr=str;
          st=stack.pushState();
        }
        continue;
      }
      case opNamedBracket:
      {
          SMatch* m2=mi->hasNamedMatch(OP.nbracket.name);
          if(!m2)
          {
            SMatch sm;
            sm.start=-1;
            sm.end=-1;
            mi->nm[mi->nmCount]=NamedMatch(OP.nbracket.name,sm);
            m2=&mi->nm[mi->nmCount++].info;
          }
          {
            st->op=opNamedBracket;
            st->pos=op;
            st->min=m2->start;
            st->max=m2->end;
            st=stack.pushState();
          }
          m2->start=(int)(str-start);
        if(OP.bracket.nextalt)
        {
          st->op=opAlternative;
          st->pos=OP.bracket.nextalt;
          st->savestr=str;
          st=stack.pushState();
        }
        continue;
      }
      case opClosingBracket:
      {
        switch(OP.bracket.pairindex->op)
        {
          case opOpenBracket:
          {
            if(OP.bracket.index>=0 && static_cast<size_t>(OP.bracket.index)<mi->brCount)
            {
              match[OP.bracket.index].end=(int)(str-start);
            }
            continue;
          }
          case opNamedBracket:
          {
            SMatch* m2=mi->hasNamedMatch(OP.nbracket.name);
            if(m2)m2->end=(int)(str-start);
            continue;
          }
          case opBracketRange:
          {
            ps=stack.findByPos(OP.bracket.pairindex,opBracketRange);
            *st=*ps;
            if(str==st->startstr)
            {
              if(OP.range.bracket.index>=0 && static_cast<size_t>(OP.range.bracket.index)<mi->brSize)
              {
                match[OP.range.bracket.index].end=(int)(str-start);
              }
              //inrangebracket--;
              continue;
            }
            if(st->min>0)st->min--;
            if(st->min)
            {
              st->max--;
              st->startstr=str;
              st->savestr=str;
              op=st->pos;
              st=stack.pushState();
              if(OP.range.bracket.index>=0 && static_cast<size_t>(OP.range.bracket.index)<mi->brSize)
              {
                match[OP.range.bracket.index].start=(int)(str-start);
                st->op=opOpenBracket;
                st->pos=op;
                st->min=match[OP.range.bracket.index].start;
                st->max=match[OP.range.bracket.index].end;
                st=stack.pushState();
              }
              if(OP.range.bracket.nextalt)
              {
                st->op=opAlternative;
                st->pos=OP.range.bracket.nextalt;
                st->savestr=str;
                st=stack.pushState();
              }
              continue;
            }
            st->max--;
            if(st->max==0)
            {
              if(OP.range.bracket.index>=0 && static_cast<size_t>(OP.range.bracket.index)<mi->brSize)
              {
                match[OP.range.bracket.index].end=(int)(str-start);
              }
              //inrangebracket--;
              continue;
            }
            if(OP.range.bracket.index>=0 && static_cast<size_t>(OP.range.bracket.index)<mi->brSize)
            {
              match[OP.range.bracket.index].end=(int)(str-start);
              tmp=op;
            }
            st->startstr=str;
            st->savestr=str;
            op=st->pos;
            st=stack.pushState();
            if(OP.range.bracket.index>=0 && static_cast<size_t>(OP.range.bracket.index)<mi->brSize)
            {
              st->op=opOpenBracket;
              st->pos=tmp;
              st->min=match[OP.range.bracket.index].start;
              st->max=match[OP.range.bracket.index].end;
              st=stack.pushState();
              match[OP.range.bracket.index].start=(int)(str-start);
            }

            if(OP.range.bracket.nextalt)
            {
              st->op=opAlternative;
              st->pos=OP.range.bracket.nextalt;
              st->savestr=str;
              st=stack.pushState();
            }
            continue;
          }
          case opBracketMinRange:
          {
            ps=stack.findByPos(OP.bracket.pairindex,opBracketMinRange);
            *st=*ps;
            if(st->min>0)st->min--;
            if(st->min)
            {
              //st->min--;
              st->max--;
              st->startstr=str;
              st->savestr=str;
              op=st->pos;
              st=stack.pushState();
              if(OP.range.bracket.index>=0 && static_cast<size_t>(OP.range.bracket.index)<mi->brSize)
              {
                match[OP.range.bracket.index].start=(int)(str-start);
                st->op=opOpenBracket;
                st->pos=op;
                st->min=match[OP.range.bracket.index].start;
                st->max=match[OP.range.bracket.index].end;
                st=stack.pushState();
              }
              if(OP.range.bracket.nextalt)
              {
                st->op=opAlternative;
                st->pos=OP.range.bracket.nextalt;
                st->savestr=str;
                st=stack.pushState();
              }
              continue;
            }
            if(OP.range.bracket.index>=0 && static_cast<size_t>(OP.range.bracket.index)<mi->brSize)
            {
              match[OP.range.bracket.index].end=(int)(str-start);
            }
            st->max--;
            //inrangebracket--;
            if(st->max==0)continue;
            st->forward=str>ps->startstr?1:0;
            st->startstr=str;
            st->savestr=str;
            st=stack.pushState();
            if(OP.range.bracket.nextalt)
            {
              st->op=opAlternative;
              st->pos=OP.range.bracket.nextalt;
              st->savestr=str;
              st=stack.pushState();
            }
            continue;
          }
          case opLookAhead:
          {
            tmp=OP.bracket.pairindex;
            do{
              st=stack.popState();
            }while(st->pos!=tmp || st->op!=opLookAhead);
            str=st->savestr;
            continue;
          }
          case opNotLookAhead:
          {
            do{
              st=stack.popState();
            }while(st->op!=opNotLookAhead);
            str=st->savestr;
            break;
          }
          case opLookBehind:
          {
            continue;
          }
          case opNotLookBehind:
          {
            ps=stack.getTop();
            ps->forward=0;
            break;
          }
        }//switch(code[pairindex].op)
        break;
      }//case opClosingBracket
      case opAlternative:
      {
        op=OP.alternative.endindex->prev;
        continue;
      }
      case opNamedBackRef:
      case opBackRef:
      {
        if(OP.op==opNamedBackRef)
        {
          m=mi->hasNamedMatch(OP.refname);
          if(!m)break;
        }else
        {
          m=&match[OP.refindex];
        }
        if(m->start==-1 || m->end==-1)break;
        if(strCmp(str,strend,start+m->start,start+m->end)==0)break;
        continue;
      }
      case opAnyRange:
      case opAnyMinRange:
      {
        st->op=OP.op;
        minimizing=OP.op==opAnyMinRange;
        j=static_cast<size_t>(OP.range.min);
        st->max=static_cast<int>(OP.range.max-j);
        if(OP.range.op==opCharAny)
        {
          for(i=0;i<j;i++,++str)
          {
            if(str>strend || *str==0x0d || *str==0x0a)break;
          }
          if(i<j)
          {
            break;
          }
          st->startstr=str;
          if(!minimizing)
          {
            while(str<strend && *str!=0x0d && *str!=0x0a && st->max--!=0)++str;
          }else
          {
            MINSKIP(*str!=0x0d && *str!=0x0a);
            if(st->max==-1)break;
          }
        }else
        {
          //opCharAnyAll:
          str+=j;
          if(str>strend)break;
          st->startstr=str;
          if(!minimizing)
          {
            if(st->max>=0)
            {
              if((str+st->max)<strend)
              {
                str+=st->max;
                st->max=0;
              }else
              {
                st->max-=(int)(strend-str);
                str=strend;
              }
            }else
            {
              str=strend;
            }
          }else
          {
            if(op->next->op==opString || op->next->op==opStringIgnoreCase)
            {
              if(!BMSearch(op->next,str,strend))break;
            }else
            {
              MINSKIP(1);
            }
            if(st->max==-1)break;
          }
        }
        if(OP.range.max==static_cast<int>(j))continue;
        st->savestr=str;
        st->pos=op;
        st=stack.pushState();
        continue;
      }
      case opSymbolRange:
      case opSymbolMinRange:
      {
        st->op=OP.op;
        minimizing=OP.op==opSymbolMinRange;
        j=static_cast<size_t>(OP.range.min);
        st->max=static_cast<int>(OP.range.max-j);
        if(ignoreCase)
        {
          for(i=0;i<j;i++,++str)
          {
            if(str>strend || RE_TOLOWER(static_cast<rechar_t>(*str))!=OP.range.symbol)break;
          }
          if(i<j)break;
          st->startstr=str;
          if(!minimizing)
          {
            while(str<strend && RE_TOLOWER(static_cast<rechar_t>(*str))==OP.range.symbol && st->max--!=0)++str;
          }else
          {
            MINSKIP(RE_TOLOWER(static_cast<rechar_t>(*str))==OP.range.symbol);
            if(st->max==-1)break;
          }
        }else
        {
          for(i=0;i<j;i++,++str)
          {
            if(str>strend || *str!=OP.range.symbol)break;
          }
          if(i<j)break;
          st->startstr=str;
          if(!minimizing)
          {
            while(str<strend && *str==OP.range.symbol && st->max--!=0)++str;
          }else
          {
            MINSKIP(*str==OP.range.symbol);
            if(st->max==-1)break;
          }
        }
        if(OP.range.max==static_cast<int>(j))continue;
        st->savestr=str;
        st->pos=op;
        st=stack.pushState();
        continue;
      }
      case opNotSymbolRange:
      case opNotSymbolMinRange:
      {
        st->op=OP.op;
        minimizing=OP.op==opNotSymbolMinRange;
        j=static_cast<size_t>(OP.range.min);
        st->max=static_cast<int>(OP.range.max-j);
        if(ignoreCase)
        {
          for(i=0;i<j;i++,++str)
          {
            if(str>strend || RE_TOLOWER(static_cast<rechar_t>(*str))==OP.range.symbol)break;
          }
          if(i<j)
          {
            break;
          }
          st->startstr=str;
          if(!minimizing)
          {
            while(str<strend && RE_TOLOWER(static_cast<rechar_t>(*str))!=OP.range.symbol && st->max--!=0)++str;
          }else
          {
            MINSKIP(RE_TOLOWER(static_cast<rechar_t>(*str))!=OP.range.symbol);
            if(st->max==-1)break;
          }
        }else
        {
          for(i=0;i<j;i++,++str)
          {
            if(str>strend || *str==OP.range.symbol)break;
          }
          if(i<j)
          {
            break;
          }
          st->startstr=str;
          if(!minimizing)
          {
            while(str<strend && *str!=OP.range.symbol && st->max--!=0)++str;
          }else
          {
            MINSKIP(*str!=OP.range.symbol);
            if(st->max==-1)
            {
              break;
            }
          }
        }
        if(OP.range.max==static_cast<int>(j))
        {
          continue;
        }
        st->savestr=str;
        st->pos=op;
        st=stack.pushState();
        continue;
      }
      case opClassRange:
      case opClassMinRange:
      {
        st->op=OP.op;
        minimizing=OP.op==opClassMinRange;
        j=static_cast<size_t>(OP.range.min);
        st->max=static_cast<int>(OP.range.max-j);
        for(i=0;i<j;i++,++str)
        {
          if(str>strend || !OP.range.symbolclass->GetBit(static_cast<rechar_t>(*str)))break;
        }
        if(i<j)break;
        st->startstr=str;
        if(!minimizing)
        {
          while(str<strend && OP.range.symbolclass->GetBit(static_cast<rechar_t>(*str)) && st->max--!=0)++str;
        }else
        {
          MINSKIP(OP.range.symbolclass->GetBit(static_cast<rechar_t>(*str)));
          if(st->max==-1)break;
        }
        if(OP.range.max==static_cast<int>(j))continue;
        st->savestr=str;
        st->pos=op;
        st=stack.pushState();
        continue;
      }
      case opTypeRange:
      case opTypeMinRange:
      {
        st->op=OP.op;
        minimizing=OP.op==opTypeMinRange;
        j=static_cast<size_t>(OP.range.min);
        st->max=static_cast<int>(OP.range.max-j);
        for(i=0;i<j;i++,++str)
        {
          if(str>strend || (RE_ISTYPE(static_cast<rechar_t>(*str),OP.range.type))==0)break;
        }
        if(i<j)break;
        st->startstr=str;
        if(!minimizing)
        {
          while(str<strend && (RE_ISTYPE(static_cast<rechar_t>(*str),OP.range.type)) && st->max--!=0)++str;
        }else
        {
          MINSKIP((RE_ISTYPE(static_cast<rechar_t>(*str),OP.range.type)));
          if(st->max==-1)break;
        }
        if(OP.range.max==static_cast<int>(j))continue;
        st->savestr=str;
        st->pos=op;
        st=stack.pushState();
        continue;
      }
      case opNotTypeRange:
      case opNotTypeMinRange:
      {
        st->op=OP.op;
        minimizing=OP.op==opNotTypeMinRange;
        j=static_cast<size_t>(OP.range.min);
        st->max=static_cast<int>(OP.range.max-j);
        for(i=0;i<j;i++,++str)
        {
          if(str>strend || (RE_ISTYPE(static_cast<rechar_t>(*str),OP.range.type))!=0)break;
        }
        if(i<j)break;
        st->startstr=str;
        if(!minimizing)
        {
          while(str<strend && (RE_ISTYPE(static_cast<rechar_t>(*str),OP.range.type))==0 && st->max--!=0)++str;
        }else
        {
          MINSKIP((RE_ISTYPE(static_cast<rechar_t>(*str),OP.range.type))==0);
          if(st->max==-1)break;
        }
        if(OP.range.max==static_cast<int>(j))continue;
        st->savestr=str;
        st->pos=op;
        st=stack.pushState();
        continue;
      }
      case opNamedRefRange:
      case opNamedRefMinRange:
      case opBackRefRange:
      case opBackRefMinRange:
      {
        st->op=OP.op;
        minimizing=OP.op==opBackRefMinRange || OP.op==opNamedRefMinRange;
        j=static_cast<size_t>(OP.range.min);
        st->max=static_cast<int>(OP.range.max-j);
        if(OP.op==opBackRefRange || OP.op==opBackRefMinRange)
        {
          m=&match[OP.range.refindex];
        }else
        {
          m=mi->hasNamedMatch(OP.range.refname);
        }
        if(!m)break;
        if(m->start==-1 || m->end==-1)
        {
          if(j==0)continue;
          else break;
        }

        for(i=0;i<j;i++)
        {
          if(str>strend || strCmp(str,strend,start+m->start,start+m->end)==0)break;
        }
        if(i<j)break;
        st->startstr=str;
        if(!minimizing)
        {
          while(str<strend && strCmp(str,strend,start+m->start,start+m->end) && st->max--!=0);
        }else
        {
          MINSKIP(strCmp(str,strend,start+m->start,start+m->end));
          if(st->max==-1)break;
        }
        if(OP.range.max==static_cast<int>(j))continue;
        st->savestr=str;
        st->pos=op;
        st=stack.pushState();
        continue;
      }
      case opBracketRange:
      case opBracketMinRange:
      {
        if(/*inrangebracket &&*/ OP.range.bracket.index>=0 && OP.range.bracket.index<static_cast<int>(mi->brSize))
        {
          st->op=opOpenBracket;
          st->pos=OP.range.bracket.pairindex;
          st->min=match[OP.range.bracket.index].start;
          st->max=match[OP.range.bracket.index].end;
          st=stack.pushState();
        }
        st->op=OP.op;
        st->pos=op;
        st->min=OP.range.min;
        st->max=OP.range.max;
        st->startstr=str;
        st->savestr=str;
        st->forward=1;
        st=stack.pushState();
        if(OP.range.nextalt)
        {
          st->op=opAlternative;
          st->pos=OP.range.bracket.nextalt;
          st->savestr=str;
          st=stack.pushState();
        }
        if(OP.range.bracket.index>=0 && OP.range.bracket.index<static_cast<int>(mi->brSize))
        {
          match[OP.range.bracket.index].start=
          /*match[OP.range.bracket.index].end=*/(int)(str-start);
        }
        if(OP.op==opBracketMinRange && OP.range.min==0)
        {
          op=OP.range.bracket.pairindex;
        }else
        {
          //inrangebracket++;
        }
        continue;
      }
      case opLookAhead:
      case opNotLookAhead:
      {
        st->op=OP.op;
        st->savestr=str;
        st->pos=op;
        st->forward=1;
        st=stack.pushState();
        if(OP.assert.nextalt)
        {
          st->op=opAlternative;
          st->pos=OP.assert.nextalt;
          st->savestr=str;
          st=stack.pushState();
        }
        continue;
      }
      case opLookBehind:
      case opNotLookBehind:
      {
        if(str-OP.assert.length<start)
        {
          if(OP.op==opLookBehind)break;
          op=OP.assert.pairindex;
          continue;
        }
        st->op=OP.op;
        st->savestr=str;
        st->pos=op;
        st->forward=1;
        str-=OP.assert.length;
        st=stack.pushState();
        if(OP.assert.nextalt)
        {
          st->op=opAlternative;
          st->pos=OP.assert.nextalt;
          st->savestr=str;
          st=stack.pushState();
        }
        continue;
      }
      case opNoReturn:
      {
        st->op=opNoReturn;
        st=stack.pushState();
        continue;
      }
      case opRegExpEnd:return 1;
    }//switch(op)
    for(;;st=stack.popState())
    {
      ps=stack.getTop();
      if(!ps)return 0;
      //dpf(("ps->op:%s\n",ops[ps->op]));
      switch(ps->op)
      {
        case opAlternative:
        {
          str=ps->savestr;
          op=ps->pos;
          if(OP.alternative.nextalt)
          {
            ps->pos=OP.alternative.nextalt;
          }else
          {
            st=stack.popState();
          }
          break;
        }
        case opAnyRange:
        case opSymbolRange:
        case opNotSymbolRange:
        case opClassRange:
        case opTypeRange:
        case opNotTypeRange:
        {
          str=(ps->savestr)-1;
          op=ps->pos;
          if(str<ps->startstr)
          {
            continue;
          }
          ps->savestr=str;
          break;
        }
        case opNamedRefRange:
        case opBackRefRange:
        {
//          PMatch m;
          if(ps->op==opBackRefRange)
          {
            m=&match[ps->pos->range.refindex];
          }else
          {
            m=mi->hasNamedMatch(ps->pos->range.refname);
          }
          str=ps->savestr-(m->end-m->start);
          op=ps->pos;
          if(str<ps->startstr)
          {
            continue;
          }
          ps->savestr=str;
          break;
        }
        case opAnyMinRange:
        {
          if(ps->max--==0)continue;
          str=ps->savestr;
          op=ps->pos;
          if(ps->pos->range.op==opCharAny)
          {
            if(str<strend && *str!=0x0a && *str!=0x0d)
            {
              ++str;
              ps->savestr=str;
            }else
            {
              continue;
            }
          }else
          {
            if(str<strend)
            {
              ++str;
              ps->savestr=str;
            }
            else
            {
              continue;
            }
          }
          break;
        }
        case opSymbolMinRange:
        {
          if(ps->max--==0)continue;
          str=ps->savestr;
          op=ps->pos;
          if(ignoreCase)
          {
            if(str<strend && RE_TOLOWER(static_cast<rechar_t>(*str))==OP.symbol)
            {
              ++str;
              ps->savestr=str;
            }else
            {
              continue;
            }
          }else
          {
            if(str<strend && *str==OP.symbol)
            {
              ++str;
              ps->savestr=str;
            }else
            {
              continue;
            }
          }
          break;
        }
        case opNotSymbolMinRange:
        {
          if(ps->max--==0)continue;
          str=ps->savestr;
          op=ps->pos;
          if(ignoreCase)
          {
            if(str<strend && RE_TOLOWER(static_cast<rechar_t>(*str))!=OP.symbol)
            {
              ++str;
              ps->savestr=str;
            }else
            {
              continue;
            }
          }else
          {
            if(str<strend && *str!=OP.symbol)
            {
              ++str;
              ps->savestr=str;
            }else
            {
              continue;
            }
          }
          break;
        }
        case opClassMinRange:
        {
          if(ps->max--==0)continue;
          str=ps->savestr;
          op=ps->pos;
          if(str<strend && OP.range.symbolclass->GetBit(static_cast<rechar_t>(*str)))
          {
            ++str;
            ps->savestr=str;
          }else
          {
            continue;
          }
          break;
        }
        case opTypeMinRange:
        {
          if(ps->max--==0)continue;
          str=ps->savestr;
          op=ps->pos;
          if(str<strend && RE_ISTYPE(static_cast<rechar_t>(*str),OP.range.type))
          {
            ++str;
            ps->savestr=str;
          }else
          {
            continue;
          }
          break;
        }
        case opNotTypeMinRange:
        {
          if(ps->max--==0)continue;
          str=ps->savestr;
          op=ps->pos;
          if(str<strend && ((RE_ISTYPE(static_cast<rechar_t>(*str),OP.range.type))==0))
          {
            ++str;
            ps->savestr=str;
          }else
          {
            continue;
          }
          break;
        }
        case opNamedRefMinRange:
        case opBackRefMinRange:
        {
          if(ps->max--==0)continue;
          str=ps->savestr;
          op=ps->pos;
          if(ps->op==opBackRefMinRange)
          {
            m=&match[OP.range.refindex];
          }else
          {
            m=mi->hasNamedMatch(OP.range.refname);
          }
          if(str+m->end-m->start<strend && strCmp(str,strend,start+m->start,start+m->end))
          {
            ps->savestr=str;
          }else
          {
            continue;
          }
          break;
        }
        case opBracketRange:
        {
          if(ps->min)
          {
            //inrangebracket--;
            continue;
          }
          if(ps->forward)
          {
            ps->forward=0;
            op=ps->pos->range.bracket.pairindex;
            //inrangebracket--;
            str=ps->savestr;
            if(OP.range.nextalt)
            {
              st->op=opAlternative;
              st->pos=OP.range.bracket.nextalt;
              st->savestr=str;
              st=stack.pushState();
            }
            if(OP.bracket.index>=0 && OP.bracket.index<static_cast<int>(mi->brCount))
            {
              match[OP.bracket.index].end=str-start;
            }
            break;
          }
          continue;
        }
        case opBracketMinRange:
        {
          if(ps->max--==0)
          {
            //inrangebracket--;
            continue;
          }
          if(ps->forward)
          {
            ps->forward=0;
            op=ps->pos;
            str=ps->savestr;
            if(OP.range.bracket.index>=0 && OP.range.bracket.index<static_cast<int>(mi->brSize))
            {
              match[OP.range.bracket.index].start=(int)(str-start);
              st->op=opOpenBracket;
              st->pos=op;
              st->min=match[OP.range.bracket.index].start;
              st->max=match[OP.range.bracket.index].end;
              st=stack.pushState();
            }
            if(OP.range.nextalt)
            {
              st->op=opAlternative;
              st->pos=OP.range.bracket.nextalt;
              st->savestr=str;
              st=stack.pushState();
            }
            //inrangebracket++;
            break;
          }
          //inrangebracket--;
          continue;
        }
        case opOpenBracket:
        {
          j=static_cast<size_t>(ps->pos->bracket.index);
          if(j<mi->brSize)
          {
            match[j].start=ps->min;
            match[j].end=ps->max;
          }
          continue;
        }
        case opNamedBracket:
        {
          const rechar_t* n=ps->pos->nbracket.name;
          if(n)
          {
            m=mi->hasNamedMatch(n);
            if(m)
            {
              m->start=ps->min;
              m->end=ps->max;
            }else
            {
              SMatch sm;
              sm.start=ps->min;
              sm.end=ps->max;
              mi->nm[mi->nmCount++]=NamedMatch(n,sm);
            }
          }
          continue;
        }
        case opLookAhead:
        case opLookBehind:
        {
          continue;
        }
        case opNotLookBehind:
        case opNotLookAhead:
        {
          op=ps->pos->assert.pairindex;
          str=ps->savestr;
          if(ps->forward)
          {
            st=stack.popState();
            break;
          }else
          {
            continue;
          }
        }
        case opNoReturn:
        {
          return false;
        }

     }//switch(op)
     break;
    }
  }
  return true;
}

template <typename INPUT_ITER>
inline bool RegExp::Match(INPUT_ITER textstart,INPUT_ITER textend,MatchInfo& mi)
{
  return MatchEx(textstart,textstart,textend,mi);
}

template <typename INPUT_ITER>
inline bool RegExp::MatchEx(INPUT_ITER datastart,INPUT_ITER textstart,INPUT_ITER textend,MatchInfo& mi)
{
  if(haveFirst && !firstptr->GetBit(*textstart))return 0;
  if(minLength!=0 && textend-textstart<minLength)return 0;
  if(textend-textstart<256 && haveLast)
  {
    --textend;
    while(textend>=textstart && !lastptr->GetBit(*textend))--textend;
    if(textend<textstart)return 0;
    ++textend;
  }
  bool res=innerMatch(datastart,textstart,textend,&mi);
  if(res)
  {
    int i;
    SMatch* match=mi.br;
    for(i=0;i<mi.brCount;i++)
    {
      if(match[i].start==-1 || match[i].end==-1 || match[i].start>match[i].end)
      {
        match[i].start=match[i].end=-1;
      }
    }
  }
  return res;
}


template <typename INPUT_ITER>
inline bool RegExp::Match(INPUT_ITER textstart,MatchInfo& mi)
{
  INPUT_ITER textend=reStrEnd(textstart);
  return Match(textstart,textend,mi);
}



inline bool RegExp::Optimize()
{
  //int i;
  PREOpCode jumps[MAXDEPTH];
  size_t jumpcount=0;
  if(haveFirst)return 1;
  firstptr->Reset();
  lastptr->Reset();
  PREOpCode op,lastOp;

  minLength=0;
  int mlstackmin[MAXDEPTH];
  int mlstacksave[MAXDEPTH];
  size_t mlscnt=0;

  for(op=code;op;op=op->next)
  {
    if((op->op==opSymbol || op->op==opSymbolIgnoreCase) && (op->next->op==opSymbol || op->next->op==opSymbolIgnoreCase) )
    {
      PREOpCode op2=op->next;
      size_t l=2;
      while(op2->next->op==opSymbol || op2->next->op==opSymbolIgnoreCase)
      {
        op2=op2->next;
        ++l;
      }
      rechar_t* str=new rechar_t[l];
      {
        size_t i = l - 1;
        PREOpCode opb = op2;
        while (opb != op) {
          str[i] = opb->symbol;
          --i;
          opb = opb->prev;
        }
      }
      str[0]=op->symbol;
      op->op=op->op==opSymbol?opString:opStringIgnoreCase;
      for(size_t i=0;i<128;++i)op->str.bmskip[i]=l;
      op->str.str=str;
      op->str.length=l;
      for(size_t i=0;i<l-1;++i)
      {
        if(op->str.str[i]<128)
        {
          if(op->op==opString)
          {
            op->str.bmskip[op->str.str[i]]=l-1-i;
          }else
          {
            op->str.bmskip[RE_TOLOWER(op->str.str[i])]=l-1-i;
          }
        }
      }
      op->next=op2->next;
      op2->prev=op;
    }
  }

  lastOp=0;
  for(op=code;op;op=op->next)
  {
    switch(op->op)
    {
      case opType:
      case opNotType:
      case opCharAny:
      case opCharAnyAll:
      case opSymbol:
      case opNotSymbol:
      case opSymbolIgnoreCase:
      case opNotSymbolIgnoreCase:
      case opSymbolClass:
       minLength++;
       if(lastOp)lastOp->nextVal=op;
       lastOp=op;
       continue;
      case opString:
      case opStringIgnoreCase:
        minLength+=op->str.length;
        if(lastOp)lastOp->nextVal=op;
        lastOp=op;
        continue;
      case opSymbolRange:
      case opSymbolMinRange:
      case opNotSymbolRange:
      case opNotSymbolMinRange:
      case opAnyRange:
      case opAnyMinRange:
      case opTypeRange:
      case opTypeMinRange:
      case opNotTypeRange:
      case opNotTypeMinRange:
      case opClassRange:
      case opClassMinRange:
        minLength+=op->range.min;
        if(lastOp && op->range.min>0)lastOp->nextVal=op;
        lastOp=op;
        break;

      case opBracketRange:
      case opBracketMinRange:
        if(op->range.min==0)
        {
          lastOp=0;
        }
        /* no break */
      case opNamedBracket:
      case opOpenBracket:
        mlstacksave[mlscnt]=static_cast<int>(minLength);
        mlstackmin[mlscnt++]=-1;
        minLength=0;
        continue;
      case opClosingBracket:
      {
        if(op->bracket.pairindex->op>opAssertionsBegin &&
           op->bracket.pairindex->op<opAsserionsEnd)
        {
          lastOp=0;
          continue;
        }
        if(mlstackmin[mlscnt-1]!=-1 && static_cast<size_t>(mlstackmin[mlscnt-1])<minLength)
        {
          minLength=static_cast<size_t>(mlstackmin[mlscnt-1]);
        }

        switch(op->bracket.pairindex->op)
        {
          case opBracketRange:
          case opBracketMinRange:
            lastOp=0;
            minLength*=op->range.min;
            break;
        }
        minLength+=static_cast<size_t>(mlstacksave[--mlscnt]);
      }continue;
      case opAlternative:
      {
        if(mlstackmin[mlscnt-1]==-1)
        {
          mlstackmin[mlscnt-1]=static_cast<int>(minLength);
        }else
        {
          if(minLength<static_cast<size_t>(mlstackmin[mlscnt-1]))
          {
            mlstackmin[mlscnt-1]=static_cast<int>(minLength);
          }
        }
        lastOp=0;
        minLength=0;
        break;
      }
      case opLookAhead:
      case opNotLookAhead:
      case opLookBehind:
      case opNotLookBehind:
      {
        op=op->assert.pairindex;
        continue;
      }
      case opRegExpEnd:
        op=0;
        break;
    }
    if(!op)break;
  }

  if(!code->bracket.nextalt && !haveLookAhead)
  {
    op=code->bracket.pairindex->prev;
    haveLast=true;
    switch(op->op)
    {
      case opType:
      {
        //for(i=0;i<256;i++)if(RE_ISTYPE(i,OP.type))first[i]=1;
        lastptr->addType(static_cast<unsigned char>(OP.type));
        break;
      }
      case opNotType:
      {
        //for(i=0;i<256;i++)if(!(RE_ISTYPE(i,OP.type)))first[i]=1;
        lastptr->addNotType(static_cast<unsigned char>(OP.type));
        break;
      }
      case opSymbol:
      {
        //first[OP.symbol]=1;
        lastptr->SetBit(OP.symbol);
        break;
      }
      case opString:
      {
        lastptr->SetBit(OP.str.str[OP.str.length-1]);
        break;
      }
      case opStringIgnoreCase:
      {
        lastptr->SetBit(OP.str.str[OP.str.length-1]);
        lastptr->SetBit(static_cast<rechar_t>(RE_TOUPPER(OP.str.str[OP.str.length-1])));
        break;
      }
      case opSymbolIgnoreCase:
      {
        lastptr->SetBit(OP.symbol);
        lastptr->SetBit(static_cast<rechar_t>(RE_TOUPPER(OP.symbol)));
        break;
      }
      case opSymbolClass:
      {
        *lastptr+=*OP.symbolclass;
        break;
      }
      case opNotSymbol:
      {
        lastptr->negative=1;
        lastptr->SetBit(OP.symbol);
        break;
      }
      case opNotSymbolIgnoreCase:
      {
        lastptr->negative=1;
        lastptr->SetBit(OP.symbol);
        lastptr->SetBit(static_cast<rechar_t>(RE_TOUPPER(OP.symbol)));
        break;
      }
      case opNotSymbolRange:
      case opNotSymbolMinRange:
      {
        if(OP.range.min==0)
        {
          haveLast=false;
          break;
        }
        lastptr->negative=1;
        lastptr->SetBit(OP.range.symbol);
        lastptr->SetBit(static_cast<rechar_t>(RE_TOUPPER(OP.range.symbol)));
        break;
      }
      case opSymbolRange:
      case opSymbolMinRange:
      {
        if(OP.range.min==0)
        {
          haveLast=false;
          break;
        }
        if(ignoreCase)
        {
          lastptr->SetBit(static_cast<rechar_t>(RE_TOLOWER(OP.range.symbol)));
          lastptr->SetBit(static_cast<rechar_t>(RE_TOUPPER(OP.range.symbol)));
        }else
        {
          lastptr->SetBit(OP.range.symbol);
        }
        break;
      }
      case opTypeRange:
      case opTypeMinRange:
      {
        if(OP.range.min==0)
        {
          haveLast=false;
          break;
        }
        lastptr->addType(static_cast<unsigned char>(OP.range.type));
        break;
      }
      case opNotTypeRange:
      case opNotTypeMinRange:
      {
        if(OP.range.min==0)
        {
          haveLast=false;
          break;
        }
        lastptr->addNotType(static_cast<unsigned char>(OP.range.type));
        break;
      }
      case opClassRange:
      case opClassMinRange:
      {
        if(OP.range.min==0)
        {
          haveLast=false;
          break;
        }
        *lastptr+=*OP.range.symbolclass;
        break;
      }
      default:
        haveLast=false;
        break;
    }
  }
  bool haveAlt=false;
  bool firstOp=true;
  for(op=code;;op=op->next)
  {
    switch(OP.op)
    {
      default:
      {
        return 0;
      }
      case opType:
      {
        //for(i=0;i<256;i++)if(RE_ISTYPE(i,OP.type))first[i]=1;
        firstptr->addType(static_cast<unsigned char>(OP.type));
        break;
      }
      case opNotType:
      {
        //for(i=0;i<256;i++)if(!(RE_ISTYPE(i,OP.type)))first[i]=1;
        firstptr->addNotType(static_cast<unsigned char>(OP.type));
        break;
      }
      case opSymbol:
      {
        //first[OP.symbol]=1;
        firstptr->SetBit(OP.symbol);
        break;
      }
      case opString:
      {
        firstptr->SetBit(OP.str.str[0]);
        if(firstOp)firstStr=op;
        break;
      }
      case opStringIgnoreCase:
      {
        firstptr->SetBit(OP.str.str[0]);
        firstptr->SetBit(static_cast<rechar_t>(RE_TOUPPER(OP.str.str[0])));
        if(firstOp)firstStr=op;
        break;
      }
      case opSymbolIgnoreCase:
      {
        firstptr->SetBit(OP.symbol);
        firstptr->SetBit(static_cast<rechar_t>(RE_TOUPPER(OP.symbol)));
        break;
      }
      case opSymbolClass:
      {
//        for(i=0;i<256;i++)
//        {
//          if(GetBit(OP.symbolclass,i))first[i]=1;
//        }
        *firstptr+=*OP.symbolclass;
        break;
      }
      case opNamedBracket:
      case opOpenBracket:
      {
        if(OP.bracket.nextalt)
        {
          haveAlt=true;
          jumps[jumpcount++]=OP.bracket.nextalt;
          firstOp=false;
        }
        continue;
      }
      case opClosingBracket:
      {
        continue;
      }
      case opAlternative:
      {
        return 0;
      }
      case opNotSymbol:
      {
        if(firstptr->GetBit(OP.symbol))
        {
          return 0;
        }
        firstptr->Reset();
        firstptr->negative=1;
        firstptr->SetBit(OP.symbol);
        break;
      }
      case opNotSymbolIgnoreCase:
      {
        if(firstptr->GetBit(OP.symbol) || firstptr->GetBit(static_cast<rechar_t>(RE_TOUPPER(OP.symbol))))
        {
          return 0;
        }
        firstptr->Reset();
        firstptr->negative=1;
        firstptr->SetBit(OP.symbol);
        firstptr->SetBit(static_cast<rechar_t>(RE_TOUPPER(OP.symbol)));
        break;
      }
      case opNotSymbolRange:
      case opNotSymbolMinRange:
      {
        if(ignoreCase)
        {
          if(firstptr->GetBit(OP.range.symbol) || firstptr->GetBit(static_cast<rechar_t>(RE_TOUPPER(OP.range.symbol))))
          {
            return false;
          }
        }else
        {
          if(firstptr->GetBit(OP.range.symbol))
          {
            return false;
          }
        }
        firstptr->Reset();
        firstptr->negative=1;
        firstptr->SetBit(OP.range.symbol);
        firstptr->SetBit(static_cast<rechar_t>(RE_TOUPPER(OP.range.symbol)));
        if(OP.range.min==0){firstOp=false;continue;}
        break;
      }
      case opSymbolRange:
      case opSymbolMinRange:
      {
        if(ignoreCase)
        {
          firstptr->SetBit(static_cast<rechar_t>(RE_TOLOWER(OP.range.symbol)));
          firstptr->SetBit(static_cast<rechar_t>(RE_TOUPPER(OP.range.symbol)));
        }else
        {
          firstptr->SetBit(OP.range.symbol);
        }
        if(OP.range.min==0){firstOp=false;continue;}
        break;
      }
      case opTypeRange:
      case opTypeMinRange:
      {
        /*for(i=0;i<256;i++)
        {
          if(RE_ISTYPE(i,OP.range.type))first[i]=1;
        }*/
        firstptr->addType(static_cast<unsigned char>(OP.range.type));
        if(OP.range.min==0){firstOp=false;continue;}
        break;
      }
      case opNotTypeRange:
      case opNotTypeMinRange:
      {
        /*for(i=0;i<256;i++)
        {
          if(!(RE_ISTYPE(i,OP.range.type)))first[i]=1;
        }*/
        firstptr->addNotType(static_cast<unsigned char>(OP.range.type));
        if(OP.range.min==0){firstOp=false;continue;}
        break;
      }
      case opClassRange:
      case opClassMinRange:
      {
        /*for(i=0;i<256;i++)
        {
          if(GetBit(OP.range.symbolclass,i))first[i]=1;
        }*/
        *firstptr+=*OP.range.symbolclass;
        if(OP.range.min==0){firstOp=false;continue;}
        break;
      }
      case opBracketRange:
      case opBracketMinRange:
      {
        if(OP.range.min==0)return 0;
        if(OP.range.bracket.nextalt)
        {
          haveAlt=true;
          jumps[jumpcount++]=OP.range.bracket.nextalt;
          firstOp=false;
        }
        continue;
      }
      //case opLookAhead:
      //case opNotLookAhead:
      //case opLookBehind:
      //case opNotLookBehind:
      case opRegExpEnd:return 0;
    }
    if(jumpcount>0)
    {
      op=jumps[--jumpcount];
      if(OP.op==opAlternative && OP.alternative.nextalt)
      {
        jumps[jumpcount++]=OP.alternative.nextalt;
      }
      continue;
    }
    break;
  }
  if(haveAlt)firstStr=0;
  haveFirst=true;
  return true;
}

#undef OP
#undef MINSKIP

template <typename INPUT_ITER>
inline bool RegExp::Search(INPUT_ITER textstart,MatchInfo& mi)
{
  INPUT_ITER textend=reStrEnd(textstart);
  return SearchEx(textstart,textstart,textend,mi);
}

template <typename INPUT_ITER>
inline bool RegExp::Search(INPUT_ITER textstart,INPUT_ITER textend,MatchInfo& mi)
{
  return SearchEx(textstart,textstart,textend,mi);
}

template <typename INPUT_ITER>
inline bool RegExp::BMSearch(PREOpCode sop,INPUT_ITER& str,INPUT_ITER end)
{
  rechar_t* sstr=sop->str.str;
  size_t l=sop->str.length-1;
  size_t i;
  size_t* jtbl=sop->str.bmskip;
  if(sop->op==opString)
  {
    while(str<end)
    {
      for(i=l;sstr[i]==str[i];--i)
      {
        if(i==0)return true;
      }
      if(static_cast<unsigned char>(str[l])<128)
      {
        str+=jtbl[static_cast<unsigned char>(str[l])];
      }else
      {
        ++str;
      }
    }
  }else
  {
    while(str<end)
    {
      for(i=l;sstr[i]==RE_TOLOWER(static_cast<rechar_t>(str[i]));--i)
      {
        if(i==0)return true;
      }
      if(static_cast<unsigned char>(str[l])<128)
      {
        str+=jtbl[static_cast<unsigned char>(str[l])];
      }else
      {
        ++str;
      }
    }
  }
  return false;
}

template <typename INPUT_ITER>
inline bool RegExp::SearchEx(INPUT_ITER datastart,INPUT_ITER textstart,INPUT_ITER textend,MatchInfo& mi)
{
  INPUT_ITER str=textstart;
  if(minLength!=0 && static_cast<size_t>(textend-textstart)<minLength)return false;
  if(code->bracket.nextalt==0 && code->next->op==opDataStart)
  {
    return innerMatch(datastart,str,textend,&mi);
  }
  if(code->bracket.nextalt==0 && code->next->op==opDataEnd && code->next->next->op==opClosingBracket)
  {
    mi.brCount=1;
    mi.br[0].start=(int)(textend-textstart);
    mi.br[0].end=mi.br[0].start;
    return true;
  }
  if(textend-textstart<256 && haveLast)
  {
    --textend;
    while(textend>=textstart && !lastptr->GetBit(static_cast<rechar_t >(*textend)))--textend;
    if(textend<textstart)return false;
    ++textend;
  }
  bool res=false;
  if(firstStr)
  {
    do{
      if(!BMSearch(firstStr,str,textend))return false;
      if(0!=(res=innerMatch(datastart,str,textend,&mi)))
      {
        break;
      }
      ++str;
    }while(str<textend);
  }else
  if(haveFirst)
  {
    do{
      while(!firstptr->GetBit(static_cast<rechar_t >(*str)) && str<textend)++str;
      if(str<textend && 0!=(res=innerMatch(datastart,str,textend,&mi)))
      {
        break;
      }
      ++str;
    }while(str<textend);
    /*if(!res && innerMatch(datastart,str,textend,mi))
    {
      res=1;
    }*/
  }else
  {
    do{
      if(0!=(res=innerMatch(datastart,str,textend,&mi)))
      {
        break;
      }
      ++str;
    }while(str<textend);
  }
  if(res)
  {
    SMatch* match=mi.br;
    for(size_t i=0;i<mi.brCount;i++)
    {
      if(match[i].start==-1 || match[i].end==-1 || match[i].start>match[i].end)
      {
        match[i].start=match[i].end=-1;
      }
    }
  }
  return res;
}

/*
void RegExp::TrimTail(prechar& strend)
{
  if(havelookahead)return;
  if(!code || code->bracket.nextalt)return;
  PREOpCode op=code->bracket.pairindex->prev;
  while(OP.op==opClosingBracket)
  {
    if(OP.bracket.pairindex->op!=opOpenBracket)return;
    if(OP.bracket.pairindex->bracket.nextalt)return;
    op=op->prev;
  }
  strend--;
  switch(OP.op)
  {
    case opSymbol:
    {
      while(strend>=start && *strend!=OP.symbol)strend--;
      break;
    }
    case opNotSymbol:
    {
      while(strend>=start && *strend==OP.symbol)strend--;
      break;
    }
    case opSymbolIgnoreCase:
    {
      while(strend>=start && RE_TOLOWER(*strend)!=OP.symbol)strend--;
      break;
    }
    case opNotSymbolIgnoreCase:
    {
      while(strend>=start && RE_TOLOWER(*strend)==OP.symbol)strend--;
      break;
    }
    case opType:
    {
      while(strend>=start && !(RE_ISTYPE(*strend,OP.type)))strend--;
      break;
    }
    case opNotType:
    {
      while(strend>=start && RE_ISTYPE(*strend,OP.type))strend--;
      break;
    }
    case opSymbolClass:
    {
      while(strend>=start && !GetBit(OP.symbolclass,*strend))strend--;
      break;
    }
    case opSymbolRange:
    case opSymbolMinRange:
    {
      if(OP.range.min==0)break;
      if(ignorecase)
      {
        while(strend>=start && *strend!=OP.range.symbol)strend--;
      }else
      {
        while(strend>=start && RE_TOLOWER(*strend)!=OP.range.symbol)strend--;
      }
      break;
    }
    case opNotSymbolRange:
    case opNotSymbolMinRange:
    {
      if(OP.range.min==0)break;
      if(ignorecase)
      {
        while(strend>=start && *strend==OP.range.symbol)strend--;
      }else
      {
        while(strend>=start && RE_TOLOWER(*strend)==OP.range.symbol)strend--;
      }
      break;
    }
    case opTypeRange:
    case opTypeMinRange:
    {
      if(OP.range.min==0)break;
      while(strend>=start && !(RE_ISTYPE(*strend,OP.range.type)))strend--;
      break;
    }
    case opNotTypeRange:
    case opNotTypeMinRange:
    {
      if(OP.range.min==0)break;
      while(strend>=start && RE_ISTYPE(*strend,OP.range.type))strend--;
      break;
    }
    case opClassRange:
    case opClassMinRange:
    {
      if(OP.range.min==0)break;
      while(strend>=start && !GetBit(OP.range.symbolclass,*strend))strend--;
      break;
    }
    default:break;
  }
  strend++;
}

void RegExp::CleanStack()
{
  PStateStackPage tmp=firstpage->next,tmp2;
  while(tmp)
  {
    tmp2=tmp->next;
#ifdef RE_NO_NEWARRAY
      DeleteArray(reinterpret_cast<void**>(&tmp->stack),NULL);
#else
      delete [] tmp->stack;
#endif // RE_NO_NEWARRAY
    delete tmp;
    tmp=tmp2;
  }
}


#endif
*/

}
