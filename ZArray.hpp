#ifndef __ZORRO_ZARRAY_HPP__
#define __ZORRO_ZARRAY_HPP__

#include "RefBase.hpp"
#include "Value.hpp"
#include <memory.h>
#include "Debug.hpp"
#include "ZMemory.hpp"

namespace zorro {

#if 0
struct ZArray:RefBase{
  ZMemory* mem;
  Value* data;
  size_t count;
  size_t size;
  bool isSimpleContent;
  void init(ZMemory* argMem)
  {
    mem=argMem;
    data=0;
    count=size=0;
    isSimpleContent=true;
  }
  void clear()
  {
    mem->freeVArray(data,size);
  }
  Value& getItemRef(size_t idx)
  {
    resize(idx+1);
    return data[idx];
  }
  Value& getItem(size_t idx)
  {
    //resize(idx+1);
    return data[idx];
  }

  void resize(size_t sz)
  {
    if(sz>size)
    {
      size_t inc;
      if(sz<256)
      {
        inc=16-(sz%16);
        //if(inc==0)inc=16;
      }else if(sz<65536)
      {
        inc=256-(sz%256);
        //if(inc==0)inc=256;
      }else
      {
        inc=65536-(sz%65536);
      }
      size_t newSz=sz+inc;
      Value* newData=mem->allocVArray(newSz);
      memcpy(newData,data,sizeof(Value)*count);
      memset(newData+count,0,sizeof(Value)*(newSz-count));
      if(data)mem->freeVArray(data,size);
      data=newData;
      size=newSz;
    }
    if(sz>count)
    {
      count=sz;
    }
  }
  ZArray* copy()
  {
    ZArray* rv=mem->allocZArray();
    rv->resize(count);
    if(isSimpleContent)
    {
      memcpy(rv->data,data,sizeof(Value)*count);
      memset(rv->data+count,0,sizeof(Value)*(rv->size-rv->count));
    }else
    {
      memset(rv->data,0,sizeof(Value)*size);
      for(size_t i=0;i<count;i++)
      {
        mem->assign(rv->data[i],data[i]);
      }
    }
    return rv;
  }
  size_t getCount()const
  {
    return count;
  }
};

#else

struct ZArray : RefBase {
    union {
        Value** pages;
        Value* page;
    };
    enum {
        pageSize = 256,
        firstPageIncrement = 16,
        pagesIncrement = 16
    };
    ZMemory* mem;
    size_t itemsCount;
    unsigned int pagesCount;
    unsigned int pagesSize;
    unsigned short lastPageCount;
    unsigned short lastPageSize;
    bool isSimpleContent;

    void init(ZMemory* argMem)
    {
        mem = argMem;
        page = 0;
        pagesCount = 0;
        pagesSize = 0;
        lastPageCount = 0;
        lastPageSize = 0;
        itemsCount = 0;
        isSimpleContent = true;
    }

    void clear()
    {
        if(pagesCount == 0)
        {
            if(lastPageSize)
            {
                mem->freeVArray(page, lastPageSize);
            }
        } else
        {
            for(size_t i = 0; i < pagesCount; ++i)
            {
                mem->freeVArray(pages[i], pageSize);
            }
            mem->freeVPtrArray(pages, pagesSize);
        }
    }

    void push(const Value& val)
    {
        resize(itemsCount + 1);
        itemsCount++;
        if(pagesCount == 0)
        {
            page[lastPageCount - 1] = val;
        } else
        {
            pages[pagesCount - 1][lastPageCount - 1] = val;
        }
    }

    void pushAndRef(const Value& val)
    {
        resize(itemsCount + 1);
        itemsCount++;
        if(pagesCount == 0)
        {
            mem->assign(page[lastPageCount - 1], val);
        } else
        {
            mem->assign(pages[pagesCount - 1][lastPageCount - 1], val);
        }
        if(ZISREFTYPE(&val))
        {
            isSimpleContent = false;
        }
    }

    void pop()
    {
        if(pagesCount == 0)
        {
            --lastPageCount;
            --itemsCount;
            return;
        }
        --lastPageCount;
        --itemsCount;
        if(lastPageCount == 0)
        {
            mem->freeVArray(pages[--pagesCount], pageSize);
            pages[pagesCount] = 0;
            lastPageCount = pageSize;
            if(pagesCount == 1)
            {
                Value* fp = pages[0];
                mem->freeVPtrArray(pages, pagesSize);
                pagesCount = 0;
                pagesSize = 0;
                page = fp;
            }
        }
    }

    Value& getItemRef(size_t idx)
    {
        resize(idx + 1);
        if(idx + 1 > itemsCount)
        {
            itemsCount = idx + 1;
        }
        if(pagesCount == 0)
        {
            return page[idx];
        } else
        {
            return pages[idx / pageSize][idx % pageSize];
        }
    }

    Value& getItem(size_t idx)
    {
        if(pagesCount == 0)
        {
            return page[idx];
        } else
        {
            return pages[idx / pageSize][idx % pageSize];
        }
    }

    const Value& getItem(size_t idx) const
    {
        if(pagesCount == 0)
        {
            return page[idx];
        } else
        {
            return pages[idx / pageSize][idx % pageSize];
        }
    }

    void erase(size_t idx, size_t count)
    {
        if(!count || idx > itemsCount)
        {
            return;
        }
        if(idx + count > itemsCount)
        {
            count = itemsCount - idx;
        }
        if(!isSimpleContent)
        {
            for(size_t i = idx; i < idx + count; ++i)
            {
                mem->unref(getItem(i));
            }
        }
        if(pagesCount == 0)
        {
            if(idx + count > lastPageCount)
            {
                lastPageCount = static_cast<uint16_t>(idx);
                itemsCount = idx;
                return;
            }
            memmove(page + idx, page + idx + count, sizeof(Value) * (lastPageCount - idx - count));
            lastPageCount -= static_cast<uint16_t>(count);
            itemsCount -= count;
        } else
        {
            if(idx + count > itemsCount)
            {
                count = itemsCount - idx;
            }
            Value** toPage = pages + idx / pageSize;
            Value** fromPage = pages + (idx + count) / pageSize;
            Value** endPage = pages + pagesCount - 1;
            size_t dstoff = idx % pageSize;
            size_t srcoff = (idx + count) % pageSize;
            size_t sz;
            if(dstoff < srcoff)
            {
                size_t tail = pageSize - srcoff;
                size_t hole = srcoff - dstoff;
                memmove(*toPage + dstoff, *fromPage + srcoff, tail * sizeof(Value));
                ++fromPage;
                if(fromPage <= endPage)
                {
                    memmove(*toPage + dstoff + tail, *fromPage, hole * sizeof(Value));
                } else
                {
                    --fromPage;
                }
                ++toPage;
                sz = hole;
            } else
            {
                size_t tail = pageSize - srcoff;
                memmove(*toPage + dstoff, *fromPage + srcoff, tail * sizeof(Value));
                ++toPage;
                sz = tail + srcoff;
            }
            while(fromPage < endPage)
            {
                memmove(*toPage, *fromPage + sz, (pageSize - sz) * sizeof(Value));
                ++fromPage;
                memmove(*toPage + pageSize - sz, *fromPage, sz * sizeof(Value));
                ++toPage;
            }
            while(toPage <= fromPage)
            {
                mem->freeVArray(*toPage, pageSize);
                ++toPage;
                --pagesCount;
            }
            itemsCount -= count;
            lastPageCount = itemsCount % pageSize;
            if(pagesCount == 1)
            {
                Value* fp = pages[0];
                mem->freeVPtrArray(pages, pagesSize);
                pagesCount = 0;
                pagesSize = 0;
                page = fp;
            }
        }
    }

    void insert(size_t index, const ZArray& za, size_t subIndex, size_t subCount)
    {
        if(subIndex > za.getCount())
        {
            return;
        }
        if(index > itemsCount)
        {
            resize(index);
            itemsCount = index;
        }
        if(subIndex + subCount > za.getCount())
        {
            subCount = za.getCount() - subIndex;
        }
        resize(itemsCount + subCount);
        itemsCount += subCount;
        if(za.isSimpleContent)
        {
            if(pagesCount == 0)
            {
                memmove(page + index + subCount, page + index, (itemsCount - index) * sizeof(Value));
                if(za.pagesCount == 0)
                {
                    memcpy(page + index, za.page + subIndex, subCount * sizeof(Value));
                } else
                {
                    uint16_t off = subIndex % pageSize;
                    Value** pp = za.pages + subIndex / pageSize;
                    Value* p = *pp + off;
                    if(pageSize - off > (int) subCount)
                    {
                        memcpy(page + index, p, subCount * sizeof(Value));
                    } else
                    {
                        memcpy(page + index, p, (pageSize - subCount) * sizeof(Value));
                        ++pp;
                        p = *pp;
                        memcpy(page + index + (pageSize - subCount), p, subCount * 2 - pagesSize);
                    }
                }
            } else
            {
                size_t mvcnt = itemsCount - subCount - index;
                for(size_t i = 0; i < mvcnt; ++i)
                {
                    getItem(itemsCount - 1 - i) = getItem(itemsCount - subCount - 1 - i);
                }
                for(size_t i = 0; i < subCount; ++i)
                {
                    getItem(index + i) = za.getItem(subIndex + i);
                }
            }
        } else
        {
            if(pagesCount == 0)
            {
                memmove(page + index + subCount, page + index, (itemsCount - index) * sizeof(Value));
                memset(page + index, 0, subCount * sizeof(Value));
                for(size_t i = subIndex; i < subIndex + subCount; ++i)
                {
                    mem->assign(getItem(index++), za.getItem(i));
                }
            } else
            {
                size_t mvcnt = itemsCount - subCount - index;
                for(size_t i = 0; i < mvcnt; ++i)
                {
                    getItem(itemsCount - 1 - i) = getItem(itemsCount - subCount - 1 - i);
                }
                for(size_t i = 0; i < subCount; ++i)
                {
                    Value& v = getItem(index + i);
                    v = NilValue;
                    mem->assign(v, za.getItem(subIndex + i));
                }
            }
        }
    }

    size_t getCount() const
    {
        return itemsCount;
    }

    void resize(size_t argSize)
    {
        if(argSize <= itemsCount)
        {
            return;
        }
        if(pagesCount == 0)
        {
            if(argSize <= lastPageCount)
            {
                return;
            }
            if(argSize <= lastPageSize)
            {
                memset(page + lastPageCount, 0, sizeof(Value) * (argSize - lastPageCount));
                /*
                for(size_t i=lastPageCount;i<argSize;i++)
                {
                  page[i]=NilValue();
                }*/
                lastPageCount = static_cast<uint16_t>(argSize);
                return;
            }
            if(lastPageSize == 0)
            {
                if(argSize < pageSize)
                {
                    size_t m = argSize % firstPageIncrement;
                    lastPageSize = static_cast<uint16_t>(argSize + (m ? firstPageIncrement - m : 0));
                    page = mem->allocVArray(lastPageSize);
                    memset(page, 0, sizeof(Value) * argSize);
                    /*for(size_t i=0;i<argSize;++i)
                    {
                      page[i]=NilValue();
                    }*/
                    lastPageCount = static_cast<uint16_t>(argSize);
                    return;
                } else
                {
                    pages = mem->allocVPtrArray(pagesIncrement);
                    memset(pages, 0, sizeof(Value*) * pagesIncrement);
                    pages[0] = mem->allocVArray(pageSize);
                    memset(pages[0], 0, sizeof(Value) * pageSize);
                    /*for(size_t i=0;i<pageSize;i++)
                    {
                      pages[0][i]=NilValue();
                    }*/
                    pagesSize = pagesIncrement;
                    pagesCount = 1;
                    lastPageCount = lastPageSize = pageSize;
                    resize(argSize);
                    return;
                }
            }
            if(lastPageSize == pageSize)
            {
                Value* firstPage = page;
                pages = mem->allocVPtrArray(pagesIncrement);
                memset(pages, 0, sizeof(Value*) * pagesIncrement);
                pages[0] = firstPage;
                pagesCount = 1;
                pagesSize = pagesIncrement;
                resize(argSize);
                return;
            }
            size_t newSize;
            if(argSize <= pageSize)
            {
                size_t m = argSize % firstPageIncrement;
                newSize = argSize + (m ? firstPageIncrement - m : 0);
            } else
            {
                newSize = pageSize;
            }
            Value* newPage = mem->allocVArray(newSize);
            memcpy(newPage, page, sizeof(Value) * lastPageCount);
            mem->freeVArray(page, lastPageSize);
            lastPageSize = static_cast<uint16_t>(newSize);
            page = newPage;
            if(argSize <= pageSize)
            {
                memset(page + lastPageCount, 0, sizeof(Value) * (argSize - lastPageCount));
                /*
                for(size_t i=lastPageCount;i<argSize;i++)
                {
                  page[i]=NilValue();
                }*/
                lastPageCount = static_cast<uint16_t>(argSize);
                return;
            } else
            {
                lastPageCount = pageSize;
                resize(argSize);
                return;
            }
        }
        if(lastPageCount == pageSize)
        {
            if(pagesCount == pagesSize)
            {
                Value** newPages = mem->allocVPtrArray(pagesSize + pagesIncrement);
                memcpy(newPages, pages, sizeof(Value*) * pagesCount);
                mem->freeVPtrArray(pages, pagesSize);
                pages = newPages;
                lastPageCount = 0;
                pagesSize += pagesIncrement;
            }
            pages[pagesCount++] = mem->allocVArray(pageSize);
            lastPageCount = 0;
        }
        if(argSize == itemsCount + 1)
        {
            if(lastPageCount < lastPageSize)
            {
                pages[pagesCount - 1][lastPageCount++] = NilValue;
                return;
            }
        }
        Value* lastPage = pages[pagesCount - 1];
        if(argSize <= pagesCount * pageSize)
        {
            size_t endIdx = argSize - (pagesCount - 1) * pageSize;
            memset(lastPage + lastPageCount, 0, sizeof(Value) * (endIdx - lastPageCount));
            /*
            for(size_t i=lastPageCount;i<endIdx;++i)
            {
              lastPage[i]=NilValue();
            }*/
            lastPageCount = static_cast<uint16_t>(endIdx);
            return;
        }
        memset(lastPage + lastPageCount, 0, sizeof(Value) * (pageSize - lastPageCount));
        /*
        for(size_t i=lastPageCount;i<pageSize;++i)
        {
          lastPage[i]=NilValue();
        }*/
        if(argSize > pagesSize * pageSize)
        {
            size_t newSz = (argSize + 1) / pageSize;
            size_t m = newSz % pagesIncrement;
            newSz += m ? pagesIncrement - m : 0;
            Value** newPages = mem->allocVPtrArray(newSz);
            memcpy(newPages, pages, sizeof(Value*) * pagesCount);
            mem->freeVPtrArray(pages, pagesSize);
            pages = newPages;
            pagesSize = newSz;
        }
        while(argSize > pageSize)
        {
            lastPage = pages[pagesCount++] = mem->allocVArray(pageSize);
            memset(lastPage, 0, sizeof(Value) * pageSize);
            /*
            for(size_t i=0;i<pageSize;++i)
            {
              lastPage[i]=NilValue();
            }*/
            argSize -= pageSize;
        }
        lastPage = pages[pagesCount++] = mem->allocVArray(pageSize);
        memset(lastPage, 0, sizeof(Value) * argSize);
        /*
        for(size_t i=0;i<argSize;++i)
        {
          lastPage[i]=NilValue();
        }
        */
        lastPageCount = static_cast<uint16_t>(argSize);
        lastPageSize = pageSize;
    }

    ZArray* copy()
    {
        ZArray* rv = mem->allocZArray();
        if(pagesCount == 0)
        {
            rv->page = mem->allocVArray(lastPageSize);
            rv->lastPageCount = lastPageCount;
            rv->lastPageSize = lastPageSize;
            rv->pagesCount = 0;
            rv->pagesSize = 0;
            rv->isSimpleContent = isSimpleContent;
            if(isSimpleContent)
            {

                memcpy(rv->page, page, sizeof(Value) * lastPageCount);
            } else
            {
                memset(rv->page, 0, sizeof(Value) * lastPageCount);
                for(size_t i = 0; i < lastPageCount; ++i)
                {
                    mem->assign(rv->page[i], page[i]);
                }
            }
        } else
        {
            rv->pages = mem->allocVPtrArray(pagesSize);
            memcpy(rv->pages, pages, sizeof(Value*) * pagesCount);
            for(size_t i = 0; i < pagesCount - 1; ++i)
            {
                Value* p = rv->pages[i] = mem->allocVArray(pageSize);
                if(isSimpleContent)
                {
                    memcpy(p, pages[i], sizeof(Value) * pageSize);
                } else
                {
                    memset(p, 0, sizeof(Value) * pageSize);
                    for(size_t j = 0; j < pageSize; ++j)
                    {
                        mem->assign(p[j], pages[i][j]);
                    }
                }
            }
            Value* lp = rv->pages[pagesCount - 1] = mem->allocVArray(pageSize);
            if(isSimpleContent)
            {
                memcpy(lp, pages[pagesCount - 1], sizeof(Value) * lastPageCount);
            } else
            {
                memset(lp, 0, sizeof(Value) * pageSize);
                for(size_t j = 0; j < lastPageCount; ++j)
                {
                    mem->assign(lp[j], pages[pagesCount - 1][j]);
                }
            }
            rv->pagesCount = pagesCount;
            rv->pagesSize = pagesSize;
            rv->lastPageCount = lastPageCount;
            rv->lastPageSize = lastPageSize;
            rv->isSimpleContent = isSimpleContent;
        }
        rv->itemsCount = itemsCount;
        return rv;
    }

    void append(const ZArray& other)
    {
        size_t base = itemsCount;
        resize(itemsCount + other.itemsCount);
        if(other.isSimpleContent)
        {
            for(size_t i = 0; i < other.itemsCount; ++i)
            {
                getItemRef(base + i) = other.getItem(i);
            }
        } else
        {
            for(size_t i = 0; i < other.itemsCount; ++i)
            {
                mem->assign(getItemRef(base + i), other.getItem(i));
            }
            isSimpleContent = false;
        }
    }
};


/*
class ZArray:public RefBase{
public:
  ZArray():mem(0),data(0)
  {
  }
  ~ZArray()
  {
    clear();
  }
  void init(ZMemory* argMem)
  {
    mem=argMem;
    data=0;
  }
  void clear()
  {
    if(data)
    {
      if(data->unref())
      {
        DPRINT("delete data\n");
        mem->freeZAData(data);
      }
    }
  }
  void push(const Value& val)
  {
    onModify();
    data->push(mem,val);
  }

  void pop()
  {
    onModify();
    data->pop(mem);
  }

  const Value& get(size_t idx)
  {
    return data->getItem(idx);
  }

  Value& getRef(int idx)
  {
    onModify();
    return data->getItem(mem,idx);
  }

  size_t getCount()const
  {
    return data?data->getCount():0;
  }

  void resize(size_t argSize)
  {
    onModify();
    data->resize(mem,argSize);
  }

  ZArray* copy()
  {
    ZArray* rv=mem->allocZArray();
    rv->ref();
    rv->data=data;
    if(data)
    {
      DPRINT("copy %p rc=%d\n",data,data->refCount);
      data->copyOnWrite=true;
      data->ref();
    }
    return rv;
  }

  bool isUnique()
  {
    return data?data->refCount==1:false;
  }

  void onModify()
  {
    if(!data)
    {
      data=mem->allocZAData();
      data->init();
    }else
    if(data->copyOnWrite)
    {
      if(data->refCount>1)
      {
        ZArrayData* dataCopy=data->copy(mem);
        data->unref();
        DPRINT("copy on modify:%p->%p\n",data,dataCopy);
        data=dataCopy;
        data->ref();
      }else
      {
        data->copyOnWrite=false;
      }
    }
  }



  ZMemory* mem;
  ZArrayData* data;
};
*/
#endif

}


#endif
