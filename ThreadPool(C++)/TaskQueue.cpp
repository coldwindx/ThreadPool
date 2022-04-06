#include "TaskQueue.h"


template<typename T>
TaskQueue<T>::TaskQueue()
{
	pthread_mutex_init(&mutex, NULL);
}

template<typename T>
TaskQueue<T>::~TaskQueue()
{
	pthread_mutex_destroy(&mutex);
}

template<typename T>
void TaskQueue<T>::push(Task<T> task)
{
	pthread_mutex_lock(&mutex);
	taskQueue.push(task);
	pthread_mutex_unlock(&mutex);
}

template<typename T>
void TaskQueue<T>::push(callback f, void * arg)
{
	pthread_mutex_lock(&mutex);
	taskQueue.push(Task<T>(f, arg));
	pthread_mutex_unlock(&mutex);
}

template<typename T>
Task<T> TaskQueue<T>::pop()
{
	Task<T> task;
	pthread_mutex_lock(&mutex);
	if (!taskQueue.empty()) {
		task = taskQueue.front();
		taskQueue.pop();
	}
	pthread_mutex_unlock(&mutex);
	return task;
}
