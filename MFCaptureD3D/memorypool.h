#pragma once

class MemoryReader 
{

public:
    virtual uint32_t Read(void * data) = 0;
    virtual uint32_t Read(void * data, uint32_t size) = 0;
    virtual void Release() = 0;
};

class MemoryPool
{

public:

    static MemoryPool * Create(uint32_t size);
    virtual void Destory() = 0;

    virtual MemoryReader* GetReader() = 0;
    virtual void ReleaseReader(MemoryReader * reader) = 0;

    virtual uint32_t Write(const void * data, uint32_t size) = 0;
    virtual uint32_t Read(MemoryReader * reader, void * data) = 0;
    virtual uint32_t Read(MemoryReader * reader, void * data, uint32_t size) = 0;

    virtual uint32_t Lock(void ** ptr, uint32_t size) = 0;
    virtual uint32_t UnLock(void * ptr, uint32_t size) = 0;

};

