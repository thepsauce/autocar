int cmd_generate(char **args, size_t num_args, FILE *out)
{
    static const struct {
        const char *name;
        int (*generate)(FILE *fp);
    } generators[] = {
        { "shell", generate_shell_script },
        { "make", generate_make_file },
    };
    size_t gen;
    size_t pref;

    if (num_args != 1) {
        printf("need one argument for 'generate' (shell or make)\n");
        return -1;
    }
    for (gen = 0; gen < ARRAY_SIZE(generators); gen++) {
        for (pref = 0; args[0][pref] != '\0'; pref++) {
            if (generators[gen].name[pref] != tolower(args[0][pref])) {
                break;
            }
        }
        if (args[0][pref] == '\0') {
            break;
        }
    }
    if (gen == ARRAY_SIZE(generators)) {
        printf("invalid argument for 'generate',"
                " expected 'shell' or 'make'\n");
        return -1;
    }
    return generators[gen].generate(out);
}
