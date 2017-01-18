#include <stdio.h>
#include <stdlib.h>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        printf("Syntax: %s <N>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    int N = atoi(argv[1]);
    
    int* array = malloc(N*sizeof(int));
    
    int i;    
    for (i = 0; i < N; ++i) { // from 0 to N-1    
        array[i] = i+1;
    }
    
    unsigned long sum = 0;
    for (i = 0; i < N; ++i) { // same as above
        sum += array[i];
    }
    
    unsigned long expected = N*(N+1)/2;
    printf("Sum is: %lu\n", sum);
    printf("Expected: %lu\n", expected);
	
	free(array);
    
    return 0;    
}