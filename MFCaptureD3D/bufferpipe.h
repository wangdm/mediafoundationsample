#pragma once

class BufferPipe
{

public:

	static BufferPipe * Create(uint32_t _size, uint32_t _flag);
	virtual void Destory() = 0;

	virtual uint32_t Write(const void * data, uint32_t len) = 0;
	virtual uint32_t Read(void * data, uint32_t len) = 0;

};

