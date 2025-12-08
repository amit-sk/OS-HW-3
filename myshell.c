#define GENERAL_SUCCESS (0)
#define GENERAL_FAILURE (-1)
#define PROC_ARGLIST_CONTINUE (1)
#define PROC_ARGLIST_STOP (0)

int prepare(void)
{
    return GENERAL_SUCCESS;
}

/*
 * This function receives a null-terminated array arglist with count non-NULL words. This array contains the parsed command line.
 * The function executes the command(s) specified in arglist, and waits for their completion if they are foreground commands.
 * This function does not return until every foreground child process it created exits.
 * returns 1 if should continue, 0 otherwise (error).
*/
int process_arglist(int count, char** arglist)
{
    // first detect special operations if there are any.
    // assumption: A command line will contain at most one type of special operation.
    return PROC_ARGLIST_CONTINUE;
}

int finalize(void)
{
    return GENERAL_SUCCESS;
}
