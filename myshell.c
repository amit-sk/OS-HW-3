#include <stdbool.h>
#include <string.h>

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

int prepare(void)
{
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
    // first detect special operations if there are any.
    // assumption: a command line will contain at most one type of special operation.
    if (is_piping_command(count, arglist)) {
        // handle piping commands
    }
    else if (is_background_command(count, arglist)) {
        // handle background commands
    }
    else if (is_input_redirection_command(count, arglist)) {
        // handle input redirection commands
    }
    else if (is_output_redirection_command(count, arglist)) {
        // handle output redirection commands
    }
    else {
        // handle simple commands
    }

    return PROC_ARGLIST_CONTINUE;
}

int finalize(void)
{
    return GENERAL_SUCCESS;
}
