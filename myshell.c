
int prepare(void)
{
    return 0;
}

int process_arglist(int count, char** arglist)
{
    // this function does not return until every foreground child process it created exits.
    // returns 1 if should continue, 0 otherwise (error).
    return 1;
}

int finalize(void)
{
    return 0;
}
