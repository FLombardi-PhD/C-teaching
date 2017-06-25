#include "performance.h"
#include <math.h>

struct timespec diff(struct timespec start, struct timespec end)
{
	struct timespec temp;
	if (end.tv_nsec - start.tv_nsec < 0) {
		temp.tv_sec = end.tv_sec - start.tv_sec-1;
		temp.tv_nsec = 1000000000 + end.tv_nsec-start.tv_nsec;
	} else {
		temp.tv_sec = end.tv_sec - start.tv_sec;
		temp.tv_nsec = end.tv_nsec - start.tv_nsec;
	}
	return temp;
}

void begin(timer* t) {
	clock_gettime(CLOCK_MONOTONIC, &(t->begin));
}

void end(timer* t) {
	clock_gettime(CLOCK_MONOTONIC, &(t->end));
	t->elapsed = diff(t->begin, t->end);
}

unsigned long int get_nanoseconds(timer* t) {
	return t->elapsed.tv_sec * 1000000000 + t->elapsed.tv_nsec;
}

unsigned long int get_microseconds(timer* t) {
	return (unsigned int)round(get_nanoseconds(t)/1000);
}

unsigned long int get_milliseconds(timer* t) {
	return (unsigned int)round(get_nanoseconds(t)/1000000);
}

unsigned long int get_seconds(timer* t) {
	return (unsigned int)round(get_nanoseconds(t)/1000000000);
}
