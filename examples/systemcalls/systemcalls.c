#include "systemcalls.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

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
 *  Call the system() function with the command set in the cmd
 *   and return a boolean true if the system() call completed with success
 *   or false() if it returned a failure
*/

    return system(cmd) == 0;
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
    pid_t pid = 0;
    pid_t wait_pid = 0;
    int execv_return = 0;
    int wstatus = 0;
    bool return_val = false;

    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;

/*
 *   Execute a system command by calling fork, execv(),
 *   and wait instead of system (see LSP page 161).
 *   Use the command[0] as the full path to the command to execute
 *   (first argument to execv), and use the remaining arguments
 *   as second argument to the execv() command.
 *
*/
    switch (pid = fork()) {
    case -1:
        // fork failed
        return_val = false;
        perror("fork");
        break;
    case 0:
        // kid
        execv_return = execv(command[0], &command[1]);
        return_val = execv_return != -1;
        if (!return_val) {
            perror(command[0]);
        }
        break;
    
    default:
        // parent
        wait_pid = waitpid(pid, &wstatus, 0);

        // use MACROS to make sure that the wait worked
        return_val = WIFEXITED(wstatus);
        if (!return_val) {
            perror("Child did not exit succesfully");
        }
        else {
            return_val &= wait_pid == pid;
        }
    }

    va_end(args);

    return return_val;
}

/**
* @param outputfile - The full path to the file to write with command output.
*   This file will be closed at completion of the function call.
* @param ... - A list of 1 or more arguments after the @param count argument.
*   The first is always the full path to the command to execute with execv()
*   The remaining arguments are a list of arguments to pass to the command in execv()
* @return true if the command @param ... with arguments @param arguments were executed successfully
*   using the execv() call, false if an error occurred, either in invocation of the
*   fork, waitpid, or execv() command, or if a non-zero return value was returned
*   by the command issued in @param arguments with the specified arguments.
*/
bool do_exec_redirect(const char *outputfile, int count, ...)
{
    pid_t pid = 0;
    pid_t wait_pid = 0;
    int execv_return = 0;
    int wstatus = 0;
    int fd = 0;

    bool return_val = false;
    va_list args;
    va_start(args, count);
    char * command[count+1];
    int i;
    for(i=0; i<count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;


/*
 * TODO
 *   Call execv, but first using https://stackoverflow.com/a/13784315/1446624 as a refernce,
 *   redirect standard out to a file specified by outputfile.
 *   The rest of the behaviour is same as do_exec()
 *
*/
    fd = open(outputfile, O_WRONLY|O_TRUNC|O_CREAT, 0644);
    if (fd < 0) {
        return_val = false;
        perror("open");
    }
    else {
        switch (pid = fork()) {
        case -1:
            // fork failed
            return_val = false;
            perror("fork");
            break;
        case 0:
            // kid
            if (dup2(fd, 1) < 0) {
                return_val = false;
            }
            else {
                execv_return = execv(command[0], &command[1]);
                return_val = execv_return != -1;
                if (!return_val) {
                    perror(command[0]);
                }
            }
            close(fd);
            break;
        
        default:
            // parent
            wait_pid = waitpid(pid, &wstatus, 0);

            // use MACROS to make sure that the wait worked
            return_val = WIFEXITED(wstatus);
            if (!return_val) {
                perror("Child did not exit succesfully");
            }
            else {
                return_val &= wait_pid == pid;
            }
        }
    }

    va_end(args);

    return return_val;
}
