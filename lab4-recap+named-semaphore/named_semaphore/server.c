#include "util.h"

#include <errno.h>
#include <fcntl.h>  // O_CREAT and O_EXCL flags
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define LOG_INTERVAL        1
#define NUM_RESOURCES       3
#define SEMAPHORE_NAME      "/simple_scheduler"

// we use a global variable to store a pointer to the named semaphore
sem_t* named_semaphore;

void cleanup() {
    /** Close and then unlink the named semaphore **/
    printf("\rShutting down the server...\n");

    sem_close(named_semaphore);

    /* We unlink the semaphore, otherwise it would remain in the
     * system after the server dies. */
    sem_unlink(SEMAPHORE_NAME);

    exit(0);
}

int main(int argc, char* argv[]) {
    int ret;

    /** Create a named semaphore to be shared with different processes.
     *
     * O_CREAT tells the system to create the semaphore named SEMAPHORE_NAMED
     * if it does not already exist. When used in an OR combination with
     * O_EXCL, the call fails if the semaphore already exists.
     *
     * Mode 0600 means that only the processes owned by the user that creates
     * the semaphore can access it, with read & write (4+2) permissions.
     *
     * We initialize the semaphore with a value equal to NUM_RESOURCES.
     **/

    named_semaphore = sem_open(SEMAPHORE_NAME, O_CREAT | O_EXCL, 0600, NUM_RESOURCES);

    if (named_semaphore == SEM_FAILED && errno == EEXIST) {
        printf("[WARNING] The named semaphore already exits. Did you forget to destroy it? :-)\n");

        sem_unlink(SEMAPHORE_NAME);

        // now we can try to create the semaphore from scratch
        named_semaphore = sem_open(SEMAPHORE_NAME, O_CREAT | O_EXCL, 0600, NUM_RESOURCES);
    }

    if (named_semaphore == SEM_FAILED) {
        printf("[FATAL ERROR] Could not open the named semaphore, the reason is: %s\n", strerror(errno));
        exit(1);
    }

    /* Since a CTRL+C would kill the program, we rely on an auxiliary
     * method to catch the interrupt and executed a cleanup code before
     * terminating the program. */
    setQuitHandler(&cleanup);

    printf("Welcome! This is the server module of our simple resource scheduler.\n\n");
    printf("%d resources are initially available in the system. Use CTRL+C to exit!\n\n", NUM_RESOURCES);

    /* Main loop */
    while(1) {
        /** We want to periodically print statistics on our server's load to screen **/
        char timestamp[9];
        time_t now = time(0);

        // get a timestamp of the form "HH:MM:SS" and store it into a buffer
        strftime((char*)timestamp, 9, "%H:%M:%S", localtime(&now));

        /** Get the current value for the semaphore and store it into a
         * custom local variable (we pass its address through &). The
         * value of the semaphore is the number of resources not in use. **/
        int current_value;
        ret = sem_getvalue(named_semaphore, &current_value);

        if (ret) {
            printf("[FATAL ERROR] Could not access the named semaphore, the reason is: %s\n", strerror(errno));
            exit(1);
        }

        printf("[%s] %d resources are available and %d are in use\n", timestamp, current_value, NUM_RESOURCES - current_value);

        sleep(LOG_INTERVAL);
    }

    /*** We will never reach this point since we want to exit the program through CTRL+C only! ***/

    return 0;
}
