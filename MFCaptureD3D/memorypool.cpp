
extern "C" {
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
}

#include <list>

#include "memorypool.h"

using namespace std;

//////////////////////////////////////////////////////////////////////////
// Reader
//////////////////////////////////////////////////////////////////////////




//////////////////////////////////////////////////////////////////////////
// MemoryReader
//////////////////////////////////////////////////////////////////////////
class MemoryReaderImpl : public MemoryReader
{

public:
    MemoryReaderImpl();
    ~MemoryReaderImpl();

    uint32_t Read(void * data) {
        return pool->Read(this, data);
    }

    uint32_t Read(void * data, uint32_t size) {
        return pool->Read(this, data, size);
    }

    void Release() {
        return pool->ReleaseReader(this);
    }

    friend class MemoryPoolImpl;

private:
    MemoryPool * pool;
    void * rptr;
    bool lost;
    uint32_t length;
};


MemoryReaderImpl::MemoryReaderImpl()
{
    this->lost = false;
}


MemoryReaderImpl::~MemoryReaderImpl()
{
}


//////////////////////////////////////////////////////////////////////////
// MemoryPoolImpl
//////////////////////////////////////////////////////////////////////////
class MemoryPoolImpl : public MemoryPool
{

public:
    MemoryPoolImpl();
    ~MemoryPoolImpl();

    void Create(uint32_t size);
    void Destory();

    MemoryReader* GetReader();
    void ReleaseReader(MemoryReader * reader);

    uint32_t Write(const void * data, uint32_t size);
    uint32_t Read(MemoryReader * reader, void * data);
    uint32_t Read(MemoryReader * reader, void * data, uint32_t size);

    uint32_t Lock(void ** ptr, uint32_t size);
    uint32_t UnLock(void * ptr, uint32_t size);

private:
    void Init();
    void Uninit();

    uint32_t GetWritePtr(void **p, uint32_t size);
    void CheckReader(uint32_t size);


private:
    bool created = false;

    void * poolhead;
    void * pooltail;
    uint32_t poolsize;

    void * wptr;

    std::list<MemoryReader *> readerList;

    pthread_rwlock_t memorylock;
    pthread_mutex_t  readmutex;

    struct bufferhead 
    {
        uint32_t size;
        uint32_t length;
    };
};


MemoryPool * MemoryPool::Create(uint32_t size) {

    MemoryPoolImpl * pool = new MemoryPoolImpl();
    if (pool)
    {
        pool->Create(size);
    }

    return pool;

}


MemoryPoolImpl::MemoryPoolImpl()
{
    Init();
}


MemoryPoolImpl::~MemoryPoolImpl()
{
    Uninit();
}


void MemoryPoolImpl::Init()
{
    pthread_rwlock_init(&memorylock, NULL);
}


uint32_t MemoryPoolImpl::GetWritePtr(void **p, uint32_t size)
{
    bufferhead * head = NULL;
    uint32_t offset = size + sizeof(bufferhead);

    uint32_t taillen = (char*)this->pooltail - (char*)this->wptr;
    if (taillen < size + sizeof(bufferhead))
    {
        if (taillen>=sizeof(bufferhead)) //确保尾部剩余空间不小于 bufferhead 的大小
        {
            head = (bufferhead *)this->wptr;
            head->size = taillen;
            head->length = 0;
        }
        head = (bufferhead *)this->poolhead;
        offset += taillen;
    }
    else
    {
        head = (bufferhead *)this->wptr;
    }

    head->size = size + sizeof(bufferhead);
    *p = head;

    uint32_t tailleft = (char*)this->pooltail - (char*)head;
    if (tailleft<sizeof(bufferhead)) //确保不会出现尾部剩余空间小于 bufferhead 的情况
    {
        head->size += tailleft;
        offset += tailleft;
    } 

    head->length = 0;
    this->wptr = (char*)head + head->size;

    head = (bufferhead*)this->wptr; //初始化下个要读的地址的头部
    head->size = 0;
    head->length = 0;

    return offset;
}


void MemoryPoolImpl::CheckReader(uint32_t size)
{
    pthread_mutex_lock(&readmutex);
    for (std::list<MemoryReader *>::iterator iter = readerList.begin(); iter != readerList.end(); ++iter)
    {
        MemoryReaderImpl* reader = (MemoryReaderImpl*)*iter;
        bufferhead* head = (bufferhead*)reader->rptr;
        bool overlap = false;
        if (reader->rptr==this->wptr && head->size==0)
        {
            //MemoryPool为空，一定不会丢失数据
        } 
        else if(reader->rptr==this->wptr && head->size != 0)
        {
            //MemoryPool已满，一定会丢失数据
            overlap = true;
        }
        else if (reader->rptr > this->wptr)
        {
            if ((char*)this->wptr+size > reader->rptr)
            {
                overlap = true;
            }
        } 
        else if(reader->rptr < this->wptr)
        {
            uint32_t tailen = (char*)this->pooltail - (char*)this->wptr;
            uint32_t headlen = (char*)reader->rptr - (char*)this->poolhead;
            if (size > tailen + headlen)
            {
                overlap = true;
            }
        }
        if (overlap)
        {
            reader->lost = true;

            uint32_t tailen = (char*)this->pooltail - (char*)this->wptr;
            if (size >= tailen)
            {
                reader->rptr = (char*)this->poolhead + (size - tailen);
            }
            else
            {
                reader->rptr = (char*)this->wptr + size;
            }
        }
        else 
        { 
            reader->lost = false;
        }
    }
    pthread_mutex_unlock(&readmutex);
}


void MemoryPoolImpl::Uninit()
{
    pthread_rwlock_destroy(&memorylock);
}


void MemoryPoolImpl::Create(uint32_t size)
{
    if (size<=sizeof(bufferhead))
    {
        return;
    }

    pthread_rwlock_wrlock(&memorylock);

    this->poolhead = malloc(size);
    this->pooltail = (char*)this->poolhead + size;
    this->poolsize = size;
    this->wptr = this->poolhead;

    bufferhead *head = (bufferhead*)this->wptr;
    head->size = 0;
    head->length = 0;

    this->created = true;

    pthread_rwlock_unlock(&memorylock);

}


void MemoryPoolImpl::Destory()
{
    delete this;
}


MemoryReader* MemoryPoolImpl::GetReader()
{
    MemoryReaderImpl * reader = new MemoryReaderImpl();
    reader->pool = this;
    reader->rptr = this->wptr;
    reader->length = 0;

    pthread_mutex_lock(&readmutex);
    readerList.push_back(reader);
    pthread_mutex_unlock(&readmutex);

    return reader;
}


void MemoryPoolImpl::ReleaseReader(MemoryReader * reader)
{
    if (!reader)
    {
        return;
    }

    pthread_mutex_lock(&readmutex);
    readerList.remove(reader);
    pthread_mutex_unlock(&readmutex);

    delete reader;
}


uint32_t MemoryPoolImpl::Write(const void * data, uint32_t size)
{
    if (!data || size>this->poolsize || !created)
    {
        return 0;
    }

    pthread_rwlock_wrlock(&memorylock);

    bufferhead * head = NULL;
    uint32_t offset = GetWritePtr((void**)&head, size);

    CheckReader(offset);

    void *p = (char*)head + sizeof(bufferhead);
    memcpy(p, data, size);
    head->length = size;

    pthread_rwlock_unlock(&memorylock);

    return size;
}


uint32_t MemoryPoolImpl::Read(MemoryReader * reader, void * data)
{
    if (!reader || !data)
    {
        return 0;
    }

    MemoryReaderImpl *r = (MemoryReaderImpl*)reader;

    uint32_t len = 0;

    pthread_rwlock_rdlock(&memorylock);

    void * ptr = r->rptr;
    bufferhead * head = (bufferhead*)ptr;
    if (head->size == 0)
    {
        goto FAILED;
    }
    while (head->length==0)
    {
        ptr = (char*)ptr + head->size;
        if (ptr == this->pooltail)
        {
            ptr = this->poolhead;
        }
        head = (bufferhead*)((char*)head + head->size);
        if (head->size==0)
        {
            goto FAILED;
        }
    }
    void * p = (char*)head + sizeof(bufferhead);
    len = head->length;
    memcpy(data, p, len);

    ptr = (char*)ptr + head->size;
    if (ptr == this->pooltail)
    {
        ptr = this->poolhead;
    }
    r->rptr = ptr;

FAILED:
    pthread_rwlock_unlock(&memorylock);

    return len;
}


uint32_t MemoryPoolImpl::Read(MemoryReader * reader, void * data, uint32_t size)
{
    if (!reader || !data || !size)
    {
        return 0;
    }

    MemoryReaderImpl *r = (MemoryReaderImpl*)reader;

    uint32_t len = 0;

    pthread_rwlock_rdlock(&memorylock);

    void * ptr = r->rptr;
    bufferhead * head = (bufferhead*)ptr;
    if (head->size == 0)
    {
        goto FAILED;
    }
    while (head->length == 0)
    {
        ptr = (char*)ptr + head->size;
        if (ptr == this->pooltail)
        {
            ptr = this->poolhead;
        }
        head = (bufferhead*)((char*)head + head->size);
        if (head->size == 0)
        {
            goto FAILED;
        }
    }
    if (head->length > size)
    {
        goto FAILED;
    }
    void * p = (char*)head + sizeof(bufferhead);
    len = head->length;
    memcpy(data, p, len);

    ptr = (char*)ptr + head->size;
    if (ptr == this->pooltail)
    {
        ptr = this->poolhead;
    }
    r->rptr = ptr;

FAILED:
    pthread_rwlock_unlock(&memorylock);

    return len;
}


uint32_t MemoryPoolImpl::Lock(void ** ptr, uint32_t size)
{
    if (!ptr || size > this->poolsize || !created)
    {
        return 0;
    }

    pthread_rwlock_wrlock(&memorylock);

    bufferhead * head = NULL;
    uint32_t offset = GetWritePtr((void**)&head, size);

    CheckReader(offset);

    *ptr = (char*)head + sizeof(bufferhead);
    head->length = 0;

    return size;
}


uint32_t MemoryPoolImpl::UnLock(void * ptr, uint32_t size)
{
    if (!ptr)
    {
        return 0;
    }

    uint32_t ret = size;

    bufferhead * head = (bufferhead*)((char*)ptr - sizeof(bufferhead));
    if (size > head->size-sizeof(bufferhead))
    {
        head->length = 0;
        ret = 0;
    }
    else
    {
        head->length = size;
    }

    pthread_rwlock_unlock(&memorylock);
    return ret;
}