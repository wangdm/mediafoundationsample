
#include "MFCaptureD3D.h"
#include "bufferpool.h"


class Buffer {

public:
	Buffer();
	~Buffer();

	int GetBufferSize() {
		return size;
	}

	int Write(const void *data, int size);
	int Read(void *data);

	friend class BufferPool;

private:
	int size = 0;
	int length = 0;
	void *ptr = NULL;

	bool used = false;

	Buffer * next;
};



Buffer::Buffer()
{
}


Buffer::~Buffer()
{
}


int Buffer::Write(const void * data, int len) {

    if (len > this->size || !data)
    {
        return 0;
    }

    memcpy(this->ptr, data, len);
    this->length = len;

    return len;
}


int Buffer::Read(void * data) {

	if (!data)
	{
		return 0;
	}

	memcpy(data, this->ptr, this->length);

	return this->length;
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
	pthread_mutex_init(&poolmutex, NULL);
	pthread_mutex_init(&buffermutex, NULL);
}


void BufferPool::Uninit()
{
	pthread_mutex_destroy(&poolmutex);
	pthread_mutex_destroy(&buffermutex);
}


void BufferPool::Create(int size, int count)
{
    pthread_mutex_lock(&poolmutex);
    if (!created && size>0 && count>0)
    {

		bufferpool = new Buffer[count];
        if (bufferpool ==NULL)
        {
            goto failed;
        }
        ptr = malloc( size * count);
        if (ptr==NULL)
        {
            delete bufferpool;
			bufferpool = NULL;
            goto failed;
        }

        for (int i = 0; i < count; i++)
        {
			bufferpool[i].ptr = (void *)((char *)ptr + size * i);
			bufferpool[i].used = false;
			bufferpool[i].size = size;
        }

        freeCount = count;
        totalCount = count;
		bufferSize = size;
        created = true;
    }
failed:
    pthread_mutex_unlock(&poolmutex);
}


void BufferPool::Destory()
{
    pthread_mutex_lock(&poolmutex);
    if (created)
    {
        int busycnt = 0;
        for (int i = 0; i < totalCount; i++)
        {
            if (bufferpool[i].used==true)
            {
                busycnt++;
            }
        }

        if (busycnt>0)
        {
        }

        if (bufferpool)
        {
            delete bufferpool;
			bufferpool = NULL;
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
}


bool BufferPool::IsCreated()
{
    return created;
}


Buffer * BufferPool::GetBuffer()
{
	Buffer * buf = NULL;
    pthread_mutex_lock(&poolmutex);
    if (created && freeCount>0)
    {
        for (int i = 0; i < totalCount; i++)
        {
            if (bufferpool[i].used==false)
            {
                buf = &bufferpool[i];
				buf->used = true;
				freeCount--;
                break;
            }
        }
    }
    pthread_mutex_unlock(&poolmutex);
	return buf;
}


void BufferPool::ReleaseBuffer(Buffer * buf)
{
    if (buf)
    {
        pthread_mutex_lock(&poolmutex);
        buf->used = false;
        buf->length = 0;
		buf->next = NULL;
        freeCount++;
        pthread_mutex_unlock(&poolmutex);
    }
}


int BufferPool::Write(const void * data, int len)
{
	Buffer *buffer = NULL;
	if (data==NULL || GetBufferSize() < len)
	{
		return -1;
	}

	buffer = GetBuffer();
	if (buffer==NULL)
	{
		return -2;
	}

	pthread_mutex_lock(&buffermutex);
	buffer->Write(data, len);
	if (head == NULL)
	{
		head = buffer;
		tail = buffer;
		head->next = tail;
	}
	pthread_mutex_unlock(&buffermutex);

	return len;
}


int BufferPool::Read(void * data)
{
	if (data==NULL || head==NULL)
	{
		return 0;
	}

	int len = 0;

	pthread_mutex_lock(&buffermutex);
	Buffer *buffer = head;
	if (head==tail)
	{
		head = tail = NULL;
	}
	else
	{
		head = head->next;
	}
	len = buffer->Read(data);
	pthread_mutex_unlock(&buffermutex);

	ReleaseBuffer(buffer);

	return len;
}


int BufferPool::Read(void * data, int size)
{
	if (data == NULL || head == NULL || size<=head->length)
	{
		return 0;
	}

	return Read(data);
}
