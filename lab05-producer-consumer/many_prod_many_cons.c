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
#define NUM_CONSUMERS       3
#define NUM_PRODUCERS       3

/*
 * When we have multiple producers or consumers, we need to make sure
 * that two threads never read from or write to the same element.
 *
 * For instance, a producer might get interrupted after the wait on
 * empty_count returns but before it has written a new element and
 * incremented write_index. Another producer will then read an outdated
 * value for write_index and one of the two produced elements will be
 * lost. A similar behavior can be observed when two consumers read the
 * read_index value and thus the same element from the buffer, while the
 * element in read_index+1 is definitely lost.
 *
 * For this reason, each index can be protected using a mutex, which is
 * a binary semaphore whose value is either 0 or 1.
 */
 
/** Globals **/
int transactions[BUFFER_SIZE];  // circular buffer

sem_t fill_count;   // to check if new data is available for processing
sem_t empty_count;  // to check if there are available slots for new data

sem_t read_mutex;   // to avoid race conditions between two consumers
sem_t write_mutex;  // to avoid race conditions between two producers

int read_index;     // index of the next slot containing data to read
int write_index;    // index of the next available slot to write to

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
    // trick: we pass the index with a cast to void* and back to long!
    printf("Starting producer thread %ld\n", (long)arg);
    
    while (1) {
        // produce the item
        int currentTransaction = performRandomTransaction();

        int ret = sem_wait(&empty_count);
        ERROR_HELPER(ret, "Wait on empty_count failed");

        ret = sem_wait(&write_mutex);
        ERROR_HELPER(ret, "Wait on write_mutex failed");
        
        // write the item and update write_index accordingly
        transactions[write_index] = currentTransaction;
        write_index = (write_index + 1) % BUFFER_SIZE;

        ret = sem_post(&write_mutex);
        ERROR_HELPER(ret, "Post on write_mutex failed");

        ret = sem_post(&fill_count);
        ERROR_HELPER(ret, "Post on fill_count failed");
    }
}


/** Consumer thread **/
void* processTransactions(void* arg) {
    // trick: we pass the index with a cast to void* and back to long!
    printf("Starting consumer thread %ld\n", (long)arg);
    
    while (1) {
        int ret = sem_wait(&fill_count);
        ERROR_HELPER(ret, "Wait on fill_count failed");

        ret = sem_wait(&read_mutex);
        ERROR_HELPER(ret, "Wait on read_mutex failed");

        // as we have multiple consumers, in the critical section we
        // also update deposit to avoid race conditions on it
        deposit += transactions[read_index];
        read_index = (read_index + 1) % BUFFER_SIZE;        
        if (read_index % 10 == 0)
			printf("After the last 10 transactions balance is now %d.\n", deposit);

        ret = sem_post(&read_mutex);
        ERROR_HELPER(ret, "Post on read_mutex failed");

        ret = sem_post(&empty_count);
        ERROR_HELPER(ret, "Post on empty_count failed");    
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

    ret = sem_init(&read_mutex, 0, 1);
    ERROR_HELPER(ret, "Could not initialize read_mutex");
    
    ret = sem_init(&write_mutex, 0, 1);
    ERROR_HELPER(ret, "Could not initialize write_mutex");

    /* nanosleep() takes as first argument a pointer to a timespec
     * object containing the desired interval. It also takes a pointer
     * (which can be NULL) to another timespec object where it stores
     * the remaining time if it gets interrupted by a signal. */
    pause_interval.tv_sec = 0;
    pause_interval.tv_nsec = 100000000; // 100 ms (100*10^6 ns)

    pthread_t producer, consumer;

    long i;
    for (i = 0; i < NUM_PRODUCERS; ++i) {
        // trick: rather than allocating a struct, we pass the thread
        // index by casting it to a pointer (in C arguments are passed
        // by value) since a long has the same size of a pointer
        ret = pthread_create(&producer, NULL, performTransactions, (void*)i);
        PTHREAD_ERROR_HELPER(ret, "Could not create producer thread");
        ret = pthread_detach(producer);
        PTHREAD_ERROR_HELPER(ret, "Could not detach producer thread");
        memset(&producer, 0, sizeof(pthread_t)); // to reuse the object
    }

    for (i = 0; i < NUM_CONSUMERS; ++i) {
        // trick: rather than allocating a struct, we pass the thread
        // index by casting it to a pointer (in C arguments are passed
        // by value) since a long has the same size of a pointer
		ret = pthread_create(&consumer, NULL, processTransactions, (void*)i);
		PTHREAD_ERROR_HELPER(ret, "Could not create consumer thread");
		ret = pthread_detach(consumer);
        PTHREAD_ERROR_HELPER(ret, "Could not detach consumer thread");
		memset(&consumer, 0, sizeof(pthread_t)); // to reuse the object
	}

    /* We do not use a canonical return, since we do not want the
     * producer and consumer threads to die after main() terminates. */
    pthread_exit(NULL);
}
