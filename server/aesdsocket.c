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

    // bind server socket
    if (init_server() < 0)
    {
        closelog();
        exit(EXIT_FAILURE);
    }

    // check if program shall run as daemon
    if (argc == 2 && strcmp("-d", argv[1]) == 0)
        run_as_daemon();

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

/*


int main(int argc, char **argv)
{
    int ret = 0;
    int client_sock = -1;







clean:
    // delete file
    remove("/var/tmp/aesdsocketdata");


}

static int handle_connection(const int client_sock)
{
    // open the socketdata file
    FILE *fp = fopen("/var/tmp/aesdsocketdata", "a+");
    if (fp == NULL)
    {
        syslog(LOG_ERR, "Error opening file: %s", strerror(errno));
        return -1;
    }

    // transform client socket to fd to read like a file
    FILE *sock_fp = fdopen(client_sock, "a+");
    if (sock_fp < 0)
    {
        syslog(LOG_ERR, "Error open socket fd: %s", strerror(errno));
        fclose(fp);
        return -1;
    }

    // read from socket, line by line
    while (is_app_running())
    {
        char *line = NULL;
        size_t len;
        size_t read = getline(&line, &len, sock_fp);

        // if client did close connection (or any other error), we're done here
        if (read == -1)
            break;

        // copy the line to the output file
        if (fputs(line, fp) < 0)
        {
            syslog(LOG_ERR, "Eror writing to file: %s", strerror(errno));
            break;
        }

        // in response, send the complete content of the file back
        free(line);
        line = NULL;
        len = 0;

        rewind(fp);
        while (true)
        {
            read = getline(&line, &len, fp);
            if (read == -1)
                break;

            if (fputs(line, sock_fp) < 0)
            {
                syslog(LOG_ERR, "Eror writing to client: %s", strerror(errno));
                break;
            }

            free(line);
            line = NULL;
            len = 0;
        }
    }

    fclose(sock_fp);
    fclose(fp);
    return 0;
}

*/
