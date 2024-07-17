int cmd_help(char **args, size_t num_args, FILE *out)
{
    (void) args;
    (void) num_args;
    (void) out;

    printf("available commands:\n");
    for (size_t i = 0; i < ARRAY_SIZE(Commands); i++) {
        printf("  %s\n", Commands[i].help);
    }
    return 0;
}
