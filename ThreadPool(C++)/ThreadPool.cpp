#include "ThreadPool.h"
#include <iostream>
#include <string.h>
#include <string>
#include <unistd.h>

using namespace std;

template<typename T>
ThreadPool<T>::ThreadPool(int min, int max)
{
	do {
		taskQueue = new TaskQueue<T>();
		if (taskQueue == nullptr) {
			cout << "malloc taskQueue fail ..." << endl;
			break;
		}

		this->threadIDs = new pthread_t[max];
		if (this->threadIDs == nullptr) {
			cout << "malloc threadIDs fail ..." << endl;
			break;
		}
		memset(this->threadIDs, 0, sizeof(pthread_t) * max);

		this->minNum = min;
		this->maxNum = max;
		this->busyNum = 0;
		this->liveNum = min;
		this->exitNum = 0;

		if (pthread_mutex_init(&this->mutex, NULL) != 0) {
			cout << "mutex init fail ..." << endl;
			break;
		}
		if (pthread_cond_init(&this->notEmpty, NULL) != 0) {
			cout << "condition init fail ..." << endl;
			break;
		}

		this->shutdown = false;
		// pthread_create(线程ID，线程属性，线程任务函数，任务函数参数);
		pthread_create(&managerID, NULL, manager, this);
		for (int i = 0; i < min; ++i) {
			pthread_create(&threadIDs[i], NULL, worker, this);
		}
		return ;
	} while (0);

	if (threadIDs) delete[] threadIDs;
	if (taskQueue) delete taskQueue;
}

template<typename T>
ThreadPool<T>::~ThreadPool()
{
	cout << "thread pool is closing..." << endl;
	// 关闭线程池
	this->shutdown = true;
	// 回收管理者线程
	pthread_join(managerID, NULL);
	// 唤醒消费者线程
	pthread_cond_broadcast(&notEmpty);
	// 释放内存空间
	if (this->taskQueue) delete taskQueue;
	if (this->threadIDs) delete[] threadIDs;

	// 释放锁
	pthread_mutex_destroy(&mutex);
	pthread_cond_destroy(&notEmpty);
}

template<typename T>
void ThreadPool<T>::addTask(Task<T> task)
{
	if (this->shutdown) return;
	// 添加任务
	this->taskQueue->push(task);
	pthread_cond_signal(&notEmpty);
}

template<typename T>
int ThreadPool<T>::getBusyNum()
{
	pthread_mutex_lock(&mutex);
	int busyNum = this->busyNum;
	pthread_mutex_unlock(&mutex);
	return busyNum;
}

template<typename T>
int ThreadPool<T>::getAliveNum()
{
	pthread_mutex_lock(&mutex);
	int liveNum = this->liveNum;
	pthread_mutex_unlock(&mutex);
	return liveNum;
}

template<typename T>
void * ThreadPool<T>::worker(void * arg)
{
	ThreadPool<T> * pool = (ThreadPool<T>*)arg;
	while (true) {
		pthread_mutex_lock(&pool->mutex);

		// 没有任务时，阻塞工作线程
		while (!pool->shutdown && 0 == pool->taskQueue->size()) {
			pthread_cond_wait(&pool->notEmpty, &pool->mutex);
			// 判断是否销毁线程
			if (0 < pool->exitNum) {
				pool->exitNum--;
				if (pool->minNum < pool->liveNum) {
					pool->liveNum--;
					pthread_mutex_unlock(&pool->mutex);
					pool->exit();
				}
			}
		}
		// 线程池关闭时
		if (pool->shutdown) {
			pthread_mutex_unlock(&pool->mutex);	// 防死锁
			pool->exit();
		}
		// 消费任务
		Task<T> task = pool->taskQueue->pop();
		pool->busyNum++;

		pthread_mutex_unlock(&pool->mutex);
		
		// 执行任务
		cout << "thread " << pthread_self() << " start working ..." << endl;
		task.function(task.arg);
		delete[] task.arg;
		task.arg = nullptr;
		cout << "thread " << pthread_self() << " end working ..." << endl;

		pthread_mutex_lock(&pool->mutex);
		pool->busyNum--;
		pthread_mutex_unlock(&pool->mutex);
	}
	return NULL;
}

template<typename T>
void * ThreadPool<T>::manager(void * arg)
{
	ThreadPool<T> * pool = (ThreadPool<T>*)arg;
	while (!pool->shutdown) {
		// 每隔3s检测一次
		sleep(3);
		// 取任务数量和线程数量
		pthread_mutex_lock(&pool->mutex);
		int taskSize = pool->taskQueue->size();
		int liveNum = pool->liveNum;
		int busyNum = pool->busyNum;
		pthread_mutex_unlock(&pool->mutex);

		// 添加线程
		//cout << "taskSize = " << taskSize << ", liveNum = " << liveNum << ", maxNum = " << pool->maxNum << endl;
		if (liveNum < taskSize && liveNum < pool->maxNum) {
			pthread_mutex_lock(&pool->mutex);

			int counter = 0;
			for (int i = 0; i < pool->maxNum && counter < NUMBER && pool->liveNum < pool->maxNum; ++i) {
				if (0 == pool->threadIDs[i]) {
					pthread_create(&pool->threadIDs[i], NULL, worker, pool);
					++counter;
					pool->liveNum++;
				}
			}

			pthread_mutex_unlock(&pool->mutex);
		}
		// 销毁线程
		if (busyNum * 2 < liveNum && pool->minNum < liveNum) {
			pthread_mutex_lock(&pool->mutex);

			pool->exitNum = NUMBER;
			// 让工作线程“自杀”
			for (int i = 0; i < NUMBER; ++i) {
				// 唤醒阻塞在条件变量上的无任务线程
				pthread_cond_signal(&pool->notEmpty);
			}
			pthread_mutex_unlock(&pool->mutex);
		}
	}
	return NULL;
}

template<typename T>
void ThreadPool<T>::exit()
{
	pthread_t tid = pthread_self();
	for (int i = 0; i < this->maxNum; ++i)
		if (tid == this->threadIDs[i]) {
			this->threadIDs[i] = 0;
			cout << "ThreadExit() called, " << tid << " exiting ..." << endl;
			break;
		}
	pthread_exit(NULL);
}
