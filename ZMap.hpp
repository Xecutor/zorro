#ifndef __ZORRO_ZMAP_HPP__
#define __ZORRO_ZMAP_HPP__

#include <memory.h>
#include "RefBase.hpp"
#include "Value.hpp"
#include "ZMemory.hpp"

namespace zorro {

struct ZMapValueType {
    Value m_key;
    Value m_value;
};

struct ZMapDataNode;
struct ZMapDataNodeBase {
    ZMapDataNode* m_prev;
    ZMapDataNode* m_next;
};
struct ZMapDataNode : ZMapDataNodeBase {
    ZMapValueType m_keyval;
};

struct ZMapDataArrayNode {
    intptr_t m_nodeType;
    union {
        ZMapDataNode** m_array;
        ZMapDataArrayNode* m_next;
    };
    size_t m_count;
};


struct ZMapNode {
    intptr_t m_nodeType;
    union {
        ZMapNode* m_next;
        ZMapNode* m_ch[4];
    };

    ZMapDataNode& asData()
    {
        return *(ZMapDataNode*) this;
    }

    ZMapDataNode* asDataPtr()
    {
        return (ZMapDataNode*) this;
    }

    ZMapDataArrayNode& asArray()
    {
        return *(ZMapDataArrayNode*) this;
    }

    ZMapDataArrayNode* asArrayPtr()
    {
        return (ZMapDataArrayNode*) this;
    }
};


class ZMap : public RefBase {
public:
    typedef ZMapNode Node;
    typedef ZMapDataNode DataNode;
    typedef ZMapDataArrayNode DataArrayNode;
    typedef ZMapDataNodeBase DataNodeBase;

    typedef ZMapValueType value_type;

    ZMap() :
        m_root(0), m_count(0), m_begin((DataNode*) &m_end), m_forIterators(0), m_mem(0)
    {
        m_end.m_prev = 0;
        m_end.m_next = 0;
    }

    class const_iterator;

    class iterator {
    public:
        iterator() : m_node(0)
        {
        }

        iterator operator++(int)
        {
            m_node = m_node->m_next;
            return *this;
        }

        iterator operator++()
        {
            DataNode* rv = m_node;
            m_node = m_node->m_next;
            return iterator(rv);
        }

        iterator operator--(int)
        {
            m_node = m_node->m_prev;
            return *this;
        }

        iterator operator--()
        {
            DataNode* rv = m_node;
            m_node = m_node->m_prev;
            return iterator(rv);
        }

        iterator& operator=(const iterator& a_other)
        {
            m_node = a_other.m_node;
            return *this;
        }

        bool operator==(const iterator& a_other) const
        {
            return m_node == a_other.m_node;
        }

        bool operator!=(const iterator& a_other) const
        {
            return m_node != a_other.m_node;
        }

        value_type& operator*()
        {
            return m_node->m_keyval;
        }

        value_type* operator->()
        {
            return &m_node->m_keyval;
        }

        const value_type& operator*() const
        {
            return m_node->m_keyval;
        }

        const value_type* operator->() const
        {
            return &m_node->m_keyval;
        }

    protected:
        iterator(DataNode* a_node) : m_node(a_node)
        {
        }

        friend class ZMap;

        ZMap::DataNode* m_node;

        friend class ZMap::const_iterator;
    };

    class const_iterator {
    public:
        const_iterator() : m_node(0)
        {
        }

        const_iterator(const iterator& a_other) : m_node(a_other.m_node)
        {
        }

        const_iterator operator++(int)
        {
            m_node = m_node->m_next;
            return *this;
        }

        const_iterator operator++()
        {
            const DataNode* rv = m_node;
            m_node = m_node->m_next;
            return const_iterator(rv);
        }

        const_iterator operator--(int)
        {
            m_node = m_node->m_prev;
            return *this;
        }

        const_iterator operator--()
        {
            const DataNode* rv = m_node;
            m_node = m_node->m_prev;
            return const_iterator(rv);
        }

        const_iterator& operator=(const const_iterator& a_other)
        {
            m_node = a_other.m_node;
            return *this;
        }

        bool operator==(const const_iterator& a_other) const
        {
            return m_node == a_other.m_node;
        }

        bool operator!=(const const_iterator& a_other) const
        {
            return m_node != a_other.m_node;
        }

        const value_type& operator*() const
        {
            return m_node->m_keyval;
        }

        const value_type* operator->() const
        {
            return &m_node->m_keyval;
        }

    protected:
        const_iterator(const DataNode* a_node) : m_node(a_node)
        {
        }

        friend class ZMap;

        const ZMap::DataNode* m_node;
    };


    iterator begin()
    {
        return iterator(m_begin);
    }

    iterator end()
    {
        return iterator((DataNode*) &m_end);
    }

    const_iterator begin() const
    {
        return const_iterator(m_begin);
    }

    const_iterator end() const
    {
        return const_iterator((const DataNode*) &m_end);
    }

    iterator insert(const Value& a_key, const Value& a_value)
    {
        if(a_key.vt == vtWeakRef)
        {
            Value newKey = a_key;
            if(newKey.weakRef->cont.vt != vtNil)
            {
                newKey = m_mem->mkWeakRef(&newKey);
            }
            Value mcont;
            mcont.vt = vtMap;
            mcont.flags = 0;
            mcont.map = this;
            newKey.weakRef->cont = m_mem->mkWeakRef(&mcont);
            newKey.weakRef->cont.refBase->ref();
            return insert(newKey, a_value, true);
        } else
        {
            return insert(a_key, a_value, true);
        }
    }

    iterator insert(const value_type& a_keyval)
    {
        return insert(a_keyval.m_key, a_keyval.m_value);
    }

    template<class InputIterator>
    void insert(InputIterator it, InputIterator end)
    {
        for(; it != end; ++it)
        {
            insert(*it);
        }
    }

    iterator find(const Value& a_key)
    {
        if(!m_root)
        {
            return end();
        }
        uint32_t hashCode = hashFunc(a_key);
        Node* ptr = m_root;
        while(ptr)
        {
            if(ptr->m_nodeType == ntInnerNode)
            {
                ptr = ptr->m_ch[hashCode & 3];
                hashCode >>= 2;
                continue;
            }
            if(ptr->m_nodeType == ntValuesArray)
            {
                DataArrayNode& va = ptr->asArray();
                for(size_t i = 0; i < va.m_count; i++)
                {
                    if(m_mem->isEqual(va.m_array[i]->m_keyval.m_key, a_key))
                    {
                        return iterator(va.m_array[i]);
                    }
                }
                return end();
            }
            if(m_mem->isEqual(ptr->asData().m_keyval.m_key, a_key))
            {
                return iterator(ptr->asDataPtr());
            }
            return end();
        }
        return end();
    }

    const_iterator find(const Value& a_key) const
    {
        if(!m_root)
        {
            return end();
        }
        uint32_t hashCode = hashFunc(a_key);
        Node* ptr = m_root;
        while(ptr)
        {
            if(ptr->m_nodeType == ntInnerNode)
            {
                ptr = ptr->m_ch[hashCode & 3];
                hashCode >>= 2;
                continue;
            }
            if(ptr->m_nodeType == ntValuesArray)
            {
                DataArrayNode& va = ptr->asArray();
                for(size_t i = 0; i < va.m_count; i++)
                {
                    if(m_mem->isEqual(va.m_array[i]->m_keyval.m_key, a_key))
                    {
                        return const_iterator(va.m_array[i]);
                    }
                }
                return end();
            }
            if(m_mem->isEqual(ptr->asData().m_keyval.m_key, a_key))
            {
                return const_iterator(ptr->asDataPtr());
            }
            return end();
        }
        return end();
    }

    void erase(const Value& a_key)
    {
        if(!m_root)
        {
            return;
        }
        uint32_t hashCode = hashFunc(a_key);
        Node* ptr = m_root;
        Node* path[16];
        unsigned char pathIdx[16];
        size_t pathCount = 0;
        while(ptr)
        {
            unsigned char idx = hashCode & 3;
            if(ptr->m_nodeType == ntInnerNode)
            {
                path[pathCount] = ptr;
                pathIdx[pathCount++] = idx;
                ptr = ptr->m_ch[idx];
                hashCode >>= 2;
                continue;
            }
            if(ptr->m_nodeType == ntValuesArray)
            {
                DataArrayNode& va = ptr->asArray();
                bool found = false;
                for(size_t i = 0; i < va.m_count; i++)
                {
                    if(m_mem->isEqual(va.m_array[i]->m_keyval.m_key, a_key))
                    {
                        found = true;
                        DataNode* vptr = va.m_array[i];
                        ForIterator* fptr = m_forIterators;
                        while(fptr)
                        {
                            if(fptr->ptr == vptr)
                            {
                                fptr->ptr = vptr->m_prev;
                                break;
                            }
                            fptr = fptr->next;
                        }

                        if(vptr->m_prev)
                        {
                            vptr->m_prev->m_next = vptr->m_next;
                            vptr->m_next->m_prev = vptr->m_prev;
                        } else
                        {
                            m_begin = vptr->m_next;
                            m_begin->m_prev = 0;
                        }
                        m_mem->assign(vptr->m_keyval.m_key, NilValue);
                        m_mem->assign(vptr->m_keyval.m_value, NilValue);
                        m_mem->freeZMapDataNode(vptr);
                        if(va.m_count > 1)
                        {
                            DataNode** newValues = new DataNode* [va.m_count - 1];
                            if(i > 0)
                            {
                                memcpy(newValues, va.m_array, i * sizeof(DataNode*));
                            }
                            if(i < va.m_count - 1)
                            {
                                memcpy(newValues + i, va.m_array + i + 1, (va.m_count - i - 1) * sizeof(DataNode*));
                            }
                            delete[] va.m_array;
                            va.m_array = newValues;
                            va.m_count--;
                            m_count--;
                            return;
                        } else
                        {
                            delete[] va.m_array;
                            m_mem->freeZMapDataArrayNode(ptr->asArrayPtr());
                            m_count--;
                            break;
                        }
                    }
                }
                if(!found)
                {
                    return;
                }
            } else
            {
                DataNode* dptr = ptr->asDataPtr();
                if(!m_mem->isEqual(dptr->m_keyval.m_key, a_key))
                {
                    return;
                }
                ForIterator* fptr = m_forIterators;
                while(fptr)
                {
                    if(fptr->ptr == dptr)
                    {
                        fptr->ptr = dptr->m_prev;
                        break;
                    }
                    fptr = fptr->next;
                }
                if(dptr->m_prev)
                {
                    dptr->m_prev->m_next = dptr->m_next;
                    dptr->m_next->m_prev = dptr->m_prev;
                } else
                {
                    m_begin = dptr->m_next;
                    m_begin->m_prev = 0;
                }
                m_mem->assign(dptr->m_keyval.m_key, NilValue);
                m_mem->assign(dptr->m_keyval.m_value, NilValue);
                m_mem->freeZMapDataNode(dptr);
                m_count--;
            }

            while(pathCount > 0)
            {
                ptr = path[pathCount - 1];
                idx = pathIdx[pathCount - 1];
                ptr->m_ch[idx] = 0;
                if(ptr->m_ch[0] || ptr->m_ch[1] || ptr->m_ch[2] || ptr->m_ch[3])
                {
                    return;
                }
                m_mem->freeZMapNode(ptr);
                if(ptr == m_root)
                {
                    m_root = 0;
                }
                pathCount--;
            }
            return;
        }
    }

    void erase(iterator a_it)
    {
        erase(a_it.m_node->m_keyval.m_key);
    }

    void erase(const_iterator a_it)
    {
        erase(a_it.m_node->m_keyval.m_key);
    }

    size_t size() const
    {
        return m_count;
    }

    bool empty() const
    {
        return !m_count;
    }

    void clear()
    {
        if(m_root)
        {
            recClear(m_root);
        }
        m_root = 0;
        m_count = 0;
        m_begin = (DataNode*) &m_end;
        m_end.m_prev = 0;
    }

    ZMap* copy()
    {
        ZMap* rv = m_mem->allocZMap();
        for(iterator it = begin(), ed = end(); it != ed; ++it)
        {
            rv->insert(it->m_key, it->m_value);
        }
        return rv;
    }

    ForIterator* getForIter()
    {
        ForIterator* rv = m_mem->allocForIterator();
        rv->ptr = m_begin;
        rv->next = m_forIterators;
        return rv;
    }

    bool nextForIter(ForIterator* a_iter)
    {
        DataNode* ptr = (DataNode*) a_iter->ptr;
        if(!ptr)
        {
            ptr = m_begin;
        }
        if(ptr == &m_end || ptr->m_next == &m_end)
        {
            return false;
        }
        ptr = ptr->m_next;
        a_iter->ptr = ptr;
        return true;
    }

    void getForIterValue(ForIterator* a_iter, Value* key, Value* val)
    {
        DataNode* ptr = (DataNode*) a_iter->ptr;
        m_mem->assign(*key, ptr->m_keyval.m_key);
        m_mem->assign(*val, ptr->m_keyval.m_value);
    }

    void endForIter(ForIterator* a_iter)
    {
        ForIterator* prev = 0;
        ForIterator* ptr = m_forIterators;
        while(ptr != a_iter)
        {
            prev = ptr;
            ptr = ptr->next;
        }
        if(prev)
        {
            prev->next = ptr->next;
        } else
        {
            m_forIterators = 0;
        }
    }


    iterator insert(const Value& a_key, const Value& a_value, bool overwrite)
    {
        uint32_t hashCode = hashFunc(a_key);
        size_t bitIndex = 0;
        if(!m_root)
        {
            m_root = m_mem->allocZMapNode();
            m_root->m_nodeType = ntInnerNode;
            m_root->m_ch[3] = m_root->m_ch[2] = m_root->m_ch[1] = m_root->m_ch[0] = 0;
        }
        Node* ptr = m_root;
        Node* parent = 0;
        unsigned char parentIdx = 0;
        unsigned char idx;
        for(;;)
        {
            idx = hashCode & 3;
            if(ptr->m_nodeType == ntInnerNode)
            {
                if(ptr->m_ch[idx])
                {
                    parent = ptr;
                    parentIdx = idx;
                    ptr = ptr->m_ch[idx];
                    hashCode >>= 2;
                    bitIndex += 2;
                    continue;
                } else
                {
                    DataNode& n = *(DataNode*) (ptr->m_ch[idx] = (Node*) m_mem->allocZMapDataNode());
                    m_mem->assign(n.m_keyval.m_key, a_key);
                    m_mem->assign(n.m_keyval.m_value, a_value);

                    ForIterator* fptr = m_forIterators;
                    while(fptr)
                    {
                        if(fptr->ptr == 0)
                        {
                            fptr->ptr = &n;
                            break;
                        }
                        fptr = fptr->next;
                    }

                    n.m_next = (DataNode*) &m_end;
                    n.m_prev = m_end.m_prev;
                    if(m_end.m_prev)
                    {
                        m_end.m_prev->m_next = &n;
                    } else
                    {
                        m_begin = &n;
                    }
                    m_end.m_prev = &n;
                    m_count++;
                    return iterator(&n);
                }
            }

            if(ptr->m_nodeType == ntValuesArray)
            {
                DataArrayNode& va = ptr->asArray();
                for(size_t i = 0; i < va.m_count; i++)
                {
                    if(m_mem->isEqual(va.m_array[i]->m_keyval.m_key, a_key))
                    {
                        if(overwrite)
                        {
                            m_mem->assign(va.m_array[i]->m_keyval.m_value, a_value);
                        }
                        return iterator(va.m_array[i]);
                    }
                }
                DataNode** newArr = new DataNode* [va.m_count + 1];
                memcpy(newArr, va.m_array, sizeof(DataNode*) * va.m_count);
                delete[] va.m_array;
                va.m_array = newArr;
                DataNode& n = *m_mem->allocZMapDataNode();
                ForIterator* fptr = m_forIterators;
                while(fptr)
                {
                    if(fptr->ptr == 0)
                    {
                        fptr->ptr = &n;
                        break;
                    }
                    fptr = fptr->next;
                }
                m_mem->assign(n.m_keyval.m_key, a_key);
                m_mem->assign(n.m_keyval.m_value, a_value);
                n.m_next = (DataNode*) &m_end;
                n.m_prev = m_end.m_prev;
                if(m_end.m_prev)
                {
                    m_end.m_prev->m_next = &n;
                } else
                {
                    m_begin = &n;
                }
                m_end.m_prev = &n;
                va.m_array[va.m_count++] = &n;
                m_count++;
                return iterator(&n);
            } else
            {
                DataNode* dptr = ptr->asDataPtr();
                if(m_mem->isEqual(dptr->m_keyval.m_key, a_key))
                {
                    if(overwrite)
                    {
                        m_mem->assign(dptr->m_keyval.m_value, a_value);
                    }
                    return iterator(dptr);
                }
                if(bitIndex != 30)
                {
                    uint32_t oldHashCode = hashFunc(dptr->m_keyval.m_key);
                    oldHashCode >>= bitIndex;
                    do
                    {
                        Node* newNode = parent->m_ch[parentIdx] = m_mem->allocZMapNode();
                        newNode->m_nodeType = ntInnerNode;
                        newNode->m_ch[3] = newNode->m_ch[2] = newNode->m_ch[1] = newNode->m_ch[0] = 0;
                        if((oldHashCode & 3) != idx)
                        {
                            newNode->m_ch[oldHashCode & 3] = ptr;
                            ptr = newNode;
                            DataNode& n = *(DataNode*) (ptr->m_ch[idx] = (Node*) (dptr = m_mem->allocZMapDataNode()));
                            m_mem->assign(n.m_keyval.m_key, a_key);
                            m_mem->assign(n.m_keyval.m_value, a_value);
                            ForIterator* fptr = m_forIterators;
                            while(fptr)
                            {
                                if(fptr->ptr == 0)
                                {
                                    fptr->ptr = &n;
                                    break;
                                }
                                fptr = fptr->next;
                            }
                            n.m_next = (DataNode*) &m_end;
                            n.m_prev = m_end.m_prev;
                            if(m_end.m_prev)
                            {
                                m_end.m_prev->m_next = dptr;
                            } else
                            {
                                m_begin = dptr;
                            }
                            m_end.m_prev = dptr;
                            m_count++;
                            return iterator(dptr);
                        }
                        parent = newNode;
                        parentIdx = idx;
                        hashCode >>= 2;
                        oldHashCode >>= 2;
                        bitIndex += 2;
                        idx = hashCode & 3;
                    } while(bitIndex < 30);
                }

                DataArrayNode* newNode = (DataArrayNode*) (parent->m_ch[parentIdx] = (Node*) m_mem->allocZMapDataArrayNode());
                newNode->m_nodeType = ntValuesArray;
                newNode->m_count = 0;
                newNode->m_array = new DataNode* [2];
                newNode->m_array[0] = dptr;
                DataNode& n = *(newNode->m_array[1] = m_mem->allocZMapDataNode());
                ForIterator* fptr = m_forIterators;
                while(fptr)
                {
                    if(fptr->ptr == 0)
                    {
                        fptr->ptr = &n;
                        break;
                    }
                    fptr = fptr->next;
                }
                m_mem->assign(n.m_keyval.m_key, a_key);
                m_mem->assign(n.m_keyval.m_value, a_value);
                n.m_next = (DataNode*) &m_end;
                n.m_prev = m_end.m_prev;
                if(m_end.m_prev)
                {
                    m_end.m_prev->m_next = &n;
                } else
                {
                    m_begin = &n;
                }
                m_end.m_prev = &n;
                m_count++;
                return iterator(&n);
            }
        }
        //return end();
    }

    friend class iterator;

    friend class const_iterator;

    static const intptr_t ntInnerNode = intptr_t(-1);//0xffffffff,
    static const intptr_t ntValuesArray = intptr_t(-2);//0xfffffffe

    uint32_t hashFunc(const Value& a_key) const;
    //bool isEqual(const Value& a_key1,const Value& a_key2)const;

    Node* m_root;
    size_t m_count;
    DataNodeBase m_end;
    DataNode* m_begin;
    ForIterator* m_forIterators;
    ZMemory* m_mem;

    void recClear(Node* a_ptr)
    {
        if(a_ptr->m_nodeType == ntInnerNode)
        {
            if(a_ptr->m_ch[0])
            {
                recClear(a_ptr->m_ch[0]);
            }
            if(a_ptr->m_ch[1])
            {
                recClear(a_ptr->m_ch[1]);
            }
            if(a_ptr->m_ch[2])
            {
                recClear(a_ptr->m_ch[2]);
            }
            if(a_ptr->m_ch[3])
            {
                recClear(a_ptr->m_ch[3]);
            }
            m_mem->freeZMapNode(a_ptr);
            return;
        } else if(a_ptr->m_nodeType == ntValuesArray)
        {
            DataArrayNode& va = a_ptr->asArray();
            for(size_t i = 0; i < va.m_count; i++)
            {
                m_mem->assign(va.m_array[i]->m_keyval.m_key, NilValue);
                m_mem->assign(va.m_array[i]->m_keyval.m_value, NilValue);
                m_mem->freeZMapDataNode(va.m_array[i]);
            }
            delete[] va.m_array;
            m_mem->freeZMapDataArrayNode(&va);
            return;
        } else
        {
            m_mem->assign(a_ptr->asData().m_keyval.m_key, NilValue);
            m_mem->assign(a_ptr->asData().m_keyval.m_value, NilValue);
            m_mem->freeZMapDataNode(a_ptr->asDataPtr());
        }
    }
};


}

#endif
