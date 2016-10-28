
#include <vector>
#include <queue>

#include <pthread.h>

#include "bufferpool.h"

using namespace std;


class BufferImpl : public Buffer
{

public:
	BufferImpl();
	~BufferImpl();

	int GetBufferSize() {
		return size;
	}

	int Write(const void *data, int size);
	int Read(void *data);

	friend class BufferPoolImpl;

private:
	int size = 0;
	int length = 0;
	void *ptr = NULL;

	bool used = false;
};


BufferImpl::BufferImpl()
{
}


BufferImpl::~BufferImpl()
{
}


int BufferImpl::Write(const void * data, int len) {

    if (len > this->size || !data)
    {
        return 0;
    }

    memcpy(this->ptr, data, len);
    this->length = len;

    return len;
}


int BufferImpl::Read(void * data) {

	if (!data)
	{
		return 0;
	}

	memcpy(data, this->ptr, this->length);

	return this->length;
}



//////////////////////////////////////////////////////////////////////////
// BufferPoolImpl
//////////////////////////////////////////////////////////////////////////
class BufferPoolImpl : public BufferPool
{

public:
	BufferPoolImpl();
	BufferPoolImpl(int size, int count);
	~BufferPoolImpl();

	void Create(int size, int count);
	void Destory();
	bool IsCreated();

	Buffer* GetBuffer();
	void ReleaseBuffer(Buffer * buf);

	int Write(const void * data, int size);
	int Read(void * data);
	int Read(void * data, int size);

	int GetBufferSize() {
		return bufferSize;
	}

	int GetTotalCount() {
		return totalCount;
	}

	int GetFreeCount() {
		return freeCount;
	}

private:
	void Init();
	void Uninit();


private:
	void * ptr;
	bool created = false;

	std::vector<BufferImpl> * bufferArray;
	std::queue<BufferImpl*> bufferQueue;

	int freeCount = 0;
	int totalCount = 0;
	int bufferSize = 0;

	pthread_mutex_t poolmutex;
	pthread_mutex_t buffermutex;
};


BufferPool * BufferPool::Create(uint32_t size, uint32_t count) {

	BufferPoolImpl * pool = new BufferPoolImpl();
	if (pool)
	{
		pool->Create(size, count);
	}

	return pool;

}


BufferPoolImpl::BufferPoolImpl()
{
    Init();
}


BufferPoolImpl::BufferPoolImpl(int size, int count)
{
    Init();
    Create(size, count);
}


BufferPoolImpl::~BufferPoolImpl()
{
    Uninit();
}


void BufferPoolImpl::Init()
{
	pthread_mutex_init(&poolmutex, NULL);
	pthread_mutex_init(&buffermutex, NULL);
}


void BufferPoolImpl::Uninit()
{
	pthread_mutex_destroy(&poolmutex);
	pthread_mutex_destroy(&buffermutex);
}


void BufferPoolImpl::Create(int size, int count)
{
    pthread_mutex_lock(&poolmutex);
    if (!created && size>0 && count>0)
    {

		bufferArray = new std::vector<BufferImpl>[count];
        if (bufferArray ==NULL)
        {
            goto failed;
        }
        ptr = malloc( size * count);
        if (ptr==NULL)
        {
            delete bufferArray;
			bufferArray = NULL;
            goto failed;
        }

        for (int i = 0; i < count; i++)
        {
			(*bufferArray)[i].ptr = (void *)((char *)ptr + size * i);
			(*bufferArray)[i].used = false;
			(*bufferArray)[i].size = size;
        }

        freeCount = count;
        totalCount = count;
		bufferSize = size;
        created = true;
    }
failed:
    pthread_mutex_unlock(&poolmutex);
}


void BufferPoolImpl::Destory()
{
    pthread_mutex_lock(&poolmutex);
    if (created)
    {
        int busycnt = 0;
        for (int i = 0; i < totalCount; i++)
        {
            if ((*bufferArray)[i].used==true)
            {
                busycnt++;
            }
        }

        if (busycnt>0)
        {
        }

        if ((bufferArray))
        {
            delete bufferArray;
			bufferArray = NULL;
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
    pthread_mutex_unlock(&poolmutex);

	delete this;
}


bool BufferPoolImpl::IsCreated()
{
    return created;
}


Buffer * BufferPoolImpl::GetBuffer()
{
	BufferImpl * buf = NULL;
    pthread_mutex_lock(&poolmutex);
    if (created && freeCount>0)
    {
        for (int i = 0; i < totalCount; i++)
        {
            if ((*bufferArray)[i].used==false)
            {
                buf = &(*bufferArray)[i];
				buf->used = true;
				freeCount--;
                break;
            }
        }
    }
    pthread_mutex_unlock(&poolmutex);
	return buf;
}


void BufferPoolImpl::ReleaseBuffer(Buffer * buf)
{
    if (buf)
    {
		BufferImpl * buffer = (BufferImpl *)buf;
        pthread_mutex_lock(&poolmutex);
		buffer->used = false;
		buffer->length = 0;
        freeCount++;
        pthread_mutex_unlock(&poolmutex);
    }
}


int BufferPoolImpl::Write(const void * data, int len)
{
	BufferImpl *buffer = NULL;
	if (data==NULL || GetBufferSize() < len)
	{
		return -1;
	}

	buffer = (BufferImpl*)GetBuffer();
	if (buffer==NULL)
	{
		return -2;
	}

	pthread_mutex_lock(&buffermutex);
	
	buffer->Write(data, len);
	bufferQueue.push(buffer);

	pthread_mutex_unlock(&buffermutex);

	return len;
}


int BufferPoolImpl::Read(void * data)
{
	if (data==NULL)
	{
		return 0;
	}

	int len = 0; 
	BufferImpl *buffer = NULL;

	pthread_mutex_lock(&buffermutex);

	if (!bufferQueue.empty())
	{
		buffer = bufferQueue.front();
		len = buffer->Read(data);
		bufferQueue.pop();
	}

	pthread_mutex_unlock(&buffermutex);

	if (buffer)
	{
		ReleaseBuffer(buffer);
	}

	return len;
}


int BufferPoolImpl::Read(void * data, int size)
{
	if (data == NULL)
	{
		return 0;
	}

	int len = 0;
	BufferImpl *buffer = NULL;

	pthread_mutex_lock(&buffermutex);

	if (!bufferQueue.empty())
	{
		buffer = bufferQueue.front();
		if (buffer->length <= size)
		{
			len = buffer->Read(data);
			bufferQueue.pop();
		}
		else {
			buffer = NULL;
		}
	}

	pthread_mutex_unlock(&buffermutex);

	if (buffer)
	{
		ReleaseBuffer(buffer);
	}

	return len;
}
