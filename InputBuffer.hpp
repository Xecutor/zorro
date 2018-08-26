#ifndef __ZORRO_INPUT_BUFFER_HPP__
#define __ZORRO_INPUT_BUFFER_HPP__

#include <exception>
#include <string>
#include <kst/Format.hpp>

namespace zorro {

class ReadBeyondEndException : public std::exception {
public:
    ReadBeyondEndException(size_t data, size_t pos, size_t bufSize)
    {
        msg = FORMAT("Attempt to read %{} bytes at pos %{}, but bufSize=%{}", data, pos, bufSize);
    }

    ~ReadBeyondEndException() noexcept override = default;

    const char* what() const noexcept override
    {
        return msg.c_str();
    }

protected:
    std::string msg;
};

class InputBuffer {
public:
    InputBuffer() : buf(nullptr), bufSize(0), pos(0)
    {
    }

    InputBuffer(const void* argBuf, size_t argBufSize) : buf((const uint8_t*) argBuf), bufSize(argBufSize), pos(0)
    {
    }

    InputBuffer(const InputBuffer&) = delete;
    void operator=(const InputBuffer&) = delete;

    void assign(const void* argBuf, size_t argBufSize)
    {
        buf = (const uint8_t*) argBuf;
        bufSize = argBufSize;
        pos = 0;
    }

    uint8_t get8()
    {
        if(pos + 1 > bufSize)
        {
            throw ReadBeyondEndException(1, pos, bufSize);
        }
        ++pos;
        return *buf++;
    }

    uint16_t get16()
    {
        if(pos + 2 > bufSize)
        {
            throw ReadBeyondEndException(2, pos, bufSize);
        }
        pos += 2;
        uint16_t rv = *buf;
        rv <<= 8;
        rv |= *++buf;
        ++buf;
        return rv;
    }

    uint32_t get32()
    {
        if(pos + 4 > bufSize)
        {
            throw ReadBeyondEndException(4, pos, bufSize);
        }
        pos += 4;
        uint32_t rv = *buf;
        rv <<= 8;
        rv |= *++buf;
        rv <<= 8;
        rv |= *++buf;
        rv <<= 8;
        rv |= *++buf;
        ++buf;
        return rv;
    }

    uint64_t get64()
    {
        if(pos + 8 > bufSize)
        {
            throw ReadBeyondEndException(8, pos, bufSize);
        }
        pos += 8;
        uint32_t rv = *buf;
        rv <<= 8;
        rv |= *++buf;
        rv <<= 8;
        rv |= *++buf;
        rv <<= 8;
        rv |= *++buf;
        rv <<= 8;
        rv |= *++buf;
        rv <<= 8;
        rv |= *++buf;
        rv <<= 8;
        rv |= *++buf;
        rv <<= 8;
        rv |= *++buf;
        ++buf;
        return rv;
    }

    const char* getCString()
    {
        const char* rv = (const char*) buf;
        while(*buf++)
        {
            if(++pos > bufSize)
            {
                throw ReadBeyondEndException(static_cast<size_t>(reinterpret_cast<const char*>(buf) - rv), pos,
                                             bufSize);
            }
        }
        if(++pos > bufSize)
        {
            throw ReadBeyondEndException(static_cast<size_t>(reinterpret_cast<const char*>(buf) - rv), pos, bufSize);
        }
        return rv;
    }

    const char* skip(size_t bytes)
    {
        if(pos + bytes > bufSize)
        {
            throw ReadBeyondEndException(bytes, pos, bufSize);
        }
        pos += bytes;
        const uint8_t* rv = buf;
        buf += bytes;
        return (const char*) rv;
    }

    void read(void* dst, size_t bytes)
    {
        memcpy(dst, skip(bytes), bytes);
    }

    size_t getLen() const
    {
        return bufSize;
    }

    size_t getPos() const
    {
        return pos;
    }

    const char* getBuf() const
    {
        return (const char*) buf;
    }

protected:

    const uint8_t* buf;
    size_t bufSize;
    size_t pos;
};

}

#endif
