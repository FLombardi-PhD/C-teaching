#include "util.h"

#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

void (*cleanupFunction)();  // dirty but effective for our goal

static void sigint_handler(int sig_no) {
    cleanupFunction();
    exit(0);
}

void setQuitHandler(void(*f)()) {
    cleanupFunction = f;

    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = &sigint_handler;
    sigaction(SIGINT, &action, NULL);
}
