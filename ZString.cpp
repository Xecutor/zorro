#include "ZString.hpp"

namespace zorro {

int ZString::compare(const char* ptr1, int cs1, uint32_t sz1, const char* ptr2, int cs2, uint32_t sz2)
{
    if(cs1 == 1 && cs2 == 1)
    {
        int rv = memcmp(ptr1, ptr2, sz1 < sz2 ? sz1 : sz2);
        if(rv == 0)
        {
            if(sz1 < sz2)
            {
                rv = -1;
            } else if(sz1 > sz2)
            {
                rv = 1;
            }
        }
        return rv;
    }
    const char* end1 = ptr1 + sz1;
    const char* end2 = ptr2 + sz2;
    while(ptr1 != end1 && ptr2 != end2)
    {
        uint16_t c1 = getNextChar(ptr1, cs1);
        uint16_t c2 = getNextChar(ptr2, cs2);
        if(c1 < c2)
        {
            return -1;
        }
        if(c1 > c2)
        {
            return 1;
        }
    }
    if(ptr1 != end1)
    {
        return 1;
    }
    if(ptr2 != end2)
    {
        return -1;
    }
    return 0;
}

int64_t ZString::parseInt(const char* ptr)
{
    int64_t val = 0;
    bool neg = false;
    if(*ptr == '-')
    {
        ptr++;
        neg = true;
    }
    if(*ptr == '+')
    {
        ptr++;
    }
    if(ptr[1] == 'x' || ptr[1] == 'X')
    {
        ptr += 2;
        while(*ptr)
        {
            val <<= 4;
            val |= *ptr <= '9' ? *ptr - '0' : *ptr <= 'F' ? *ptr + 10 - 'A' : *ptr + 10 - 'a';
            ptr++;
        }
    } else
    {
        while(*ptr)
        {
            val *= 10;
            val += *ptr - '0';
            ptr++;
        }
    }
    return neg ? -val : val;
}

double ZString::parseDouble(const char* ptr)
{
    bool neg = false;
    if(*ptr == '-')
    {
        ptr++;
        neg = true;
    }
    double val = 0;
    while(*ptr >= '0' && *ptr <= '9')
    {
        val *= 10.0;
        val += 1.0 * (*ptr - '0');
        ptr++;
    }
    if(!*ptr || *ptr != '.')
    {
        return neg ? -val : val;
    }
    ptr++;
    double fract = 10.0;
    while(*ptr >= '0' && *ptr <= '9')
    {
        val += (*ptr - '0') / fract;
        fract *= 10.0;
        ptr++;
    }
    if(*ptr == 'e' || *ptr == 'E')
    {
        ptr++;
        int64_t mnt = parseInt(ptr);
        if(mnt > 0)
        {
            while(mnt--)
            {
                val *= 10.0;
            }
        } else
        {
            while(mnt++)
            {
                val /= 10.0;
            }
        }
    }
    return neg ? -val : val;
}

ZString* ZString::copy(ZMemory* mem)
{
    ZString* rv = mem->allocZString();
    if(!data || !size)
    {
        return rv;
    }
    uint32_t sz = getSize();
    rv->size = size;
    rv->hashCode = hashCode;
    rv->data = mem->allocStr(sz);
    memcpy(rv->data, data, sz);
    return rv;
}

int ZString::find(const ZString& argStr, uint32_t startPos) const
{
    if(startPos >= getLength())
    {
        return -1;
    }
    if(getCharSize() == 1 && argStr.getCharSize() == 1)
    {
        char* ptr = strstr(data + startPos, argStr.data);
        return ptr ? ptr - data : -1;
    } else
    {
        int cs1 = getCharSize();
        const char* ptr1 = data + startPos * cs1;
        const char* end1 = ptr1 + getDataSize();
        const char* ptr2 = argStr.data;
        int cs2 = argStr.getCharSize();
        const char* end2 = ptr2 + argStr.getDataSize();
        while(ptr1 != end1)
        {
            const char* p1 = ptr1;
            const char* p2 = ptr2;
            while(p2 != end2 && getNextChar(p1, cs1) == getNextChar(p2, cs2))
            {
            }
            if(p2 == end2)
            {
                return (p1 - data) / cs1;
            }
            ptr1 += cs1;
        }
        return -1;
    }
}

ZString* ZString::concat(ZMemory* mem, const ZString* s1, const ZString* s2)
{
    ZString* rv = mem->allocZString();
    if(s1->getCharSize() == 1 && s2->getCharSize() == 1)
    {
        rv->size = s1->size + s2->size;
        rv->data = mem->allocStr(rv->size + 1);
        memcpy(rv->data, s1->data, s1->size);
        memcpy(rv->data + s1->size, s2->data, s2->size);
        rv->data[rv->size] = 0;
        rv->hashCode = calcHash(rv->data, rv->data + rv->size);
    } else
    {
        rv->size = 2 * s1->getLength() + 2 * s2->getLength();
        char* ptr = rv->data = mem->allocStr(rv->size);
        rv->size |= unicodeFlag;
        if(s1->getCharSize() == 1)
        {
            char* s1ptr = s1->data;
            char* s1end = s1->data + s1->size;
            while(s1ptr < s1end)
            {
                uint16_t c = *((unsigned char*) s1ptr++);
                char* cp = (char*) &c;
                *ptr++ = cp[0];
                *ptr++ = cp[1];
            }
        } else
        {
            memcpy(ptr, s1->data, s1->getDataSize());
            ptr += s1->getSize();
        }
        if(s2->getCharSize() == 1)
        {
            char* s2ptr = s2->data;
            char* s2end = s2->data + s2->size;
            while(s2ptr < s2end)
            {
                uint16_t c = *((unsigned char*) s2ptr++);
                char* cp = (char*) &c;
                *ptr++ = cp[0];
                *ptr++ = cp[1];
            }
        } else
        {
            memcpy(ptr, s2->data, s2->getDataSize());
        }
    }
    return rv;
}

void ZString::uappend(char*& dst) const
{
    if(getCharSize() == 2)
    {
        uint32_t sz = getDataSize();
        memcpy(dst, data, sz);
        dst += sz;
        return;
    }
    char* ptr = data;
    char* end = ptr + getDataSize();
    while(ptr < end)
    {
        uint16_t c = *((unsigned char*) ptr++);
        char* cp = (char*) &c;
        *dst++ = cp[0];
        *dst++ = cp[1];
    }
}

const char* ZString::c_str(ZMemory* mem, const CStringWrap& wrap) const
{
    if(size == 0)
    {
        return "";
    }
    int cs;
    if((cs = getCharSize()) == 1)
    {
        wrap.assign(0, data, getLength());
        return data;
    }
    uint32_t rvSize = calcUtf8Length();
    char* rv = mem->allocStr(rvSize + 1);
    wrap.assign(mem, rv, rvSize);
    const char* ptr = data;
    const char* end = ptr + getDataSize();

    while(ptr != end)
    {
        uint16_t symbol = getNextChar(ptr, cs);
        rv += ucs2ToUtf8(symbol, rv);
    }
    *rv = 0;
    return wrap.c_str();
}

const char* ZString::c_substr(ZMemory* mem, uint32_t offset, uint32_t len, const CStringWrap& wrap) const
{
    if(offset >= getLength())
    {
        return "";
    }
    if(offset + len > getLength())
    {
        len = getLength() - offset;
    }
    int cs;
    if((cs = getCharSize()) == 1)
    {
        char* dst = mem->allocStr(len + 1);
        memcpy(dst, data + offset, len);
        dst[len] = 0;
        wrap.assign(mem, dst, len);
        return dst;
    }
    const char* ptr = data + offset * cs;
    const char* end = ptr + len * cs;
    uint32_t rvSize = calcUtf8Length(ptr, end, cs);
    char* rv = mem->allocStr(rvSize + 1);
    wrap.assign(mem, rv, rvSize);

    while(ptr != end)
    {
        uint16_t symbol = getNextChar(ptr, cs);
        rv += ucs2ToUtf8(symbol, rv);
    }
    *rv = 0;
    return wrap.c_str();

}

bool ZString::operator==(const ZString& argStr) const
{
    if(getLength() != argStr.getLength())
    {
        return false;
    }
    if(!(size & unicodeFlag) && !(argStr.size & unicodeFlag))
    {
        return memcmp(data, argStr.data, size) == 0;
    } else
    {
        int cs1 = getCharSize();
        int cs2 = argStr.getCharSize();
        const char* ptr1 = data;
        const char* ptr2 = argStr.data;
        const char* end1 = ptr1 + (size & unicodeMask);
        while(ptr1 != end1)
        {
            if(getNextChar(ptr1, cs1) != getNextChar(ptr2, cs2))
            {
                return false;
            }
        }
        return true;
    }
}

ZString* ZString::substr(ZMemory* mem, uint32_t startPos, uint32_t len)
{
    ZString* rv = mem->allocZString();
    int cs = getCharSize();
    if(!data || !size || startPos * cs >= getDataSize())
    {
        return rv;
    }
    if((startPos + len) > getLength())
    {
        len = getLength() - startPos;
    }
    len *= cs;
    startPos *= cs;
    bool hasUnicode = false;
    if(cs != 1)
    {
        const char* ptr = data + startPos;
        const char* end = ptr + len;
        uint16_t symbol;
        while(ptr < end)
        {
            symbol = getNextChar(ptr, cs);
            if(symbol > 127)
            {
                hasUnicode = true;
                break;
            }
        }
    }
    if(cs != 1 && !hasUnicode)
    {
        rv->size = len / cs;
        char* dst = rv->data = mem->allocStr(rv->size + 1);
        const char* ptr = data + startPos;
        const char* end = ptr + len;
        uint16_t symbol;

        while(ptr < end)
        {
            symbol = getNextChar(ptr, cs);
            *dst++ = static_cast<char>(symbol);
        }
        *dst = 0;
    } else
    {
        rv->size = len | (cs != 1 ? unicodeFlag : 0);
        rv->data = mem->allocStr(len + (cs == 1 ? 1 : 0));
        memcpy(rv->data, data + startPos, len);
    }
    if(cs == 1)
    {
        rv->data[rv->size] = 0;
    }
    rv->hashCode = 0xffffffff;//calcHash(rv->data,rv->data+rv->getDataSize());
    return rv;
}

ZString* ZString::erase(ZMemory* mem, uint32_t startPos, uint32_t len)
{
    ZString* rv = mem->allocZString();
    int cs = getCharSize();
    if(!data || !size || startPos * cs >= getDataSize())
    {
        return rv;
    }
    uint32_t tail;
    if((startPos + len) > getLength())
    {
        len = getLength() - startPos;
        tail = 0;
    } else
    {
        tail = getLength() - (startPos + len);
    }
    len *= cs;
    startPos *= cs;
    tail *= cs;
    uint32_t newLen = startPos + tail;

    bool hasUnicode = false;
    if(cs != 1)
    {
        const char* ptr = data;
        const char* end = ptr + startPos;
        uint16_t symbol;
        while(ptr < end)
        {
            symbol = getNextChar(ptr, cs);
            if(symbol > 127)
            {
                hasUnicode = true;
                break;
            }
        }
        if(!hasUnicode && tail)
        {
            ptr = data + startPos + len;
            end = ptr + tail;
            while(ptr < end)
            {
                symbol = getNextChar(ptr, cs);
                if(symbol > 127)
                {
                    hasUnicode = true;
                    break;
                }
            }
        }
    }
    if(cs != 1 && !hasUnicode)
    {
        rv->size = newLen / cs;
        char* dst = rv->data = mem->allocStr(rv->size + 1);
        const char* ptr = data;
        const char* end = ptr + startPos;
        uint16_t symbol;

        while(ptr < end)
        {
            symbol = getNextChar(ptr, cs);
            *dst++ = static_cast<char>(symbol);
        }
        if(tail)
        {
            ptr = data + startPos + len;
            end = ptr + tail;
            while(ptr < end)
            {
                symbol = getNextChar(ptr, cs);
                *dst++ = static_cast<char>(symbol);
            }
        }
        *dst = 0;
    } else
    {
        rv->size = newLen | (cs != 1 ? unicodeFlag : 0);
        rv->data = mem->allocStr(newLen + (cs == 1 ? 1 : 0));
        memcpy(rv->data, data, startPos);
        memcpy(rv->data + startPos, data + startPos + len, tail);
    }
    if(cs == 1)
    {
        rv->data[rv->size] = 0;
    }
    rv->hashCode = 0xffffffff;//calcHash(rv->data,rv->data+rv->getDataSize());
    return rv;
}

ZString* ZString::insert(ZMemory* mem, uint32_t pos, ZString* str)
{
    ZString* rv = mem->allocZString();
    int cs = getCharSize();
    int scs = str->getCharSize();
    pos *= cs;
    if(pos > size)
    {
        pos = size;
    }
    if(cs == scs)
    {
        rv->size = getDataSize() + str->getDataSize();
        rv->data = mem->allocStr(rv->size + (cs == 1 ? 1 : 0));
        char* ptr = rv->data;
        memcpy(ptr, data, pos);
        ptr += pos;
        memcpy(ptr, str->data, str->getDataSize());
        ptr += str->getDataSize();
        memcpy(ptr, data + pos, getDataSize() - pos);
        if(cs == 1)
        {
            rv->data[rv->size] = 0;
        } else
        {
            rv->size |= unicodeFlag;
        }
    } else
    {
        rv->size = getLength() * 2 + str->getLength() * 2;
        rv->data = mem->allocStr(rv->size);
        rv->size |= unicodeFlag;
        char* dst = rv->data;
        if(cs == 1)
        {
            char* ptr = data;
            char* end = data + pos;
            while(ptr < end)
            {
                putUcs2Char(dst, static_cast<char_type>(*ptr));
                ++ptr;
            }
        } else
        {
            memcpy(dst, data, pos);
            dst += pos;
        }
        if(scs == 1)
        {
            char* ptr = str->data;
            char* end = str->data + str->size;
            while(ptr < end)
            {
                putUcs2Char(dst, static_cast<char_type>(*ptr));
                ++ptr;
            }
        } else
        {
            memcpy(dst, str->data, str->getDataSize());
            dst += str->getDataSize();
        }
        if(cs == 1)
        {
            char* ptr = data + pos;
            char* end = data + size;
            while(ptr < end)
            {
                putUcs2Char(dst, static_cast<char_type>(*ptr));
                ++ptr;
            }
        } else
        {
            memcpy(dst, data + pos, getDataSize() - pos);
        }
    }
    return rv;
}

uint32_t ZString::calcU8Symbols(const char* ptr, uint32_t len)
{
    uint32_t rv = 0;
    const char* end = ptr + len;
    while(ptr < end)
    {
        unsigned char c = static_cast<unsigned char>(*ptr);
        if(c < 0x80)
        {
            ptr += 1;
        } else if((c >> 5) == 0x06)
        {
            ptr += 2;
        } else if((c >> 4) == 0x0e)
        {
            ptr += 3;
        } else if((c >> 3) == 0x1e)
        {
            ptr += 4;
        } else
        {
            //error?
            ptr += 1;
        }
        ++rv;
    }
    return rv;
}

int ZString::ucs2ToUtf8(uint16_t symbol, char* buf)
{
    int rv = 0;
    if(symbol < 0x80)
    {
        buf[rv++] = symbol & 0xff;
    } else if(symbol < 0x800)
    {
        buf[rv++] = ((symbol >> 6) | 0xc0) & 0xff;
        buf[rv++] = (symbol & 0x3f) | 0x80;
    } else
    {
        buf[rv++] = ((symbol >> 12) | 0xe0) & 0xff;
        buf[rv++] = ((symbol >> 6) & 0x3f) | 0x80;
        buf[rv++] = (symbol & 0x3f) | 0x80;
    }
    return rv;
}

uint32_t ZString::calcUtf8Length(const char* ptr, const char* end, int cs)
{
    uint16_t symbol;
    uint32_t rv = 0;
    while(ptr < end)
    {
        symbol = getNextChar(ptr, cs);
        if(symbol < 0x80)
        {
            ++rv;
        } else if(symbol < 0x800)
        {
            rv += 2;
        } else
        {
            rv += 3;
        }
    }
    return rv;
}

uint32_t ZString::calcUtf8Length() const
{
    int cs;
    if((cs = getCharSize()) == 1)
    {
        return getLength();
    }
    const char* ptr = data;
    const char* end = ptr + getDataSize();
    return calcUtf8Length(ptr, end, cs);
}

}
