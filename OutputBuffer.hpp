#ifndef __ZORRO_OUTPUT_BUFFER_HPP__
#define __ZORRO_OUTPUT_BUFFER_HPP__

#include <stddef.h>
#include <stdint.h>
#include <memory.h>
#include <exception>

namespace zorro {

class OutputBufferResizeException : public std::exception {
public:
    const char* what() const throw()
    {
        return "Attempt to resize external fixed eyeline::smpp::pdu::OutputBuffer";
    }
};

class OutputBuffer {
public:
    explicit OutputBuffer(size_t initSize = 128) : buf(0), bufSize(0), pos(0), bufMode(bmOwned)
    {
        resize(initSize);
    }

    OutputBuffer(void* argBuf, size_t argBufSize, bool argIsInitial = false) :
        buf((uint8_t*) argBuf), bufSize(argBufSize), pos(0), bufMode(argIsInitial ? bmExternalInitial : bmExternalFixed)
    {

    }

    ~OutputBuffer()
    {
        if(buf && bufMode == bmOwned)
        {
            delete[] buf;
        }
    }

    void* release()
    {
        void* rv = buf;
        buf = 0;
        bufSize = 0;
        pos = 0;
        bufMode = bmOwned;
        return rv;
    }

    void resize(size_t newSize)
    {
        if(newSize < bufSize)
        {
            return;
        }
        if(bufMode == bmExternalFixed)
        {
            throw OutputBufferResizeException();
        }
        uint8_t* oldBuf = buf;
        buf = new uint8_t[newSize];
        if(oldBuf)
        {
            memcpy(buf, oldBuf, pos < bufSize ? pos : bufSize);
            if(bufMode != bmExternalInitial)
            {
                delete[] oldBuf;
            }
            bufMode = bmOwned;
        }
        bufSize = newSize;
    }

    void skip(size_t bytes)
    {
        checkSize(bytes);
        pos += bytes;
    }

    void set8(uint8_t val)
    {
        checkSize(1);
        buf[pos++] = val;
    }

    void set16(uint16_t val)
    {
        checkSize(2);
        buf[pos++] = static_cast<uint8_t>(val >> 8);
        buf[pos++] = static_cast<uint8_t>(val & 0xff);
    }

    void set32(uint32_t val)
    {
        checkSize(4);
        buf[pos++] = static_cast<uint8_t>((val >> 24) & 0xff);
        buf[pos++] = static_cast<uint8_t>((val >> 16) & 0xff);
        buf[pos++] = static_cast<uint8_t>((val >> 8) & 0xff);
        buf[pos++] = static_cast<uint8_t>(val & 0xff);
    }

    void set64(uint64_t val)
    {
        checkSize(8);
        set32((uint32_t) (val >> 32));
        set32((uint32_t) (val & 0xffffffffu));
    }

    void copy(size_t bytes, const void* from)
    {
        checkSize(bytes);
        memcpy(buf + pos, from, bytes);
        pos += bytes;
    }

    void setCString(const char* str)
    {
        size_t len = strlen(str) + 1;
        checkSize(len);
        memcpy(buf + pos, str, len);
        pos += len;
    }

    size_t getPos() const
    {
        return pos;
    }

    size_t getSize() const
    {
        return bufSize;
    }

    void setPos(size_t argPos)
    {
        pos = argPos;
    }

    void* getBuf() const
    {
        return buf;
    }

protected:
    void checkSize(size_t sz)
    {
        if(pos + sz > bufSize)
        {
            resize(bufSize * 2 > pos + sz ? bufSize * 2 : pos + sz + bufSize);
        }
    }

    uint8_t* buf;
    size_t bufSize;
    size_t pos;
    enum : uint8_t {
        bmOwned,
        bmExternalInitial,
        bmExternalFixed
    };
    uint8_t bufMode;
};

struct TempRewind {
    OutputBuffer& ob;
    size_t savedPos;

    TempRewind(OutputBuffer& argOb, size_t pos) : ob(argOb), savedPos(ob.getPos())
    {
        ob.setPos(pos);
    }

    TempRewind& operator=(const TempRewind&) = delete;

    ~TempRewind()
    {
        ob.setPos(savedPos);
    }

    OutputBuffer* operator->()
    {
        return &ob;
    }

    OutputBuffer& operator*()
    {
        return ob;
    }
};

}

#endif
