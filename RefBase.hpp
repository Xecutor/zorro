#ifndef __ZORRO_REFBASE_HPP__
#define __ZORRO_REFBASE_HPP__

#include "Debug.hpp"

namespace zorro {

struct ListItem {
    ListItem* next;
};

struct RefBase {
    union {
        struct {
            mutable unsigned refCount;
            unsigned weakRefId;
        };
        ListItem* next;
    };

    RefBase() : refCount(0)
    {
    }

    inline void ref() const
    {
        ++refCount;
//    DPRINT("ref:%p->%d\n",this,refCount);
    }

    inline bool unref() const
    {
//DPRINT("unref:%p->%d\n",this,refCount-1);
        return --refCount == 0;
    }
};

}

#endif
