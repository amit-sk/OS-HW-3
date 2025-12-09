#define _POSIX_C_SOURCE 200809L

#include <stdbool.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#define GENERAL_SUCCESS (0)
#define GENERAL_FAILURE (-1)
#define PROC_ARGLIST_CONTINUE (1)
#define PROC_ARGLIST_STOP (0)

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

int run_command(int count, char** arglist, bool is_foreground)
{
    int return_code = GENERAL_FAILURE;
    int child_exit_code = -1;
    pid_t pid = fork();
    if (-1 == pid) {
        perror("fork failed");
        goto cleanup;
    } else if (0 == pid) {
        // child process
        if (is_foreground) {
            // Foreground child processes should terminate upon SIGINT.
            if (SIG_ERR == signal(SIGINT, SIG_DFL)) {  // restore default behavior for SIGINT before execvp.
                // this does not cause the shell (parent process) to exit, only the child process.
                perror("signal failed");
                exit(1);
            }
        }
        if (-1 == execvp(arglist[0], arglist)) {
            // should not return here unless error.
            // this does not cause the shell (parent process) to exit, only the child process.
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
        if (run_command(count, arglist, false) != GENERAL_SUCCESS) {
            goto cleanup;
        }
    } else if (is_input_redirection_command(count, arglist)) {
        // handle input redirection commands
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
