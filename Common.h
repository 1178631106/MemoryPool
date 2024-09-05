#pragma once

#include<iostream>
#include<vector>
#include <cassert>
#include<mutex>
#include<math.h>
#include<unordered_map>

#ifdef _WIN32
	#include<Windows.h>
#else
	
#endif // _WIN32


using std::vector;
using std::cout;
using std::endl;

static const size_t FREE_LIST_NUM = 208;
static const size_t MAX_BYTES = 256 * 1024;
static const size_t PAGE_NUM = 129;
static const size_t PAGE_SHIFT = 13;

typedef size_t PageID;

inline static void* SystemAlloc(size_t kpage) {
#ifdef _WIN32
	void* ptr = VirtualAlloc(0, kpage << PAGE_SHIFT, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else

#endif // _WIN32

	if (ptr == nullptr) {
		throw std::bad_alloc();
	}

	return ptr;
}

// 直接去堆上释放空间
inline static void SystemFree(void* ptr) {
#ifdef _WIN32
	VirtualFree(ptr, 0, MEM_RELEASE);
#else

#endif // _WIN32
}

static void*& ObjNext(void* obj) {
	return *(void**)obj;
}

#include "ObjectPool.h"

struct Span {
	PageID _pageID = 0;      // 页号
	size_t _n = 0;           // 当前span管理的页的数量
	size_t _objSize = 0;
	                
	void* _freeList = nullptr;   // 每个span下面挂的小块空间的头结点
	size_t use_count = 0;    // 当前span分配出去了多少个块空间

	Span* _prev = nullptr;   // 前一个节点
	Span* _next = nullptr;   // 后一个节点

	bool _isUse = false;
};

class SpanList
{
public:
	// 删除掉第一个span
	Span* PopFront()
	{
		// 先获取到_head后面的第一个span
		Span* front = _head->_next;
		// 删除掉这个span，直接复用Erase
		Erase(front);

		// 返回原来的第一个span
		return front;
	}

	// 判空
	bool Empty()
	{ // 带头双向循环空的时候_head指向自己
		return _head == _head->_next;
	}

	// 头插
	void PushFront(Span* span)
	{
		Insert(Begin(), span);
	}

	// 头结点
	Span* Begin()
	{
		return _head->_next;
	}

	// 尾结点
	Span* End()
	{
		return _head;
	}

	void Erase(Span* pos)
	{
		assert(pos); // pos不为空
		assert(pos != _head); // pos不能是哨兵位

		Span* prev = pos->_prev;
		Span* next = pos->_next;

		prev->_next = next;
		next->_prev = prev;

		/*pos节点不需要调用delete删除，因为
		pos节点的Span需要回收，而不是直接删掉*/
		// 回收相关逻辑
	}

	void Insert(Span* pos, Span* ptr)
	{ // 在pos前面插入ptr
		assert(pos); // pos不为空
		assert(ptr); // ptr不为空

		// 插入相关逻辑
		Span* prev = pos->_prev;

		prev->_next = ptr;
		ptr->_prev = prev;

		ptr->_next = pos;
		pos->_prev = ptr;
	}

	SpanList()
	{ // 构造函数中搞哨兵位头结点
		_head = new Span;

		// 因为是双向循环的，所以都指向_head
		_head->_next = _head;
		_head->_prev = _head;
	}

private:
	Span* _head; // 哨兵位头结点
public:
	std::mutex _mtx; // 每个CentralCache中的哈希桶都要有一个桶锁
};


class FreeList {
public:
	// 删除掉桶中n个块（头删），并把删除的空间作为输出型参数返回
	void PopRange(void*& start, void*& end, size_t n) {
		//删除块数不能超过size块
		assert(n <= _size);

		start = end = _freeList;

		for (size_t i = 0; i < n - 1; i++) {
			end = ObjNext(end);
		}

		_freeList = ObjNext(end);
		ObjNext(end) = nullptr;
		_size -= n;
	}

	// 向自由链表中头插，且插入多块空间
	void PushRange(void* start, void* end, size_t size) {
		ObjNext(end) = _freeList;
		_freeList = start;

		_size += size;
	}

	// 回收空间
	void Push(void* obj) { // 头插法
		assert(obj);

		ObjNext(obj) = _freeList;
		_freeList = obj;

		++_size;
	}

	// 提供空间
	void* Pop() {
		assert(_freeList);

		void* obj = _freeList;
		_freeList = ObjNext(obj);

		--_size;

		return obj;
	}

	bool Empty() {
		return _freeList == nullptr;
	}

	size_t& MaxSize() {
		return _maxSize;
	}

	size_t Size() {
		return _size;
	}

private:
	void* _freeList = nullptr; // 自由链表，初始为空
	size_t _maxSize = 1;       // 当前自由链表申请未达到上限时，能够申请的最大块空间
	
	size_t _size = 0;          // 当前自由链表中有多少块空间
};


class SizeClass {
public:
	// 计算对齐字节数
	static size_t RoundUp(size_t size) // 计算对齐后的字节数，size为线程申请的空间大小
	{
		if (size <= 128)
		{ // [1,128] 8B
			return _RoundUp(size, 8);
		}
		else if (size <= 1024)
		{ // [128+1,1024] 16B
			return _RoundUp(size, 16);
		}
		else if (size <= 8 * 1024)
		{ // [1024+1,8*1024] 128B
			return _RoundUp(size, 128);
		}
		else if (size <= 64 * 1024)
		{ // [8*1024+1,64*1024] 1024B
			return _RoundUp(size, 1024);
		}
		else if (size <= 256 * 1024)
		{ // [64*1024+1,256*1024] 8 * 1024B
			return _RoundUp(size, 8 * 1024);
		}
		else
		{ // 单次申请空间大于256KB，直接按照页来对齐
			return _RoundUp(size, 1 << PAGE_SHIFT);
			// 这里直接给PAGE_SHIFT虽然说和前一个的8 * 1024KB一样，但
			// 是我们如果后续想要修改页大小的时候就直接修改PAGE_SHIFT
			// 这样就和前面的不一样了
		}
	}

	// 大佬写法，使用二进制计算每个分区对应的对齐后的字节数
	static size_t _RoundUp(size_t size, size_t alignNum) {
		return ((size + alignNum - 1) & ~(alignNum - 1));
	}

	// 求size对应在哈希桶中的下标
	static size_t _Index(size_t size, size_t align_shift) {
		return ((size + (1 << align_shift) - 1) >> align_shift) - 1;
	}

	// 计算映射的哪一个自由链表桶
	static size_t Index(size_t size) {
		assert(size <= MAX_BYTES);

		// 每个区间有多少个链
		static int group_array[4] = { 16, 56, 56, 56 };
		if (size <= 128)
		{ // [1,128] 8B -->8B就是2^3B，对应二进制位为3位
			return _Index(size, 3); // 3是指对齐数的二进制位位数，这里8B就是2^3B，所以就是3
		}
		else if (size <= 1024)
		{ // [128+1,1024] 16B -->4位
			return _Index(size - 128, 4) + group_array[0];
		}
		else if (size <= 8 * 1024)
		{ // [1024+1,8*1024] 128B -->7位
			return _Index(size - 1024, 7) + group_array[1] + group_array[0];
		}
		else if (size <= 64 * 1024)
		{ // [8*1024+1,64*1024] 1024B -->10位
			return _Index(size - 8 * 1024, 10) + group_array[2] + group_array[1]
				+ group_array[0];
		}
		else if (size <= 256 * 1024)
		{ // [64*1024+1,256*1024] 8 * 1024B  -->13位
			return _Index(size - 64 * 1024, 13) + group_array[3] +
				group_array[2] + group_array[1] + group_array[0];
		}
		else
		{
			assert(false);
		}
		return -1;
	}

	// tc申请块空间时的单次申请上限
	static size_t NumMoveSize(size_t size) {
		assert(size > 0);

		int num = MAX_BYTES / size;

		if (num > 512) {
			num = 512;
		}

		if (num < 2) {
			num = 2;
		}

		return num;
	}

	// 块页匹配算法
	static size_t NumMovePage(size_t size) {
		// NumMoveSize是算出tc向cc申请size大小的块时的单词最大申请块数
		size_t num = NumMoveSize(size);

		// num * size就是单次申请最大空间大小
		size_t npage = num * size;

		/* PAGE_SHIFT表示一页要占用多少位，比如说size为8KB就是13位，这里右移
		其实就是除以页大小，算出来就是单次申请最大空间有多少页*/
		npage >>= PAGE_SHIFT;

		if (npage == 0) {
			return 1;
		}
		return npage;
	}

};

// Single-level array
template <int BITS>
class TCMalloc_PageMap1 {
private:
	static const int LENGTH = 1 << BITS; // 数组要开的长度
	void** array_; // 底层存放指针的数组

public:
	typedef uintptr_t Number;

	explicit TCMalloc_PageMap1() {// 开空间
		size_t size = sizeof(void*) << BITS;
		size_t alignSize = SizeClass::_RoundUp(size, 1 << PAGE_SHIFT);
		array_ = (void**)SystemAlloc(alignSize >> PAGE_SHIFT);
		memset(array_, 0, sizeof(void*) << BITS);
	}

	// Return the current value for KEY.  Returns NULL if not yet set,
	// or if k is out of range.
	void* get(Number k) const { // 通过k来获取对应的指针
		if ((k >> BITS) > 0) {
			return NULL;
		}
		return array_[k];
	}

	// REQUIRES "k" is in range "[0,2^BITS-1]".
	// REQUIRES "k" has been ensured before.
	//
	// Sets the value 'v' for key 'k'.
	void set(Number k, void* v) { // 将v设置到k下标
		array_[k] = v;
	}
};


// Two-level radix tree
template <int BITS>
class TCMalloc_PageMap2 {
private:
	// Put 32 entries in the root and (2^BITS)/32 entries in each leaf.
	static const int ROOT_BITS = 5; // 32位下前5位搞一个第一层的数组
	static const int ROOT_LENGTH = 1 << ROOT_BITS;

	static const int LEAF_BITS = BITS - ROOT_BITS; // 32位下后14位搞成第二层的数组
	static const int LEAF_LENGTH = 1 << LEAF_BITS;

	// Leaf node
	struct Leaf { // 叶子就是后14位的数组
		void* values[LEAF_LENGTH];
	};

	Leaf* root_[ROOT_LENGTH];             // 根就是前5位的数组
public:
	typedef uintptr_t Number;

	//explicit TCMalloc_PageMap2(void* (*allocator)(size_t)) {
	explicit TCMalloc_PageMap2() { // 直接把所有的空间都开好
		memset(root_, 0, sizeof(root_));
		PreallocateMoreMemory(); // 直接开2M的span*全开出来
	}

	void* get(Number k) const {
		const Number i1 = k >> LEAF_BITS;
		const Number i2 = k & (LEAF_LENGTH - 1);
		if ((k >> BITS) > 0 || root_[i1] == NULL) {
			return NULL;
		}
		return root_[i1]->values[i2];
	}

	void set(Number k, void* v) {
		const Number i1 = k >> LEAF_BITS;
		const Number i2 = k & (LEAF_LENGTH - 1);
		ASSERT(i1 < ROOT_LENGTH);
		root_[i1]->values[i2] = v;
	}

	// 确保从start开始往后的n页空间开好了
	bool Ensure(Number start, size_t n) {
		for (Number key = start; key <= start + n - 1;) {
			const Number i1 = key >> LEAF_BITS;

			// Check for overflow
			if (i1 >= ROOT_LENGTH)
				return false;

			// 如果没开好就开空间
			if (root_[i1] == NULL) {
				static ObjectPool<Leaf>	leafPool;
				Leaf* leaf = (Leaf*)leafPool.New();

				memset(leaf, 0, sizeof(*leaf));
				root_[i1] = leaf;
			}

			// Advance key past whatever is covered by this leaf node
			key = ((key >> LEAF_BITS) + 1) << LEAF_BITS;
		}
		return true;
	}

	// 提前开好空间，这里就把2M的直接开好
	void PreallocateMoreMemory() {
		// Allocate enough to keep track of all possible pages
		Ensure(0, 1 << BITS);
	}
};
