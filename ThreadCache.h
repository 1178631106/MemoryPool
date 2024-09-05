#pragma once
#include "Common.h"



class ThreadCache {
public:
	// 线程申请size大小的空间
	void* Allocate(size_t size);

	// 回收线程中大小为size的obj空间
	void Dellocate(void* obj, size_t size);

	// ThreadCache中空间不够时，向CC申请空间
	void* FetchFromCentralCache(size_t index, size_t alignSize);

	// tc向cc归还空间List桶中的空间
	void ListTooLong(FreeList& list, size_t size); 
private:
	FreeList _freeLists[FREE_LIST_NUM];
};

// TLS的全局对象的指针，这样每个线程都一个独立的全局对象
static _declspec(thread) ThreadCache* pTLSThreadCache = nullptr;
