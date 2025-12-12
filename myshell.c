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

void sigchld_handler(int signum)
{
    // wait for any child process to prevent zombies - best effort.
    while (waitpid(-1, NULL, WNOHANG) > 0) {}
}

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

int count_pipes(int count, char** arglist)
{
    int pipe_count = 0;
    for (int i = 0; i < count; ++i) {
        if (strcmp(arglist[i], "|") == 0) {
            pipe_count++;
        }
    }
    return pipe_count;
}

void set_pipes_to_null(int count, char** arglist)
{
    for (int i = 0; i < count; ++i) {
        if (strcmp(arglist[i], "|") == 0) {
            arglist[i] = NULL;
        }
    }
}

/*
 * Assumes input (arglist) is valid - the last two arguments are "<" and a filename.
*/
int input_redirection_preparation_handler(int count, char** arglist)
{
    int return_code = GENERAL_FAILURE;
    int fd = -1;

    fd = open(arglist[count - 1], O_RDONLY, (S_IRUSR | S_IWUSR));
    if (-1 == fd) {
        perror("open failed");
        goto cleanup;
    }

    // setting STDIN in the child process to be the file. This does not affect the parent process.
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

/*
 * Assumes input (arglist) is valid - the last two arguments are ">" and a filename.
*/
int output_redirection_preparation_handler(int count, char** arglist)
{
    int return_code = GENERAL_FAILURE;
    int fd = -1;

    fd = open(arglist[count - 1], (O_RDWR | O_CREAT | O_TRUNC), (S_IRUSR | S_IWUSR));
    if (-1 == fd) {
        perror("open failed");
        goto cleanup;
    }

    // setting STDOUT in the child process to be the file. This does not affect the parent process.
    if (-1 == dup2(fd, STDOUT_FILENO)) {
        perror("dup2 failed");
        goto cleanup;
    }

    arglist[count - 2] = NULL; // remove the ">" and filename from arglist for execvp.

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
    pid_t pid = fork();
    if (-1 == pid) {
        perror("fork failed");
        goto cleanup;
    } else if (0 == pid) {
        // child process
        // on errors, the child process calls exit. this does not cause the shell (parent process) to exit, only the child process.

        if (SIG_ERR == signal(SIGCHLD, SIG_DFL)) {  // restore default behavior for SIGCHLD before execvp.
            perror("signal failed");
            exit(1);
        }

        if (is_foreground) {
            // Foreground child processes should terminate upon SIGINT.
            if (SIG_ERR == signal(SIGINT, SIG_DFL)) {  // restore default behavior for SIGINT before execvp.
                perror("signal failed");
                exit(1);
            }
        }

        if (NULL != preparation_handler) {
            // call child handler for preprocessing (for redirections)
            if (GENERAL_SUCCESS != preparation_handler(count, arglist)) {
                fprintf(stderr, "Error: preparation handler failed.\n");
                exit(1);
            }
        }

        if (-1 == execvp(arglist[0], arglist)) {   // should not return from here unless error.
            perror("execvp failed");
            exit(1);
        }
        // should not reach here. just to verify child always exists.
        fprintf(stderr, "Error: execvp unknown behaviour.\n");
        exit(1);
    } else {
        // parent process
        if (is_foreground) {
            // ECHILD and EINTR are not considered an actual error that requires exiting the shell.
            if ((-1 == waitpid(pid, NULL, 0)) && (errno != ECHILD) && (errno != EINTR)) {
                perror("waitpid failed");
                goto cleanup;
            }
            // not checking child status, assuming child prints and handles its own errors.
        } else {
            waitpid(-1, NULL, WNOHANG); // reap any zombie processes that are already done - best effort.
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

int run_output_redirection_command(int count, char** arglist)
{
    // A command line will contain at most one type of special operation, so output redirection is always a foreground command.
    return run_command_internal(count, arglist, true, output_redirection_preparation_handler);
}

int run_piped_commands(int count, char** arglist)
{
    int return_code = GENERAL_FAILURE;
    int arglist_index = 0;
    int pipe_from_prev[2] = { -1, -1 };
    int pipe_to_next[2] = { -1, -1 };
    int pids[10] = { 0 };
    int pipe_count = count_pipes(count, arglist);
    set_pipes_to_null(count, arglist);

    if (9 < pipe_count) {
        fprintf(stderr, "Error: too many pipes (maximum allowed is 10 commands).\n");
        // drop the pipeline command and continue to the next one.
        return_code = GENERAL_SUCCESS;  // not considered a shell (parent process) failure.
        goto cleanup;
    }

    // run commands concurrently in a pipeline
    for (int i = 0; i <= pipe_count; i++) {
        // for each command pair inthe pipeline
        if ((i < pipe_count) && (-1 == pipe(pipe_to_next))) {
            perror("pipe failed");
            goto cleanup;
        }

        pid_t pid = fork();
        if (-1 == pid) {
            perror("fork failed");
            goto cleanup;
        } else if (0 == pid) {
            // child process
            // on errors, the child process calls exit. this does not cause the shell (parent process) to exit, only the child process.
            
            // close read end of pipe to next, this child only writes to it.
            // (writing end of pipe from prev is expected to be closed by the parent before fork)
            if (-1 != pipe_to_next[0]) {
                close(pipe_to_next[0]);
                pipe_to_next[0] = -1;
            }

            if (SIG_ERR == signal(SIGCHLD, SIG_DFL)) {  // restore default behavior for SIGCHLD before execvp.
                perror("signal failed");
                exit(1);
            }

            // Foreground child processes should terminate upon SIGINT.
            if (SIG_ERR == signal(SIGINT, SIG_DFL)) {  // restore default behavior for SIGINT before execvp.
                perror("signal failed");
                exit(1);
            }
            
            if (i > 0) {
                // not the first command - set stdin to be the read end of the previous pipe
                if (-1 == dup2(pipe_from_prev[0], STDIN_FILENO)) {
                    perror("dup2 failed");
                    exit(1);
                }

                close(pipe_from_prev[0]); // close after dup, best effort.
                pipe_from_prev[0] = -1;
            }
            if (i < pipe_count) {
                // not the last command - set stdout to be the write end of the next pipe
                if (-1 == dup2(pipe_to_next[1], STDOUT_FILENO)) {
                    perror("dup2 failed");
                    exit(1);
                }

                close(pipe_to_next[1]); // close after dup, best effort.
                pipe_to_next[1] = -1;
            }
 
            if (-1 == execvp(arglist[arglist_index], &arglist[arglist_index])) {
                // should not return here unless error.
                perror("execvp failed");
                exit(1);
            }
        } else {
            // parent process

            pids[i] = pid;  // save all pids to wait for them later.

            // close pipe ends in parent. mark as closed.
            if (-1 != pipe_from_prev[0]) {
                close(pipe_from_prev[0]);
                pipe_from_prev[0] = -1;
            }
            if (-1 != pipe_from_prev[1]) {
                close(pipe_from_prev[1]);
                pipe_from_prev[1] = -1;
            }

            // close writing end of previous pipe
            if (-1 != pipe_to_next[1]) {
                close(pipe_to_next[1]);
                pipe_to_next[1] = -1;
            }

            // move up the pipeline. reading end inherited by the next child process.
            pipe_from_prev[0] = pipe_to_next[0];
            pipe_to_next[0] = -1;

            // progress arglist to the next command (unless on last command)
            while (arglist[arglist_index] != NULL) {
                arglist_index++;
            }
            arglist_index++; // skip the NULL
        }
    }

    for (int i = 0; i <= pipe_count; i++) {
        // wait for all child processes to complete

        // ECHILD and EINTR are not considered an actual error that requires exiting the shell.
        if ((-1 == waitpid(pids[i], NULL, 0)) && (errno != ECHILD) && (errno != EINTR)) {
            perror("waitpid failed");
            goto cleanup;
        }
    }

    return_code = GENERAL_SUCCESS;
cleanup:
    return return_code;
}

int prepare(void)
{
    struct sigaction sa = {0};
    sa.sa_handler = sigchld_handler;
    sa.sa_flags = SA_RESTART;

    if (SIG_ERR == signal(SIGINT, SIG_IGN)) {  // the parent (shell) should not terminate upon SIGINT.
        perror("signal failed");
        return GENERAL_FAILURE;
    }

    if (-1 == sigaction(SIGCHLD, &sa, NULL)) { // set handler for SIGCHLD to prevent zombies.
        perror("sigaction failed");
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
        if (GENERAL_SUCCESS != run_piped_commands(count, arglist)) {
            goto cleanup;
        }
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
        if (GENERAL_SUCCESS != run_output_redirection_command(count, arglist)) {
            goto cleanup;
        }
    } else {
        if (GENERAL_SUCCESS != run_command(count, arglist, true)) {
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
    signal(SIGCHLD, SIG_DFL); // restore default behavior for SIGCHLD - best effort, doesn't check for errors.
    return GENERAL_SUCCESS;
}
