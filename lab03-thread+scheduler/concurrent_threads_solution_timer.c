#include "performance.h"
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>

#define N 1000 // number of threads
#define M 10000 // number of iterations per thread
#define V 1 // value added to the balance by each thread at each iteration

/*
 * Solving race conditions without any os-level synchronization
 * primitive can be achieved by preventing threads to access the same
 * variable: by providing a distinct accumulator to each thread we avoid
 * race conditions and manage to include all the adds made by all the 
 * threads.
 * The solution proposed here uses a shared array with an entry for each 
 * thread. Each thread has to be aware about its own index so as to 
 * understand where exactly to add value m. Once a thread ends, main
 * thread can get its entry in the array and add its value to the global
 * accumulator computed_value.
 * 
 * For questions, send an email to aniello@dis.uniroma1.it
 */

int n = N, m = M, v = V;
unsigned long int* shared_array;

void* thread_work(void *arg) {
	/*
	 * arg is a pointer to int, so we first need to cast it to int* and
	 * then apply the dereference operator * in order to get the int 
	 * value.
	 */
	int thread_idx = *((int*)arg);
	int i;
	for (i = 0; i < m; i++)
		shared_array[thread_idx] += v;
	return NULL;
}

int main(int argc, char **argv)
{
	if (argc > 1) n = atoi(argv[1]);
	if (argc > 2) m = atoi(argv[2]);
	if (argc > 3) v = atoi(argv[3]);
	shared_array = (unsigned long int*)calloc(n, sizeof(unsigned long int));
	timer t;
	
	printf("Going to start %d threads, each adding %d times %d to a shared data structure initialized to zero...", n, m, v); fflush(stdout);
	pthread_t* threads = (pthread_t*)malloc(n * sizeof(pthread_t));
	/*
	 * We need to tell the i-th thread that....it is the i-th thread.
	 * Passing i as argument to the i-th thread is the simplest
	 * solution, so we prepare an int array where the i-th entry is i
	 * and pass &thread_ids[i] as fourth parameter of pthread_create
	 * function.
	 * Exercise: why can't we simply use &i as fourth argument?
	 */
	int* thread_ids = (int*)malloc(n * sizeof(int));
	int i;
	begin(&t);
	for (i = 0; i < n; i++) {
		thread_ids[i] = i;
		if (pthread_create(&threads[i], NULL, thread_work, &thread_ids[i]) != 0) {
			fprintf(stderr, "Can't create a new thread, error %d\n", errno);
			exit(EXIT_FAILURE);
		}
	}
	printf("ok\n");
	
	/*
	 * When the i-th thread terminates, we get the sum of all its adds
	 * (stored in shared_array[i]) and add it to computed_value
	 * variable.
	 */
	printf("Waiting for the termination of all the %d threads...", n); fflush(stdout);
	unsigned long int computed_value = 0;
	for (i = 0; i < n; i++) {
		pthread_join(threads[i], NULL);
		computed_value += shared_array[i];
	}
	end(&t);
	printf("ok\n");
	
	
	unsigned long int expected_value = (unsigned long int)n*m*v;
	printf("The value computed on the array is %lu. It should have been %lu\n", computed_value, expected_value);
	if (expected_value > computed_value) {
		unsigned long int lost_adds = (expected_value - computed_value) / v;
		printf("Number of lost adds: %lu\n", lost_adds);
	}
	printf("It took %lu milliseconds\n", get_milliseconds(&t));
	
	free(shared_array);
	free(threads);
	free(thread_ids);
	return EXIT_SUCCESS;
}
