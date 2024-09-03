/*
 * Acts as server for the aesd
 * Author: Heiko Schmidt
 */
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <syslog.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/stat.h>

#include <netinet/in.h>
#include <arpa/inet.h>

static void signal_handler(const int signum);

static int register_sighandler();
static int get_server_socket();
static int handle_connection(const int client_sock);
static void run_as_daemon();

static bool appRun = true;
static int srv_sock = -1;

int main(int argc, char **argv)
{
    int ret = 0;
    int client_sock = -1;

    openlog("aesdsocket", 0, LOG_USER);

    // register signal handler
    if (register_sighandler() < 0) {
        syslog(LOG_ERR, "Error register signal handler: %s", strerror(errno));
        ret = -1;
        goto clean;
    }

    // bind server socket
    srv_sock = get_server_socket();
    if(srv_sock < 0) {
        ret = -1;
        goto clean;
    }

    // check if program shall run as daemon
    if (argc == 2 && strcmp("-d", argv[1]) == 0)
        run_as_daemon();

    while(appRun) {
        // wait for next client connection
        struct sockaddr_in client_addr;
        socklen_t l = sizeof(struct sockaddr_in);

        client_sock = accept(srv_sock, (struct sockaddr*)&client_addr, &l);
        if(client_sock < 0) {
            syslog(LOG_ERR, "Error on accept: %s", strerror(errno));
            ret = -1;
            goto clean;
        }

        // log new connection
        char buf[INET_ADDRSTRLEN];
        if(inet_ntop(AF_INET, (const void*)&client_addr.sin_addr, buf, INET_ADDRSTRLEN) == NULL) {
            syslog(LOG_ERR, "Error getting IP string: %s", strerror(errno));
            ret = -1;
            goto clean;
        }
        syslog(LOG_INFO, "Accepted connection from %s", buf);

        handle_connection(client_sock);
    }

    syslog(LOG_INFO, "Caught signal, exiting");

clean:
    // delete file
    remove("/var/tmp/aesdsocketdata");

    // close client socket
    if(client_sock >= 0)
        close(client_sock);

    // close server socket
    if(srv_sock >= 0)
        close(srv_sock);

    // close syslog
    closelog();

    return ret;
}

static void signal_handler(const int signum)
{
    if (srv_sock >= 0)
        shutdown(srv_sock, SHUT_RDWR);
    appRun = false;
}

static int register_sighandler()
{
    struct sigaction sa;

    sa.sa_handler = &signal_handler;

    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

static int get_server_socket()
{
    // get the socket
    int fd = socket(AF_INET, SOCK_STREAM, 0);

    if (fd < 0) {
        syslog(LOG_ERR, "Error getting server socket: %s", strerror(errno));
        return -1;
    }

    // set option to reuse address and port
    int enable = 1;
    if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0) {
        syslog(LOG_ERR, "Error setting socket options: %s", strerror(errno));
        close(fd);
        return -1;
    }

    // bind the socket
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(9000);

    if(bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        syslog(LOG_ERR, "Error binding socket: %s", strerror(errno));
        close(fd);
        return -1;
    }

    // listen on the socket
    if(listen(fd, 1) < 0) {
        syslog(LOG_ERR, "Error listen to socket: %s", strerror(errno));
        close(fd);
        return -1;
    }

    return fd;
}

static int handle_connection(const int client_sock)
{
    // open the socketdata file
    FILE *fp = fopen("/var/tmp/aesdsocketdata", "a+");
    if (fp == NULL) {
        syslog(LOG_ERR, "Error opening file: %s", strerror(errno));
        return -1;
    }

    // transform client socket to fd to read like a file
    FILE *sock_fp = fdopen(client_sock, "a+");
    if (sock_fp < 0) {
        syslog(LOG_ERR, "Error open socket fd: %s", strerror(errno));
        fclose(fp);
        return -1;
    }

    // read from socket, line by line
    while(appRun) {
        char *line = NULL;
        size_t len;
        size_t read = getline(&line, &len, sock_fp);

        // if client did close connection (or any other error), we're done here
        if(read == -1)
            break;

        // copy the line to the output file
        if(fputs(line, fp) < 0) {
            syslog(LOG_ERR, "Eror writing to file: %s", strerror(errno));
            break;
        }

        // in response, send the complete content of the file back
        free(line);
        line = NULL;
        len = 0;

        rewind(fp);
        while(true) {
            read = getline(&line, &len, fp);
            if (read == -1)
                break;

            if(fputs(line, sock_fp) < 0) {
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

static void run_as_daemon()
{
    const pid_t pid = fork();

    if (pid < 0) {
        syslog(LOG_ERR, "Error forking: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (pid == 0) {
        // this is the daemon

        // reset file umask
        umask(0);

        // set the sid
        if (setsid() == -1) {
            syslog(LOG_ERR, "Error on setsid: %s", strerror(errno));
            exit(EXIT_FAILURE);
        }

        // change to root dir
        if (chdir("/") < 0) {
            syslog(LOG_ERR, "Error on changing to root: %s", strerror(errno));
            exit(EXIT_FAILURE);
        }
    
        // redirect stdin, stdout and stderr to /dev/null
        stdin = freopen("/dev/null", "r", stdin);
        stdout = freopen("/dev/null", "r", stdout);
        stderr = freopen("/dev/null", "r", stderr);
    } else {
        // let init inherit the process
        exit(EXIT_SUCCESS);
    }
}