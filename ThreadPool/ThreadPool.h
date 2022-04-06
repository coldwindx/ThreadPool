#ifndef _THREADPOOL_H_
#define _THREADPOOL_H_

const int NUMBER = 2;

//typedef struct Task Task;
typedef struct ThreadPool ThreadPool;

ThreadPool* createThreadPool(int min, int max, int size);
/* �������߳����� */
void* worker(void * arg);
/* �������߳����� */
void* manager(void * arg);
/* ����ȫ���߳��˳����� */
void threadExit(ThreadPool * pool);
/* ������� */
void addTask(ThreadPool * pool, void(*func)(void*), void * arg);

int destroy(ThreadPool * pool);

int threadPoolBusyNum(ThreadPool * pool);
int threadPoolAliveNum(ThreadPool * pool);

#endif // !_THREADPOOL_H_
