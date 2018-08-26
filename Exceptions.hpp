#ifndef __ZORRO_EXCEPTIONS_HPP__
#define __ZORRO_EXCEPTIONS_HPP__

#include <exception>
#include <stdexcept>
#include <kst/Format.hpp>
#include "FileReader.hpp"
#include "NameList.hpp"

namespace zorro {

class ZorroException : public std::exception {
public:
    ZorroException() = default;

    explicit ZorroException(const char* argMsg) : msg(argMsg)
    {
    }

    ZorroException(const ZorroException&)= default;

    ~ZorroException() noexcept override = default;

    const char* what() const noexcept override
    {
        return msg.c_str();
    }

protected:
    std::string msg;
};

class InvalidRule : public ZorroException {
public:
    explicit InvalidRule(const char* argRule)
    {
        msg = "Invalid rule:";
        msg += argRule;
    }
};

class RuleUndefined : public ZorroException {
public:
    explicit RuleUndefined(const char* argRule)
    {
        msg = "Rule undefined:";
        msg += argRule;
    }
};

class FileNotFound : public ZorroException {
public:
    FileNotFound(const char* fileName)
    {
        msg = "File not found:";
        msg += fileName;
    }
};

class SyntaxErrorException : public ZorroException {
public:
    FileLocation pos;

    SyntaxErrorException(const char* argMsg, const FileLocation& argPos) : pos(argPos)
    {
        msg = argPos.backTrace();
        msg += ": ";
        msg += argMsg;
    }
};


class UnexpectedSymbol : public SyntaxErrorException {
public:
    UnexpectedSymbol(char argChar, const FileLocation& argPos) :
        SyntaxErrorException(
            FORMAT("Unexpected symbol '%{}'(0x%{x:02})", argChar >= 32 ? argChar : '?', (int) (unsigned char) argChar),
            argPos)
    {
    }
};

class UnexpectedEndOfLine : public SyntaxErrorException {
public:
    UnexpectedEndOfLine(const char* argMsg, const FileLocation& argPos) :
        SyntaxErrorException(FORMAT("Unexpected end of line %{}", argMsg), argPos)
    {
    }
};

class CGException : public SyntaxErrorException {
public:
    CGException(const char* argMsg, const FileLocation& argPos) : SyntaxErrorException(argMsg, argPos)
    {
    }
};

class UndefinedSymbolException : public SyntaxErrorException {
public:
    explicit UndefinedSymbolException(const Symbol& argSym) :
        SyntaxErrorException(FORMAT("Undefined symbol %{}", argSym), argSym.name.pos)
    {
    }
};

class RuntimeException : public ZorroException {
public:
    RuntimeException(const char* argMsg, const char* argTrace) : ZorroException(argMsg)
    {
        if(argTrace && *argTrace)
        {
            msg += " at\n";
            msg += argTrace;
        }
    }
};

class TypeException : public RuntimeException {
public:
    TypeException(const char* argMsg, const char* argTrace) : RuntimeException(argMsg, argTrace)
    {
    }
};

class OutOfBoundsException : public RuntimeException {
public:
    OutOfBoundsException(const char* argMsg, const char* argTrace) : RuntimeException(argMsg, argTrace)
    {
    }
};

class NoSuchKeyException : public RuntimeException {
public:
    NoSuchKeyException(const char* argMsg, const char* argTrace) : RuntimeException(argMsg, argTrace)
    {
    }
};

class ArithmeticException : public RuntimeException {
public:
    ArithmeticException(const char* argMsg, const char* argTrace) : RuntimeException(argMsg, argTrace)
    {
    }
};


#define ZTHROW(ex_type, pos, ...) throw ex_type(FORMAT(__VA_ARGS__),pos)
#define ZTHROW0(ex_type, ...) throw ex_type(FORMAT(__VA_ARGS__))
#define ZTHROWR(ex_type, vm, ...) throw ex_type(FORMAT(__VA_ARGS__),vm->getStackTraceText().c_str())

}

#endif
