#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define MAX_SLEEP           6
#define THREAD_BURST        5
#define SEMAPHORE_NAME      "/simple_scheduler"

typedef struct thread_args_s {
    int     ID;
} thread_args_t;

void* client(void *arg_ptr) {
    thread_args_t* args = (thread_args_t*) arg_ptr;

    /*** Open an existing named semaphore - COMPLETE HERE ***/
    sem_t* my_named_semaphore = sem_open(SEMAPHORE_NAME, 0); // mode is 0: sem_open is not allowed to create it!
    if (my_named_semaphore == SEM_FAILED) {
        printf("[FATAL ERROR] Could not open the named semaphore from thread %d, the reason is: %s\n", args->ID, strerror(errno));
        exit(1);
    }

    /*** Acquire the resource ***/
    if (sem_wait(my_named_semaphore)) {
        printf("[FATAL ERROR] Could not lock the semaphore from thread %d, the reason is: %s\n", args->ID, strerror(errno));
        exit(1);
    }

    printf("[@Thread%d] Resource acquired...\n", args->ID);

    // we simulate some work by sleeping for 0 up to MAX_SLEEP seconds
    sleep(rand() % (MAX_SLEEP+1));

    /*** Free the resource ***/
    if (sem_post(my_named_semaphore)) {
        printf("[FATAL ERROR] Could not unlock the semaphore from thread %d, the reason is: %s\n", args->ID, strerror(errno));
        exit(1);
    }

    printf("[@Thread%d] Done. Resource released!\n", args->ID);

    /*** Close the named semaphore - COMPLETE HERE ***/
    sem_close(my_named_semaphore);

    free(args);
    return NULL;
}

int main(int argc, char* argv[]) {
    int thread_ID = 0;

    printf("Welcome! This is a simple client for our FCFS scheduler.\n\n");
    printf("Please make sure that the server is already running in a separate terminal.\n\n");

    /* Main loop */
    printf("[DRIVER] Press ENTER to spawn %d new threads. Press CTRL+D to quit!\n", THREAD_BURST);

    while(1) {
        int input_char;

        /* We want to skip any character that is not allowed:
         * - when ENTER is pressed, on Linux the character '\n' is read by getchar()
         * - CTRL+D is read as EOF, a special sequence defined in stdio.h */
        while ( (input_char = getchar()) != '\n' && input_char != EOF ) continue;

        if (input_char == EOF) break;

        printf("==> [DRIVER] Spawning %d threads now...\n", THREAD_BURST);

        int i;
        for (i = 0; i < THREAD_BURST; ++i) {
            pthread_t thread_handle;

            thread_args_t* args = malloc(sizeof(thread_args_t));
            args->ID = thread_ID;

            int ret = pthread_create(&thread_handle, NULL, client, args);			
            if (ret) {
                printf("==> [DRIVER] FATAL ERROR: cannot create thread %d, the reason is: %s\nExiting...\n", thread_ID, strerror(ret));
                exit(1);
            }

            ++thread_ID;

            // I won't wait for this thread to terminate: let's detach it!
            ret = pthread_detach(thread_handle);
			if (ret) {
                printf("==> [DRIVER] FATAL ERROR: cannot detach thread %d, the reason is: %s\nExiting...\n", thread_ID, strerror(ret));
                exit(1);
            }
        }

        printf("==> [DRIVER] Press ENTER to spawn %d new threads. Press CTRL+D to quit!\n", THREAD_BURST);
    }

    printf("[DRIVER] Waiting for any running thread to complete and then exiting...\n");
    pthread_exit(NULL); /*** TODO: what would be an alternative solution? ***/
}
