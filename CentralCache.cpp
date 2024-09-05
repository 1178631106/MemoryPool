#include"CentralCache.h"
#include"PageCache.h"

CentralCache CentralCache::_sInst;

size_t CentralCache::FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t size) {
	size_t index = SizeClass::Index(size);

	_spanLists[index]._mtx.lock();

	Span* span = GetOneSpan(_spanLists[index], size);
	assert(span);
	assert(span->_freeList);

	start = end = span->_freeList;
	size_t actualNum = 1;

	// 在end的next不为空的前提下，让end走batchNum - 1步
	size_t i = 0;
	while (i < batchNum - 1 && ObjNext(end) != nullptr) {
		end = ObjNext(end);
		++actualNum;
		++i;
	}

	// 将 [start, end] 返回给ThreadCache后，调整span的_freeList
	span->_freeList = ObjNext(end);
	span->use_count += actualNum;
	ObjNext(end) = nullptr;

	_spanLists[index]._mtx.unlock();

	return actualNum;
}

Span* CentralCache::GetOneSpan(SpanList& list, size_t size) {
	// 先在cc中找一下有没有管理空间非空的sapn
	Span* it = list.Begin();
	while (it != list.End()) {
		if (it->_freeList != nullptr)
			return it;
		else
			it = it->_next;
	}

	// 解掉桶锁，让其他向该cc桶进行操作的线程能拿到锁
	list._mtx.unlock();

	// 走到这里就是cc中没有找到管理非空空间的span

	// 将size转换成匹配的页数，以为pc提供一个合适的span
	size_t k = SizeClass::NumMovePage(size);

	// 加锁
	PageCache::GetInstance()->_pageMtx.lock();
	// 调用NewSpan获取一个全新的span
	Span* span = PageCache::GetInstance()->NewSpan(k);
	span->_isUse = true;
	span->_objSize = size;
	PageCache::GetInstance()->_pageMtx.unlock();

	/* 这里要强转一下，因为_pageID是PageID类型
	（size_t或者unsigned long long）的，不能直接复制给指针*/
	char* start = (char*)(span->_pageID << PAGE_SHIFT);
	char* end = (char*)(start + (span->_n << PAGE_SHIFT));

	// 开始切分span管理的空间
	
	span->_freeList = start; // 管理的空间放到span->_freeList
	
	void* tail = start;
	start += size;

	int i = 0;
	// 连接各个块
	while (start < end) {
		++i;
		ObjNext(tail) = start;
		start += size;
		tail = ObjNext(tail);
	}

	ObjNext(tail) = nullptr; // 最后一个位置置空

	// 切好span以后，需要把span挂到cc对应下标的桶里面去
	list._mtx.lock();  // span挂上去之前加锁
	list.PushFront(span);

	return span;
}

void CentralCache::ReleaseListToSpans(void* start, size_t size) {
	// 先通过size找到对应的桶在哪里
	size_t index = SizeClass::Index(size);

	// 下面要对cc中的span进行操作，要对cc加锁
	_spanLists[index]._mtx.lock();

	while (start) {
		// 记录一下start下一位
		void* next = ObjNext(start);

		// 找到对应span
		Span* span = PageCache::GetInstance()->MapObjectToSpan(start);

		// 把当前块插入到对应的span中
		ObjNext(start) = span->_freeList;
		span->_freeList = start;

		// 还回了一块空间，对应span的useCount要减1
		span->use_count--;
		// 这个span管理的所有页都回来了
		if (span->use_count == 0) {
			// 将这个span交给pc管理

			// 先将span从cc中去掉
			_spanLists[index].Erase(span);
			span->_freeList = nullptr;
			span->_next = nullptr;
			span->_prev = nullptr;

			// 归还span，解掉当前桶锁
			_spanLists[index]._mtx.unlock();

			// 归还span，加上整体锁
			PageCache::GetInstance()->_pageMtx.lock();
			PageCache::GetInstance()->ReleaseSpanToPageCache(span);
			PageCache::GetInstance()->_pageMtx.unlock();

			// 归还完毕，再加上当前桶的锁
			_spanLists[index]._mtx.lock();
		}

		// 换下一个块
		start = next;
	}

	_spanLists[index]._mtx.unlock(); 
}
