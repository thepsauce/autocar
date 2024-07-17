int cmd_quit(char **args, size_t num_args, FILE *out)
{
    (void) args;
    (void) num_args;
    (void) out;

    CliRunning = false;
    return 0;
}


