#include "performance.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <pthread.h>
 
/*
 * La semantica di una fork() prevede che il processo figlio prosegua
 * da una copia della memoria del padre. I moderni sistemi operativi
 * implementano un meccanismo di copy-on-write che postpone l'effettiva
 * copia di una pagina di memoria finché non vi avviene un accesso in
 * scrittura. Per questa ragione, in assenza di scritture nel padre o
 * nel figlio, indirizzi identici saranno mappati sulle stesse pagine,
 * evitando così copie dispendiose e inutili.
 * 
 * Il codice del programma originario è stato modificato inserendo
 * l'allocazione di un grande buffer di memoria (2^24 ITEMS di tipo int,
 * per un totale di 16 MB x 4 bytes = 64 MB) su cui ciascun processo e
 * poi thread effettua una scrittura ogni STEP*4 bytes (ossia ogni STEP
 * elementi). Nella sezione multi-processo del programma, tali scritture
 * impongono al sistema operativo di effettuare la copia delle pagine
 * coinvolte, lasciando così il buffer nel processo padre intatto.
 * 
 * Si noti che il valore scelto per STEP influenza in modo determinante
 * lo speedup calcolato. Perché scegliere 1 minimizza lo speedup? Cosa
 * succede assegnando a STEP un multiplo o sottomultiplo del valore 1024
 * utilizzato di default (e.g., 128, 256, 512, 2048, 4096)?
 * 
 * Per rispondere a queste domande, riflettere sulle operazioni che
 * vengono coinvolte (i.e., scrittura su memoria, copia delle pagine) e
 * su come cambia intuitivamente il rapporto tra i loro costi a seconda
 * dei valori di STEP utilizzati.
 */ 


#define ITEMS   (1 << 24)
#define STEP    1024
int* global_buff = NULL;

void* thread_fun(void *arg) {
    int j;
    for (j = 0; j < ITEMS; j += STEP) {
        global_buff[j] = j;
    }    
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
	
	// allocate a large buffer of zeroed memory 
	global_buff = (int*)calloc(ITEMS, sizeof(int));
    if (global_buff == NULL) {
        fprintf(stderr, "Cannot allocate memory!\n");
        exit(EXIT_FAILURE);
    }
            
	// process reactivity
	printf("Process reactivity, %d tests...", n); fflush(stdout);
	pid_t pid;
	unsigned long int sum = 0;
	for (i = 0; i < n; i++) {
		begin(&t);
		pid = fork();
		if (pid == -1) {
			fprintf(stderr, "Can't fork, error %d\n", errno);
			exit(EXIT_FAILURE);
		} else if (pid == 0) {
            int j;
            for (j = 0; j < ITEMS; j += STEP) {
                global_buff[j] = j;
            }
            free(global_buff);           
			exit(EXIT_SUCCESS);
		} else {
			wait(0);
		}
		end(&t);
		sum += get_microseconds(&t);
		if (debug)
			printf("[%d] %lu us\n", i, get_microseconds(&t));
	}
	unsigned long int process_avg = sum / n;
	printf("ok, average: %lu microseconds\n", process_avg);
	
	// thread reactivity
	printf("Thread reactivity, %d tests...", n); fflush(stdout);
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
	unsigned long int thread_avg = sum / n;
	printf("ok, average: %lu microseconds\n", thread_avg);
	
	float speedup = (float)process_avg / thread_avg;
	printf("Speedup: %.2f\n", speedup);
	
	free(global_buff);
	return EXIT_SUCCESS;
}
