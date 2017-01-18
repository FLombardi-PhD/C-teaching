#include <stdlib.h> 
#include <string.h> 
#include <stdio.h>
#include <string.h>
  
int main(int argc, char* argv[]) {         
    if (argc != 2) {
        printf("Syntax: %s <string>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
     
    // 7 bytes for "Input: " + 1 byte for terminator
    char *p = malloc(strlen(argv[1])+7+1); 
  
    sprintf(p, "Input: %s", argv[1]);
    printf("%s\n", p);
    
    free(p);      
    return 0; 
}