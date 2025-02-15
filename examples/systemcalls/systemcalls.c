#include "systemcalls.h"

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
    // system() returns -1 if call fails. return value will depend on what
    // the return value from cmd is. assume 0 is success
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
    bool ret = false;
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
 * TODO:
 *   Execute a system command by calling fork, execv(),
 *   and wait instead of system (see LSP page 161).
 *   Use the command[0] as the full path to the command to execute
 *   (first argument to execv), and use the remaining arguments
 *   as second argument to the execv() command.
 *
*/
    int pid = fork();

    if(pid == -1)
    {
        perror("error invoking fork()");
        ret = false;
    }
    else
    {
        // child
        if(pid == 0)
        {
            execv(command[0], command);
            // only set happens if execv returns, this means it failed
            perror("execv() failure");
            exit(-1);
        }
        // parent
        else
        {
            int status;
            if(wait(&status) != -1)
            {
                // true if child terminated normally and had an exit status of 0 (success)
                ret = WIFEXITED(status) && (WEXITSTATUS(status) == 0);
            }
            else
            {
                ret = false;
            }
        }
    }

    va_end(args);
    printf("ret = %d\n",ret);
    return ret;
}

/**
* @param outputfile - The full path to the file to write with command output.
*   This file will be closed at completion of the function call.
* All other parameters, see do_exec above
*/
bool do_exec_redirect(const char *outputfile, int count, ...)
{
    bool ret = false;
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
    int pid = fork();

    if(pid == -1)
    {
        perror("error invoking fork()");
        ret = false;
    }
    else
    {
        // child
        if(pid == 0)
        {
            int fd = open(outputfile, O_RDWR|O_TRUNC|O_CREAT, 0644);
            if (dup2(fd, STDOUT_FILENO) < 0)
            { 
                perror("dup2 failure");
                ret = false;
            }
            else
            {
                execv(command[0], command);
                // only set if execv return, this means it failed
                perror("execv() failure");
                exit(-1);
            }
        }
        // parent
        else
        {
            int status;
            if(wait(&status) != -1)
            {
                // true if child terminated normally and had an exit status of 0 (success)
                ret = WIFEXITED(status) && (WEXITSTATUS(status) == 0);
            }
            else
            {
                ret = false;
            }

        }
    }

    va_end(args);

    return ret;
}

