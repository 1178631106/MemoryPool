#pragma once
#include"Common.h"
#include"PageCache.h"

class CentralCache {
public:
	static CentralCache* GetInstance() {
		return &_sInst;
	}

	// cc 从自己的_spanLists中为tc提供tc所需要的块空间
	/* start 和 end 表示cc提供的空间的开始结尾，输出型参数 */
	/* n 表示tc需要多少块size大小的空间 */
	/* size 表示tc需要的单块空间的大小 */
	/* 返回值是cc实际提供的空间大小 */
	size_t FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t size);

	// 获取一个管理空间非空的Span
	Span* GetOneSpan(SpanList& list, size_t size);

	// 将tc换回来的多块空间放到span中
	void ReleaseListToSpans(void* start, size_t size);

private:
	CentralCache() {}
	CentralCache(const CentralCache& copy) = delete;
	CentralCache& operator=(const CentralCache& copy) = delete;

private:
	SpanList _spanLists[FREE_LIST_NUM];  // 哈希桶中挂的是Span 双向链表
	static CentralCache _sInst;          // 饿汉模式创建一个CentralCache
};