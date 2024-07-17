int cmd_delete(char **args, size_t num_args, FILE *out)
{
    struct file *file;

    (void) out;

    pthread_mutex_lock(&Files.lock);
    for (size_t i = 0; i < num_args; i++) {
        for (size_t f = 0; f < Files.num; ) {
            file = Files.ptr[f];
            if (fnmatch(args[i], file->path, 0) == 0) {
                free(file->path);
                free(file);
                Files.num--;
                memmove(&Files.ptr[f], &Files.ptr[f + 1],
                        sizeof(*Files.ptr) * (Files.num - f));
            } else {
                f++;
            }
        }
    }
    pthread_mutex_unlock(&Files.lock);
    return 0;
}
