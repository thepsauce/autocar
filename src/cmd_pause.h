int cmd_pause(char **args, size_t num_args, FILE *out)
{
    (void) args;
    (void) num_args;
    (void) out;

    CliWantsPause = !CliWantsPause;
    return 0;
}

