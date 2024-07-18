int cmd_list(char **args, size_t num_args, FILE *out)
{
    static const char *ext_strings[EXT_TYPE_MAX] = {
        [EXT_TYPE_SOURCE] = "source",
        [EXT_TYPE_HEADER] = "header",
        [EXT_TYPE_OBJECT] = "object",
        [EXT_TYPE_EXECUTABLE] = "executable",
        [EXT_TYPE_FOLDER] = "folder",
        [EXT_TYPE_OTHER] = "other"
    };

    struct file *file;
    char str_flags[8];

    (void) args;
    (void) num_args;
    (void) out;

    pthread_mutex_lock(&Files.lock);
    if (Files.num == 0) {
        printf("(no files in the file list)\n");
    }
    for (size_t i = 0, f; i < Files.num; i++) {
        file = Files.ptr[i];
        f = 0;
        if (file->flags & FLAG_EXISTS) {
            str_flags[f++] = 'e';
        }
        if (file->flags & FLAG_IS_FRESH) {
            str_flags[f++] = 'f';
        }
        if (file->flags & FLAG_HAS_MAIN) {
            str_flags[f++] = 'm';
        }
        if (file->flags & FLAG_IS_RECURSIVE) {
            str_flags[f++] = 'r';
        }
        if (file->flags & FLAG_IS_TEST) {
            str_flags[f++] = 't';
        }
        str_flags[f] = '\0';
        printf("(%zu) %s [%s] %s\n", i + 1,
                file->path, ext_strings[file->type], str_flags);
    }
    pthread_mutex_unlock(&Files.lock);
    return 0;
}
