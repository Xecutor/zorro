#ifndef __KST_FILE_HPP__
#define __KST_FILE_HPP__

#include <stdlib.h>
#include <algorithm>
#include <stdexcept>
#include <string>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>

#ifdef _WIN32
#define NOGDI
# include <winsock2.h>
# include <io.h>
#define lseek _lseek
#ifndef _SSIZE_T_DEFINED
typedef long ssize_t;
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4996)
#endif
#endif
#else
#define O_BINARY 0
# include <netinet/in.h>
# include <inttypes.h>
# include <unistd.h>
#endif

#ifndef O_LARGEFILE
#define O_LARGEFILE 0
#endif

#include <kst/Throw.hpp>

namespace kst{

class FileException:public std::exception{
public:
  enum{
    errOpenFailed,errReadFailed,errEndOfFile,
    errWriteFailed,errSeekFailed,errFileNotOpened,
    errRenameFailed,errUnlinkFailed,errFlushFailed
  };
  FileException(int err):errCode(err)
  {
    error=errno;
  }
  FileException(int err,const char* fn):errCode(err),fileName(fn)
  {
    error=errno;
  }
  FileException(const FileException& e)
  {
    errCode=e.errCode;
    error=e.error;
  }
  FileException& operator=(const FileException& e)throw()
  {
    errCode=e.errCode;
    error=e.error;
    return *this;
  }
  ~FileException()throw(){}
  const char* what()const throw()
  {
    if(fileName.length())
    {
      errbuf="(";
      errbuf+=fileName;
      errbuf+=")";
    }
    switch(errCode)
    {
      case errOpenFailed:
        errbuf="Failed to open file"+errbuf;
        errbuf+=":";
        errbuf+=strerror(error);
        return errbuf.c_str();
      case errReadFailed:
        errbuf="Failed to read from file"+errbuf;
        errbuf+=":";
        errbuf+=strerror(error);
        return errbuf.c_str();
      case errEndOfFile:
        errbuf="End of file reached"+errbuf;
        return errbuf.c_str();
      case errWriteFailed:
        errbuf="Failed to write to file"+errbuf;
        errbuf+=":";
        errbuf+=strerror(error);
        return errbuf.c_str();
      case errSeekFailed:
        errbuf="Failed to change file pointer"+errbuf;
        errbuf+=":";
        errbuf+=strerror(error);
        return errbuf.c_str();
      case errFileNotOpened:
        errbuf="Attempt to make an operation on file that is not opened"+errbuf;
        return errbuf.c_str();
      case errRenameFailed:
        errbuf="Failed to rename file"+errbuf;
        errbuf+=":";
        errbuf+=strerror(error);
        return errbuf.c_str();
      case errUnlinkFailed:
        errbuf="Failed to unlink file"+errbuf;
        errbuf+=":";
        errbuf+=strerror(error);
        return errbuf.c_str();
      default: return "Unknown error";
    }
  }
  int getErrorCode(){return errCode;}
  int getErrNo(){return error;}
protected:
  FileException();
  int errCode;
  int error;
  std::string fileName;
  mutable std::string errbuf;
};

class File{
public:
  typedef off_t offset_type;
  File():fd(-1)
  {
    flags=FLG_BUFFERED;
    buffer=initBuffer;
    bufferSize=sizeof(initBuffer);
    bufferPosition=0;
    fileSize=0;
  }
  ~File()
  {
    Close();
  }

  void Swap(File& swp)
  {
    if(flags&FLG_RDBUF)flags&=~FLG_RDBUF;
    if(flags&FLG_WRBUF)Flush();
    if(swp.flags&FLG_RDBUF)swp.flags&=~FLG_RDBUF;
    if(swp.flags&FLG_WRBUF)swp.Flush();

    if(buffer!=initBuffer || swp.buffer!=swp.initBuffer)
    {
      std::swap(buffer,swp.buffer);
    }

    std::swap(fd,swp.fd);
    std::swap(bufferSize,swp.bufferSize);
    std::swap(fileSize,swp.fileSize);
    std::swap(bufferPosition,swp.bufferPosition);
    std::swap(filename,swp.filename);
    std::swap(flags,swp.flags);
  }


  const std::string& getFileName() const
  {
    return filename;
  }


  void ROpen(const char* fn)
  {
    Close();
    filename=fn;
    fd=open(fn,O_BINARY|O_RDONLY|O_LARGEFILE,0644);
    if(fd==-1)throw FileException(FileException::errOpenFailed,fn);
  }
  void WOpen(const char* fn)
  {
    Close();
    filename=fn;
    fd=open(fn,O_BINARY|O_WRONLY|O_LARGEFILE|O_CREAT,0644);
    if(fd==-1)throw FileException(FileException::errOpenFailed,fn);
  }
  void RWOpen(const char* fn)
  {
    Close();
    filename=fn;
    fd=open(fn,O_BINARY|O_RDWR|O_LARGEFILE,0644);
    if(fd==-1)throw FileException(FileException::errOpenFailed,fn);
  }
  void RWCreate(const char* fn)
  {
    Close();
    filename=fn;
    fd=open(fn,O_BINARY|O_CREAT|O_RDWR|O_TRUNC|O_LARGEFILE,0644);
    if(fd==-1)throw FileException(FileException::errOpenFailed,fn);
  }
  void Append(const char* fn)
  {
    Close();
    filename=fn;
    fd=open(fn,O_BINARY|O_CREAT|O_WRONLY|O_APPEND|O_LARGEFILE,0644);
    if(fd==-1)throw FileException(FileException::errOpenFailed,fn);
    SeekEnd(0);
  }

  void SetUnbuffered()
  {
    if(fd!=-1)Flush();
    flags&=~FLG_BUFFERED;
    ResetBuffer();
  }

  void SetAutoBuffer(int sz=0)
  {
    if(sz!=0)
    {
      SetBuffer(sz);
    }
    flags|=FLG_AUTOBUFFERED;
  }

  void SetBuffer(size_t sz)
  {
    if(sz<bufferSize)return;
    if(fd!=-1)Flush();
    ResetBuffer();
    buffer=new char[sz];
    bufferSize=sz;
    flags|=FLG_BUFFERED;
  }

  void Close()
  {
    if(fd!=-1)
    {
      try{
        Flush();
      }catch(std::exception&)
      {
        flags&=~FLG_WRBUF;
      }
      close(fd);
      fd=-1;
      if(isBuffered())
      {
        ResetBuffer();
        fileSize=0;
      }
    }
  }

  bool isOpened()
  {
    return fd!=-1;
  }

  size_t Read(void* buf,size_t sz)
  {
    Check();
    if(flags&FLG_WRBUF)Flush();
    if((flags&FLG_BUFFERED) && (flags&FLG_RDBUF)!=FLG_RDBUF && sz<(size_t)bufferSize)
    {
      bufferUsed=read(fd,buffer,bufferSize);
      bufferPosition=0;
      flags|=FLG_RDBUF;
    }

    if(flags&FLG_RDBUF)
    {
      if(bufferPosition+sz<bufferUsed)
      {
        memcpy(buf,buffer+bufferPosition,sz);
        bufferPosition+=sz;
        return sz;
      }
      size_t avail=bufferUsed-bufferPosition;
      memcpy(buf,buffer+bufferPosition,avail);
      sz-=avail;
      char* tmp=(char*)buf;
      tmp+=avail;
      buf=tmp;
      if(sz<bufferSize)
      {
        bufferUsed=read(fd,buffer,bufferSize);
        if(bufferUsed<sz)
          throw FileException(FileException::errEndOfFile,filename.c_str());
        memcpy(buf,buffer,sz);
        bufferPosition=sz;
        return sz+avail;
      }
      bufferPosition=0;
      bufferUsed=0;

    }

    size_t rv=read(fd,buf,sz);
    if(rv!=sz)
    {
      if(errno)
        throw FileException(FileException::errReadFailed,filename.c_str());
      else
      {
        throw FileException(FileException::errEndOfFile,filename.c_str());
      }
    }
    return rv;
  }

  template <class T>
  offset_type XRead(T& t)
  {
    return Read(&t,sizeof(T));
  }
  void Write(const void* buf,size_t sz)
  {
    Check();
    if(flags&FLG_RDBUF)
    {
      long off = bufferUsed-bufferPosition;
      lseek(fd,-off,SEEK_CUR);
      flags&=~FLG_RDBUF;
    }

    if(flags&FLG_BUFFERED)
    {
      if(!(flags&FLG_WRBUF))
      {
        bufferPosition=0;
      }
      flags|=FLG_WRBUF;

      if(flags&FLG_AUTOBUFFERED)
      {
        if(bufferPosition+sz>bufferSize)
        {
          ResizeBuffer(bufferPosition+sz);
        }
      }
      if(bufferPosition+sz>bufferSize)
      {
        size_t towr=bufferSize-bufferPosition;
        memcpy(buffer+bufferPosition,buf,towr);
        const char* tmp=(const char*)buf;
        tmp+=towr;
        buf=tmp;
        sz-=towr;
        bufferPosition=0;
        if(write(fd,buffer,bufferSize)!=(ssize_t)bufferSize)
        {
          throw FileException(FileException::errWriteFailed,filename.c_str());
        }
        if(sz>bufferSize)
        {
          if(write(fd,buf,sz)!=(ssize_t)sz)
          {
            throw FileException(FileException::errWriteFailed,filename.c_str());
          }
        }else
        {
          memcpy(buffer,buf,sz);
          bufferPosition=sz;
        }
      }else
      {
        memcpy(buffer+bufferPosition,buf,sz);
        bufferPosition+=sz;
      }
    }else
    {
      if(write(fd,buf,sz)!=(ssize_t)sz)throw FileException(FileException::errWriteFailed,filename.c_str());
    }

  }
  template <class T>
  void XWrite(const T& t)
  {
    Write(&t,sizeof(T));
  }

  void ZeroFill(size_t sz)
  {
    char buf[8192]={0,};
    size_t blksz;
    while(sz>0)
    {
      blksz=sz>sizeof(buf)?sizeof(buf):sz;
      Write(buf,blksz);
      sz-=blksz;
    }
  }

  void Seek(offset_type off,int whence=SEEK_SET)
  {
    Check();
    Flush();
    if(flags&FLG_RDBUF)
    {
      if(whence==SEEK_CUR)
      {
        if(bufferPosition+off>0 && bufferPosition+off<bufferUsed)
        {
          bufferPosition+=off;
          return;
        }
        off-=bufferUsed-bufferPosition;
      }
      flags&=~FLG_RDBUF;

    }
    if(lseek(fd,off,whence)==-1)throw FileException(FileException::errSeekFailed,filename.c_str());
  }

  void SeekEnd(offset_type off)
  {
    Seek(off,SEEK_END);
  }

  void SeekCur(offset_type off)
  {
    Seek(off,SEEK_CUR);
  }

  offset_type Size()
  {
    Check();
    Flush();
#ifdef __GNUC__
    struct stat st;
#else
    struct ::stat st;
#endif
    fstat(fd,&st);
    return st.st_size;
  }

  offset_type Pos()
  {
    Check();
    offset_type realPos=lseek(fd,0,SEEK_CUR);
    if(flags&FLG_RDBUF)
    {
      return realPos-bufferUsed+bufferPosition;
    }
    if(flags&FLG_WRBUF)
    {
      return realPos+bufferPosition;
    }
    return realPos;
  }

  uint64_t ReadInt64()
  {
    uint64_t t;
    XRead(t);
    return t;
  }

  uint64_t ReadNetInt64()
  {
    uint32_t h=ReadNetInt32();
    uint32_t l=ReadNetInt32();
    return (((uint64_t)h)<<32)|l;
  }

  uint32_t ReadInt32()
  {
    uint32_t t;
    XRead(t);
    return t;
  }

  uint32_t ReadNetInt32()
  {
    return ntohl(ReadInt32());
  }

  uint16_t ReadInt16()
  {
    uint16_t t;
    XRead(t);
    return t;
  }
  uint16_t ReadNetInt16()
  {
    return ntohs(ReadInt16());
  }

  uint8_t ReadByte()
  {
    uint8_t b;
    XRead(b);
    return b;
  }

  void WriteInt32(uint32_t t)
  {
    XWrite(t);
  }
  void WriteNetInt32(uint32_t t)
  {
    WriteInt32(htonl(t));
  }

  void WriteInt16(uint16_t t)
  {
    XWrite(t);
  }
  void WriteNetInt16(uint16_t t)
  {
    WriteInt16(htons(t));
  }

  void WriteInt64(uint64_t t)
  {
    XWrite(t);
  }
  void WriteNetInt64(uint64_t t)
  {
    uint32_t h=htonl((uint32_t)((t>>32)&0xFFFFFFFFUL));
    uint32_t l=htonl((uint32_t)(t&0xFFFFFFFFUL));
    XWrite(h);
    XWrite(l);
  }

  void WriteByte(uint8_t t)
  {
    XWrite(t);
  }

  void Flush()
  {
    Check();
    if(flags&FLG_WRBUF)
    {
      if(bufferPosition>0)
      {
        if(write(fd,buffer,bufferPosition)!=(ssize_t)bufferPosition)
          throw FileException(FileException::errWriteFailed,filename.c_str());
        bufferPosition=0;
      }
      flags&=~FLG_WRBUF;
    }
  }

  bool ReadLine(std::string& str)
  {
    Check();
    str="";
    char c;
    offset_type pos=Pos();
    offset_type sz=Size();
    while(pos<sz)
    {
      pos+=Read(&c,1);
      if(c==0x0a)
      {
        if(str.length() && str[str.length()-1]==0x0d)str.erase(str.length()-1);
        return true;
      }
      str+=c;
    }
    return false;
  }

  void WriteLine(const std::string& str)
  {
    Write(str.c_str(),str.length());
  }

  template <size_t SZ>
  void WriteFixedString(const char (&str)[SZ])
  {
    char buf[SZ]={0,};
    memcpy(buf,str,std::min((size_t)SZ,strlen(str)));
    Write(buf,SZ);
  }

  template <size_t SZ>
  void WriteFixedString(const std::string& str)
  {
    char buf[SZ]={0,};
    memcpy(buf,str.c_str(),std::min((size_t)SZ,str.length()));
    Write(buf,SZ);
  }

  template <unsigned int SZ>
  void ReadFixedString(char (&str)[SZ])
  {
    Read(str,SZ);
    str[SZ-1]=0;
  }

  template <unsigned int SZ>
  void ReadFixedString(std::string& str)
  {
    char buf[SZ+1];
    buf[SZ-1]=0;
    Read(buf,SZ);
    str=buf;
  }


  void Rename(const char* newname)
  {
    if(rename(filename.c_str(),newname)!=0)throw FileException(FileException::errRenameFailed,filename.c_str());
    filename=newname;
  }

  static bool Exists(const char* file)
  {
#ifdef _WIN32
    struct _stat st;
    return _stat(file,&st)==0;
#else
#ifdef __GNUC__
    struct stat st;
#else
    struct ::stat st;
#endif
    return ::stat(file,&st)==0;
#endif
  }

  static void Rename(const char* oldname,const char* newname)
  {
    if(rename(oldname,newname)!=0)throw FileException(FileException::errRenameFailed,oldname);
  }

  static void Unlink(const char* fn)
  {
    if(unlink(fn)!=0)throw FileException(FileException::errUnlinkFailed,fn);
  }

protected:
  enum{INIT_BUFFER_SIZE=4096};
  enum{MAX_BUFFER_SIZE=64*1024*1024};
  enum{
    FLG_BUFFERED=1,
    FLG_RDBUF=2,
    FLG_WRBUF=4,
    FLG_AUTOBUFFERED=8
  };
  char  initBuffer[INIT_BUFFER_SIZE];
  char *buffer;
  int   fd;
  size_t bufferSize,bufferUsed;
  size_t bufferPosition;
  int   flags;
  offset_type fileSize;
  std::string filename;

  bool isBuffered()
  {
    return flags&FLG_BUFFERED;
  }

  void Check()
  {
    if(fd==-1)
    {
      throw FileException(FileException::errFileNotOpened,filename.c_str());
    }
  }

  void ResetBuffer()
  {
    if(buffer!=initBuffer)delete [] buffer;
    buffer=initBuffer;
    bufferSize=sizeof(initBuffer);
    bufferPosition=0;
  }

  void ResizeBuffer(size_t newsz)
  {
    newsz+=newsz/4;
    if(newsz>MAX_BUFFER_SIZE || newsz<=0)
    {
      KSTHROW("File buffer size exceeded limit:%d/%d",(unsigned long)newsz,(int)MAX_BUFFER_SIZE);
    }
    char* newbuf=new char[newsz];
    memcpy(newbuf,buffer,bufferSize);
    delete [] buffer;
    buffer=newbuf;
    bufferSize=newsz;
  }
};

}

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif
