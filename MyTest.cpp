// MyTest.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

//#include <iostream>
//#include <vector>
//#include <mutex>
//#include <condition_variable>
//#include <thread>
//#include <queue>
//#include <functional>
//#include "ConcurrentAlloc.h"

//using namespace std;

//void Alloc1() {
//    for (int i = 0; i < 5; i++) {
//        ConcurrentAlloc(6);
//    }
//}
//
//void Alloc2() {
//    for (int i = 0; i < 5; i++) {
//        ConcurrentAlloc(7);
//    }
//}
//
//void AllocTest() {
//    std::thread t1(Alloc1);
//    std::thread t2(Alloc2);
//
//    t1.join();
//    t2.join();
//}
//
//void ConcurrentAllocTest1() {
//    void* ptr1 = ConcurrentAlloc(5);
//    void* ptr2 = ConcurrentAlloc(8);
//    void* ptr3 = ConcurrentAlloc(4);
//    void* ptr4 = ConcurrentAlloc(6);
//    void* ptr5 = ConcurrentAlloc(3);
//
//    cout << ptr1 << endl;
//    cout << ptr2 << endl;
//    cout << ptr3 << endl;
//    cout << ptr4 << endl;
//    cout << ptr5 << endl;
//}
//
//void ConcurrentAllocTest2() {
//
//    for (int i = 0; i < 1024; i++) {
//        void* ptr1 = ConcurrentAlloc(5);
//        cout << ptr1 << endl;
//    }
//
//    void* ptr = ConcurrentAlloc(3);
//    cout << "--------" << ptr << endl;
//}
//
//void testConcurrentFree1() {
//    void* ptr1 = ConcurrentAlloc(5);
//    void* ptr2 = ConcurrentAlloc(8);
//    void* ptr3 = ConcurrentAlloc(4);
//    void* ptr4 = ConcurrentAlloc(6);
//    void* ptr5 = ConcurrentAlloc(3);
//    void* ptr6 = ConcurrentAlloc(3);
//    void* ptr7 = ConcurrentAlloc(3);
//
//    cout << ptr1 << endl;
//    cout << ptr2 << endl;
//    cout << ptr3 << endl;
//    cout << ptr4 << endl;
//    cout << ptr5 << endl;
//    cout << ptr6 << endl;
//    cout << ptr7 << endl;
//
//    ConcurrentFree(ptr1);
//    ConcurrentFree(ptr2);
//    ConcurrentFree(ptr3);
//    ConcurrentFree(ptr4);
//    ConcurrentFree(ptr5);
//    ConcurrentFree(ptr6);
//    ConcurrentFree(ptr7);
//}
//
//void BigAlloc() {
//    void* p1 = ConcurrentAlloc(257 * 1024);
//    ConcurrentFree(p1);
//
//    void* p2 = ConcurrentAlloc(129 * 8 * 1024);
//    ConcurrentFree(p2);
//}

//int main()
//{
//    testConcurrentFree1();
//    //BigAlloc();
//
//    system("pause");
//    
//    return 0;
//}

