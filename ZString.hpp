#ifndef __ZORRO_ZSTRING_HPP__
#define __ZORRO_ZSTRING_HPP__

#include <string.h>
#ifdef __SunOS_5_9
#include <inttypes.h>
#else
#include <stdint.h>
#endif
#include "RefBase.hpp"
#include "ZMemory.hpp"
#include <kst/Format.hpp>

namespace zorro{

class CStringWrap{
  mutable ZMemory* mem;
  mutable char* str;
  mutable uint32_t size;
  void operator=(const CStringWrap&);
public:
  CStringWrap(ZMemory* argMem,char* argStr,uint32_t argSize):mem(argMem),str(argStr),size(argSize){}
  CStringWrap(const CStringWrap& argOther):mem(argOther.mem),str(argOther.str),size(argOther.size)
  {
    argOther.mem=0;
  }
  void assign(ZMemory* argMem,char* argStr,uint32_t argSize)const
  {
    clear();
    mem=argMem;
    str=argStr;
    size=argSize;
  }
  ~CStringWrap()
  {
    clear();
  }
  void clear()const
  {
    if(mem)
    {
      mem->freeStr(str,size+1);
      mem=0;
    }
  }
  const char* c_str()const
  {
    return str;
  }
  uint32_t getLength()const
  {
    return size;
  }
};

class ZString:public RefBase{
public:
  typedef uint16_t char_type;
  ZString():data(0),size(0),hashCode(0xffffffff){}
  void init()
  {
    data=0;
    size=0;
    hashCode=0xffffffff;
  }
  void init(char* argStr,uint32_t argLength,bool argUnicode)
  {
    data=argStr;
    size=argLength;
    if(argUnicode)size|=unicodeFlag;
    hashCode=0xffffffff;//calcHash(data,data+argLength);
  }
  void assignConst(const char* argStr,uint32_t argLength=(uint32_t)-1)
  {
    if(argLength==(uint32_t)-1)
    {
      size=argStr?strlen(argStr):0;
    }else
    {
      size=argLength;
    }
    data=(char*)argStr;
    hashCode=0xffffffff;//calcHash(data,data+size);
  }

  void assign(ZMemory* mem,const char* argStr,uint32_t argLength=(uint32_t)-1)
  {
    if(argLength==(uint32_t)-1)
    {
      size=argStr?strlen(argStr):0;
    }else
    {
      size=argLength;
    }
    data=mem->allocStr(size+1);
    memcpy(data,argStr,size);
    data[size]=0;
    hashCode=0xffffffff;//calcHash(data,data+size);
  }

  void assign(ZMemory* mem,const uint16_t* argStr,uint32_t argLengthInBytes)
  {
    size=argLengthInBytes;
    data=mem->allocStr(size);
    memcpy(data,argStr,size);
    hashCode=0xffffffff;//calcHash(data,data+size);
    size|=unicodeFlag;
  }
  bool operator==(const ZString& argStr)const;

  static int compare(const char* ptr1,int cs1,uint32_t sz1,const char* ptr2,int cs2,uint32_t sz2);

  static uint16_t getNextChar(const char*& ptr,int charSize)
  {
    if(charSize==1)return (unsigned char)*ptr++;
    uint16_t rv;
    unsigned char* rvptr=(unsigned char*)&rv;
    rvptr[0]=(unsigned char)*ptr++;
    rvptr[1]=(unsigned char)*ptr++;
    return rv;
  }
  static void putUcs2Char(char*& ptr,uint16_t symbol)
  {
    char* srcptr=(char*)&symbol;
    *ptr=srcptr[0];++ptr;
    *ptr=srcptr[1];++ptr;
  }
  static void utf8ToUcs2(const char*& ptr,char*& dst)
  {
    uint32_t c=(unsigned char)*ptr++;
    if(c>=0x80)
    {
      if((c>>5)==0x06)
      {
        c=((c<<6)&0x7ff)+(*ptr++ & 0x3f);
      }else if ((c>>4)==0x0e)
      {
        c=((c<<12)&0xffff)|((((uint32_t)(uint8_t)*ptr++)<<6)&0x0fff);
        c|=(*ptr++&0x3f);
      }else if ((c>>3)==0x1e)
      {
        c=((c<<18)&0x1fffff)+((((uint32_t)(uint8_t)*ptr++)<<12)&0x0003ffff);
        c|=(((uint32_t)(uint8_t)*ptr++)<<6)&0x0fff;
        c|=((uint32_t)(uint8_t)*ptr++)&0x3f;
      }
      else
      {
        c=' ';
      }
    }
    uint16_t v=(uint16_t)c;
    char* buf=(char*)&v;
    *dst++=*buf++;
    *dst++=*buf++;
  }
  static uint32_t calcU8Symbols(const char* ptr,uint32_t len);
  static int ucs2ToUtf8(uint16_t symbol,char* buf);

  uint32_t calcUtf8Length()const;

  static uint32_t calcUtf8Length(const char* ptr,const char* end,int cs);


  bool operator==(const char* str)const
  {
    return equalsTo(str,strlen(str));
  }
  int getCharSize()const
  {
    return size&unicodeFlag?2:1;
  }
  bool equalsTo(const char* str,size_t length)const
  {
    /*if(size!=length+1)return false;
    return memcmp(data,str,length)==0;*/
    return compare(data,getCharSize(),getLength(),str,1,length)==0;
  }

  int compare(const ZString& argStr)const
  {
    return compare(data,getCharSize(),getDataSize(),argStr.data,argStr.getCharSize(),argStr.getDataSize());
  }
  int compare(const char* argStr,size_t argLength)const
  {
    return compare(data,getCharSize(),getDataSize(),argStr,1,argLength);
  }

  void clear(ZMemory* mem)
  {
    if(data)
    {
      mem->freeStr(data,size&unicodeMask);
    }
    data=0;
    size=0;
  }

  static int64_t parseInt(const char* ptr);

  static double parseDouble(const char* ptr);

  /*static uint32_t calcHash(const char* data)
  {
    unsigned char* ptr=(unsigned char*)data;
    uint32_t hashCode=0x7654321**ptr;
    while(*ptr)
    {
      hashCode+=37*hashCode+*ptr;
      ptr++;
    }
    return hashCode;
  }*/

  static uint32_t calcHash(const char* data,const char* end)
  {
    unsigned char* ptr=(unsigned char*)data;
    uint32_t hashCode=0x7654321u*(*ptr);
    while(ptr!=(unsigned char*)end)
    {
      hashCode+=37*hashCode+*ptr;
      ptr++;
    }
    return hashCode;
  }

  uint32_t getSize()const
  {
    return size&unicodeFlag?size&unicodeMask:size+1;
  }

  uint32_t getDataSize()const
  {
    return size&unicodeFlag?size&unicodeMask:size;
  }


  uint32_t getLength()const
  {
    return size&unicodeFlag?(size&unicodeMask)/2:size;
  }

  bool isEmpty()
  {
    return data==0 || size==0;
  }

  uint32_t getHashCode()const
  {
    if(hashCode==0xffffffff && data)
    {
      hashCode=calcHash(data,data+getDataSize());
    }
    return hashCode;
  }

  ZString* copy(ZMemory* mem);

  ZString* substr(ZMemory* mem,uint32_t startPos,uint32_t len);

  ZString* erase(ZMemory* mem,uint32_t startPos,uint32_t len);

  ZString* insert(ZMemory* mem,uint32_t pos,ZString* str);

  static ZString* concat(ZMemory* mem,const ZString* s1,const ZString* s2);

  /* append to dst as unicode */
  void uappend(char*& dst)const;

  const char* getDataPtr()const
  {
    return data;
  }

  int find(const ZString& argStr,uint32_t startPos=0)const;
  const char* c_str(ZMemory* mem,const CStringWrap& wrap=CStringWrap(0,0,0))const;

  const char* c_substr(ZMemory* mem,uint32_t offset,uint32_t len,const CStringWrap& wrap=CStringWrap(0,0,0))const;

  uint16_t getCharAt(uint32_t idx)
  {
    if(idx>=getLength())
    {
      return 0;
    }
    if(getCharSize()==1)
    {
      return static_cast<uint8_t>(data[idx]);
    }else
    {
      return (reinterpret_cast<uint16_t*>(data))[idx];
    }
  }

protected:
  enum{
    unicodeFlag=0x80000000,
    unicodeMask=0x7fffffff
  };
  char* data;
  uint32_t size;
  mutable uint32_t hashCode;
  void operator=(const ZString&);
  ZString(const ZString&);
};

class ZStringRef{
protected:
  ZMemory* mem;
  ZString* str;
  void unref()
  {
    if(str && str->unref())
    {
      str->clear(mem);
      mem->freeZString(str);
    }
  }
public:
  ZStringRef(ZMemory* argMem=0,ZString* argStr=0):mem(argMem),str(argStr)
  {
    if(str)str->ref();
  }
  ZStringRef(ZMemory* argMem,const char* argStr):mem(argMem),str(mem->allocZString(argStr))
  {
    str->ref();
  }
  ZStringRef(const ZStringRef& argOther):mem(argOther.mem),str(argOther.str)
  {
    if(str)str->ref();
  }
  ~ZStringRef()
  {
    unref();
  }
  ZStringRef& operator=(const ZStringRef& argOther)
  {
    if(this==&argOther)
    {
      return *this;
    }
    unref();
    mem=argOther.mem;
    str=argOther.str;
    if(str)str->ref();
    return *this;
  }
  ZString* operator->()
  {
    return str;
  }
  ZString& operator*()
  {
    return *str;
  }
  const ZString* operator->()const
  {
    return str;
  }
  const ZString& operator*()const
  {
    return *str;
  }
  ZString* get()
  {
    return str;
  }
  const ZString* get()const
  {
    return str;
  }
  bool operator!()const
  {
    return !str;
  }
  operator bool()const
  {
    return str!= nullptr;
  }
  const char* c_str(const CStringWrap& wrap=CStringWrap(nullptr, nullptr,0))const
  {
    return str->c_str(mem,wrap);
  }

  bool operator==(const ZStringRef& other)const
  {
    return str->compare(*other.str)==0;
  }
  bool operator!=(const ZStringRef& other)const
  {
    return str->compare(*other.str)!=0;
  }

  friend ZStringRef operator+(ZStringRef s1,ZStringRef s2)
  {
    return ZStringRef(s1.mem,ZString::concat(s1.mem,s1.get(),s2.get()));
  }
  friend ZStringRef operator+(const char* s1,ZStringRef s2)
  {
    ZStringRef tmp=s2.mem->mkZString(s1);
    return ZStringRef(s2.mem,ZString::concat(s2.mem,tmp.get(),s2.get()));
  }
  friend ZStringRef operator+(ZStringRef s1,const char* s2)
  {
    ZStringRef tmp=s1.mem->mkZString(s2);
    return ZStringRef(s1.mem,ZString::concat(s1.mem,s1.get(),tmp.get()));
  }

};

inline void customformat(kst::FormatBuffer& buf,const zorro::ZStringRef& str,int,int)
{
  CStringWrap wrap(0,0,0);
  str.c_str(wrap);
  buf.Append(wrap.c_str(),wrap.getLength());
}

}

#endif
