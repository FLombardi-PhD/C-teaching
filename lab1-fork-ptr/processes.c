#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

unsigned int factorial(unsigned int n) {
    if (n == 0) return 1;
    return n * factorial(n - 1);
}

unsigned int fibonacci(unsigned int i) {  
    if (i == 0) return 0;
    else if (i == 1) return 1;
    else return fibonacci(i-1) + fibonacci(i-2);
}

int main(int argc, char* argv[]) {    
    if (argc != 2) {
        printf("Syntax: %s <N>\n", argv[0]);
        exit(EXIT_FAILURE);        
    }
    
    int N = atoi(argv[1]);
    if (N > 10) {
        printf("N is too large. Using 10 instead...\n");
        N = 10;
    }    
    
    int i;
    unsigned int* buffer = malloc(N*sizeof(unsigned int));    
    
    pid_t pid = fork();
    
    if (pid < 0) {
        fprintf(stderr, "Could not create process: %s\n", strerror(errno));
    } else if (pid == 0) {
        // child process
        pid_t child_pid = getpid();
        
		// compute and print results
		for (i = 0; i < N; ++i) buffer[i] = factorial(i);
        for (i = 0; i < N; ++i) 
            printf("[CHILD %d] Factorial for %d: %u\n", child_pid, i, buffer[i]);
        
        printf("[CHILD %d] Exiting...\n", child_pid);
		
		// free buffer in child process before quitting
        free(buffer);
        exit(EXIT_SUCCESS);
    } else {
        // parent process (note that pid contains child's PID)    
		pid_t parent_pid = getpid();
		
        // compute results
		for (i = 0; i < N; ++i) buffer[i] = fibonacci(i);
		
		// wait for child completion before printing results		
        int child_status;
        wait(&child_status);
        printf("[PARENT %d] Child terminated with status %d\n", parent_pid, child_status); 
		
        for (i = 0; i < N; ++i)
            printf("[PARENT %d] Fibonacci number for %d: %u\n", parent_pid, i, buffer[i]);
        
        printf("[PARENT %d] Exiting...\n", parent_pid);        
		
		// free buffer in parent process before quitting
        free(buffer);
        exit(EXIT_SUCCESS);
    }
    
    return 0;
}