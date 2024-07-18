int cmd_run(char **args, size_t num_args, FILE *out)
{
    int result = 0;
    struct file *file;
    bool has_exec = false;
    char **exec_args;

    (void) out;

    if (num_args == 0) {
        pthread_mutex_lock(&Files.lock);
        for (size_t i = 0; i < Files.num; i++) {
            file = Files.ptr[i];
            if (file->type != EXT_TYPE_EXECUTABLE) {
                continue;
            }
            has_exec = true;
        }
        if (has_exec) {
            printf("choose an executable:\n");
            for (size_t i = 0; i < Files.num; i++) {
                file = Files.ptr[i];
                if (file->type != EXT_TYPE_EXECUTABLE) {
                    continue;
                }
                printf("(%zu) %s\n", i + 1, file->path);
            }
        } else {
            printf("(no executables)\n");
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
            exec_args = sreallocarray(NULL, num_args + 1, sizeof(*exec_args));
            for (size_t i = 0; i < num_args; i++) {
                exec_args[i] = args[i];
            }
            exec_args[num_args] = NULL;
            result = run_executable(exec_args, NULL, NULL);
            free(exec_args);
            file->flags &= ~FLAG_IS_FRESH;
            pthread_mutex_unlock(&Files.lock);
        }
    }
    return result;
}

