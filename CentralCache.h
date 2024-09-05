#pragma once
#include"Common.h"
#include"PageCache.h"

class CentralCache {
public:
	static CentralCache* GetInstance() {
		return &_sInst;
	}

	// cc ���Լ���_spanLists��Ϊtc�ṩtc����Ҫ�Ŀ�ռ�
	/* start �� end ��ʾcc�ṩ�Ŀռ�Ŀ�ʼ��β������Ͳ��� */
	/* n ��ʾtc��Ҫ���ٿ�size��С�Ŀռ� */
	/* size ��ʾtc��Ҫ�ĵ���ռ�Ĵ�С */
	/* ����ֵ��ccʵ���ṩ�Ŀռ��С */
	size_t FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t size);

	// ��ȡһ������ռ�ǿյ�Span
	Span* GetOneSpan(SpanList& list, size_t size);

	// ��tc�������Ķ��ռ�ŵ�span��
	void ReleaseListToSpans(void* start, size_t size);

private:
	CentralCache() {}
	CentralCache(const CentralCache& copy) = delete;
	CentralCache& operator=(const CentralCache& copy) = delete;

private:
	SpanList _spanLists[FREE_LIST_NUM];  // ��ϣͰ�йҵ���Span ˫������
	static CentralCache _sInst;          // ����ģʽ����һ��CentralCache
};