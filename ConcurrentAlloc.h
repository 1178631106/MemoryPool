#pragma once
#include "ThreadCache.h"
#include "PageCache.h"

// 其实就是tcmalloc,线程调用这个函数申请空间
void* ConcurrentAlloc(size_t size) {
	if (size > MAX_BYTES) {
		size_t alignSize = SizeClass::RoundUp(size);
		size_t k = alignSize >> PAGE_SHIFT;

		PageCache::GetInstance()->_pageMtx.lock();

		Span* span = PageCache::GetInstance()->NewSpan(k);
		span->_objSize = size;

		PageCache::GetInstance()->_pageMtx.unlock();

		void* ptr = (void*)(span->_pageID << PAGE_SHIFT);
		return ptr;
	}
	else {

		if (pTLSThreadCache == nullptr) {
			//pTLSThreadCache = new ThreadCache;

			static ObjectPool<ThreadCache> objPool;
			objPool._poolMtx.lock();
			pTLSThreadCache = objPool.New();
			objPool._poolMtx.unlock();

		}
		return pTLSThreadCache->Allocate(size);
	}
}

// 线程调用这个函数用来回收空间
void ConcurrentFree(void* ptr) {
	assert(ptr);

	Span* span = PageCache::GetInstance()->MapObjectToSpan(ptr);
	size_t size = span->_objSize;

	if (size > MAX_BYTES) {
		PageCache::GetInstance()->_pageMtx.lock();

		PageCache::GetInstance()->ReleaseSpanToPageCache(span);

		PageCache::GetInstance()->_pageMtx.unlock();
	}
	else {
		pTLSThreadCache->Dellocate(ptr, size);
	}
}