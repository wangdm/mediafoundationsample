
#include "MFCaptureD3D.h"
#include "bufferpool.h"


Buffer::Buffer()
{
}


Buffer::~Buffer()
{
}


int Buffer::Write(void * data, int len) {

    if (len > this->size || !data)
    {
        return 0;
    }

    memcpy(ptr, data, size);
    this->len = len;

    return len;
}


BufferPool::BufferPool()
{
    Init();
}


BufferPool::BufferPool(int size, int count)
{
    Init();
    Create(size, count);
}


BufferPool::~BufferPool()
{
    Destory();
    Uninit();
}


void BufferPool::Init()
{
    pthread_mutex_init(&mutex, NULL);
}


void BufferPool::Uninit()
{
    pthread_mutex_destroy(&mutex);
}


void BufferPool::Create(int size, int count)
{
    pthread_mutex_lock(&mutex);
    if (!created && size>0 && count>0)
    {

        buffer = new Buffer[count];
        if (buffer==NULL)
        {
            goto failed;
        }
        ptr = malloc( size * count);
        if (ptr==NULL)
        {
            delete buffer;
            buffer = NULL;
            goto failed;
        }

        for (int i = 0; i < count; i++)
        {
            buffer[i].ptr = (void *)((int)ptr + size * i);
            buffer[i].used = false;
            buffer[i].size = size;
        }

        freeCount = count;
        totalCount = count;
        created = true;
    }
failed:
    pthread_mutex_unlock(&mutex);
}


void BufferPool::Destory()
{
    pthread_mutex_lock(&mutex);
    if (created)
    {
        int busycnt = 0;
        for (int i = 0; i < totalCount; i++)
        {
            if (buffer[i].used==true)
            {
                busycnt++;
            }
        }

        if (busycnt>0)
        {
        }

        if (buffer)
        {
            delete buffer;
            buffer = NULL;
        }
        if (ptr)
        {
            free(ptr);
            ptr = NULL;
        }
        created = false;
        totalCount = 0;
        freeCount = 0;

    }
    pthread_mutex_unlock(&mutex);
}


bool BufferPool::IsCreated()
{
    return created;
}


Buffer * BufferPool::GetBuffer()
{
    pthread_mutex_lock(&mutex);
    Buffer * buf = NULL;
    if (created && freeCount>0)
    {
        for (int i = 0; i < totalCount; i++)
        {
            if (buffer[i].used==false)
            {
                buf = &buffer[i];
                buffer->used = true;
                freeCount--;
                break;
            }
        }
    }
    pthread_mutex_unlock(&mutex);
}


void BufferPool::ReleaseBuffer(Buffer * buf)
{
    if (buf)
    {
        pthread_mutex_lock(&mutex);
        buf->used = false;
        buf->len = 0;
        freeCount++;
        pthread_mutex_unlock(&mutex);
    }
}