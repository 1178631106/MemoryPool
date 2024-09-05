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

	// ��end��next��Ϊ�յ�ǰ���£���end��batchNum - 1��
	size_t i = 0;
	while (i < batchNum - 1 && ObjNext(end) != nullptr) {
		end = ObjNext(end);
		++actualNum;
		++i;
	}

	// �� [start, end] ���ظ�ThreadCache�󣬵���span��_freeList
	span->_freeList = ObjNext(end);
	span->use_count += actualNum;
	ObjNext(end) = nullptr;

	_spanLists[index]._mtx.unlock();

	return actualNum;
}

Span* CentralCache::GetOneSpan(SpanList& list, size_t size) {
	// ����cc����һ����û�й���ռ�ǿյ�sapn
	Span* it = list.Begin();
	while (it != list.End()) {
		if (it->_freeList != nullptr)
			return it;
		else
			it = it->_next;
	}

	// ���Ͱ�������������ccͰ���в������߳����õ���
	list._mtx.unlock();

	// �ߵ��������cc��û���ҵ�����ǿտռ��span

	// ��sizeת����ƥ���ҳ������Ϊpc�ṩһ�����ʵ�span
	size_t k = SizeClass::NumMovePage(size);

	// ����
	PageCache::GetInstance()->_pageMtx.lock();
	// ����NewSpan��ȡһ��ȫ�µ�span
	Span* span = PageCache::GetInstance()->NewSpan(k);
	span->_isUse = true;
	span->_objSize = size;
	PageCache::GetInstance()->_pageMtx.unlock();

	/* ����Ҫǿתһ�£���Ϊ_pageID��PageID����
	��size_t����unsigned long long���ģ�����ֱ�Ӹ��Ƹ�ָ��*/
	char* start = (char*)(span->_pageID << PAGE_SHIFT);
	char* end = (char*)(start + (span->_n << PAGE_SHIFT));

	// ��ʼ�з�span����Ŀռ�
	
	span->_freeList = start; // ����Ŀռ�ŵ�span->_freeList
	
	void* tail = start;
	start += size;

	int i = 0;
	// ���Ӹ�����
	while (start < end) {
		++i;
		ObjNext(tail) = start;
		start += size;
		tail = ObjNext(tail);
	}

	ObjNext(tail) = nullptr; // ���һ��λ���ÿ�

	// �к�span�Ժ���Ҫ��span�ҵ�cc��Ӧ�±��Ͱ����ȥ
	list._mtx.lock();  // span����ȥ֮ǰ����
	list.PushFront(span);

	return span;
}

void CentralCache::ReleaseListToSpans(void* start, size_t size) {
	// ��ͨ��size�ҵ���Ӧ��Ͱ������
	size_t index = SizeClass::Index(size);

	// ����Ҫ��cc�е�span���в�����Ҫ��cc����
	_spanLists[index]._mtx.lock();

	while (start) {
		// ��¼һ��start��һλ
		void* next = ObjNext(start);

		// �ҵ���Ӧspan
		Span* span = PageCache::GetInstance()->MapObjectToSpan(start);

		// �ѵ�ǰ����뵽��Ӧ��span��
		ObjNext(start) = span->_freeList;
		span->_freeList = start;

		// ������һ��ռ䣬��Ӧspan��useCountҪ��1
		span->use_count--;
		// ���span���������ҳ��������
		if (span->use_count == 0) {
			// �����span����pc����

			// �Ƚ�span��cc��ȥ��
			_spanLists[index].Erase(span);
			span->_freeList = nullptr;
			span->_next = nullptr;
			span->_prev = nullptr;

			// �黹span�������ǰͰ��
			_spanLists[index]._mtx.unlock();

			// �黹span������������
			PageCache::GetInstance()->_pageMtx.lock();
			PageCache::GetInstance()->ReleaseSpanToPageCache(span);
			PageCache::GetInstance()->_pageMtx.unlock();

			// �黹��ϣ��ټ��ϵ�ǰͰ����
			_spanLists[index]._mtx.lock();
		}

		// ����һ����
		start = next;
	}

	_spanLists[index]._mtx.unlock(); 
}
