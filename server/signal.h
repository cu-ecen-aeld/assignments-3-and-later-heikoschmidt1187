/*
 * Acts as server for the aesd
 * Author: Heiko Schmidt
 */
#include <stdbool.h>

extern void signal_handler(const int signum);
extern bool is_app_running(void);
extern int register_sighandler(void);