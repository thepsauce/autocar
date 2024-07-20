#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/wait.h>

#define ARRAY_SIZE(a) (sizeof(a)/sizeof(*(a)))

const char *CC = "gcc";
const char *C_FLAGS[] = { "-std=gnu99", "-Wall", "-Wextra", "-Werror", "-Wpedantic", "-g", "-fsanitize=address" };
const char *C_LIBS[] = { "-lm", "-lbfd", "-lreadline" };
const char *SOURCES[] = { "src/args.c", "src/cli.c", "src/cmd.c", "src/conf.c", "src/eval.c", "src/file.c", "src/salloc.c", "src/util.c" };
const char *MAIN_SOURCES[] = { "src/main.c", "tests/lol.c" };

const char *OBJECTS[] = { "bulid/src/args.o", "bulid/src/cli.o", "bulid/src/cmd.o", "bulid/src/conf.o", "bulid/src/eval.o", "bulid/src/file.o", "bulid/src/salloc.o", "bulid/src/util.o" };
const char *MAIN_OBJECTS[] = { "bulid/src/main.o", "bulid/tests/lol.o" };

const char *MAIN_EXECUTABLES[] = { "bulid/src/main", "bulid/tests/lol" };

void make_directory(const char *path)
{
    char *cur, *s;
    char p[strlen(path) + 1];

    strcpy(p, path);

    cur = p;
    while (s = strchr(cur, '/'), s != NULL) {
        s[0] = '\0';
        if (mkdir(p, 0755) == -1) {
            if (errno != EEXIST) {
                printf("mkdir '%s': %s\n", p, strerror(errno));
                abort();
            }
        } else {
            printf("mkdir %s\n", p);
        }
        s[0] = '/';
        cur = s + 1;
    }
}

void run_executable(char **args)
{
    int pid;
    int wstatus;

    for (char **a = args; a[0] != NULL; a++) {
        printf("%s ", a[0]);
    }
    printf("\n");

    pid = fork();
    if (pid == -1) {
        printf("fork: %s\n", strerror(errno));
        abort();
    }
    if (pid == 0) {
        if (execvp(args[0], args) < 0) {
            printf("execvp: %s\n", strerror(errno));
            abort();
        }
    } else {
        waitpid(pid, &wstatus, 0);
        if (WEXITSTATUS(wstatus) != 0) {
            printf("`%s` returned: %d\n", args[0], wstatus);
            abort();
        }
    }
}

int main(void)
{
    char *args[1 + ARRAY_SIZE(C_FLAGS) + 1 + ARRAY_SIZE(OBJECTS) + 3 +
        ARRAY_SIZE(C_LIBS) + 1];

    args[0] = (char*) CC;
    for (size_t i = 0; i < ARRAY_SIZE(C_FLAGS); i++) {
        args[1 + i] = (char*) C_FLAGS[i];
    }

    args[1 + ARRAY_SIZE(C_FLAGS)] = (char*) "-c";
    args[3 + ARRAY_SIZE(C_FLAGS)] = (char*) "-o";
    args[5 + ARRAY_SIZE(C_FLAGS)] = (char*) NULL;
    for (size_t i = 0; i < ARRAY_SIZE(SOURCES); i++) {
        args[2 + ARRAY_SIZE(C_FLAGS)] = (char*) SOURCES[i];
        args[4 + ARRAY_SIZE(C_FLAGS)] = (char*) OBJECTS[i];
        make_directory(OBJECTS[i]);
        run_executable(args);
    }

    for (size_t i = 0; i < ARRAY_SIZE(MAIN_SOURCES); i++) {
        args[2 + ARRAY_SIZE(C_FLAGS)] = (char*) MAIN_SOURCES[i];
        args[4 + ARRAY_SIZE(C_FLAGS)] = (char*) MAIN_OBJECTS[i];
        make_directory(MAIN_OBJECTS[i]);
        run_executable(args);
    }

    if (ARRAY_SIZE(MAIN_OBJECTS) == 0) {
        printf("no main objects\n");
        return 0;
    }

    for (size_t i = 0; i < ARRAY_SIZE(OBJECTS); i++) {
        args[1 + ARRAY_SIZE(C_FLAGS) + i] = (char*) OBJECTS[i];
    }
    args[1 + ARRAY_SIZE(C_FLAGS) + ARRAY_SIZE(OBJECTS) + 1] = (char*) "-o";
    for (size_t i = 0; i < ARRAY_SIZE(C_LIBS); i++) {
        args[1 + ARRAY_SIZE(C_FLAGS) + ARRAY_SIZE(OBJECTS) + 3 + i] =
            (char*) C_LIBS[i];
    }
    args[1 + ARRAY_SIZE(C_FLAGS) + ARRAY_SIZE(OBJECTS) + 3 +
        ARRAY_SIZE(C_LIBS)] = NULL;
    for (size_t i = 0; i < ARRAY_SIZE(MAIN_OBJECTS); i++) {
        args[1 + ARRAY_SIZE(C_FLAGS) + ARRAY_SIZE(OBJECTS)] =
            (char*) MAIN_OBJECTS[i];
        args[1 + ARRAY_SIZE(C_FLAGS) + ARRAY_SIZE(OBJECTS) + 2] =
            (char*) MAIN_EXECUTABLES[i];
        run_executable(args);
    }

    puts("run any of the main executables:");
    for (size_t i = 0; i < ARRAY_SIZE(MAIN_EXECUTABLES); i++) {
        puts(MAIN_EXECUTABLES[i]);
    }
    return 0;
}
