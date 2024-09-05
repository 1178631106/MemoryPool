#pragma once

#include"Common.h"

class PageCache {
public:
	static PageCache* GetInstance() {
		return &_sInst;
	}

	// pc从_spanLists中拿出来一个k页的span
	Span* NewSpan(size_t k);

	// 通过页地址找到span
	Span* MapObjectToSpan(void* obj);

	// 管理cc还回来的span
	void ReleaseSpanToPageCache(Span* span);

private:
	PageCache() {}

	PageCache(const PageCache& pageCache) = delete;
	PageCache& operator=(const PageCache& pc) = delete;

	static PageCache _sInst;

private:
	// pc中的哈希
	SpanList _spanLists[PAGE_NUM];

	// 哈希映射，用来快速通过页号找到对应的span
	//std::unordered_map<PageID, Span*> _idSpanMap;
	TCMalloc_PageMap1<32 - PAGE_SHIFT> _idSpanMap;

	// 创建span的对象池
	ObjectPool<Span> _spanPool;

public:
	std::mutex _pageMtx;
	
};
