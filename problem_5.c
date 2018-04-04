
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

//#define atomic_xadd(P, V) __sync_fetch_and_add((P), (V))

int THREAD_COUNT = 0;
volatile int in_cs = 0;
volatile int forceStop = 0;
volatile int *threadTickets, *threadAllocCounter, *threadSelection;
pthread_t *threadArray;

// static inline int atomic_cmpxchg ( volatile int *ptr, int old, int new )
// {
// 	int ret;
// 	asm volatile ("lock cmpxchgl %2,%1"
// 		: "=a" (ret), "+m" (*ptr)
// 		: "r" (new), "0" (old)
// 		: "memory");
// 	return ret;
// }
static inline int atomic_xadd(volatile int *ptr) {
  register int val __asm__("eax") = 1;
  asm volatile("lock xaddl %0,%1"
  : "+r" (val)
  : "m" (*ptr)
  : "memory"
  );  
  return val;
}
// static inline int atomic_xadd ( volatile int *ptr )
// {
// 	register int val __asm__("eax") = 1;
// 	asm volatile ("lock xaddl %0,%1"
// 		: "+r" (val)
// 		: "m" (*ptr)
// 		: "memory"
// 		);
// 	return val;
// }

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

void mfence (void) {
  asm volatile ("mfence" : : : "memory");
}

void lock (int tid)
{
	/*mfence required when ever we modify the data shared among the threads*/
	threadSelection[tid] = 1;
	mfence();

	int max_ticket = 0;
	int i = 0;
	for (i = 0 ; i < THREAD_COUNT; ++i) {
		max_ticket = threadTickets[i] > max_ticket ? threadTickets[i] : max_ticket;
	}
	
	threadTickets[tid] = max_ticket + 1;
	mfence();

	threadSelection[tid] = 0;
	mfence();

	for (i = 0; i < THREAD_COUNT; ++i) {
		while (threadSelection[i]) {sched_yield();}
		while (threadTickets[i] != 0 && (threadTickets[i] < threadTickets[tid] || (threadTickets[i] == threadTickets[tid] && i > tid))) {sched_yield();}
	}

}

void unlock (int tid)
{
	threadTickets[tid] = 0;
}

void critSection(void *id)
{
	
	long tid = (long)id;
	/*Critical Section*/
	while (!forceStop)
	{
		spin_lock(&critSectionLock);
		//lock(tid);

		threadAllocCounter[tid]++;

		assert(in_cs == 0);
		in_cs++;
		assert(in_cs == 1);
		in_cs++;
		assert(in_cs == 2);
		in_cs++;
		assert(in_cs == 3);
		in_cs = 0;
	
		spin_unlock(&critSectionLock);
		//unlock(tid);
	}
	/*----------------*/
	return;
}

void cleanUp ()
{
	free((void*)threadTickets);
	free((void*)threadSelection);
	free((void*)threadAllocCounter);
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

	threadTickets = malloc ( sizeof(int) * THREAD_COUNT );
	threadSelection = malloc ( sizeof(int) * THREAD_COUNT );
	threadAllocCounter = malloc ( sizeof(int) * THREAD_COUNT );

	memset((void*) threadTickets, 0 , sizeof(int) * THREAD_COUNT);
	memset((void*) threadSelection, 0 , sizeof(int) * THREAD_COUNT);
	memset((void*) threadAllocCounter, 0 , sizeof(int) * THREAD_COUNT);
	threadArray = malloc ( sizeof(pthread_t) * THREAD_COUNT );
	
	critSectionLock.served = 0;
	critSectionLock.waiting = 0;

	int status = 0;
	for(i = 0; i < THREAD_COUNT; ++i)
	{
		status = pthread_create(&threadArray[i], NULL, (void*)&critSection, (void *)((long)i));
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

	for(i = 0; i < THREAD_COUNT; ++i)
	{
		printf("thread (%d) count: %d\n", i, threadAllocCounter[i]);
	}

	cleanUp();

	return 0;

}
