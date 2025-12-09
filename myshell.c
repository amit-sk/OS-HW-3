#define _POSIX_C_SOURCE 200809L

#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <fcntl.h>

#define GENERAL_SUCCESS (0)
#define GENERAL_FAILURE (-1)
#define PROC_ARGLIST_CONTINUE (1)
#define PROC_ARGLIST_STOP (0)

typedef int (*cmd_preparation_handler_t)(int, char**);

bool is_piping_command(int count, char** arglist)
{
    // If a command line contains the | symbol, then it may appear multiple times. detect one such appearance.
    for (int i = 0; i < count; ++i) {
        if (strcmp(arglist[i], "|") == 0) {
            return true;
        }
    }
    return false;
}

bool is_background_command(int count, char** arglist)
{
    // If a command line contains the & symbol, then it is the last word of the command line.
    if (strcmp(arglist[count - 1], "&") == 0) {
        return true;
    }
    return false;
}

bool is_input_redirection_command(int count, char** arglist)
{
    // If a command line contains the < symbol, then it appears one before last on the command line.
    if ((count >= 2) && (strcmp(arglist[count - 2], "<") == 0)) {
        return true;
    }
    return false;
}

bool is_output_redirection_command(int count, char** arglist)
{
    // If a command line contains the > symbol, then it appears one before last on the command line.
    if ((count >= 2) && (strcmp(arglist[count - 2], ">") == 0)) {
        return true;
    }
    return false;
}

/*
 * Assumes input (arglist) is valid - the last two arguments are "<" and a filename.
*/
int input_redirection_preparation_handler(int count, char** arglist) {
    int return_code = GENERAL_FAILURE;
    int fd = -1;

    fd = open(arglist[count - 1], O_RDONLY, (S_IRUSR | S_IWUSR));
    if (-1 == fd) {
        perror("open failed");
        goto cleanup;
    }

    // setting STDIN in child process to be the file. This does not affect the parent process.
    if (-1 == dup2(fd, STDIN_FILENO)) {
        perror("dup2 failed");
        goto cleanup;
    }

    arglist[count - 2] = NULL; // remove the "<" and filename from arglist for execvp.

    return_code = GENERAL_SUCCESS;
cleanup:
    if (-1 != fd) {
        close(fd);
    }
    return return_code;
}

int run_command_internal(int count, char** arglist, bool is_foreground, cmd_preparation_handler_t preparation_handler)
{
    int return_code = GENERAL_FAILURE;
    int child_exit_code = -1;
    pid_t pid = fork();
    if (-1 == pid) {
        perror("fork failed");
        goto cleanup;
    } else if (0 == pid) {
        // child process
        // on errors, the child process calls exit. this does not cause the shell (parent process) to exit, only the child process.
        if (is_foreground) {
            // Foreground child processes should terminate upon SIGINT.
            if (SIG_ERR == signal(SIGINT, SIG_DFL)) {  // restore default behavior for SIGINT before execvp.
                perror("signal failed");
                exit(1);
            }
        }
        if (NULL != preparation_handler) {
            // call child handler for preprocessing (for redirections, pipes...)
            if (GENERAL_SUCCESS != preparation_handler(count, arglist)) {
                exit(1);
            }
        }
        if (-1 == execvp(arglist[0], arglist)) {
            // should not return here unless error.
            perror("execvp failed");
            exit(1);
        }
    } else {
        // parent process
        if (is_foreground) {
            // ECHILD and EINTR are not considered an actual error that requires exiting the shell.
            if ((-1 == waitpid(pid, &child_exit_code, 0)) && (errno != ECHILD) && (errno != EINTR)) {
                perror("waitpid failed");
                goto cleanup;
            }
            // do we even care about the child's exit code?
        }
    }

    return_code = GENERAL_SUCCESS;
cleanup:
    return return_code;
}

int run_command(int count, char** arglist, bool is_foreground)
{
    return run_command_internal(count, arglist, is_foreground, NULL);
}

int run_input_redirection_command(int count, char** arglist)
{
    // A command line will contain at most one type of special operation, so input redirection is always a foreground command.
    return run_command_internal(count, arglist, true, input_redirection_preparation_handler);
}

int prepare(void)
{
    if (SIG_ERR == signal(SIGINT, SIG_IGN)) {  // the parent (shell) should not terminate upon SIGINT.
        perror("signal failed");
        return GENERAL_FAILURE;
    }
    return GENERAL_SUCCESS;
}

/*
 * This function receives a null-terminated array arglist with count non-NULL words. This array contains the parsed command line.
 * The function executes the command(s) specified in arglist, and waits for their completion if they are foreground commands.
 * It assumes count >= 1 and arglist valid.
 * This function does not return until every foreground child process it created exits.
 * returns 1 if should continue, 0 otherwise (error).
*/
int process_arglist(int count, char** arglist)
{
    int return_value = PROC_ARGLIST_STOP;

    // first detect special operations if there are any.
    // assumption: a command line will contain at most one type of special operation.
    if (is_piping_command(count, arglist)) {
        // handle piping commands
    } else if (is_background_command(count, arglist)) {
        arglist[count - 1] = NULL; // Do not pass the & argument to execvp().
        if (GENERAL_SUCCESS != run_command(count, arglist, false)) {
            goto cleanup;
        }
    } else if (is_input_redirection_command(count, arglist)) {
        if (GENERAL_SUCCESS != run_input_redirection_command(count, arglist)) {
            goto cleanup;
        }
    } else if (is_output_redirection_command(count, arglist)) {
        // handle output redirection commands
    } else {
        if (run_command(count, arglist, true) != GENERAL_SUCCESS) {
            goto cleanup;
        }
    }

    return_value = PROC_ARGLIST_CONTINUE;
cleanup:
    return return_value;
}

int finalize(void)
{
    signal(SIGINT, SIG_DFL); // restore default behavior for SIGINT - best effort, doesn't check for errors.
    return GENERAL_SUCCESS;
}
