#include <stdio.h>
#include <stdlib.h>
#include <math.h>

typedef struct punto_s {
    double x, y;
} punto_t;


float distanza(punto_t *p1, punto_t *p2) {
    return sqrt( (p1->x - p2->x)*(p1->x - p2->x) +
                 (p1->y - p2->y)*(p1->y - p2->y) );
}

punto_t orig = {0.0, 0.0};

int main(int argc, char* argv[]) {
        
    // allocate space for a punto_t object
	punto_t* dest = malloc(sizeof(punto_t));
        
    if (argc != 3) {
        printf("Syntax: %s <X_coord> <Y_coord>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    float x = atof(argv[1]);
    float y = atof(argv[2]);
    
    dest->x = x;
    dest->y = y;
    
    float dist = distanza(&orig, dest);
    
    printf("Euclidean distance: %f\n", dist);
    
    free(dest);
    
    return 0;        
}