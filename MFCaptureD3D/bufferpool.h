#pragma once

class Buffer
{

public:

	virtual int GetBufferSize() = 0;

	virtual int Write(const void *data, int size) = 0;
	virtual int Read(void *data) = 0;

};


class BufferPool
{

public:

	static BufferPool * Create(uint32_t size, uint32_t count);
	virtual void Destory() = 0;
	virtual bool IsCreated() = 0;

	virtual Buffer* GetBuffer() = 0;
	virtual void ReleaseBuffer(Buffer * buf) = 0;

	virtual int Write(const void * data, int size) = 0;
	virtual int Read(void * data) = 0;
	virtual int Read(void * data, int size) = 0;

	virtual int GetBufferSize() = 0;
	virtual int GetTotalCount() = 0;
	virtual int GetFreeCount() = 0;

};

