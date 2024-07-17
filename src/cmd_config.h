int cmd_config(char **args, size_t num_args, FILE *out)
{
    (void) args;
    (void) num_args;

    dump_conf(out);
    return 0;
}
