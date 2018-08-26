#ifndef __ZORRO_ZSTACK_HPP__
#define __ZORRO_ZSTACK_HPP__

namespace zorro {

template<class T, class OWNER, void (OWNER::*onResize)()>
struct ZStack {
    T* stack;
    T* stackTop;
    T* stackMax;
    OWNER* owner;

    ZStack(OWNER* argOwner) : owner(argOwner)
    {
        stackMax = stackTop = stack = 0;
        --stackTop;
    }

    ~ZStack()
    {
        if(stack)
        {
            delete[] stack;
        }
    }

    void swap(ZStack<T, OWNER, onResize>& other)
    {
        T* t = stack;
        stack = other.stack;
        other.stack = t;
        t = stackTop;
        stackTop = other.stackTop;
        other.stackTop = t;
        t = stackMax;
        stackMax = other.stackMax;
        other.stackMax = t;
        //OWNER* o=owner;owner=other.owner;other.owner=o;
    }

    void push(const T& v)
    {
        if(++stackTop == stackMax)
        {
            resize(size() * 2);
        }
        *stackTop = v;
    }

    T* push()
    {
        if(++stackTop == stackMax)
        {
            resize(size() * 2);
        }
        return stackTop;
    }

    void pushBulk(size_t inc)
    {
        if(stackTop + inc >= stackMax)
        {
            resize(inc > size() * 2 ? inc : size() * 2);
        }
        //long* start=(long*)(stackTop+1);
        //long* end=start+(sizeof(T)*inc)/sizeof(long);
        //for(;start!=end;++start)*start=0;
        //memset(stackTop+1,0,sizeof(T)*inc);
        stackTop += inc;
    }

    void reset()
    {
        stackTop = stack - 1;
    }

    void resize(size_t inc)
    {
        if(inc == 0)
        {
            inc = 16;
        }
        size_t topSz = static_cast<size_t>(stackTop - stack);
        size_t sz = static_cast<size_t>(stackMax - stack);
        T* newStack = new T[sz + inc];
        if(stack)
        {
            memcpy(newStack, stack, sizeof(T) * sz);
            delete[] stack;
        }
        memset(newStack + sz, 0, sizeof(T) * inc);
        stack = newStack;
        stackMax = stack + sz + inc;
        stackTop = stack + topSz;
        (owner->*onResize)();
    }

    void setSize(size_t newSize)
    {
        stackTop = stack + newSize - 1;
    }

    void pop()
    {
        --stackTop;
    }

    size_t size()
    {
        return static_cast<size_t>(stackTop - stack + 1);
    }
};


}

#endif
