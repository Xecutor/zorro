#ifndef __KST_LOGGER_HPP__
#define __KST_LOGGER_HPP__

#include <map>
#include <string>
#include <time.h>
#include <kst/File.hpp>
#include <kst/Format.hpp>

namespace kst{

class Logger{
public:
  enum LogLevel{
    LL_DEBUG,
    LL_INFO,
    LL_WARN,
    LL_ERROR,
    LL_NONE
  };
  static void Init(const char* filename,const char* inifile="logger.ini")
  {
    Logger* log=new Logger();
    log->setLogLevel(LL_INFO);
    cats.insert(CatMap::value_type("",log));
    if(File::Exists(inifile))
    {
      File ini;
      ini.ROpen(inifile);
      std::string l;
      while(ini.ReadLine(l))
      {
        if(!l.empty() && l[0]=='#')continue;
        std::string::size_type p=l.find('=');
        if(p==std::string::npos)continue;
        std::string n=l.substr(0,p);
        if(n=="ROOT")n="";
        std::string v=l.substr(p+1);
        Logger* log=getLogger(n);
        log->setLogLevel(llbyname(v));
      }
    }
    f.RWCreate(filename);
  }

  int getLogLevel()
  {
    return logLevel;
  }
  void setLogLevel(int newLogLevel)
  {
    logLevel=newLogLevel;
  }

  void log(int level,const FormatBuffer& buf)
  {
    if(level<logLevel)return;
    time_t t=time(NULL);
#ifdef _MSC_VER
    tm ltime;
    tm* lt=&ltime;
    localtime_s(&ltime, &t);
#else
    tm* lt=localtime(&t);
#endif
    FormatBuffer buf2;
    format((buf2.getArgList(),"%{}:%{:02}:%{:02}:%{:02} %{:10,10}:",llprefix(level),lt->tm_hour,lt->tm_min,lt->tm_sec,catName));
    f.Write(buf2.Str(),buf2.Length());
    f.Write(buf.Str(),buf.Length());
    f.Write("\n",1);
    f.Flush();
  }

  static Logger* getLogger(Logger* logger)
  {
    return logger;
  }

  static Logger* getLogger(const std::string& cat)
  {
    CatMap::iterator it=cats.find(cat);
    if(it!=cats.end())return it->second;
    std::string tmp=cat;
    int ll=-1;
    while(tmp.length())
    {
      std::string::size_type p=tmp.rfind('.');
      if(p==std::string::npos)break;
      tmp.erase(p);
      it=cats.find(tmp);
      if(it!=cats.end())
      {
        ll=it->second->getLogLevel();
        break;
      }
    }
    if(ll==-1)
    {
      it=cats.find("");
      ll=it->second->getLogLevel();
    }
    Logger* log=new Logger();
    log->logLevel=ll;
    log->catName=cat;
    cats.insert(CatMap::value_type(cat,log));
    return log;
  }

protected:
  int logLevel;
  std::string catName;

  typedef std::map<std::string,Logger*> CatMap;
  static CatMap cats;
  static File f;

  static char llprefix(int ll)
  {
    if(ll==LL_DEBUG)return 'D';
    if(ll==LL_INFO)return 'I';
    if(ll==LL_WARN)return 'W';
    if(ll==LL_ERROR)return 'E';
    return '?';
  }

  static LogLevel llbyname(const std::string& llname)
  {
    if(llname=="DEBUG")return LL_DEBUG;
    if(llname=="INFO")return LL_INFO;
    if(llname=="WARN")return LL_WARN;
    if(llname=="ERROR")return LL_ERROR;
    if(llname=="NONE")return LL_NONE;
    return LL_DEBUG;
  }

};

#ifdef NOLOGGER
#define LOGDEBUG(logger,fmt,...)
#define LOGINFO(logger,fmt,...)
#define LOGWARN(logger,fmt,...)
#define LOGERROR(logger,fmt,...)
#else
#define LOGDEBUG(logger,...) if(kst::Logger::getLogger(logger)->getLogLevel()>kst::Logger::LL_DEBUG);else kst::Logger::getLogger(logger)->log(kst::Logger::LL_DEBUG,format((kst::FormatBuffer().getArgList(),__VA_ARGS__)))
#define LOGINFO(logger,...) if(kst::Logger::getLogger(logger)->getLogLevel()>kst::Logger::LL_INFO);else kst::Logger::getLogger(logger)->log(kst::Logger::LL_INFO,format((kst::FormatBuffer().getArgList(),__VA_ARGS__)))
#define LOGWARN(logger,...) if(kst::Logger::getLogger(logger)->getLogLevel()>kst::Logger::LL_WARN);else kst::Logger::getLogger(logger)->log(kst::Logger::LL_WARN,format((kst::FormatBuffer().getArgList(),__VA_ARGS__)))
#define LOGERROR(logger,...) if(kst::Logger::getLogger(logger)->getLogLevel()>kst::Logger::LL_ERROR);else kst::Logger::getLogger(logger)->log(kst::Logger::LL_ERROR,format((kst::FormatBuffer().getArgList(),__VA_ARGS__)))
#endif

}

#endif
