#include "args.h"
#include "salloc.h"
#include "macros.h"
#include "file.h"
#include "conf.h"

#include <bfd.h>
#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

struct file_list Files;

static struct file *add_file(const char *path);

static struct file *search_file(const char *name, unsigned flags, size_t *pIndex)
{
    size_t l, r;
    int cmp;

    l = 0;
    r = Files.num;
    while (l < r) {
        const size_t m = (l + r) / 2;

        struct file *const file = Files.ptr[m];
        cmp = strcmp(file->name, name);
        if (cmp == 0) {
            cmp = (int) (file->flags & FILE_TYPE_MASK) -
                (int) (flags & FILE_TYPE_MASK);
        }
        if (cmp == 0) {
            if (pIndex != NULL) {
                *pIndex = m;
            }
            return file;
        }
        if (cmp < 0) {
            l = m + 1;
        } else {
            r = m;
        }
    }
    if (pIndex != NULL) {
        *pIndex = r;
    }
    return NULL;
}

static int get_path_flags(const char *path, char **pname, unsigned *pflags)
{
    const char *ext;
    const char *folder;
    size_t folder_len;
    size_t len;
    char *name;
    unsigned flags;

    ext = strrchr(path, '.');
    if (ext == NULL) {
        flags = FILE_EXEC;
        ext = path + strlen(path);
    } else {
        if (ext[1] == '\0') {
            return 1;
        }

        if (ext[2] == '\0') {
            switch (ext[1]) {
            case 'c':
                flags = FILE_SOURCE;
                break;
            case 'h':
                flags = FILE_HEADER;
                break;
            case 'o':
                flags = FILE_OBJECT;
                break;
            default:
                return 1;
            }
        } else {
            if (strcmp(&ext[1], "data") != 0) {
                flags = FILE_DATA;
            } else if (strcmp(&ext[1], "input") != 0) {
                flags = FILE_INPUT;
            } else if (strcmp(&ext[1], "output") != 0) {
                flags = FILE_OUTPUT;
            } else {
                return 1;
            }
        }
    }

    folder = strchr(path, '/');
    if (folder != NULL) {
        folder_len = folder - path;
        if (len = strlen(Config.build), folder_len == len &&
                memcmp(path, Config.build, len) == 0) {
            path = folder + 1;
            folder = strchr(path, '/');
        }

        if (folder != NULL) {
            folder_len = folder - path;
            if (len = strlen(Config.tests), folder_len == len &&
                    memcmp(path, Config.tests, len) == 0) {
                flags |= FILE_IS_TEST;
            }
            name = smalloc(ext - folder);
            memcpy(name, folder + 1, ext - folder - 1);
            name[ext - folder - 1] = '\0';
        }
    } /* not else */
    if (folder == NULL) {
        name = smalloc(ext - path + 1);
        memcpy(name, path, ext - path);
        name[ext - path] = '\0';
    }
    *pname = name;
    *pflags = flags;
    return 0;
}

static bool run_executable(char **args)
{
    int pid;
    int wstatus;

    for (char **a = args; *a != NULL; a++) {
        LOG("%s ", a[0]);
    }
    LOG("\n");

    pid = fork();
    if (pid == -1) {
        LOG("fork: %s\n", strerror(errno));
        return false;
    }
    if (pid == 0) {
        dup2(STDOUT_FILENO, STDERR_FILENO);
        if (execvp(args[0], args) < 0) {
            LOG("execvp: %s\n", strerror(errno));
            exit(1);
        }
    } else {
        waitpid(pid, &wstatus, 0);
        if (WEXITSTATUS(wstatus) != 0) {
            LOG("%s returned: %d\n", args[0], wstatus);
            return false;
        }
    }
    return true;
}

static char *get_build_file_path(struct file *file)
{
    char *path;
    char *folder;

    if (file->flags & FILE_IS_TEST) {
        folder = Config.tests;
    } else {
        folder = Config.sources;
    }
    path = smalloc(strlen(Config.build) + 1 + strlen(folder) + 1 + strlen(file->name) + 3);
    sprintf(path, "%s/%s/%s.%s", Config.build, folder, file->name, "o");
    return path;
}

static bool object_has_main(const char *o)
{
    bfd *b;
    long size_needed;
    asymbol **symbol_table;
    long num_symbols;

    b = bfd_openr(o, NULL);
    if (b == NULL) {
        LOG("bfd_openr: %s\n", bfd_errmsg(bfd_get_error()));
        return false;
    }

    if (!bfd_check_format(b, bfd_object)) {
        LOG("%s is not an object file\n", o);
        return false;
    }

    size_needed = bfd_get_symtab_upper_bound(b);
    if (size_needed > 0) {
        symbol_table = malloc(size_needed);
        num_symbols = bfd_canonicalize_symtab(b, symbol_table);
        for (long i = 0; i < num_symbols; i++) {
            asymbol *const symbol = symbol_table[i];
            if ((symbol->flags & BSF_FUNCTION) && strcmp(symbol->name, "main") == 0) {
                free(symbol_table);
                bfd_close(b);
                return true;
            }
        }
        free(symbol_table);
    }

    bfd_close(b);
    return false;
}

static int create_base_directory(/* const */char *path)
{
    for (char *cur = path, *s; (s = strchr(cur, '/')) != NULL;) {
        s[0] = '\0';
        if (mkdir(path, 0755) == -1) {
            if (errno != EEXIST) {
                LOG("mkdir '%s': %s\n", path, strerror(errno));
                return -1;
            }
            DLOG("mkdir '%s': exists\n", path);
        } else {
            DLOG("mkdir '%s'\n", path);
        }
        s[0] = '/';
        cur = s + 1;
    }
    return 0;
}

static bool compile_file(struct file *file)
{
    char *args[6 + Config.num_c_flags];
    char *path;
    struct file *obj;

    path = get_build_file_path(file);
    obj = add_file(path);
    free(path);
    if (obj->st.st_mtime >= file->st.st_mtime) {
        return false;
    }

    args[0] = Config.cc;
    for (size_t f = 0; f < Config.num_c_flags; f++) {
        args[f + 1] = Config.c_flags[f];
    }
    args[Config.num_c_flags + 1] = (char*) "-c";
    args[Config.num_c_flags + 2] = file->path;
    args[Config.num_c_flags + 3] = (char*) "-o";
    args[Config.num_c_flags + 4] = obj->path;
    args[Config.num_c_flags + 5] = NULL;
    if (create_base_directory(obj->path) == -1) {
        return false;
    }
    if (!run_executable(args)) {
        return false;
    }
    return true;
}

static struct file *add_file(const char *path)
{
    char *name;
    unsigned flags;
    size_t index;
    struct file *file;
    struct stat st;

    DLOG("add file: %s\n", path);

    if (get_path_flags(path, &name, &flags) != 0) {
        return NULL;
    }

    file = search_file(name, flags, &index);
    if (file != NULL) {
        DLOG("file was already cached\n");
        if (stat(path, &st) == -1) {
            file->flags &= ~FILE_EXISTS;
        } else {
            file->flags |= FILE_EXISTS;
            file->st = st;
        }
    } else {
        Files.ptr = sreallocarray(Files.ptr,
                Files.num + 1, sizeof(*Files.ptr));
        memmove(&Files.ptr[index + 1], &Files.ptr[index],
                sizeof(*Files.ptr) * (Files.num - index));
        file = smalloc(sizeof(*file));
        Files.ptr[index] = file;
        Files.num++;
        file->flags = flags;
        file->name = name;
        file->path = sstrdup(path);
        file->st.st_mtime = 0;
        if (stat(path, &file->st) == -1) {
            file->flags &= ~FILE_EXISTS;
        } else {
            file->flags |= FILE_EXISTS;
            if (file->flags & FILE_OBJECT) {
                if (object_has_main(file->path)) {
                    file->flags |= FILE_HAS_MAIN;
                } else {
                    file->flags &= ~FILE_HAS_MAIN;
                }
            }
        }
    }

    if ((file->flags & (FILE_SOURCE | FILE_EXISTS)) ==
            (FILE_SOURCE | FILE_EXISTS)) {
        compile_file(file);
    }
    return file;
}

bool compile_files(void)
{
    char *paths[2];
    char *path;
    DIR *dir;
    struct dirent *ent;
    char *s;
    size_t n;
    size_t path_len;
    size_t name_len;

    paths[0] = Config.sources;
    paths[1] = Config.tests;
    for (size_t i = 0; i < ARRAY_SIZE(paths); i++) {
        path = paths[i];
        DLOG("collect sources from: '%s'\n", path);

        dir = opendir(path);
        if (dir == NULL) {
            LOG("opendir: %s\n", strerror(errno));
            return false;
        }
        path_len = strlen(path);

        n = path_len + 1 + 32 + 1;
        s = smalloc(n);
        memcpy(s, path, path_len);
        s[path_len++] = '/';
        while ((ent = readdir(dir)) != NULL) {
            if (ent->d_type != DT_REG) {
                continue;
            }
            name_len = strlen(ent->d_name);
            if (path_len + 1 + name_len + 1 > n) {
                n += name_len;
                s = srealloc(s, n);
            }
            strcpy(&s[path_len], ent->d_name);
            add_file(s);
        }
        free(s);

        closedir(dir);
    }
    return true;
}

static void get_objects(unsigned flags, struct file ***pobjects, size_t *pnum)
{
    struct file **objects = NULL;
    size_t num = 0;

    for (size_t i = 0; i < Files.num; i++) {
        struct file *const file = Files.ptr[i];
        if (FILE_TYPE(file->flags) != FILE_OBJECT) {
            continue;
        }
        if (FILE_FLAGS(file->flags) != flags) {
            continue;
        }
        objects = sreallocarray(objects, num + 1, sizeof(*objects));
        objects[num++] = file;
    }
    *pobjects = objects;
    *pnum = num;
}

static bool link_program(struct file *exe, struct file **objects, size_t num_objects,
        struct file *main_object)
{
    char *args[1 + Config.num_c_flags + num_objects + 3 + Config.num_c_libs + 1];
    size_t argi = 0;

    args[argi++] = (char*) "gcc";
    for (size_t i = 0; i < Config.num_c_flags; i++) {
        args[argi++] = Config.c_flags[i];
    }
    for (size_t i = 0; i < num_objects; i++) {
        args[argi++] = objects[i]->path;
    }
    args[argi++] = main_object->path;
    args[argi++] = "-o";
    args[argi++] = exe->path;
    for (size_t i = 0; i < Config.num_c_libs; i++) {
        args[argi++] = Config.c_libs[i];
    }
    args[argi] = NULL;
    if (!run_executable(args)) {
        return false;
    }
    return true;
}

static char *get_exe_file_path(struct file *file)
{
    char *path;
    char *folder;

    if (file->flags & FILE_IS_TEST) {
        folder = Config.tests;
    } else {
        folder = Config.sources;
    }
    path = smalloc(strlen(Config.build) + 1 + strlen(folder) + 1 + strlen(file->name) + 1);
    sprintf(path, "%s/%s/%s", Config.build, folder, file->name);
    return path;
}

bool link_binaries(void)
{
    char *path_exe;
    struct file *exe;
    struct file **objects;
    size_t num_objects;
    bool up;
    bool r = true;

    get_objects(FILE_EXISTS, &objects, &num_objects);
    for (size_t i = 0; i < Files.num; i++) {
        struct file *const file = Files.ptr[i];
        if (!(file->flags & FILE_HAS_MAIN)) {
            continue;
        }
        path_exe = get_exe_file_path(file);
        exe = add_file(path_exe);
        up = false;
        if (file->st.st_mtime > exe->st.st_mtime) {
            up = true;
        } else {
            for (size_t j = 0; j < num_objects; j++) {
                if (objects[j]->st.st_mtime > exe->st.st_mtime) {
                    up = true;
                    break;
                }
            }
        }
        if (up) {
            r &= link_program(exe, objects, num_objects, file);
        }
        free(path_exe);
    }
    free(objects);
    return r;
}

bool run_tests(void)
{
    /* TODO: */
    return false;
}
