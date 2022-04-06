#pragma once

#include "TaskQueue.h"
#include "TaskQueue.cpp"

// 线程池定义类
template<typename T>
class ThreadPool
{
	pthread_mutex_t mutex;		// 线程池互斥锁
	bool shutdown;				// 线程池销毁标志位

	TaskQueue<T> * taskQueue;		// 任务队列
	pthread_cond_t notEmpty;

	pthread_t managerID;		// 管理者线程
	pthread_t * threadIDs;		// 线程队列

	int minNum;					// 最小线程数
	int maxNum;
	int busyNum;				// 忙碌线程数
	int liveNum;				// 存活线程数
	int exitNum;				// 需销毁线程数

	static const int NUMBER = 2;
public:
	ThreadPool(int min, int max);
	~ThreadPool();

	void addTask(Task<T> task);
	int getBusyNum();
	int getAliveNum();

private:
	static void* worker(void * arg);		/* 工作者线程任务 */
	static void* manager(void * arg);		/* 管理者线程任务 */
	void exit();					/* 更安全的线程退出函数 */
};

