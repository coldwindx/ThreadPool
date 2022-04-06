#include "threadpool.h"

#include <pthread.h>
#include <string.h>
#include <unistd.h> 
#include <stdlib.h>

// ����ṹ��
typedef struct Task {

	void(*func)(void * arg);		// ��������
	void * arg;						// ���������Ĳ����б�
}Task;

// �̳߳ض�����
struct ThreadPool
{
	int taskCapacity;
	int taskSize;
	int front;
	int rear;
	Task * tasks;				// �������

	pthread_t manager;			// �������߳�
	pthread_t * threads;		// �̶߳���

	int minNum;					// ��С�߳���
	int maxNum;
	int busyNum;				// æµ�߳���
	int liveNum;				// ����߳���
	int exitNum;				// �������߳���

	pthread_mutex_t mutexPool;	// �̳߳ػ�����
	pthread_mutex_t mutexBusy;	// busyNum������

	int shutdown;				// �̳߳����ٱ�־λ

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
		// pthread_create(�߳�ID���߳����ԣ��߳�������������������);
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
		// û������ʱ�����������߳�
		while (0 == pool->taskSize && !pool->shutdown) {
			pthread_cond_wait(&pool->notEmpty, &pool->mutexPool);
			// �ж��Ƿ������߳�
			if (0 < pool->exitNum) {
				pool->exitNum--;
				pool->liveNum--;
				pthread_mutex_unlock(&pool->mutexPool);
				threadExit(pool);
			}
		}
		// �̳߳عر�ʱ
		if (pool->shutdown) {
			pthread_mutex_unlock(&pool->mutexPool);	// ������
			threadExit(pool);
		}
		// ��������
		Task task;
		task.func = pool->tasks[pool->front].func;
		task.arg = pool->tasks[pool->front].arg;
		pool->front = (pool->front + 1) % pool->taskCapacity;
		pool->taskSize--;

		// �����������������
		pthread_cond_signal(&pool->notFull);
		pthread_mutex_unlock(&pool->mutexPool);
		// �̹߳���
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
		// ÿ��3s���һ��
		sleep(3);
		// ȡ�����������߳�����
		pthread_mutex_lock(&pool->mutexPool);
		int taskSize = pool->taskSize;
		int liveNum = pool->liveNum;
		int busyNum = pool->busyNum;
		pthread_mutex_unlock(&pool->mutexPool);

		// ����߳�
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
		// �����߳�
		if (busyNum * 2 < liveNum && pool->minNum < liveNum) {
			pthread_mutex_lock(&pool->mutexPool);

			pool->exitNum = NUMBER;
			// �ù����̡߳���ɱ��
			for (int i = 0; i < NUMBER; ++i) {
				// �������������������ϵ��������̣߳�Line��60��
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

	// �������
	pool->tasks[pool->rear].func = func;
	pool->tasks[pool->rear].arg = arg;
	pool->rear = (pool->rear + 1) % pool->taskCapacity;
	pool->taskSize++;

	// ������������������
	pthread_cond_signal(&pool->notEmpty);

	pthread_mutex_unlock(&pool->mutexPool);
}

int destroy(ThreadPool * pool)
{
	if (pool == NULL) return 0;
	// �ر��̳߳�
	pool->shutdown = 1;
	// ���չ������߳�
	pthread_join(pool->manager, NULL);
	// �����������߳�
	for (int i = 0; i < pool->liveNum; ++i) {
		pthread_cond_signal(&pool->notEmpty);
	}
	// �ͷ��ڴ�ռ�
	if (pool->tasks) free(pool->tasks);
	if (pool->threads) free(pool->threads);

	// �ͷ���
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
