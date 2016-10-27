#pragma once

class Buffer {

public:
    Buffer();
    ~Buffer();

    int GetBufferSize() {
        return size;
    }

    int Write(void *data, int size);

    friend class BufferPool;

private:
    int size = 0;
    int len = 0;
    void *ptr = NULL;

    bool used = false;
};


class BufferPool
{

public:
    BufferPool();
    BufferPool(int size, int count);
    ~BufferPool();

    void Create(int size, int count);
    void Destory();
    bool IsCreated();

    Buffer* GetBuffer();
    void ReleaseBuffer(Buffer * buf);

private:
    void Init();
    void Uninit();


private:
    void * ptr;
    Buffer * buffer;
    bool created = false;

    int freeCount = 0;
    int totalCount = 0;

    pthread_mutex_t mutex;
};

