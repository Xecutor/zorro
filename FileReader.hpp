#ifndef __ZORRO_FILEREADER_HPP__
#define __ZORRO_FILEREADER_HPP__

#include <stdio.h>
#include <vector>
#include <string>
#include <map>
#include <set>

#ifdef __SunOS_5_9
#include <inttypes.h>
#else

#include <stdint.h>

#endif

#include "kst/Format.hpp"

namespace zorro {

class FileReader;

struct FileLocation {
    FileReader* fileRd;
    uint32_t line;
    uint32_t col;
    uint32_t offset;

    FileLocation() : fileRd(0), line(0), col(0), offset(0)
    {
    }

    FileLocation(uint32_t argLine, uint32_t argCol) : fileRd(0), line(argLine), col(argCol), offset(0)
    {
    }

    bool operator==(const FileLocation& rhs) const
    {
        return offset == rhs.offset && fileRd == rhs.fileRd;
    }

    bool operator<(const FileLocation& rhs) const
    {
        return line < rhs.line || (line == rhs.line && col < rhs.col);
    }

    bool operator<=(const FileLocation& rhs) const
    {
        return line < rhs.line || (line == rhs.line && col <= rhs.col);
    }

    bool operator>(const FileLocation& rhs) const
    {
        return line > rhs.line || (line == rhs.line && col > rhs.col);
    }

    bool operator>=(const FileLocation& rhs) const
    {
        return line > rhs.line || (line == rhs.line && col >= rhs.col);
    }


    inline std::string backTrace() const;
};

class FileRegistry {
public:
    struct Entry {
        int id;
        std::string name;
        std::vector<char> data;
    };
protected:
    typedef std::map<std::string, Entry*> EntryMap;
    EntryMap entryMap;
    typedef std::map<int, Entry*> IdMap;
    IdMap idMap;

    typedef std::vector<std::string> StringVector;
    StringVector searchPaths;

    typedef std::vector<Entry*> EntryVector;
    EntryVector entries;

    int idSeq;

    std::vector<FileReader*> readers;
public:

    FileRegistry()
    {
        searchPaths.push_back("");
        idSeq = 0;
    }

    ~FileRegistry();

    FileReader* newReader(Entry* argEntry = 0);

    Entry* openFile(const std::string& fileName)
    {
        EntryMap::iterator it = entryMap.find(fileName);
        if(it == entryMap.end())
        {
            for(StringVector::iterator spit = searchPaths.begin(), spend = searchPaths.end(); spit != spend; ++spit)
            {
                std::string fullPath = *spit;
                fullPath += fileName;
                FILE* f = fopen(fullPath.c_str(), "rb");
                if(f)
                {
                    Entry* e = new Entry;
                    entries.push_back(e);
                    e->id = ++idSeq;
                    e->name = fileName;
                    fseek(f, 0, SEEK_END);
                    size_t size = static_cast<size_t>(ftell(f));
                    fseek(f, 0, SEEK_SET);
                    e->data.resize(size);
                    fread(&e->data[0], size, 1, f);
                    fclose(f);
                    entryMap.insert(EntryMap::value_type(fileName, e));
                    idMap.insert(IdMap::value_type(e->id, e));
                    return e;
                }
            }
            return 0;
        }
        return it->second;
    }

    Entry* addEntry(const std::string& argName, const std::vector<char>& argData)
    {
        Entry* e = new Entry;
        entries.push_back(e);
        entryMap[argName] = e;
        e->id = ++idSeq;
        e->name = argName;
        e->data = argData;
        idMap.insert(IdMap::value_type(e->id, e));
        return e;
    }

    Entry* addEntry(const std::string& argName, const char* argData, size_t argDataLen)
    {
        Entry* e = new Entry;
        entries.push_back(e);
        entryMap[argName] = e;
        e->id = ++idSeq;
        e->name = argName;
        e->data.assign(argData, argData + argDataLen);
        idMap.insert(IdMap::value_type(e->id, e));
        return e;
    }

    Entry* getEntry(int id)
    {
        IdMap::iterator it = idMap.find(id);
        if(it == idMap.end())
        {
            return 0;
        }
        return it->second;
    }
};

class FileReader {
public:
    FileReader(FileRegistry* argOwner, FileRegistry::Entry* e = 0) : owner(argOwner)
    {
        if(e)
        {
            assignEntry(e);
        }
    }

    virtual ~FileReader()
    {
    }

    void setOwner(FileRegistry* argOwner)
    {
        owner = argOwner;
    }

    void assignEntry(FileRegistry::Entry* argEntry)
    {
        entry = argEntry;
        size = entry->data.size();
        buf = size ? &entry->data[0] : nullptr;
        loc.fileRd = this;
        loc.offset = 0;
        loc.line = 0;
        loc.col = 0;
    }

    char getNextChar()
    {
        if(isEof())
        {
            return 0;
        }
        char rv = buf[loc.offset++];
        if(rv == 0x0d || rv == 0x0a)
        {
            if(rv == 0x0d && loc.offset < size)
            {
                if(buf[loc.offset] == 0x0a)
                {
                    rv = 0x0a;
                    loc.offset++;
                }
            }
            loc.line++;
            loc.col = 0;
            return rv;
        }
        loc.col++;
        return rv;
    }

    char peek()
    {
        if(isEof())
        {
            return 0;
        }
        if(buf[loc.offset] == 0x0d && loc.offset + 1 < size && buf[loc.offset + 1] == 0x0a)
        {
            return buf[loc.offset + 1];
        }
        return buf[loc.offset];
    }

    template<class CheckFunc>
    const CheckFunc& readWhile(const CheckFunc& op)
    {
        while(!isEof() && op(buf[loc.offset]))
        {
            getNextChar();
        }
        return op;
    }

    bool readLine(std::string& line, int& eoln, bool append = false)
    {
        eoln = 0;
        if(isEof())
        {
            return false;
        }
        uint32_t startOff = loc.offset;
        while(loc.offset < size && buf[loc.offset] != 0x0a && buf[loc.offset] != 0x0d)
        {
            ++loc.offset;
        }
        if(append)
        {
            line.append(buf + startOff, loc.offset - startOff);
        } else
        {
            line.assign(buf + startOff, loc.offset - startOff);
        }
        if(loc.offset == size)
        {
            loc.col += loc.offset - startOff;
            eoln = 0;
            return true;
        }
        ++loc.line;
        loc.col = 0;
        eoln = buf[loc.offset];
        if(buf[loc.offset] == 0x0d && loc.offset + 1 < size && buf[loc.offset + 1] == 0x0a)
        {
            ++loc.offset;
            eoln <<= 8;
            eoln |= buf[loc.offset];
        }
        ++loc.offset;
        return true;
    }

    bool isEof()
    {
        if(loc.offset == size)
        {
            onEof();
        }
        return loc.offset == size;
    }

    const FileLocation& getLoc() const
    {
        return loc;
    }

    void setLoc(const FileLocation& argLoc)
    {
        loc = argLoc;
    }

    void setParent(FileLocation argParent)
    {
        parent = argParent;
    }

    FileLocation getParent() const
    {
        return parent;
    }

    int32_t getId() const
    {
        return entry->id;
    }

    FileRegistry::Entry* getEntry()
    {
        return entry;
    }

    std::string getValue(size_t start, size_t end)
    {
        return std::string(buf + start, buf + end);
    }

    const char* getBufferAt(size_t start)
    {
        return buf + start;
    }

    FileRegistry* getOwner()
    {
        return owner;
    }

    uint32_t getSize() const
    {
        return size;
    }

    void setSize(uint32_t argSize)
    {
        size = argSize;
    }

    void restoreSize()
    {
        buf = &entry->data[0];
        size = entry->data.size();
    }

    virtual void reset()
    {
        loc.offset = 0;
        loc.line = 0;
        loc.col = 0;
    }

protected:
    FileLocation loc;
    char* buf;
    size_t size;
    FileLocation parent;
    FileRegistry* owner;
    FileRegistry::Entry* entry;

    virtual void onEof()
    {
    }
};


//extern FileRegistry freg;

inline std::string FileLocation::backTrace() const
{
    FileLocation loc = *this;
    std::string result;
    if(!loc.fileRd)
    {
        result += "???:";
        char buf[32];
        snprintf(buf, sizeof(buf), "%u", loc.line + 1);
        result += buf;
        result += ':';
        snprintf(buf, sizeof(buf), "%u", loc.col + 1);
        result += buf;
        return result;
    }
    while(loc.fileRd)
    {
        if(!result.empty())
        {
            result += "\n";
        }
        FileRegistry::Entry* e = loc.fileRd->getEntry();
        result += e->name;
        result += ':';
        char buf[32];
        snprintf(buf, sizeof(buf), "%u", loc.line + 1);
        result += buf;
        result += ':';
        snprintf(buf, sizeof(buf), "%u", loc.col + 1);
        result += buf;
        loc = loc.fileRd->getParent();
    }
    return result;
}

inline void customformat(kst::FormatBuffer& buf, const FileLocation& pos, int, int)
{
    const std::string& bt = pos.backTrace();
    buf.Append(bt.c_str(), bt.length());
}


}
#endif
