#pragma once

class BufferPipe
{

public:
    BufferPipe();
    ~BufferPipe();

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

