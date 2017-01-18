#include "performance.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <pthread.h>

void* thread_fun(void *arg) {
	return NULL;
}

int main(int argc, char **argv)
{
	if (argc < 2) {
		fprintf(stderr, "Syntax: %s <N> [<debug>]\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	
	// parse N from the command line
	int n = atoi(argv[1]);
	
	// debug mode: use 0 (off) when only N is given as argument
	int debug = (argc > 2) ? atoi(argv[2]) : 0;
		
	timer t;
	int i;
	unsigned long int sum;
	
	// process reactivity
	printf("Process reactivity, %d tests...", n);
	fflush(stdout);
	pid_t pid;
	sum = 0;
	for (i = 0; i < n; i++) {
		begin(&t);
		pid = fork();
		if (pid == -1) {
			fprintf(stderr, "Can't fork, error %d\n", errno);
			exit(EXIT_FAILURE);
		} else if (pid == 0) {
			exit(EXIT_SUCCESS);
		} else {
			wait(0);
		}
		end(&t);
		sum += get_microseconds(&t);
		if (debug) {
			printf("[%d] %lu us\n", i, get_microseconds(&t));
		}
	}
	unsigned long int process_avg = sum / n;
	printf("ok, average: %lu microseconds\n", process_avg);
	
	// thread reactivity
	printf("Thread reactivity, %d tests...", n);
	fflush(stdout);
	pthread_t thread;
	sum = 0;
	for (i = 0; i < n; i++) {
		begin(&t);
		if (pthread_create(&thread, NULL, thread_fun, NULL) != 0) {
			fprintf(stderr, "Can't create a new thread, error %d\n", errno);
			exit(EXIT_FAILURE);
		}
		pthread_join(thread, NULL);
		end(&t);
		sum += get_microseconds(&t);
		if (debug)
			printf("[%d] %lu us\n", i, get_microseconds(&t));
	}
	
	// compute statistics
	unsigned long int thread_avg = sum / n;
	printf("ok, average: %lu microseconds\n", thread_avg);
	
	float speedup = (float)process_avg / thread_avg;
	printf("Speedup: %.2f\n", speedup);
	
	return EXIT_SUCCESS;
}
