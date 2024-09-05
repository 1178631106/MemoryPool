#include "PageCache.h"

PageCache PageCache::_sInst;   // 单例对象

Span* PageCache::NewSpan(size_t k) {
	assert(k > 0);

	if (k > PAGE_NUM - 1) {
		void* ptr = SystemAlloc(k);
		Span* span = _spanPool.New();

		span->_pageID = ((PageID)ptr) >> PAGE_SHIFT;
		span->_n = k;

		_idSpanMap.set(span->_pageID, span);

		return span;
	}

	// 1、k号桶中有span
	if (!_spanLists[k].Empty()) {
		Span* span = _spanLists[k].PopFront();

		// 记录分配出去的span管理的页号和其地址的映射关系
		for (PageID i = 0; i < span->_n; i++) {
			_idSpanMap.set(span->_pageID + i, span);
		}
		
		return span;
	}

	// 2、k号桶中没有，但是后面的桶中有
	for (int i = k + 1; i < PAGE_NUM; ++i) {
		if (!_spanLists[i].Empty()) {
			// 获取到该桶中的span
			Span* nSpan = _spanLists[i].PopFront();

			// 切分该span为 k 和 n - k

			// Span的空间是需要新建的，而不是用当前内存池的空间
			Span* kSpan = _spanPool.New();

			// 分一个k页的span
			kSpan->_pageID = nSpan->_pageID;
			kSpan->_n = k;

			// 分n-k页的span
			nSpan->_pageID += k;
			nSpan->_n -= k;

			// n-k页的放回对应的哈希桶中
			_spanLists[nSpan->_n].PushFront(nSpan);

			// 再把n-k页的span边缘页映射一下，方便后续合并
			_idSpanMap.set(nSpan->_pageID, nSpan);
			_idSpanMap.set(nSpan->_pageID + nSpan->_n - 1, nSpan);

			// 记录kSpan的页号与span地址的映射
			for (PageID i = 0; i < kSpan->_n; ++i) {
				_idSpanMap.set(kSpan->_pageID + i, kSpan);
			}

			return kSpan;
		}
	}

	// 3、所有桶都没有，向系统申请

	// 直接向系统申请128页的span
	void* ptr = SystemAlloc(PAGE_NUM - 1);

	// 开一个新的span来维护这块空间
	Span* bigSpan = _spanPool.New();

	// 只需要修改_pageID和_n即可，系统调用接口申请的空间一定能保证申请的空间是对齐的
	bigSpan->_pageID = ((PageID)ptr) >> PAGE_SHIFT;
	bigSpan->_n = PAGE_NUM - 1;

	// 将这个span放到对应的哈希桶中
	_spanLists[PAGE_NUM - 1].PushFront(bigSpan);

	return NewSpan(k);
}

Span* PageCache::MapObjectToSpan(void* obj) {
	// 通过块地址找到页号
	PageID id = (((PageID)obj) >> PAGE_SHIFT);

	// 智能锁
	//std::unique_lock<std::mutex> lc(_pageMtx);

	// 通过哈希找到页号对应的span
	auto ret = _idSpanMap.get(id);

	// 一定能保证通过块地址找到一个span，如果没找到就是出错了
	if (ret != nullptr) {
		return (Span*)ret;
	}
	else {
		assert(false);
		return nullptr;
	}
}

void PageCache::ReleaseSpanToPageCache(Span* span) {
	if (span->_n > PAGE_NUM - 1) {
		void* ptr = (void*)(span->_pageID << PAGE_SHIFT);
		SystemFree(ptr);
		//delete span;
		_spanPool.Delete(span);

		return;
	}


	// 向左不断合并
	while (1) {
		PageID leftID = span->_pageID - 1;
		auto ret = _idSpanMap.get(leftID);

		// 没有相邻span，停止合并
		if (ret == nullptr) {
			break;
		}

		Span* leftSpan = (Span*)ret;
		// 相邻span在cc中，停止合并
		if (leftSpan->_isUse == true) {
			break;
		}

		// 相邻span与当前span合并后超出128页，停止合并
		if (leftSpan->_n + span->_n > PAGE_NUM - 1) {
			break;
		}

		// 当前span和相邻span进行合并
		span->_pageID = leftSpan->_pageID;
		span->_n += leftSpan->_n;

		_spanLists[leftSpan->_n].Erase(leftSpan);  // 将相邻span对象从桶中删掉
		//delete leftSpan;                           // 删除掉相邻span对象
		_spanPool.Delete(leftSpan);
	}

	// 向右不断合并
	while (1) {
		PageID rightID = span->_pageID + span->_n;
		auto ret = _idSpanMap.get(rightID);

		// 没有相邻span，停止合并
		if (ret == nullptr) {
			break;
		}

		Span* rightSpan = (Span*)ret;
		// 相邻span在cc中，停止合并
		if (rightSpan->_isUse == true) {
			break;
		}

		// 相邻span与当前span合并后超出128页，停止合并
		if (rightSpan->_n + span->_n > PAGE_NUM - 1) {
			break;
		}

		// 当前span和相邻span进行合并
		span->_n += rightSpan->_n;

		_spanLists[rightSpan->_n].Erase(rightSpan);  // 将相邻span对象从桶中删掉
		/*delete rightSpan;*/                           // 删除掉相邻span对象
		_spanPool.Delete(rightSpan);
	}

	// 合并完毕，将当前span挂到对应桶中
	_spanLists[span->_n].PushFront(span);
	span->_isUse = false;   

	// 映射当前span的边缘页，后续还可以对这个span进行合并
	/*_idSpanMap[span->_pageID] = span;
	_idSpanMap[span->_pageID + span->_n - 1] = span;*/
	_idSpanMap.set(span->_pageID, span);
	_idSpanMap.set(span->_pageID + span->_n - 1, span);
}
