/*
 * Acts as server for the aesd
 * Author: Heiko Schmidt
 */
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>

#include "signal.h"
#include "server.h"

static void run_as_daemon();

int main(int argc, char **argv)
{
    openlog("aesdsocket", 0, LOG_USER);

    // register signal handler
    if (register_sighandler() < 0)
    {
        syslog(LOG_ERR, "Error register signal handler: %s", strerror(errno));
        closelog();
        exit(EXIT_FAILURE);
    }

    // init server stage 1
    if (init_server_stage1() < 0)
    {
        closelog();
        exit(EXIT_FAILURE);
    }

    // check if program shall run as daemon
    if (argc == 2 && strcmp("-d", argv[1]) == 0)
        run_as_daemon();

    // init server stage 2
    if (init_server_stage2() < 0)
    {
        closelog();
        exit(EXIT_FAILURE);
    }

    while (is_app_running())
    {
        if (process_server() < 0)
            break;
    }

    if (!is_app_running())
        syslog(LOG_INFO, "Caught signal, exiting");

    shutdown_server();

    // close syslog
    closelog();

    exit(EXIT_SUCCESS);
}

static void run_as_daemon()
{
    const pid_t pid = fork();

    if (pid < 0)
    {
        syslog(LOG_ERR, "Error forking: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (pid == 0)
    {
        // this is the daemon

        // reset file umask
        umask(0);

        // set the sid
        if (setsid() == -1)
        {
            syslog(LOG_ERR, "Error on setsid: %s", strerror(errno));
            exit(EXIT_FAILURE);
        }

        // change to root dir
        if (chdir("/") < 0)
        {
            syslog(LOG_ERR, "Error on changing to root: %s", strerror(errno));
            exit(EXIT_FAILURE);
        }

        // redirect stdin, stdout and stderr to /dev/null
        stdin = freopen("/dev/null", "r", stdin);
        stdout = freopen("/dev/null", "r", stdout);
        stderr = freopen("/dev/null", "r", stderr);
    }
    else
    {
        // let init inherit the process
        exit(EXIT_SUCCESS);
    }
}
