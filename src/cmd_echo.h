int cmd_echo(char **args, size_t num_args, FILE *out)
{
    for (size_t i = 0; i < num_args; i++) {
        if (i > 0) {
            fputc(' ', out);
        }
        fprintf(out, "%s", args[i]);
    }
    fputc('\n', out);
    return 0;
}
