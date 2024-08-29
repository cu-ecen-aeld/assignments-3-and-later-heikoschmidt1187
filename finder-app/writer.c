/*
 * Writes the given string to a given file
 * Author: Heiko Schmidt
 */
#include <stdlib.h>
#include <stdio.h>
#include <syslog.h>
#include <string.h>
#include <errno.h>

int main(int argc, char **argv)
{
    int ret = 0;
    FILE *f = NULL;

    openlog(NULL, 0, LOG_USER);
    
    // ensure arguments are there
    if (argc < 3) {
        syslog(LOG_ERR, "Please provide path and string as parameters");
        ret = 1;
        goto cleanup;
    }

    // debug parameter output to syslog
    syslog(LOG_DEBUG, "Writing %s to %s", argv[2], argv[1]);

    // open the file
    f = fopen(argv[1], "w");
    if (f == NULL) {
        syslog(LOG_ERR, "Error opening file: %s", strerror(errno));
        ret = 1;
        goto cleanup;
    }

    // write to file
    if(fprintf(f, "%s", argv[2]) < 0) {
        syslog(LOG_ERR, "Error writing to file: %s", strerror(errno));
        ret = 1;
        goto cleanup;
    }

    // close the file
    if(fclose(f) != 0) {
        syslog(LOG_ERR, "Error closing file: %s", strerror(errno));
        ret = 1;
    }

cleanup:
    closelog();
    return ret;
}