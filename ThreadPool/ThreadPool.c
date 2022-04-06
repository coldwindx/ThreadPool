#include "threadpool.h"

#include <pthread.h>
#include <string.h>
#include <unistd.h> 
#include <stdlib.h>

// 任务结构体
typedef struct Task {

	void(*func)(void * arg);		// 任务处理函数
	void * arg;						// 任务处理函数的参数列表
}Task;

// 线程池定义类
struct ThreadPool
{
	int taskCapacity;
	int taskSize;
	int front;
	int rear;
	Task * tasks;				// 任务队列

	pthread_t manager;			// 管理者线程
	pthread_t * threads;		// 线程队列

	int minNum;					// 最小线程数
	int maxNum;
	int busyNum;				// 忙碌线程数
	int liveNum;				// 存活线程数
	int exitNum;				// 需销毁线程数

	pthread_mutex_t mutexPool;	// 线程池互斥锁
	pthread_mutex_t mutexBusy;	// busyNum互斥锁

	int shutdown;				// 线程池销毁标志位

	pthread_cond_t notFull;
	pthread_cond_t notEmpty;
};


ThreadPool* createThreadPool(int min, int max, int size)
{
	ThreadPool * pool = (ThreadPool*)malloc(sizeof(ThreadPool));
	do {
		if (pool == NULL)
			break;

		pool->threads = (pthread_t*)malloc(sizeof(pthread_t) * max);
		if (pool->threads == NULL)
			break;
		memset(pool->threads, 0, sizeof(pthread_t) * max);

		pool->minNum = min;
		pool->maxNum = max;
		pool->busyNum = 0;
		pool->liveNum = min;
		pool->exitNum = 0;

		if (pthread_mutex_init(&pool->mutexPool, NULL) != 0)
			break;
		if (pthread_mutex_init(&pool->mutexBusy, NULL) != 0)
			break;
		if (pthread_cond_init(&pool->notFull, NULL) != 0)
			break;
		if (pthread_cond_init(&pool->notEmpty, NULL) != 0)
			break;

		pool->tasks = (Task*)malloc(sizeof(Task) * size);
		pool->taskCapacity = size;
		pool->taskSize = 0;
		pool->front = 0;
		pool->rear = 0;

		pool->shutdown = 0;
		// pthread_create(线程ID，线程属性，线程任务函数，任务函数参数);
		pthread_create(&pool->manager, NULL, manager, pool);
		for (int i = 0; i < min; ++i) {
			pthread_create(&pool->threads[i], NULL, worker, pool);
		}

		return pool;
	} while (0);

	if (pool && pool->threads) free(pool->threads);
	if (pool && pool->tasks) free(pool->tasks);
	if (pool) free(pool);
	return NULL;
}

void * worker(void * arg)
{
	ThreadPool * pool = (ThreadPool*)arg;
	while (1) {
		pthread_mutex_lock(&pool->mutexPool);
		// 没有任务时，阻塞工作线程
		while (0 == pool->taskSize && !pool->shutdown) {
			pthread_cond_wait(&pool->notEmpty, &pool->mutexPool);
			// 判断是否销毁线程
			if (0 < pool->exitNum) {
				pool->exitNum--;
				pool->liveNum--;
				pthread_mutex_unlock(&pool->mutexPool);
				threadExit(pool);
			}
		}
		// 线程池关闭时
		if (pool->shutdown) {
			pthread_mutex_unlock(&pool->mutexPool);	// 防死锁
			threadExit(pool);
		}
		// 消费任务
		Task task;
		task.func = pool->tasks[pool->front].func;
		task.arg = pool->tasks[pool->front].arg;
		pool->front = (pool->front + 1) % pool->taskCapacity;
		pool->taskSize--;

		// 唤醒生产者添加任务
		pthread_cond_signal(&pool->notFull);
		pthread_mutex_unlock(&pool->mutexPool);
		// 线程工作
		pthread_mutex_lock(&pool->mutexBusy);
		pool->busyNum++;
		pthread_mutex_unlock(&pool->mutexBusy);

		task.func(task.arg);
		free(task.arg);
		task.arg = NULL;

		pthread_mutex_lock(&pool->mutexBusy);
		pool->busyNum--;
		pthread_mutex_unlock(&pool->mutexBusy);
	}
	return NULL;
}

void * manager(void * arg)
{
	ThreadPool * pool = (ThreadPool*)arg;
	while (!pool->shutdown) {
		// 每隔3s检测一次
		sleep(3);
		// 取任务数量和线程数量
		pthread_mutex_lock(&pool->mutexPool);
		int taskSize = pool->taskSize;
		int liveNum = pool->liveNum;
		int busyNum = pool->busyNum;
		pthread_mutex_unlock(&pool->mutexPool);

		// 添加线程
		if (liveNum < taskSize && liveNum < pool->maxNum) {
			pthread_mutex_lock(&pool->mutexPool);

			int counter = 0;
			for (int i = 0; i < pool->maxNum && counter < NUMBER && pool->liveNum < pool->maxNum; ++i) {
				if (0 == pool->threads[i]) {
					pthread_create(&pool->threads[i], NULL, worker, pool);
					++counter;
					pool->liveNum++;
				}
			}

			pthread_mutex_unlock(&pool->mutexPool);
		}
		// 销毁线程
		if (busyNum * 2 < liveNum && pool->minNum < liveNum) {
			pthread_mutex_lock(&pool->mutexPool);

			pool->exitNum = NUMBER;
			// 让工作线程“自杀”
			for (int i = 0; i < NUMBER; ++i) {
				// 唤醒阻塞在条件变量上的无任务线程（Line：60）
				pthread_cond_signal(&pool->notEmpty);
			}
			pthread_mutex_unlock(&pool->mutexPool);
		}
	}
	return NULL;
}

void threadExit(ThreadPool * pool)
{
	pthread_t tid = pthread_self();
	for (int i = 0; i < pool->maxNum; ++i)
		if (tid == pool->threads[i]) {
			pool->threads[i] = 0;
			printf("ThreadExit() called, %ld exiting ...\n", tid);
			break;
		}
	pthread_exit(NULL);
}

void addTask(ThreadPool * pool, void(*func)(void *), void * arg)
{
	pthread_mutex_lock(&pool->mutexPool);

	while (pool->taskSize == pool->taskCapacity && !pool->shutdown) {
		pthread_cond_wait(&pool->notFull, &pool->mutexPool);
	}
	if (pool->shutdown) {
		pthread_mutex_unlock(&pool->mutexPool);
		return;
	}

	// 添加任务
	pool->tasks[pool->rear].func = func;
	pool->tasks[pool->rear].arg = arg;
	pool->rear = (pool->rear + 1) % pool->taskCapacity;
	pool->taskSize++;

	// 唤醒消费者消费任务
	pthread_cond_signal(&pool->notEmpty);

	pthread_mutex_unlock(&pool->mutexPool);
}

int destroy(ThreadPool * pool)
{
	if (pool == NULL) return 0;
	// 关闭线程池
	pool->shutdown = 1;
	// 回收管理者线程
	pthread_join(pool->manager, NULL);
	// 唤醒消费者线程
	for (int i = 0; i < pool->liveNum; ++i) {
		pthread_cond_signal(&pool->notEmpty);
	}
	// 释放内存空间
	if (pool->tasks) free(pool->tasks);
	if (pool->threads) free(pool->threads);

	// 释放锁
	pthread_mutex_destroy(&pool->mutexPool);
	pthread_mutex_destroy(&pool->mutexBusy);
	pthread_cond_destroy(&pool->notEmpty);
	pthread_cond_destroy(&pool->notFull);

	free(pool);
	pool = NULL;
	return 1;
}

int threadPoolBusyNum(ThreadPool * pool)
{
	pthread_mutex_lock(&pool->mutexBusy);
	int busyNum = pool->busyNum;
	pthread_mutex_unlock(&pool->mutexBusy);
	return busyNum;
}

int threadPoolAliveNum(ThreadPool * pool)
{
	pthread_mutex_lock(&pool->mutexPool);
	int liveNum = pool->liveNum;
	pthread_mutex_unlock(&pool->mutexPool);
	return liveNum;
}
