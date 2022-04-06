#ifndef _THREADPOOL_H_
#define _THREADPOOL_H_

const int NUMBER = 2;

//typedef struct Task Task;
typedef struct ThreadPool ThreadPool;

ThreadPool* createThreadPool(int min, int max, int size);
/* 工作者线程任务 */
void* worker(void * arg);
/* 管理者线程任务 */
void* manager(void * arg);
/* 更安全的线程退出函数 */
void threadExit(ThreadPool * pool);
/* 添加任务 */
void addTask(ThreadPool * pool, void(*func)(void*), void * arg);

int destroy(ThreadPool * pool);

int threadPoolBusyNum(ThreadPool * pool);
int threadPoolAliveNum(ThreadPool * pool);

#endif // !_THREADPOOL_H_
