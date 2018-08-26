#ifndef __ZORRO_ZHASH_HPP__
#define __ZORRO_ZHASH_HPP__

#include "ZString.hpp"
#include <stdio.h>

namespace zorro {

template<class V>
class ZHash {
public:
    ZHash() : arr(0), size(0), count(0)
    {
    }

    ~ZHash()
    {
        clear();
    }

    void clear()
    {
        if(arr)
        {
            delete[] arr;
        }
        arr = 0;
        count = 0;
        size = 0;
    }

    V& insert(ZString* argKey, const V& argValue)
    {
        if(arr == 0)
        {
            size = 16;
            arr = new KeyValue[size];
            memset(arr, 0, sizeof(KeyValue) * size);
        }
        uint32_t kh = argKey->getHashCode();
        uint32_t att = 0;
        for(;;)
        {
            uint32_t index = (kh + att) % size;
            KeyValue& kv = arr[index];
            if(!kv.key)
            {
                kv.key = argKey;
                kv.value = argValue;
                argKey->ref();
                ++count;
                return kv.value;
            }
            if(kh == kv.key->getHashCode() && *kv.key == *argKey)
            {
                kv.value = argValue;
                return kv.value;
            }
            att++;
            if(att == 8)
            {
                resize();
                att = 0;
            }
        }
        //return arr[0].value;//make compiler happy
    }

    V* getPtr(const ZString* argKey) const
    {
        if(!count)
        {
            return 0;
        }
        uint32_t hk = argKey->getHashCode();
        uint32_t att = 0;
        for(;;)
        {
            uint32_t index = (hk + att) % size;
            KeyValue* kv = &arr[index];
            if(!kv->key)
            {
                return 0;
            }
            if(hk == kv->key->getHashCode() && *kv->key == *argKey)
            {
                return &kv->value;
            }
            att++;
            if(att == 8)
            {
                break;
            }
        }
        return 0;
    }

    V* getPtr(const char* argKey) const
    {
        if(!count)
        {
            return 0;
        }
        size_t kl = strlen(argKey);
        uint32_t hk = ZString::calcHash(argKey, argKey + kl);
        uint32_t att = 0;
        for(;;)
        {
            uint32_t index = (hk + att) % size;
            KeyValue* kv = &arr[index];
            if(!kv->key)
            {
                return 0;
            }
            if(hk == kv->key->getHashCode() && kv->key->equalsTo(argKey, kl))
            {
                return &kv->value;
            }
            att++;
            if(att == 8)
            {
                break;
            }
        }
        return 0;
    }

    V* getPtr(const char* argKey, size_t argLength) const
    {
        if(!count)
        {
            return 0;
        }
        uint32_t hk = ZString::calcHash(argKey, argKey + argLength);
        uint32_t att = 0;
        for(;;)
        {
            uint32_t index = (hk + att) % size;
            KeyValue* kv = &arr[index];
            if(!kv->key)
            {
                return 0;
            }
            if(hk == kv->key->getHashCode() && kv->key->equalsTo(argKey, argLength))
            {
                return &kv->value;
            }
            att++;
            if(att == 8)
            {
                break;
            }
        }
        return 0;
    }

    uint32_t getCount() const
    {
        return count;
    }

    uint32_t getSize() const
    {
        return size;
    }/*
  V& operator[](ZString* key)
  {
    V* ptr=getPtr(key);
    if(ptr)return *ptr;
    return insert(key,V());
  }
  V& operator[](ZStringRef key)
  {
    V* ptr=getPtr(key.get());
    if(ptr)return *ptr;
    return insert(key.get(),V());
  }*/
    void assign(ZMemory* mem, const ZHash& rhs)
    {
        for(uint32_t i = 0; i < size; i++)
        {
            if(!arr[i].key)
            {
                continue;
            }
            if(arr[i].key->unref())
            {
                mem->freeZString(arr[i].key);
            }
        }
        delete[] arr;
        count = rhs.count;
        size = rhs.size;
        if(size)
        {
            arr = new KeyValue[size];
        } else
        {
            arr = 0;
            return;
        }
        memcpy(arr, rhs.arr, sizeof(KeyValue) * size);
        for(uint32_t i = 0; i < size; i++)
        {
            if(!arr[i].key)
            {
                continue;
            }
            arr[i].key->ref();
        }
    }

protected:
    struct KeyValue {
        ZString* key;
        V value;
    };
public:
    struct Iterator {
        Iterator(ZHash& argHash) : hash(argHash)
        {
            index = (uint32_t) -1;
        }

        Iterator(const Iterator&) = delete;

        bool getNext(ZString*& key, V*& val)
        {
            do
            {
                index++;
            } while(index < hash.size && !hash.arr[index].key);
            if(index < hash.size)
            {
                key = hash.arr[index].key;
                val = &hash.arr[index].value;
                return true;
            } else
            {
                return false;
            }
        }

        Iterator& operator=(const Iterator&) = delete;

    protected:
        ZHash& hash;
        uint32_t index;
    };

protected:
    KeyValue* arr;
    uint32_t size;
    uint32_t count;

    void resize()
    {
        uint32_t newSize = size * 2;
        KeyValue* arr2;
        for(;;)
        {
            arr2 = new KeyValue[newSize];
            memset(arr2, 0, sizeof(KeyValue) * newSize);
            bool restart = false;
            for(uint32_t i = 0; i < size; i++)
            {
                if(!arr[i].key)
                {
                    continue;
                }
                uint32_t hashCode = arr[i].key->getHashCode();
                uint32_t att = 0;
                while(arr2[(hashCode + att) % newSize].key && att < 8)
                {
                    att++;
                }
                if(att >= 8)
                {
                    delete[] arr2;
                    newSize = newSize * 2;
                    restart = true;
                    break;
                }
                arr2[(hashCode + att) % newSize] = arr[i];
            }
            if(!restart)
            {
                break;
            }
        }
        delete[] arr;
        arr = arr2;
        size = newSize;
    }

    ZHash(const ZHash&);
};

}

#endif
