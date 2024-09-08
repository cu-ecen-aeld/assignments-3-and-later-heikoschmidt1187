/*
 * Acts as server for the aesd
 * Author: Heiko Schmidt
 */
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>

#include <stdint.h>
#include <fcntl.h>
#include <errno.h>
#include <syslog.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include <netinet/in.h>

#define DATAFILE "/var/tmp/aesdsocketdata"
#define LOCAL_LINE_BUF_SIZE 512

static int srv_sock = -1;
static int client_sock = -1;

static char *line_buf = NULL;
static uint32_t cur_buf_len = 0;

static int handle_connection(void);
static void write_line_to_file(const char *const line);
static void send_all_lines(void);

int init_server(void)
{
    // delete file
    remove(DATAFILE);

    // get the socket
    srv_sock = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);

    if (srv_sock < 0)
    {
        syslog(LOG_ERR, "Error getting server socket: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    // set option to reuse address and port
    int enable = 1;
    if (setsockopt(srv_sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0)
    {
        syslog(LOG_ERR, "Error setting socket options: %s", strerror(errno));
        close(srv_sock);
        exit(EXIT_FAILURE);
    }

    // bind the socket
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(9000);

    if (bind(srv_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        syslog(LOG_ERR, "Error binding socket: %s", strerror(errno));
        close(srv_sock);
        exit(EXIT_FAILURE);
    }

    // listen on the socket
    if (listen(srv_sock, 1) < 0)
    {
        syslog(LOG_ERR, "Error listen to socket: %s", strerror(errno));
        close(srv_sock);
        exit(EXIT_FAILURE);
    }

    return 0;
}

int process_server(void)
{
    if (client_sock < 0)
    {
        // wait for next client connection
        struct sockaddr_in client_addr;
        socklen_t l = sizeof(struct sockaddr);

        client_sock = accept(srv_sock, (struct sockaddr *)&client_addr, &l);
        if (client_sock < 0 && errno != EWOULDBLOCK && errno != EAGAIN)
        {
            syslog(LOG_ERR, "Error on accept: %s", strerror(errno));
            return -1;
        }

        // log new connection
        char buf[INET_ADDRSTRLEN];
        if (inet_ntop(AF_INET, (const void *)&client_addr.sin_addr, buf, INET_ADDRSTRLEN) == NULL)
        {
            syslog(LOG_ERR, "Error getting IP string: %s", strerror(errno));
            return -1;
        }
        syslog(LOG_INFO, "Accepted connection from %s", buf);
    }
    else
    {
        return handle_connection();
    }

    return 0;
}

void shutdown_server(void)
{
    // close client socket
    if (client_sock >= 0)
        close(client_sock);

    // close server socket
    if (srv_sock >= 0)
        close(srv_sock);

    // delete file
    remove(DATAFILE);
}

static int handle_connection()
{
    // wait with timeout to read from the socket
    struct timeval to;
    to.tv_sec = 0;
    to.tv_usec = 10000;

    fd_set set;

    FD_ZERO(&set);
    FD_SET(client_sock, &set);

    int ret = select(client_sock + 1, &set, NULL, NULL, &to);

    if (ret < 0)
    {
        syslog(LOG_ERR, "Error on select: %s", strerror(errno));
        return -1;
    }
    else if (ret > 0)
    {
        char local_buf[LOCAL_LINE_BUF_SIZE + 1];
        memset(&local_buf[0], 0x0, LOCAL_LINE_BUF_SIZE + 1);

        ssize_t recv_len = recv(client_sock, &local_buf[0], LOCAL_LINE_BUF_SIZE, 0);

        if (recv_len < 0)
        {
            syslog(LOG_ERR, "Error on recv call: %s", strerror(errno));
            return -1;
        }

        for (uint32_t local_start_pos = 0; local_start_pos < recv_len;)
        {
            // check for \n
            uint32_t npos;
            for (npos = 0; npos < recv_len; ++npos)
                if (local_buf[npos] == '\n')
                    break;

            // resize line buffer
            line_buf = (char *)realloc(line_buf, cur_buf_len + npos + 1);

            if (line_buf == NULL)
            {
                syslog(LOG_ERR, "Error re-allocating memory: %s", strerror(errno));
                exit(EXIT_FAILURE);
            }

            // copy line until \n inclusive
            memcpy(&line_buf[cur_buf_len], local_buf, npos);

            // only if \n has been found, write to file and return
            if (npos < recv_len)
            {
                write_line_to_file(line_buf);
                send_all_lines();

                // reset the buffer
                free(line_buf);
                line_buf = NULL;
                cur_buf_len = 0;
            }

            local_start_pos += npos;
        }
    }
    else
    {
        // timeout
    }

    return 0;
}

static void write_line_to_file(const char *const line)
{
    // open the socketdata file
    FILE *fp = fopen(DATAFILE, "a+");
    if (fp == NULL)
    {
        syslog(LOG_ERR, "Error opening file: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (fputs(line_buf, fp) < 0)
    {
        syslog(LOG_ERR, "Eror writing to file: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    fclose(fp);
}

static void send_all_lines(void)
{
    char *line = NULL;
    size_t len = 0;

    if (client_sock < 0)
        return;

    // open the socketdata file
    FILE *fp = fopen(DATAFILE, "a+");

    if (fp == NULL)
    {
        syslog(LOG_ERR, "Error opening file: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    while (getline(&line, &len, fp) > 0)
    {
        if (send(client_sock, line, len, 0) < 0)
        {
            syslog(LOG_ERR, "Error sending line to client: %s", strerror(errno));
            exit(EXIT_FAILURE);
        }

        free(line);

        line = NULL;
        len = 0;
    }

    fclose(fp);
}