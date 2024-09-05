#include "PageCache.h"

PageCache PageCache::_sInst;   // ��������

Span* PageCache::NewSpan(size_t k) {
	assert(k > 0);

	if (k > PAGE_NUM - 1) {
		void* ptr = SystemAlloc(k);
		Span* span = _spanPool.New();

		span->_pageID = ((PageID)ptr) >> PAGE_SHIFT;
		span->_n = k;

		_idSpanMap.set(span->_pageID, span);

		return span;
	}

	// 1��k��Ͱ����span
	if (!_spanLists[k].Empty()) {
		Span* span = _spanLists[k].PopFront();

		// ��¼�����ȥ��span�����ҳ�ź����ַ��ӳ���ϵ
		for (PageID i = 0; i < span->_n; i++) {
			_idSpanMap.set(span->_pageID + i, span);
		}
		
		return span;
	}

	// 2��k��Ͱ��û�У����Ǻ����Ͱ����
	for (int i = k + 1; i < PAGE_NUM; ++i) {
		if (!_spanLists[i].Empty()) {
			// ��ȡ����Ͱ�е�span
			Span* nSpan = _spanLists[i].PopFront();

			// �зָ�spanΪ k �� n - k

			// Span�Ŀռ�����Ҫ�½��ģ��������õ�ǰ�ڴ�صĿռ�
			Span* kSpan = _spanPool.New();

			// ��һ��kҳ��span
			kSpan->_pageID = nSpan->_pageID;
			kSpan->_n = k;

			// ��n-kҳ��span
			nSpan->_pageID += k;
			nSpan->_n -= k;

			// n-kҳ�ķŻض�Ӧ�Ĺ�ϣͰ��
			_spanLists[nSpan->_n].PushFront(nSpan);

			// �ٰ�n-kҳ��span��Եҳӳ��һ�£���������ϲ�
			_idSpanMap.set(nSpan->_pageID, nSpan);
			_idSpanMap.set(nSpan->_pageID + nSpan->_n - 1, nSpan);

			// ��¼kSpan��ҳ����span��ַ��ӳ��
			for (PageID i = 0; i < kSpan->_n; ++i) {
				_idSpanMap.set(kSpan->_pageID + i, kSpan);
			}

			return kSpan;
		}
	}

	// 3������Ͱ��û�У���ϵͳ����

	// ֱ����ϵͳ����128ҳ��span
	void* ptr = SystemAlloc(PAGE_NUM - 1);

	// ��һ���µ�span��ά�����ռ�
	Span* bigSpan = _spanPool.New();

	// ֻ��Ҫ�޸�_pageID��_n���ɣ�ϵͳ���ýӿ�����Ŀռ�һ���ܱ�֤����Ŀռ��Ƕ����
	bigSpan->_pageID = ((PageID)ptr) >> PAGE_SHIFT;
	bigSpan->_n = PAGE_NUM - 1;

	// �����span�ŵ���Ӧ�Ĺ�ϣͰ��
	_spanLists[PAGE_NUM - 1].PushFront(bigSpan);

	return NewSpan(k);
}

Span* PageCache::MapObjectToSpan(void* obj) {
	// ͨ�����ַ�ҵ�ҳ��
	PageID id = (((PageID)obj) >> PAGE_SHIFT);

	// ������
	//std::unique_lock<std::mutex> lc(_pageMtx);

	// ͨ����ϣ�ҵ�ҳ�Ŷ�Ӧ��span
	auto ret = _idSpanMap.get(id);

	// һ���ܱ�֤ͨ�����ַ�ҵ�һ��span�����û�ҵ����ǳ�����
	if (ret != nullptr) {
		return (Span*)ret;
	}
	else {
		assert(false);
		return nullptr;
	}
}

void PageCache::ReleaseSpanToPageCache(Span* span) {
	if (span->_n > PAGE_NUM - 1) {
		void* ptr = (void*)(span->_pageID << PAGE_SHIFT);
		SystemFree(ptr);
		//delete span;
		_spanPool.Delete(span);

		return;
	}


	// ���󲻶Ϻϲ�
	while (1) {
		PageID leftID = span->_pageID - 1;
		auto ret = _idSpanMap.get(leftID);

		// û������span��ֹͣ�ϲ�
		if (ret == nullptr) {
			break;
		}

		Span* leftSpan = (Span*)ret;
		// ����span��cc�У�ֹͣ�ϲ�
		if (leftSpan->_isUse == true) {
			break;
		}

		// ����span�뵱ǰspan�ϲ��󳬳�128ҳ��ֹͣ�ϲ�
		if (leftSpan->_n + span->_n > PAGE_NUM - 1) {
			break;
		}

		// ��ǰspan������span���кϲ�
		span->_pageID = leftSpan->_pageID;
		span->_n += leftSpan->_n;

		_spanLists[leftSpan->_n].Erase(leftSpan);  // ������span�����Ͱ��ɾ��
		//delete leftSpan;                           // ɾ��������span����
		_spanPool.Delete(leftSpan);
	}

	// ���Ҳ��Ϻϲ�
	while (1) {
		PageID rightID = span->_pageID + span->_n;
		auto ret = _idSpanMap.get(rightID);

		// û������span��ֹͣ�ϲ�
		if (ret == nullptr) {
			break;
		}

		Span* rightSpan = (Span*)ret;
		// ����span��cc�У�ֹͣ�ϲ�
		if (rightSpan->_isUse == true) {
			break;
		}

		// ����span�뵱ǰspan�ϲ��󳬳�128ҳ��ֹͣ�ϲ�
		if (rightSpan->_n + span->_n > PAGE_NUM - 1) {
			break;
		}

		// ��ǰspan������span���кϲ�
		span->_n += rightSpan->_n;

		_spanLists[rightSpan->_n].Erase(rightSpan);  // ������span�����Ͱ��ɾ��
		/*delete rightSpan;*/                           // ɾ��������span����
		_spanPool.Delete(rightSpan);
	}

	// �ϲ���ϣ�����ǰspan�ҵ���ӦͰ��
	_spanLists[span->_n].PushFront(span);
	span->_isUse = false;   

	// ӳ�䵱ǰspan�ı�Եҳ�����������Զ����span���кϲ�
	/*_idSpanMap[span->_pageID] = span;
	_idSpanMap[span->_pageID + span->_n - 1] = span;*/
	_idSpanMap.set(span->_pageID, span);
	_idSpanMap.set(span->_pageID + span->_n - 1, span);
}
