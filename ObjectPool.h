#pragma once

/*�����ڴ��*/

#include"Common.h"

template<class T>
class ObjectPool
{
public:
	T* New() // ����һ��T���ʹ�С�Ŀռ�
	{
		T* obj = nullptr; // ���շ��صĿռ�

		if (_freelist)
		{ // _freelist��Ϊ�գ���ʾ�л��յ�T��С��С������ظ�����
			void* next = *(void**)_freelist;
			obj = (T*)_freelist;
			_freelist = next;
			// ͷɾ����
		}
		else
		{ // ����������û�п飬Ҳ��û�п����ظ����õĿռ�
			// _memory��ʣ��ռ�С��T�Ĵ�С��ʱ���ٿ��ռ�
			if (_remanentBytes < sizeof(T)) // ����Ҳ�����ʣ��ռ�Ϊ0�����
			{
				_remanentBytes = 128 * 1024; // ��128K�Ŀռ�
				//_memory = (char*)malloc(_remanentBytes);

				// ����13λ�����ǳ���8KB��Ҳ���ǵõ�����16������ͱ�ʾ����16ҳ
				_memory = (char*)SystemAlloc(_remanentBytes >> PAGE_SHIFT);

				if (_memory == nullptr) // ��ʧ�������쳣
				{
					throw std::bad_alloc();
				}
			}

			obj = (T*)_memory; // ����һ��T���͵Ĵ�С
			// �ж�һ��T�Ĵ�С��С��ָ��͸�һ��ָ���С������ָ��ͻ���T�Ĵ�С
			size_t objSize = sizeof(T) < sizeof(void*) ? sizeof(void*) : sizeof(T);
			_memory += objSize; // _memory����һ��T���͵Ĵ�С
			_remanentBytes -= objSize; // �ռ������_remanetBytes������T���͵Ĵ�С
		}

		new(obj)T; // ͨ����λnew���ù��캯�����г�ʼ��

		if (obj == nullptr)
		{
			int x = 0;
		}
		return obj;
	}

	void Delete(T* obj) // ���ջ�������С�ռ�
	{
		// ��ʾ������������������������
		obj->~T();

		// ͷ��
		*(void**)obj = _freelist; // �¿�ָ��ɿ�(���)
		_freelist = obj; // ͷָ��ָ���¿�
	}

private:
	char* _memory = nullptr; // ָ���ڴ���ָ��
	size_t _remanentBytes = 0; // ����ڴ����зֹ����е�ʣ���ֽ���
	void* _freelist = nullptr; // �����������������ӹ黹�Ŀ��пռ�
public:
	std::mutex _poolMtx; // ��ֹThreadCache����ʱ���뵽��ָ��
};


struct TreeNode // һ�����ṹ�Ľڵ㣬�Ȼ�����ռ��ʱ�����������ڵ�������
{
	int _val;
	TreeNode* _left;
	TreeNode* _right;

	TreeNode()
		:_val(0)
		, _left(nullptr)
		, _right(nullptr)
	{}
};

//void TestObjectPool() // malloc�͵�ǰ�����ڴ�����ܶԱ�
//{
//	// �����ͷŵ��ִ�
//	const size_t Rounds = 10;
//
//	// ÿ�������ͷŶ��ٴ�
//	const size_t N = 100000;
//
//	// �����ܹ�������ͷŵĴ�������Rounds * N�Σ�������ôЩ��˭����
//
//	std::vector<TreeNode*> v1;
//	v1.reserve(N);
//
//	// ����malloc������
//	size_t begin1 = clock();
//	for (size_t j = 0; j < Rounds; ++j)
//	{
//		for (int i = 0; i < N; ++i)
//		{
//			v1.push_back(new TreeNode); // ������Ȼ�õ���new������new�ײ��õ�Ҳ��malloc
//		}
//		for (int i = 0; i < N; ++i)
//		{
//			delete v1[i]; // ͬ���ģ�delete�ײ�Ҳ��free
//		}
//		v1.clear(); // ����clear���þ��ǽ�vector�е�������գ�size���㣬
//		// ��capacity���ֲ��䣬��������ѭ����ȥ����push_back
//	}
//	size_t end1 = clock();
//
//
//	std::vector<TreeNode*> v2;
//	v2.reserve(N);
//
//	// �����ڴ�أ�����������ͷŵ�T���;������ڵ�
//	ObjectPool<TreeNode> TNPool;
//	size_t begin2 = clock();
//	for (size_t j = 0; j < Rounds; ++j)
//	{
//		for (int i = 0; i < N; ++i)
//		{
//			v2.push_back(TNPool.New()); // �����ڴ���е�����ռ�
//		}
//		for (int i = 0; i < N; ++i)
//		{
//			TNPool.Delete(v2[i]); // �����ڴ���еĻ��տռ�
//		}
//		v2.clear();// ����clear���þ��ǽ�vector�е�������գ�size���㣬
//		// ��capacity���ֲ��䣬��������ѭ����ȥ����push_back
//	}
//	size_t end2 = clock();
//
//
//	cout << "new cost time:" << end1 - begin1 << endl; // ���������Ϊʱ�䵥λ����ms
//	cout << "object pool cost time:" << end2 - begin2 << endl;
//}