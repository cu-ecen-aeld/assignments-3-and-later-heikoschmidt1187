#include "systemcalls.h"

#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>

/**
 * @param cmd the command to execute with system()
 * @return true if the command in @param cmd was executed
 *   successfully using the system() call, false if an error occurred,
 *   either in invocation of the system() call, or if a non-zero return
 *   value was returned by the command issued in @param cmd.
*/
bool do_system(const char *cmd)
{

/*
 * TODO  add your code here
 *  Call the system() function with the command set in the cmd
 *   and return a boolean true if the system() call completed with success
 *   or false() if it returned a failure
*/

    int ret = system(cmd);

    // if cmd == NULL, the return value indicates if a shell is available
    // handle "no shell" as error
    return cmd == NULL ? ret > 0 : ret == 0;
}

bool do_exec_internal(const char * outfile, int count, va_list args)
{
    char * command[count+1];
    int i;
    bool ret = false;

    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;
    // this line is to avoid a compile warning before your implementation is complete
    // and may be removed
    //command[count] = command[count];

    // create child process that will handle execv
    fflush(stdout);
    pid_t pid = fork();
    int child_ret;

    if(pid < 0) {
        // error occured
        goto out;

    } else if (pid == 0) {
        if(outfile != NULL) {
            // get file descriptor for the output file to redirect standard output
            int fd = open(outfile, O_WRONLY | O_TRUNC | O_CREAT, S_IWUSR | S_IRUSR | S_IRGRP | S_IROTH);
            if (fd < 0)
                goto out;

            // redirect standard output
            if(dup2(fd, 1) < 0) {
                close(fd);
                goto out;
            }
            close(fd);
        }

        // this is the child
        if(count <= 0)
            goto out;

        if(execv(command[0], command) < 0)
            exit(EXIT_FAILURE); 

    } else {
        // this is the parent
        if(wait(&child_ret) < 0)
            goto out;

        ret = WEXITSTATUS(child_ret) == 0 ? true : false;
        goto out;
    }

out:
    return ret;
}

/**
* @param count -The numbers of variables passed to the function. The variables are command to execute.
*   followed by arguments to pass to the command
*   Since exec() does not perform path expansion, the command to execute needs
*   to be an absolute path.
* @param ... - A list of 1 or more arguments after the @param count argument.
*   The first is always the full path to the command to execute with execv()
*   The remaining arguments are a list of arguments to pass to the command in execv()
* @return true if the command @param ... with arguments @param arguments were executed successfully
*   using the execv() call, false if an error occurred, either in invocation of the
*   fork, waitpid, or execv() command, or if a non-zero return value was returned
*   by the command issued in @param arguments with the specified arguments.
*/

bool do_exec(int count, ...)
{
    va_list args;
    va_start(args, count);

/*
 * TODO:
 *   Execute a system command by calling fork, execv(),
 *   and wait instead of system (see LSP page 161).
 *   Use the command[0] as the full path to the command to execute
 *   (first argument to execv), and use the remaining arguments
 *   as second argument to the execv() command.
 *
*/

    // do exec
    bool ret = do_exec_internal(NULL, count, args);
    va_end(args);

    return ret;
}

/**
* @param outputfile - The full path to the file to write with command output.
*   This file will be closed at completion of the function call.
* All other parameters, see do_exec above
*/
bool do_exec_redirect(const char *outputfile, int count, ...)
{
    va_list args;
    va_start(args, count);

/*
 * TODO
 *   Call execv, but first using https://stackoverflow.com/a/13784315/1446624 as a refernce,
 *   redirect standard out to a file specified by outputfile.
 *   The rest of the behaviour is same as do_exec()
 *
*/
    // do exec
    bool ret = do_exec_internal(outputfile, count, args);
    va_end(args);

    return ret;
}
