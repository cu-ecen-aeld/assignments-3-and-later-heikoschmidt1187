/*
 * Acts as server for the aesd
 * Author: Heiko Schmidt
 */
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>

#include <stdbool.h>
#include <time.h>
#include <stdint.h>
#include <fcntl.h>
#include <errno.h>
#include <syslog.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/queue.h>
#include <netinet/in.h>

#define DATAFILE "/var/tmp/aesdsocketdata"
#define LOCAL_LINE_BUF_SIZE 512
#define TIME_FORMAT_BUF_SIZE 64

typedef struct thread_data_s
{
    pthread_t id;
    pthread_mutex_t *mutex;
    int client_sock;
    char client_ip[INET_ADDRSTRLEN];
    volatile bool *stop_thread;
    
} thread_data_t;

typedef struct slist_data_s
{
    thread_data_t thread_data;
    SLIST_ENTRY(slist_data_s) entries;
} slist_data_t;

static int srv_sock = -1;

static SLIST_HEAD(slisthead, slist_data_s) list;

static volatile bool stop_threads = false;

static pthread_t timer_thread;

static pthread_mutex_t file_mutex;

static void *handle_connection(void *data);
static void *log_timestamp(void *data);
static void write_line_to_file(pthread_mutex_t *mutex, const char *const line);
static void send_all_lines(pthread_mutex_t *mutex, const int sock);

int init_server(void)
{
    // initialize list of threads
    SLIST_INIT(&list);
    
    // init the mutex
    if(pthread_mutex_init(&file_mutex, NULL) != 0) {
        syslog(LOG_ERR, "Error initializing file mutex");
        exit(EXIT_FAILURE);
    }

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
    if (listen(srv_sock, 100) < 0)
    {
        syslog(LOG_ERR, "Error listen to socket: %s", strerror(errno));
        close(srv_sock);
        exit(EXIT_FAILURE);
    }
    
    // start timer thread
    thread_data_t *data = (thread_data_t*)malloc(sizeof(thread_data_t));
    if(data == NULL) {
        syslog(LOG_ERR, "Error getting thread data: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    memset((void*)data, 0x0, sizeof(thread_data_t));

    data->mutex = &file_mutex;
    data->stop_thread = &stop_threads;

    if(pthread_create(&timer_thread, NULL, log_timestamp, (void*)data) < 0) {
        syslog(LOG_ERR, "Error creating timer thread");
        exit(EXIT_FAILURE);
    }

    return 0;
}

int process_server(void)
{
    // wait for next client connection
    struct sockaddr_in client_addr;
    socklen_t l = sizeof(struct sockaddr);

    int client_sock = accept(srv_sock, (struct sockaddr *)&client_addr, &l);
    if (client_sock < 0 && errno != EWOULDBLOCK && errno != EAGAIN)
    {
        syslog(LOG_ERR, "Error on accept: %s", strerror(errno));
        return -1;
    }
    
    if(client_sock >= 0) {
        // spawn thread for socket
        slist_data_t *data = (slist_data_t*)malloc(sizeof(slist_data_t));
        if(data == NULL) {
            syslog(LOG_ERR, "Unable to get data for slist entry: %s", strerror(errno));
            exit(EXIT_FAILURE);
        }
        memset((void*)&data->thread_data, 0x0, sizeof(thread_data_t));
        
        // log new connection
        if (inet_ntop(AF_INET, (const void *)&client_addr.sin_addr, data->thread_data.client_ip, INET_ADDRSTRLEN) == NULL)
        {
            syslog(LOG_ERR, "Error getting IP string: %s", strerror(errno));
            return -1;
        }
        syslog(LOG_INFO, "Accepted connection from %s", data->thread_data.client_ip);

        data->thread_data.client_sock = client_sock;
        data->thread_data.mutex = &file_mutex;
        data->thread_data.stop_thread = &stop_threads;

        if(pthread_create(&(data->thread_data.id), NULL, &handle_connection, (void*)&data->thread_data) < 0) {
            syslog(LOG_ERR, "Error creating client thread");
            exit(EXIT_FAILURE);
        }

        SLIST_INSERT_HEAD(&list, data, entries);
    }
    
    // TODO: check all threads if joinable and join

    return 0;
}

void join_all_threads(void)
{
    // timer thread
    thread_data_t *data;
    pthread_join(timer_thread, (void**)&data);

    if(data != NULL)
        free(data);
    
    slist_data_t *e = NULL;
    SLIST_FOREACH(e, &list, entries) {
        if(e != NULL) {
            pthread_join(e->thread_data.id, NULL);
        }
    }
    
    // delete list entries
    while (!SLIST_EMPTY(&list)) {
       e = SLIST_FIRST(&list);
       SLIST_REMOVE_HEAD(&list, entries);
       free(e);
   }
}

void shutdown_server(void)
{
    stop_threads = true;

    join_all_threads();

    // close server socket
    if (srv_sock >= 0)
        close(srv_sock);

    // delete file
    remove(DATAFILE);
}

static void* handle_connection(void *data)
{
    thread_data_t *thread_data = (thread_data_t*)data;
    char *line_buf = NULL;
    uint32_t cur_buf_len = 0;
    
    // set receive timeout on socket to avoid blocking infinitely
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 10000;
    if(setsockopt(thread_data->client_sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv) < 0) {
        syslog(LOG_ERR, "Error setting socket option: %s", strerror(errno));
        goto clean;
    }
    
    while(!(*thread_data->stop_thread)) {

        char local_buf[LOCAL_LINE_BUF_SIZE + 1];
        memset(&local_buf[0], 0x0, LOCAL_LINE_BUF_SIZE + 1);

        ssize_t recv_len = recv(thread_data->client_sock, &local_buf[0], LOCAL_LINE_BUF_SIZE, 0);

        if (recv_len < 0)
        {
            if(errno != EAGAIN && errno != EWOULDBLOCK) {
                syslog(LOG_ERR, "Error on recv call: %s", strerror(errno));
                break;
            }
            
            // timeout received
            continue;
        } else if (recv_len == 0) {
            syslog(LOG_INFO, "Closed connection from %s", thread_data->client_ip);
            break;
        }

        for (uint32_t local_start_pos = 0; local_start_pos < recv_len; ++local_start_pos)
        {
            // check for \n
            uint32_t npos;
            for (npos = 0; npos < recv_len; ++npos)
                if (local_buf[npos] == '\n')
                    break;

            // resize line buffer
            line_buf = (char *)realloc(line_buf, cur_buf_len + npos + 2);

            if (line_buf == NULL)
            {
                syslog(LOG_ERR, "Error re-allocating memory: %s", strerror(errno));
                goto clean;
            }

            // zero newly allocated memory
            memset(&line_buf[cur_buf_len], 0x0, npos + 2);

            // copy line until \n inclusive
            memcpy(&line_buf[cur_buf_len], local_buf, npos + 1);

            // only if \n has been found, write to file and return
            if (npos < recv_len)
            {
                //if(pthread_mutex_lock(thread_data->mutex) < 0) {
                    //syslog(LOG_ERR, "Error locking mutex");
                    //goto clean;
                //}

                write_line_to_file(thread_data->mutex, line_buf);
                send_all_lines(thread_data->mutex, thread_data->client_sock);
                
                //pthread_mutex_unlock(thread_data->mutex);

                // reset the buffer
                free(line_buf);
                line_buf = NULL;
                cur_buf_len = 0;
            }

            local_start_pos += npos;
        }
    }

clean:
    if(thread_data->client_sock >= 0)
        close(thread_data->client_sock);

    return NULL;
}

static void write_line_to_file(pthread_mutex_t *mutex, const char *const line)
{
    if(pthread_mutex_lock(mutex) < 0) {
        syslog(LOG_ERR, "Error locking mutex");
        exit(EXIT_FAILURE);
    }

    // open the socketdata file
    FILE *fp = fopen(DATAFILE, "a+");
    if (fp == NULL)
    {
        syslog(LOG_ERR, "Error opening file: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (fputs(line, fp) < 0)
    {
        syslog(LOG_ERR, "Eror writing to file: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    fclose(fp);

    pthread_mutex_unlock(mutex);
}

static void send_all_lines(pthread_mutex_t *mutex, const int sock)
{
    char *line = NULL;
    size_t len = 0;

    if(pthread_mutex_lock(mutex) < 0) {
        syslog(LOG_ERR, "Error locking mutex");
        exit(EXIT_FAILURE);
    }

    // open the socketdata file
    FILE *fp = fopen(DATAFILE, "r");

    if (fp == NULL)
    {
        syslog(LOG_ERR, "Error opening file: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    while (getline(&line, &len, fp) > 0)
    {
        if (send(sock, line, strlen(line), 0) < 0)
        {
            syslog(LOG_ERR, "Error sending line to client: %s", strerror(errno));
            exit(EXIT_FAILURE);
        }

        free(line);

        line = NULL;
        len = 0;
    }

    if(line != NULL)
        free(line);

    fclose(fp);

    pthread_mutex_unlock(mutex);
}

static void *log_timestamp(void *data)
{

    thread_data_t *thread_data = (thread_data_t*)data;
    char time_string[TIME_FORMAT_BUF_SIZE];
    struct timespec ts;

    while(!(*thread_data->stop_thread)) {

        memset(time_string, 0x0, TIME_FORMAT_BUF_SIZE);

        // get time and set target
        if(clock_gettime(CLOCK_MONOTONIC, &ts) < 0) {
            syslog(LOG_ERR, "Error getting time: %s", strerror(errno));
            break;
        }
        
        ts.tv_sec += 10;
        
        // get the timestamp
        time_t t;
        struct tm *t_s;

        time(&t);
        t_s = localtime(&t);
        
        // format the time
        (void)strftime(time_string, TIME_FORMAT_BUF_SIZE, "timestamp: %a, %d %b %Y %T %z\n", t_s);
        
        // write timestamp to file
        //if(pthread_mutex_lock(&file_mutex) != 0) {
            //syslog(LOG_ERR, "Error locking mutex for time log");
            //break;
        //}
        write_line_to_file(thread_data->mutex, time_string);
        //pthread_mutex_unlock(&file_mutex);
        
        if(clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &ts, NULL) != 0) {
            syslog(LOG_ERR, "Error on clock_nanosleep");
            break;
        }
    }
    
    return thread_data;
}