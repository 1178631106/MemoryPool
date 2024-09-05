#include"ThreadCache.h"
#include"CentralCache.h"

// tc�пռ䲻��ʱ����cc������ռ�Ľӿ�
void* ThreadCache::FetchFromCentralCache(size_t index, size_t alignSize) {
#ifdef WIN32
	// ͨ��MaxSize �� NumMoveSize �����Ƶ�ǰ��tc�ṩ���ٿ�alignSize��С�Ŀռ�
	size_t batchNum = min(_freeLists[index].MaxSize(), SizeClass::NumMoveSize(alignSize));
#else
#endif
	

	if (batchNum == _freeLists[index].MaxSize()) {
		_freeLists[index].MaxSize()++;
	}

	// ����Ͳ���������֮��Ľ������tc��Ҫ�Ŀռ�
	void* start = nullptr;
	void* end = nullptr;

	size_t actualNum = CentralCache::GetInstance()->FetchRangeObj(start, end, batchNum, alignSize);

	assert(actualNum >= 1);

	if (actualNum == 1) {
		assert(start == end);
		return start;
	}
	else {
		_freeLists[index].PushRange(ObjNext(start), end, actualNum - 1);
		return start;
	}
}

void* ThreadCache::Allocate(size_t size) {
	assert(size <= MAX_BYTES);

	size_t alignSize = SizeClass::RoundUp(size);
	size_t index = SizeClass::Index(size);

	if (!_freeLists[index].Empty()) {
		return _freeLists[index].Pop();
	}
	else {
		return FetchFromCentralCache(index, alignSize);
	}
}

void ThreadCache::Dellocate(void* obj, size_t size) {
	assert(obj && size <= MAX_BYTES);

	size_t index = SizeClass::Index(size);
	_freeLists[index].Push(obj);

	// ��ǰͰ�еĿ������ڵ��������������ʱ��黹�ռ�
	if (_freeLists[index].Size() >= _freeLists[index].MaxSize()) {
		ListTooLong(_freeLists[index], size);
	}
}

void ThreadCache::ListTooLong(FreeList& list, size_t size) {
	void* start = nullptr;
	void* end = nullptr;

	// ��ȡMaxSize��ռ�
	list.PopRange(start, end, list.MaxSize());

	// �黹�ռ�
	CentralCache::GetInstance()->ReleaseListToSpans(start, size);
}