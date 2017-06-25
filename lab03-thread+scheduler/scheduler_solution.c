#include <errno.h>      // contains the global variable errno to determine the type of an error
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>     // strerror() formats errno into a human-readable string
#include <unistd.h>     // sleep()

/* Some constants */

#define MAX_SLEEP       3   // used to simulate a work item (max length)
#define NUM_RESOURCES   3   // number of available special resources
#define NUM_TASKS       3   // we define the number of work items per thread
#define THREAD_BURST    5   // determines how many threads are spawned at the same time

/* We use a simple structure to encapsulate a thread's arguments */
typedef struct thread_args_s {
    int     ID;
    sem_t*  semaphore;
    int     num_tasks;
} thread_args_t;


/* This is the function executed when a client thread is created */
void* client(void* arg_ptr) {
    thread_args_t* args = (thread_args_t*) arg_ptr;

    int i = 0;

    /** Process two of the work items assigned to the thread at a time **/
    while (i < args->num_tasks) {
        /* Acquire the resource */
        int ret = sem_wait(args->semaphore);
        if (ret) {
            printf("[FATAL ERROR] Could not lock the semaphore from thread %d: %s\n", args->ID, strerror(errno));
            exit(1);
        }
        printf("[@Thread%d] Resource acquired...\n", args->ID);

        sleep(rand() % (MAX_SLEEP+1)); // work item i
        ++i;

        /* When we are performing the last iteration of the while cycle,
         * the following block will not be executed if the total number
         * of work items to process is odd number. Note that we have to
         * increment i anyways in order to get out of the while cycle. */
        if (i != args->num_tasks) {
            sleep(rand() % (MAX_SLEEP+1)); // work item i+1
        }
        ++i;

        /* Free the resource */
        ret = sem_post(args->semaphore);
        if (ret) {
            printf("[FATAL ERROR] Could not unlock the semaphore from thread %d: %s\n", args->ID, strerror(errno));
            exit(1);
        }
        printf("[@Thread%d] Resource released!\n", args->ID);
    }

    printf("[@Thread%d] Done!\n", args->ID);

    free(args); // I should free my own arguments!
    return NULL;
}

int main(int argc, char* argv[]) {
    printf("Welcome! This is a very simple resource scheduler.\n\n");
    printf("We are simulating a system with %d available special resources. Hence, no more "
           "than %d threads can get exclusive access to them at the same time.\n\n", NUM_RESOURCES, NUM_RESOURCES);

    int ret = 0;
    int thread_ID = 0;

    sem_t* semaphore = malloc(sizeof(sem_t)); // we allocate a sem_t object on the heap

    /*** Initialize a semaphore to coordinate access to the resources ***/
    ret = sem_init(semaphore, 0, NUM_RESOURCES);

    if (ret) {
        printf("[FATAL ERROR] Could not create a semaphore: %s\n", strerror(errno));
        exit(1);
    }

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
            args->semaphore = semaphore;
            args->ID = thread_ID;
            args->num_tasks = NUM_TASKS;

            if (pthread_create(&thread_handle, NULL, client, args)) {
                printf("==> [DRIVER] FATAL ERROR: cannot create thread %d: %s\nExiting...\n", thread_ID, strerror(errno));
                exit(1);
            }

            ++thread_ID;

            // I won't wait for this thread: it's a good idea to detach it!
            pthread_detach(thread_handle);
        }

        printf("==> [DRIVER] Press ENTER to spawn %d new threads. Press CTRL+D to quit!\n", THREAD_BURST);
    }

    printf("Exiting...\n");

    /*** Don't forget to destroy the semaphore once you're done ***/
    sem_destroy(semaphore);

    free(semaphore);

    return 0;
}
