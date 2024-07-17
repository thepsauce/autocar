int cmd_run(char **args, size_t num_args, FILE *out)
{
    int result = 0;
    struct file *file;
    char **exe_args;

    (void) out;

    if (num_args == 0) {
        printf("choose an executable:\n");
        pthread_mutex_lock(&Files.lock);
        for (size_t i = 0; i < Files.num; i++) {
            file = Files.ptr[i];
            if (file->type != EXT_TYPE_EXECUTABLE) {
                continue;
            }
            printf("(%zu) %s\n", i + 1, file->path);
        }
        pthread_mutex_unlock(&Files.lock);
    } else {
        pthread_mutex_lock(&Files.lock);
        file = search_file(args[0], NULL);
        if (file == NULL) {
            printf("'%s' does not exist\n", args[0]);
            pthread_mutex_unlock(&Files.lock);
            result = -1;
        } else if (file->type != EXT_TYPE_EXECUTABLE) {
            printf("'%s' is not an executable\n", file->path);
            pthread_mutex_unlock(&Files.lock);
            result = -1;
        } else {
            exe_args = sreallocarray(NULL, num_args + 1, sizeof(*exe_args));
            for (size_t i = 0; i < num_args; i++) {
                exe_args[i] = args[i];
            }
            exe_args[num_args] = NULL;
            result = run_executable(exe_args, NULL, NULL);
            free(exe_args);
            pthread_mutex_unlock(&Files.lock);
        }
    }
    return result;
}

