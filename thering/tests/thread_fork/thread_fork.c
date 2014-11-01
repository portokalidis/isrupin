#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#define BUFSZ 1024


static int rescueme(int id)
{
	printf("Thread%d please rescue me\n", id);
	sleep(5);
	printf("Thread%d it's ok\n", id);
	return 0;
}

static void *thread1(void *opaque)
{
	printf("Thread 1 running\n");
	rescueme(1);
	return NULL;
}

static void *thread2(void *opaque)
{
	pid_t p;
	printf("Thread 2 running\n");

	p = fork();
	if (p == 0) {
		printf("Thread2 CHILD %d\n", getpid());
		sleep(1);
	} else if (p > 0) {
		printf("Thread2 PARENT %d\n", getpid());
		sleep(1);
	} else {
		perror("fork");
	}

	return NULL;
}

int main(void)
{
	void *ret;
	pthread_attr_t tattr;
	pthread_t t1, t2;

	pthread_attr_init(&tattr);
	pthread_create(&t1, &tattr, thread1, (void *)0);
	sleep(1);
	pthread_create(&t2, &tattr, thread2, (void *)0);

	pthread_join(t1, &ret);
	pthread_join(t2, &ret);

	return 0;
}
