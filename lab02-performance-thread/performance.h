#ifndef __PERFORMANCE__
#define __PERFORMANCE__

#include <time.h>       /* time */

typedef struct {
	struct timespec begin;
	struct timespec end;
	struct timespec elapsed;
} timer;

void begin(timer* t);
void end(timer* t);
unsigned long int get_seconds(timer* t);
unsigned long int get_milliseconds(timer* t);
unsigned long int get_microseconds(timer* t);
unsigned long int get_nanoseconds(timer* t);

#endif
