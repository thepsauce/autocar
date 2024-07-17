int cmd_source(char **args, size_t num_args, FILE *out)
{
    glob_t g;

    (void) out;

    for (size_t i = 0; i < num_args; i++) {
        switch (glob(args[i], GLOB_TILDE | GLOB_BRACE, NULL, &g)) {
        case 0:
            for (size_t p = 0; p < g.gl_pathc; p++) {
                if (source_path(g.gl_pathv[p]) != 0) {
                    printf("failed sourcing: '%s'\n", g.gl_pathv[p]);
                    break;
                }
            }
            globfree(&g);
            break;
        case GLOB_NOMATCH:
            printf("no matches found for: '%s'\n", args[i]);
            return -1;
        default:
            printf("glob() error\n");
            return -1;
        }
    }
    return 0;
}
