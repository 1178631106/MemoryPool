#pragma once

#include"Common.h"

class PageCache {
public:
	static PageCache* GetInstance() {
		return &_sInst;
	}

	// pc��_spanLists���ó���һ��kҳ��span
	Span* NewSpan(size_t k);

	// ͨ��ҳ��ַ�ҵ�span
	Span* MapObjectToSpan(void* obj);

	// ����cc��������span
	void ReleaseSpanToPageCache(Span* span);

private:
	PageCache() {}

	PageCache(const PageCache& pageCache) = delete;
	PageCache& operator=(const PageCache& pc) = delete;

	static PageCache _sInst;

private:
	// pc�еĹ�ϣ
	SpanList _spanLists[PAGE_NUM];

	// ��ϣӳ�䣬��������ͨ��ҳ���ҵ���Ӧ��span
	//std::unordered_map<PageID, Span*> _idSpanMap;
	TCMalloc_PageMap1<32 - PAGE_SHIFT> _idSpanMap;

	// ����span�Ķ����
	ObjectPool<Span> _spanPool;

public:
	std::mutex _pageMtx;
	
};
