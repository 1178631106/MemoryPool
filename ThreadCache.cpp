#include"ThreadCache.h"
#include"CentralCache.h"

// tc中空间不够时，向cc中申请空间的接口
void* ThreadCache::FetchFromCentralCache(size_t index, size_t alignSize) {
#ifdef WIN32
	// 通过MaxSize 和 NumMoveSize 来控制当前给tc提供多少块alignSize大小的空间
	size_t batchNum = min(_freeLists[index].MaxSize(), SizeClass::NumMoveSize(alignSize));
#else
#endif
	

	if (batchNum == _freeLists[index].MaxSize()) {
		_freeLists[index].MaxSize()++;
	}

	// 输出型参数，返回之后的结果就是tc想要的空间
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

	// 当前桶中的块数大于单批次申请块数的时候归还空间
	if (_freeLists[index].Size() >= _freeLists[index].MaxSize()) {
		ListTooLong(_freeLists[index], size);
	}
}

void ThreadCache::ListTooLong(FreeList& list, size_t size) {
	void* start = nullptr;
	void* end = nullptr;

	// 获取MaxSize块空间
	list.PopRange(start, end, list.MaxSize());

	// 归还空间
	CentralCache::GetInstance()->ReleaseListToSpans(start, size);
}