#pragma once

#include <queue>
#include <pthread.h>

using std::queue;
using callback = void(*)(void *arg);

template<typename T>
struct Task
{
	callback function;		// ������
	T * arg;				// �����б�

	Task() : function(nullptr), arg(nullptr) {}
	Task(callback f, void * arg) : function(f), arg((T*)arg) {}
};
// �������
template<typename T>
class TaskQueue
{
	queue<Task<T>> taskQueue;
	pthread_mutex_t mutex;
public:
	TaskQueue();
	~TaskQueue();
	
	void push(Task<T> task);
	void push(callback f, void * arg);
	Task<T> pop();

	inline int size() {
		return taskQueue.size();
	}
};

