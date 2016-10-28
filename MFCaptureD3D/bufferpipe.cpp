
extern "C" {
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
}

#include "bufferpipe.h"



class BufferPipeImpl : public BufferPipe
{

public:
	BufferPipeImpl();
	~BufferPipeImpl();

	void Create(uint32_t _size, uint32_t _flag);
	void Destory();

	uint32_t Write(const void * data, uint32_t len);
	uint32_t Read(void * data, uint32_t len);

private:
	void Init();
	void Uninit();

private:
	bool created = false;

	void * head;
	void * tail;
	uint32_t size;
	uint32_t length;

	void * rptr;
	void * wptr;

	uint32_t flag;

	pthread_mutex_t mutex;
};


BufferPipe * BufferPipe::Create(uint32_t size, uint32_t count) {

	BufferPipeImpl * pipe = new BufferPipeImpl();
	if (pipe)
	{
		pipe->Create(size, count);
	}

	return pipe;

}


BufferPipeImpl::BufferPipeImpl()
{
	Init();
}


BufferPipeImpl::~BufferPipeImpl()
{
	Uninit();
}


void BufferPipeImpl::Init()
{
	pthread_mutex_init(&mutex, NULL);
}


void BufferPipeImpl::Uninit()
{
	pthread_mutex_destroy(&mutex);
}


void BufferPipeImpl::Create(uint32_t _size, uint32_t _flag)
{
	if (_size <= 0)
	{
		return;
	}
	pthread_mutex_lock(&mutex);
	if (!created)
	{
		this->head = malloc(_size);
		if (this->head==NULL)
		{
			goto failed;
		}
		this->tail = (char *)this->head + _size;
		this->size = _size;
		this->length = 0;
		this->rptr = this->wptr = this->head;
		this->flag = _flag;
		this->created = true;
	}
failed:
	pthread_mutex_unlock(&mutex);
}


void BufferPipeImpl::Destory()
{
	pthread_mutex_lock(&mutex);
	if (created)
	{
		free(this->head);
		this->created = false;
	}
	pthread_mutex_unlock(&mutex);

	delete this;
}


uint32_t BufferPipeImpl::Write(const void * data, uint32_t len)
{
	if (data==NULL || len<=0)
	{
		return 0;
	}

	pthread_mutex_lock(&mutex);
	if (created)
	{
		if (len > this->size - this->length)
		{
			pthread_mutex_unlock(&mutex);
			return 0;
		}

		uint32_t taillen = (char*)this->tail - (char*)this->wptr;
		if (len > taillen)
		{
			memcpy(this->wptr, data, taillen);
			memcpy(this->head, &((char*)data)[taillen], len - taillen);
			this->wptr = (char *)this->head + (len - taillen);
		}
		else
		{
			memcpy(this->wptr, data, len);
			this->wptr = (char *)this->wptr + len;
		}
		this->length += len;
	}
	pthread_mutex_unlock(&mutex);

	return len;
}


uint32_t BufferPipeImpl::Read(void * data, uint32_t len)
{
	if (data == NULL || len <= 0)
	{
		return 0;
	}

	pthread_mutex_lock(&mutex);
	if (created)
	{
		if (len > this->length)
		{
			pthread_mutex_unlock(&mutex);
			return 0;
		}

		uint32_t taillen = (char*)this->tail - (char*)this->rptr;
		if (len > taillen)
		{
			memcpy(data, this->rptr, taillen);
			memcpy(&((char*)data)[taillen], this->head, len - taillen);
			this->rptr = (char *)this->head + (len - taillen);
		}
		else 
		{
			memcpy(data, this->rptr, len);
			this->rptr = (char*)this->rptr + len;
		}
		this->length -= len;
	}
	pthread_mutex_unlock(&mutex);

	return len;
}