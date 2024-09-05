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

// ֱ��ȥ�����ͷſռ�
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
	PageID _pageID = 0;      // ҳ��
	size_t _n = 0;           // ��ǰspan�����ҳ������
	size_t _objSize = 0;
	                
	void* _freeList = nullptr;   // ÿ��span����ҵ�С��ռ��ͷ���
	size_t use_count = 0;    // ��ǰspan�����ȥ�˶��ٸ���ռ�

	Span* _prev = nullptr;   // ǰһ���ڵ�
	Span* _next = nullptr;   // ��һ���ڵ�

	bool _isUse = false;
};

class SpanList
{
public:
	// ɾ������һ��span
	Span* PopFront()
	{
		// �Ȼ�ȡ��_head����ĵ�һ��span
		Span* front = _head->_next;
		// ɾ�������span��ֱ�Ӹ���Erase
		Erase(front);

		// ����ԭ���ĵ�һ��span
		return front;
	}

	// �п�
	bool Empty()
	{ // ��ͷ˫��ѭ���յ�ʱ��_headָ���Լ�
		return _head == _head->_next;
	}

	// ͷ��
	void PushFront(Span* span)
	{
		Insert(Begin(), span);
	}

	// ͷ���
	Span* Begin()
	{
		return _head->_next;
	}

	// β���
	Span* End()
	{
		return _head;
	}

	void Erase(Span* pos)
	{
		assert(pos); // pos��Ϊ��
		assert(pos != _head); // pos�������ڱ�λ

		Span* prev = pos->_prev;
		Span* next = pos->_next;

		prev->_next = next;
		next->_prev = prev;

		/*pos�ڵ㲻��Ҫ����deleteɾ������Ϊ
		pos�ڵ��Span��Ҫ���գ�������ֱ��ɾ��*/
		// ��������߼�
	}

	void Insert(Span* pos, Span* ptr)
	{ // ��posǰ�����ptr
		assert(pos); // pos��Ϊ��
		assert(ptr); // ptr��Ϊ��

		// ��������߼�
		Span* prev = pos->_prev;

		prev->_next = ptr;
		ptr->_prev = prev;

		ptr->_next = pos;
		pos->_prev = ptr;
	}

	SpanList()
	{ // ���캯���и��ڱ�λͷ���
		_head = new Span;

		// ��Ϊ��˫��ѭ���ģ����Զ�ָ��_head
		_head->_next = _head;
		_head->_prev = _head;
	}

private:
	Span* _head; // �ڱ�λͷ���
public:
	std::mutex _mtx; // ÿ��CentralCache�еĹ�ϣͰ��Ҫ��һ��Ͱ��
};


class FreeList {
public:
	// ɾ����Ͱ��n���飨ͷɾ��������ɾ���Ŀռ���Ϊ����Ͳ�������
	void PopRange(void*& start, void*& end, size_t n) {
		//ɾ���������ܳ���size��
		assert(n <= _size);

		start = end = _freeList;

		for (size_t i = 0; i < n - 1; i++) {
			end = ObjNext(end);
		}

		_freeList = ObjNext(end);
		ObjNext(end) = nullptr;
		_size -= n;
	}

	// ������������ͷ�壬�Ҳ�����ռ�
	void PushRange(void* start, void* end, size_t size) {
		ObjNext(end) = _freeList;
		_freeList = start;

		_size += size;
	}

	// ���տռ�
	void Push(void* obj) { // ͷ�巨
		assert(obj);

		ObjNext(obj) = _freeList;
		_freeList = obj;

		++_size;
	}

	// �ṩ�ռ�
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
	void* _freeList = nullptr; // ����������ʼΪ��
	size_t _maxSize = 1;       // ��ǰ������������δ�ﵽ����ʱ���ܹ����������ռ�
	
	size_t _size = 0;          // ��ǰ�����������ж��ٿ�ռ�
};


class SizeClass {
public:
	// ��������ֽ���
	static size_t RoundUp(size_t size) // ����������ֽ�����sizeΪ�߳�����Ŀռ��С
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
		{ // ��������ռ����256KB��ֱ�Ӱ���ҳ������
			return _RoundUp(size, 1 << PAGE_SHIFT);
			// ����ֱ�Ӹ�PAGE_SHIFT��Ȼ˵��ǰһ����8 * 1024KBһ������
			// ���������������Ҫ�޸�ҳ��С��ʱ���ֱ���޸�PAGE_SHIFT
			// �����ͺ�ǰ��Ĳ�һ����
		}
	}

	// ����д����ʹ�ö����Ƽ���ÿ��������Ӧ�Ķ������ֽ���
	static size_t _RoundUp(size_t size, size_t alignNum) {
		return ((size + alignNum - 1) & ~(alignNum - 1));
	}

	// ��size��Ӧ�ڹ�ϣͰ�е��±�
	static size_t _Index(size_t size, size_t align_shift) {
		return ((size + (1 << align_shift) - 1) >> align_shift) - 1;
	}

	// ����ӳ�����һ����������Ͱ
	static size_t Index(size_t size) {
		assert(size <= MAX_BYTES);

		// ÿ�������ж��ٸ���
		static int group_array[4] = { 16, 56, 56, 56 };
		if (size <= 128)
		{ // [1,128] 8B -->8B����2^3B����Ӧ������λΪ3λ
			return _Index(size, 3); // 3��ָ�������Ķ�����λλ��������8B����2^3B�����Ծ���3
		}
		else if (size <= 1024)
		{ // [128+1,1024] 16B -->4λ
			return _Index(size - 128, 4) + group_array[0];
		}
		else if (size <= 8 * 1024)
		{ // [1024+1,8*1024] 128B -->7λ
			return _Index(size - 1024, 7) + group_array[1] + group_array[0];
		}
		else if (size <= 64 * 1024)
		{ // [8*1024+1,64*1024] 1024B -->10λ
			return _Index(size - 8 * 1024, 10) + group_array[2] + group_array[1]
				+ group_array[0];
		}
		else if (size <= 256 * 1024)
		{ // [64*1024+1,256*1024] 8 * 1024B  -->13λ
			return _Index(size - 64 * 1024, 13) + group_array[3] +
				group_array[2] + group_array[1] + group_array[0];
		}
		else
		{
			assert(false);
		}
		return -1;
	}

	// tc�����ռ�ʱ�ĵ�����������
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

	// ��ҳƥ���㷨
	static size_t NumMovePage(size_t size) {
		// NumMoveSize�����tc��cc����size��С�Ŀ�ʱ�ĵ�������������
		size_t num = NumMoveSize(size);

		// num * size���ǵ����������ռ��С
		size_t npage = num * size;

		/* PAGE_SHIFT��ʾһҳҪռ�ö���λ������˵sizeΪ8KB����13λ����������
		��ʵ���ǳ���ҳ��С����������ǵ����������ռ��ж���ҳ*/
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
	static const int LENGTH = 1 << BITS; // ����Ҫ���ĳ���
	void** array_; // �ײ���ָ�������

public:
	typedef uintptr_t Number;

	explicit TCMalloc_PageMap1() {// ���ռ�
		size_t size = sizeof(void*) << BITS;
		size_t alignSize = SizeClass::_RoundUp(size, 1 << PAGE_SHIFT);
		array_ = (void**)SystemAlloc(alignSize >> PAGE_SHIFT);
		memset(array_, 0, sizeof(void*) << BITS);
	}

	// Return the current value for KEY.  Returns NULL if not yet set,
	// or if k is out of range.
	void* get(Number k) const { // ͨ��k����ȡ��Ӧ��ָ��
		if ((k >> BITS) > 0) {
			return NULL;
		}
		return array_[k];
	}

	// REQUIRES "k" is in range "[0,2^BITS-1]".
	// REQUIRES "k" has been ensured before.
	//
	// Sets the value 'v' for key 'k'.
	void set(Number k, void* v) { // ��v���õ�k�±�
		array_[k] = v;
	}
};


// Two-level radix tree
template <int BITS>
class TCMalloc_PageMap2 {
private:
	// Put 32 entries in the root and (2^BITS)/32 entries in each leaf.
	static const int ROOT_BITS = 5; // 32λ��ǰ5λ��һ����һ�������
	static const int ROOT_LENGTH = 1 << ROOT_BITS;

	static const int LEAF_BITS = BITS - ROOT_BITS; // 32λ�º�14λ��ɵڶ��������
	static const int LEAF_LENGTH = 1 << LEAF_BITS;

	// Leaf node
	struct Leaf { // Ҷ�Ӿ��Ǻ�14λ������
		void* values[LEAF_LENGTH];
	};

	Leaf* root_[ROOT_LENGTH];             // ������ǰ5λ������
public:
	typedef uintptr_t Number;

	//explicit TCMalloc_PageMap2(void* (*allocator)(size_t)) {
	explicit TCMalloc_PageMap2() { // ֱ�Ӱ����еĿռ䶼����
		memset(root_, 0, sizeof(root_));
		PreallocateMoreMemory(); // ֱ�ӿ�2M��span*ȫ������
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

	// ȷ����start��ʼ�����nҳ�ռ俪����
	bool Ensure(Number start, size_t n) {
		for (Number key = start; key <= start + n - 1;) {
			const Number i1 = key >> LEAF_BITS;

			// Check for overflow
			if (i1 >= ROOT_LENGTH)
				return false;

			// ���û���þͿ��ռ�
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

	// ��ǰ���ÿռ䣬����Ͱ�2M��ֱ�ӿ���
	void PreallocateMoreMemory() {
		// Allocate enough to keep track of all possible pages
		Ensure(0, 1 << BITS);
	}
};
