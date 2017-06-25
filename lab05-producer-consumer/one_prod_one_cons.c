#include <string.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>       // nanosleep()

#include "common.h"		// macros for error handling

#define BUFFER_SIZE         128
#define INITIAL_DEPOSIT     0
#define MAX_TRANSACTION     1000

/** Globals **/
int transactions[BUFFER_SIZE];  // circular buffer

sem_t fill_count;   // to check if any new data is available for processing
sem_t empty_count;  // to check if there are any available slots to write new data

int read_index;     // index of the next slot containing information to be read
int write_index;    // index of the next available slot for writing

int deposit = INITIAL_DEPOSIT;

struct timespec pause_interval; // used by nanosleep()


/** Auxiliary method to simulate a random non-zero transaction **/
static inline int performRandomTransaction() {
    nanosleep(&pause_interval, NULL);

    int amount = rand() % (2 * MAX_TRANSACTION); // {0, ..., 2*MAX_TRANSACTION - 1}
    if (amount++ >= MAX_TRANSACTION) {
        // now amount is in {MAX_TRANSACTION + 1, ..., 2*MAX_TRANSACTION}
        return MAX_TRANSACTION - amount; // {-MAX_TRANSACTION, ..., -1}
    } else {
        // now amount is a number between 1 and MAX_TRANSACTION
        return amount;
    }

}


/** Producer thread **/
void* performTransactions(void* arg) {
    while (1) {
        // produce the item
        int currentTransaction = performRandomTransaction();

        int ret = sem_wait(&empty_count);
        ERROR_HELPER(ret, "Wait on empty_count failed");

        // write the item and update write_index accordingly
        transactions[write_index] = currentTransaction;
        write_index = (write_index + 1) % BUFFER_SIZE;

        ret = sem_post(&fill_count);
        ERROR_HELPER(ret, "Post on fill_count failed");
    }
}


/** Consumer thread **/
void* processTransactions(void* arg) {
    while (1) {
        int ret = sem_wait(&fill_count);
        ERROR_HELPER(ret, "Wait on fill_count failed");

        // get the item and update read_index accordingly
        int lastTransaction = transactions[read_index];
        read_index = (read_index + 1) % BUFFER_SIZE;

        ret = sem_post(&empty_count);
        ERROR_HELPER(ret, "Post on empty_count failed");

        // consume the item
        deposit += lastTransaction;
        if (read_index % 10 == 0) {
            printf("After the last 10 transactions balance is now %d.\n", deposit);
        }
    }
}


int main(int argc, char* argv[]) {

    printf("Welcome! This program simulates financial transactions on a deposit.\n");
    printf("\nThe maximum amount of a single transaction is %d (negative or positive).\n", MAX_TRANSACTION);
    printf("\nInitial balance is %d. Press CTRL+C to quit.\n\n", INITIAL_DEPOSIT);

    int ret;

    // initialize read and write indexes
    read_index  = 0;
    write_index = 0;

    ret = sem_init(&fill_count, 0, 0);
    ERROR_HELPER(ret, "Could not initialize fill_count");

    ret = sem_init(&empty_count, 0, BUFFER_SIZE);
    ERROR_HELPER(ret, "Could not initialize empty_count");

    /* nanosleep() takes as first argument a pointer to a timespec
     * object containing the desired interval. It also takes a pointer
     * (which can be NULL) to another timespec object where it stores
     * the remaining time if it gets interrupted by a signal. */
    pause_interval.tv_sec = 0;
    pause_interval.tv_nsec = 100000000; // 100 ms (100*10^6 ns)

    pthread_t producer, consumer;

    ret = pthread_create(&producer, NULL, performTransactions, NULL);
    PTHREAD_ERROR_HELPER(ret, "Could not create producer thread");
    
	ret = pthread_detach(producer);
	PTHREAD_ERROR_HELPER(ret, "Could not detach producer thread");

    ret = pthread_create(&consumer, NULL, processTransactions, NULL);
    PTHREAD_ERROR_HELPER(ret, "Could not create consumer thread");
    
	ret = pthread_detach(consumer);
	PTHREAD_ERROR_HELPER(ret, "Could not detach consumer thread");

    /* We do not use a canonical return, since we want to have the
     * producer and consumer threads to live after main() terminates. */
    pthread_exit(NULL);
}
