/*
 * Acts as server for the aesd
 * Author: Heiko Schmidt
 */
#define _XOPEN_SOURCE 700

#include <signal.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static bool appRun = true;

static void signal_handler(const int signum)
{
    appRun = false;
}

bool is_app_running(void)
{
    return appRun;
}

int register_sighandler(void)
{
    struct sigaction sa;

    memset((void*)&sa, 0x0, sizeof(struct sigaction));
    sa.sa_handler = &signal_handler;

    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}
