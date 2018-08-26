#ifndef __KST_FORMAT_HPP__
#define __KST_FORMAT_HPP__

#include <stdio.h>
#include <vector>
#include <string>
#include <string.h>

namespace kst{

class FormatBuffer;

struct ArgsList{
  enum{STACK_ARG_LIST_SIZE=10};
  ArgsList():argArrayCount(0),useList(false)
  {
  }

  typedef void (*FmtMethod)(FormatBuffer&,const void*,int,int);

  template <class T>
  struct FmtGen
  {
      static void Fmt(FormatBuffer& buf,const void* val,int w,int p)
      {
          customformat(buf,*(const T*)val,w,p);
      }
  };

  enum ValueType{
      vtNone,
      vtChar,vtUChar,vtShort,vtUShort,
      vtInt,vtUInt,vtLong,vtULong,
      vtLongLong,vtULongLong,
      vtCharPtr,vtVoidPtr,
      vtString,vtCustom
  };

  struct ArgInfo{
      ValueType vt;
      union{
          char c;
          unsigned char uc;
          short s;
          unsigned short us;
          int i;
          unsigned int ui;
          long l;
          unsigned long ul;
          long long ll;
          unsigned long long ull;
          const char* sptr;
          const void* val;
      };
      FmtMethod fmt;

      ArgInfo():vt(vtNone),val(0),fmt(0){}

      ArgInfo(const ArgInfo& ai)
      {
        vt=ai.vt;
        ull=ai.ull;
        fmt=ai.fmt;
      }
      template <class T>
      ArgInfo(const T& t)
      {
          Init(t);
      }

      template <class T>
      void Init(const T& t)
      {
          val=&t;
          vt=vtCustom;
          fmt=FmtGen<T>::Fmt;
      }

      void Init(char v){vt=vtChar;c=v;}
      void Init(unsigned char v){vt=vtUChar;uc=v;}
      void Init(short v){vt=vtShort;i=0;s=v;}
      void Init(unsigned short v){vt=vtUShort;ui=0;us=v;}
      void Init(int v){vt=vtInt;i=v;}
      void Init(unsigned int v){vt=vtUInt;ui=v;}
      void Init(long v){vt=vtLong;l=v;}
      void Init(unsigned long v){vt=vtULong;ul=v;}
      void Init(long long v){vt=vtLongLong;ll=v;}
      void Init(unsigned long long v){vt=vtULongLong;ull=v;}
      void Init(const char* v){vt=vtCharPtr;sptr=v;}
      void Init(char* v){vt=vtCharPtr;sptr=v;}
      void Init(void* v){vt=vtVoidPtr;val=v;}
      void Init(const std::string& str){vt=vtString;sptr=str.c_str();}

      void Fmt(FormatBuffer& buf,int w,int p)const
      {
          fmt(buf,val,w,p);
      }
  };

  std::vector<ArgInfo> argList;
  ArgInfo argArray[STACK_ARG_LIST_SIZE];
  size_t argArrayCount;
  bool useList;

  template <class T>
  ArgsList& operator,(const T& t)
  {
    if(useList)
    {
      argList.push_back(ArgInfo(t));
    }else
    {
      if(argArrayCount==STACK_ARG_LIST_SIZE)
      {
        argList.assign(argArray,argArray+argArrayCount);
        argList.push_back(ArgInfo(t));
        useList=true;
      }else
      {
        argArray[argArrayCount].Init(t);
        argArrayCount++;
      }
    }

    return *this;
  }

  const ArgInfo* getArgInfo(size_t idx)const
  {
    if(useList)
    {
      if(idx>=argList.size())return 0;
      return &argList[idx];
    }else
    {
      if(idx>=argArrayCount)return 0;
      return argArray+idx;
    }
  }

  FormatBuffer* buf;
};


class FormatBuffer{
public:
  enum{FORMAT_RESULT_BUFFER_SIZE=256};
  FormatBuffer():ptr(buf),end(ptr),length(0),size(FORMAT_RESULT_BUFFER_SIZE),inHeap(false)
  {
    buf[0]=0;
  }
  ~FormatBuffer()
  {
    if(inHeap)delete [] ptr;
  }
  FormatBuffer(const FormatBuffer& src)
  {
    length=src.length;
    if(src.length+1>FORMAT_RESULT_BUFFER_SIZE)
    {
      inHeap=true;
      size=length+1;
      ptr=new char[size];
    }else
    {
      inHeap=false;
      size=FORMAT_RESULT_BUFFER_SIZE;
      ptr=buf;
    }
    memcpy(ptr,src.ptr,src.length);
    end=ptr+length;
    *end=0;
  }

  inline void Fill(char fillc,int w)
  {
    if(w<=0)return;
    if(length+w+1>size)
    {
      Expand(static_cast<size_t>(w));
    }
    length+=w;
    while(w--)
    {
      *end++=fillc;

    }
    *end=0;
  }

  inline void Append(const FormatBuffer& abuf)
  {
    size_t len=abuf.Length();
    if(length+len+1>size)
    {
      Expand(len);
    }

    memcpy(end,abuf.ptr,len);
    end+=len;
    length+=len;
    *end=0;
  }

  inline void Append(const char* str)
  {
    Append(str,strlen(str));
  }
  inline void Append(const char* str,size_t len)
  {

    if(length+len+1>size)
    {
      Expand(len);
    }

    memcpy(end,str,len);
    end+=len;
    length+=len;
    *end=0;
  }
  inline void Append(char c)
  {
    if(length+2>size)
    {
      Expand(1);
    }
    *end=c;
    length++;
    end++;
    *end=0;
  }
  inline char* End(size_t len)
  {
    if(length+len>=size)
    {
      Expand(len);
    }
    return end;
  }
  inline void Grow(size_t grow)
  {
    if(length+grow>size)
    {
      Expand(grow);
    }
    length+=grow;
    end+=grow;
    *end=0;
  }
  inline const char* Str()const
  {
    return ptr;
  }
  inline size_t Length()const
  {
    return length;
  }
  inline ArgsList& getArgList()
  {
      al.buf=this;
      return al;
  }
protected:
  inline void Expand(size_t len)
  {
    size=(length+len+1)*2;
    char* newptr=new char[size];
    memcpy(newptr,ptr,length+len);
    if(inHeap)delete [] ptr;
    inHeap=true;
    ptr=newptr;
    end=ptr+length;
  }

  char buf[FORMAT_RESULT_BUFFER_SIZE];
  char *ptr;
  char *end;
  size_t length;
  size_t size;
  bool inHeap;
  ArgsList al;
};

FormatBuffer& format(const ArgsList& a);

#define FORMAT(...) kst::format((kst::FormatBuffer().getArgList() , __VA_ARGS__)).Str()

#ifndef PRINTFMTOUT
#define PRINTFMTOUT stdout
#endif

#define printFmt(...) fprintf(PRINTFMTOUT,"%s",FORMAT(__VA_ARGS__))

}

#endif
