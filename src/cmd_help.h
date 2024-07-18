int cmd_help(char **args, size_t num_args, FILE *out)
{
    size_t longest_len, len;
    bool found;

    longest_len = 0;
    for (size_t i = 0; i < ARRAY_SIZE(Commands); i++) {
        len = strlen(Commands[i].name);
        longest_len = MAX(longest_len, len);
    }

    if (num_args == 0) {
        fprintf(out, "available commands:\n");
    }
    for (size_t i = 0; i < ARRAY_SIZE(Commands); i++) {
        if (num_args > 0) {
            found = false;
            for (size_t a = 0; a < num_args; a++) {
                if (strcasecmp(Commands[i].name, args[a]) == 0) {
                    found = true;
                    break;
                }
            }
        } else {
            found = true;
        }
        if (!found) {
            continue;
        }
        len = strlen(Commands[i].name);
        fprintf(out, "%s", Commands[i].name);
        for (; len <= longest_len; len++) {
            fputc(' ', out);
        }
        fputs(Commands[i].args_help, out);
        fprintf(out, "\n  %s\n", Commands[i].desc_help);
    }
    return 0;
}
