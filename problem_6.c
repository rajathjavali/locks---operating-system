
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

int THREAD_COUNT = 0;
volatile int incircle_counter = 0, total_counter = 0;
volatile int forceStop = 0;
pthread_t *threadArray;

static inline int atomic_xadd(volatile int *ptr) {
  register int val __asm__("eax") = 1;
  asm volatile("lock xaddl %0,%1"
  : "+r" (val)
  : "m" (*ptr)
  : "memory"
  );  
  return val;
}

struct spin_lock_t
{
	volatile int served;
	volatile int waiting;
}critSectionLock;

void spin_lock (struct spin_lock_t *s)
{
	int waiting = atomic_xadd(&s->waiting);
	while(waiting != s->served);
}

void spin_unlock (struct spin_lock_t *s)
{
	atomic_xadd(&s->served);
}

void critSection(void)
{
	
	int localCount = 0, totalCount = 0;
	while (!forceStop)
	{
		double x = ((double)rand()/RAND_MAX)*2-1; // to get between -1 to 1
		double y = ((double)rand()/RAND_MAX)*2-1; // to get between -1 to 1

		double test = (x * x + y * y);
		if(test <= 1) // inside the circle (radius 1)
		{
			localCount++;
		}
		totalCount++;
	}

	spin_lock(&critSectionLock);
		
	incircle_counter += localCount;
	total_counter += totalCount;

	spin_unlock(&critSectionLock);	

	return;
}

void cleanUp ()
{
	free((void*)threadArray);
}

int main(int argc, char **argv)
{
	int i = 0;
	long seconds = 0;
	
	if(argc == 3)
	{
		char *err;
		errno = 0;
		THREAD_COUNT = (int) atoi(argv[1]);
		
		seconds = (long) strtol(argv[2], &err, 10);
		
		if(*err == *argv[1]){
			fprintf(stderr, "Time not passed\n");
			return -1;
		}

		if(errno != 0){
			fprintf(stderr, "Error in strtol%d\n", errno);
			return errno;
		}

		if(THREAD_COUNT < 0 || THREAD_COUNT > 100){
			fprintf(stderr, "Invalid entry for Thread Count\n");
			return -1;
		}

		if(seconds < 0){
			fprintf(stderr, "Invalid entry for time duration\n");
			return -1;
		}
	}
	else
		return -1;

	threadArray = malloc ( sizeof(pthread_t) * THREAD_COUNT );
	
	critSectionLock.served = 0;
	critSectionLock.waiting = 0;

	int status = 0;
	for(i = 0; i < THREAD_COUNT; ++i)
	{
		status = pthread_create(&threadArray[i], NULL, (void*)&critSection, NULL);
		if ( status != 0 )
		{
			fprintf(stderr, "Error while creating thread\n");
			cleanUp();
			exit(1);
		}
	}

	sleep(seconds);
	forceStop = 1;

	status = 0;
	for(i = 0; i < THREAD_COUNT; ++i)
	{
		status = pthread_join(threadArray[i], NULL);
		if ( status != 0 )
		{
			fprintf(stderr, "Error while thread join\n");
			cleanUp();
			exit(1);
		}
	}

	printf("Points within the circle: %d\n", incircle_counter);
	printf("Total Points: %d\n", total_counter);
	printf("Pi (approx): %f\n", 4.0 * incircle_counter / ((float)total_counter));

	cleanUp();

	return 0;

}
