#include <kst/Format.hpp>
#include <type_traits>

namespace kst{

template <typename inttype>
inline void fmtintsign(inttype val,FormatBuffer* buf,int w,bool lz)
{
  char tmp[64];
  bool negative=val<0;
  typename std::make_unsigned<inttype>::type uval=negative?-val:val;

  char* ptr=tmp+64;
  if(!uval)
  {
    *--ptr='0';
  }else
  while (uval)
  {
    *--ptr=(uval%10)+'0';
    uval/=10;
  }
  size_t l=tmp+64-ptr;
  if(w && !lz)
  {
    buf->Fill(' ',w-l-(negative?1:0));
  }
  if (negative)
  {
    buf->Append('-');
    if(w)w--;
  }
  if(w && lz)
  {
    buf->Fill('0',w-l);
  }
  buf->Append(ptr,(size_t)(tmp+64-ptr));
}

template <typename inttype>
inline void fmtintunsign(inttype val,FormatBuffer* buf,size_t w,bool lz)
{
  char tmp[64];
  char* ptr=tmp+64;
  if(!val)
  {
    *--ptr='0';
  }else
  while (val)
  {
    *--ptr=(val%10)+'0';
    val/=10;
  }
  if(w)
  {
    buf->Fill(lz?'0':' ',w-(tmp+64-ptr));
  }
  buf->Append(ptr,(size_t)(tmp+64-ptr));
}

template <typename inttype>
inline void fmtinthex(inttype val,FormatBuffer* buf,int w,bool lz,bool uc)
{
  static char sym[16]={'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};
  static char symuc[16]={'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};
  char* sptr=uc?symuc:sym;
  char tmp[64];
  char* ptr=tmp+64;
  if(!val)
  {
    *--ptr='0';
  }else
  while (val)
  {
    *--ptr=sptr[val&0xf];
    val>>=4;
  }
  if(w)
  {
    buf->Fill(lz?'0':' ',w-(tmp+64-ptr));
  }
  buf->Append(ptr,(size_t)(tmp+64-ptr));
}

FormatBuffer& format(const ArgsList& a)
{
    FormatBuffer* buf=a.buf;
    const ArgsList::ArgInfo* fmtArg=a.getArgInfo(0);
    if(!fmtArg || (fmtArg->vt!=ArgsList::vtCharPtr && fmtArg->vt!=ArgsList::vtString))return *buf;
    const char* fmt=fmtArg->sptr;
    const char* ptr=fmt;
    const char* str=strchr(fmt,'%');
    int nextIdx=1;
    while(str)
    {
        if(str[1]=='%')
        {
            buf->Append(ptr,str-ptr+1);
            str+=2;
        }else
        {
          buf->Append(ptr,str-ptr);
          str++;
          int idx=0;
          int w=0;
          int p=0;
          bool lz=false;
          int hex=0;

          if(*str=='{')
          {
            str++;
            if(*str=='x' || *str=='X')
            {
              hex=*str=='x'?1:2;
              str++;
            }

            if(*str>='0' && *str<='9')
            {
              while(*str>='0' && *str<='9')
              {
                idx=idx*10+(*str++-'0');
              }
              nextIdx=idx+1;
            }else
            {
              idx=nextIdx++;
            }
            if(*str==':')
            {
              str++;
              if(*str=='0')
              {
                lz=true;
                str++;
              }
              while(*str>='0' && *str<='9')
              {
                w=w*10+(*str++-'0');
              }
              if(*str==',' || *str=='.')
              {
                str++;
                while(*str>='0' && *str<='9')
                {
                  p=p*10+(*str++-'0');
                }
              }
            }
            while(*str && *str!='}')str++;
            if(*str)str++;
          }else
          {
            if(*str>='0' && *str<='9')
            {
              while(*str>='0' && *str<='9')
              {
                idx=idx*10+(*str++-'0');
              }
            }else
            {
              if(*str=='x' || *str=='X')
              {
                hex=*str=='x'?1:2;
              }
              if(*str=='l')
              {
                str++;
              }
              if(*str>='a' && *str<='z')
              {
                str++;
              }
              idx=nextIdx++;
            }
          }
          const ArgsList::ArgInfo* arg=a.getArgInfo(idx);
          if(arg)
          {
            switch (arg->vt)
            {
              case ArgsList::vtUChar:
              case ArgsList::vtChar:
              {
                if(w)buf->Fill(' ',w);
                buf->Append(arg->c);
              }break;
              case ArgsList::vtShort:
              case ArgsList::vtInt:
              {
                hex?fmtinthex ((unsigned int)arg->i,buf,w,lz,hex==2):
                fmtintsign(arg->i,buf,w,lz);
              }break;
              case ArgsList::vtUShort:
              case ArgsList::vtUInt:
              {
                hex?fmtinthex(arg->ui,buf,w,lz,hex==2):
                fmtintunsign(arg->ui,buf,w,lz);
              }break;
              case ArgsList::vtLong:
              {
                hex?fmtinthex((unsigned long)arg->l,buf,w,lz,hex==2):
                fmtintunsign(arg->l,buf,w,lz);
              }break;
              case ArgsList::vtULong:
              {
                hex?fmtinthex(arg->ul,buf,w,lz,hex==2):
                fmtintunsign(arg->ul,buf,w,lz);
              }break;
              case ArgsList::vtLongLong:
              {
                hex?fmtinthex((unsigned long long)arg->ll,buf,w,lz,hex==2):
                fmtintsign(arg->ll,buf,w,lz);
              }break;
              case ArgsList::vtULongLong:
              {
                hex?fmtinthex(arg->ull,buf,w,lz,hex==2):
                fmtintunsign(arg->ull,buf,w,lz);
              }break;
              case ArgsList::vtVoidPtr:
              {
                buf->Grow(snprintf(buf->End(18), 18, "%p",arg->val));
              }break;
              case ArgsList::vtCharPtr:
              case ArgsList::vtString:
              {
                if(!p)
                {
                  buf->Append(arg->sptr);
                }else
                {
                  int l=strlen(arg->sptr);
                  if(l>p)l=p;
                  buf->Append(arg->sptr,l);
                }
                if(w)buf->Fill(' ',w-strlen(arg->sptr));
              }break;
              case ArgsList::vtCustom:
              {
                arg->Fmt(*buf,w,p);
              }break;
              default:
              {
                fprintf(stderr,"unexpected vt=%d\n",arg->vt);
              }
            }
          }
        }
        ptr=str;
        str=strchr(str,'%');
    }
    buf->Append(ptr);
    return *buf;
}

}
