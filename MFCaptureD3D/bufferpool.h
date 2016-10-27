#pragma once

class Buffer;

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
    Buffer * bufferpool;
    bool created = false;

	Buffer * head = NULL;
	Buffer * tail = NULL;

    int freeCount = 0;
    int totalCount = 0;
	int bufferSize = 0;

	pthread_mutex_t poolmutex;
	pthread_mutex_t buffermutex;
};

