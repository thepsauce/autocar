int cmd_add(char **args, size_t num_args, FILE *out)
{
    int result = 0;
    glob_t g;
    int flags;
    bool flag_interp;

    (void) out;

    pthread_mutex_lock(&Files.lock);
    flags = 0;
    flag_interp = true;
    for (size_t i = 0; i < num_args; i++) {
        if (flag_interp) {
            if (strcmp(args[i], "-t") == 0) {
                flags |= FLAG_IS_TEST;
                continue;
            } else if (strcmp(args[i], "-r") == 0) {
                flags |= FLAG_IS_RECURSIVE;
                continue;
            } else if (strcmp(args[i], "-rt") == 0) {
                flags |= FLAG_IS_RECURSIVE | FLAG_IS_TEST;
                continue;
            } else if (strcmp(args[i], "-tr") == 0) {
                flags |= FLAG_IS_TEST | FLAG_IS_RECURSIVE;
                continue;
            } else if (strcmp(args[i], "--") == 0) {
                flag_interp = false;
            }
        }
        switch (glob(args[i], GLOB_TILDE | GLOB_BRACE, NULL, &g)) {
        case 0:
            for (size_t p = 0; p < g.gl_pathc; p++) {
                add_file(g.gl_pathv[p], -1, flags);
            }
            globfree(&g);
            break;
        case GLOB_NOMATCH:
            printf("no matches found for: '%s'\n", args[i]);
            result = -1;
            break;
        default:
            printf("glob() error\n");
            result = -1;
            break;
        }
    }
    pthread_mutex_unlock(&Files.lock);
    return result;
}
