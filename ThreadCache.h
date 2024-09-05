#pragma once
#include "Common.h"



class ThreadCache {
public:
	// �߳�����size��С�Ŀռ�
	void* Allocate(size_t size);

	// �����߳��д�СΪsize��obj�ռ�
	void Dellocate(void* obj, size_t size);

	// ThreadCache�пռ䲻��ʱ����CC����ռ�
	void* FetchFromCentralCache(size_t index, size_t alignSize);

	// tc��cc�黹�ռ�ListͰ�еĿռ�
	void ListTooLong(FreeList& list, size_t size); 
private:
	FreeList _freeLists[FREE_LIST_NUM];
};

// TLS��ȫ�ֶ����ָ�룬����ÿ���̶߳�һ��������ȫ�ֶ���
static _declspec(thread) ThreadCache* pTLSThreadCache = nullptr;
