int cmd_help(char **args, size_t num_args, FILE *out)
{
    (void) args;
    (void) num_args;

    fprintf(out, "available commands:\n");
    for (size_t i = 0; i < ARRAY_SIZE(Commands); i++) {
        fprintf(out, "  %s\n", Commands[i].help);
    }
    return 0;
}
