int cmd_build(char **args, size_t num_args, FILE *out)
{
    (void) out;

    if (num_args == 1) {
        if (strcmp(args[0], "-c") == 0 || strcmp(args[0], "--collect") == 0) {
            DLOG("build wants to collect files\n");
            if (collect_files() != 0) {
                return -1;
            }
        } else {
            goto invalid_arg;
        }
    } else if (num_args > 1) {
        goto invalid_arg;
    }
    return build_objects() && link_executables() ? 0 : -1;

invalid_arg:
    printf("invalid arguments, try: `help build`\n");
    return -1;
}
