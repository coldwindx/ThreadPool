#include "threadpool.h"
#include <stdio.h>
#include <unistd.h>


void testFunc(void * arg) {
	int num = *(int*)arg;
	printf("thread %ld is working, number = %d\n", pthread_self(), num);
	usleep(1000);
}

int main()
{
	ThreadPool * pool = createThreadPool(3, 10, 100);
	for (int i = 0; i < 100; ++i) {
		int * num = (int*)malloc(sizeof(int));
		*num = i + 100;
		addTask(pool, testFunc, num);
	}
	sleep(30);
	destroy(pool);
	return 0;
}