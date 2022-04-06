#pragma once

#include "TaskQueue.h"
#include "TaskQueue.cpp"

// �̳߳ض�����
template<typename T>
class ThreadPool
{
	pthread_mutex_t mutex;		// �̳߳ػ�����
	bool shutdown;				// �̳߳����ٱ�־λ

	TaskQueue<T> * taskQueue;		// �������
	pthread_cond_t notEmpty;

	pthread_t managerID;		// �������߳�
	pthread_t * threadIDs;		// �̶߳���

	int minNum;					// ��С�߳���
	int maxNum;
	int busyNum;				// æµ�߳���
	int liveNum;				// ����߳���
	int exitNum;				// �������߳���

	static const int NUMBER = 2;
public:
	ThreadPool(int min, int max);
	~ThreadPool();

	void addTask(Task<T> task);
	int getBusyNum();
	int getAliveNum();

private:
	static void* worker(void * arg);		/* �������߳����� */
	static void* manager(void * arg);		/* �������߳����� */
	void exit();					/* ����ȫ���߳��˳����� */
};

