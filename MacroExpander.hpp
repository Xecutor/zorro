#ifndef __ZORRO_MACROEXPANDER_HPP__
#define __ZORRO_MACROEXPANDER_HPP__

namespace zorro {

class FuncParamList;

class StmtList;

class ZorroVM;

class ZParser;

struct Term;

class ZMacroExpander {
public:
    virtual ~ZMacroExpander()
    {
    }

    void init(ZorroVM* argVM, ZParser* argP)
    {
        vm = argVM;
        p = argP;
    }

    void registerMacro(const Term& name, FuncParamList* params, StmtList* body);

    void expandMacro();

protected:
    ZorroVM* vm;
    ZParser* p;
};


}

#endif
