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

static struct file *add_file(unsigned flags, const char *path)
{
    struct file *file;

    DLOG("add file: %s\n", path);

    Files.ptr = sreallocarray(Files.ptr,
            Files.num + 1, sizeof(*Files.ptr));
    file = &Files.ptr[Files.num++];
    file->flags = flags;
    file->path = sstrdup(path);
    if (stat(path, &file->st) == -1) {
        LOG("stat '%s': %s\n", path, strerror(errno));
        return NULL;
    }
    return file;
}

static bool collect_files(const char *path, unsigned flags)
{
    DIR *dir;
    struct dirent *ent;
    char *s;
    size_t n;
    size_t path_len;
    size_t name_len;

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
        add_file(flags, s);
    }
    free(s);

    closedir(dir);
    return true;
}

bool collect_sources(const char *path)
{
    return collect_files(path, FILE_SOURCE);
}

bool collect_tests(const char *path)
{
    return collect_files(path, FILE_TESTS);
}

/* changes the folder and extension of a file,
 * the last two arguments may be null.
 * examples:
 * change_file("src/file.c", "build", "o") => "build/file.o"
 * change_file("src/file.c", "build", NULL) => "build/file"
 * change_file("src/file.c", NULL, "data") => "src/file.data"
 * change_file("src/file.c", NULL, NULL) => "src/file"
 * change_file("file.c", "say", "what") => "say/file.what"
 *
 * IMPORTANT: It is not allowed for a path to end or start with a slash!!
 */
static char *change_file(const char *path, const char *new_folder, const char *new_ext)
{
    const char *folder;
    const char *e, *ext;
    size_t len_new_folder;
    size_t len_new_ext;
    char *s, *p;

    if (new_folder != NULL) {
        folder = strchr(path, '/');
        if (folder == NULL) {
            folder = path;
        } else {
            folder++;
        }
    } else {
        folder = path;
    }

    e = strrchr(folder, '/');
    ext = strrchr(folder, '.');
    if (ext == NULL || (e != NULL && ext < e)) {
        ext = folder + strlen(folder);
    }

    len_new_folder = new_folder == NULL ? 0 : strlen(new_folder);
    len_new_ext = new_ext == NULL ? 0 : strlen(new_ext);

    s = smalloc(len_new_folder + 1 + (ext - folder) + 1 + len_new_ext + 1);
    p = s;
    if (len_new_folder > 0) {
        memcpy(p, new_folder, len_new_folder);
        p += len_new_folder;
        p[0] = '/';
        p++;
    }
    memcpy(p, folder, ext - folder);
    p += ext - folder;
    if (new_ext == NULL) {
        p[0] = '\0';
    } else {
        p[0] = '.';
        p++;
        strcpy(p, new_ext);
    }
    return s;
}

static int create_base_directory(/* const */char *path)
{
    for (char *cur = path, *s; (s = strchr(cur, '/')) != NULL;) {
        s[0] = '\0';
        DLOG("mkdir '%s'", path);
        if (mkdir(path, 0755) == -1) {
            if (errno != EEXIST) {
                return -1;
            }
            DLOG(": exists");
        }
        DLOG("\n");
        s[0] = '/';
        cur = s + 1;
    }
    return 0;
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

bool compile_files(void)
{
    char *ext;
    char *o;
    struct stat st;
    char *args[6 + Config.num_c_flags];
    int pid;
    int wstatus;

    DLOG("compiling sources/tests\n");

    args[0] = Config.cc;
    for (size_t f = 0; f < Config.num_c_flags; f++) {
        args[f + 1] = Config.c_flags[f];
    }
    args[Config.num_c_flags + 1] = (char*) "-c";
    args[Config.num_c_flags + 3] = (char*) "-o";
    args[Config.num_c_flags + 5] = NULL;

    for (size_t i = 0; i < Files.num; i++) {
        struct file *const file = &Files.ptr[i];

        ext = strrchr(file->path, '.');
        if (ext == NULL || ext[1] != 'c' || ext[2] != '\0') {
            continue;
        }

        o = change_file(file->path, Config.build, "o");
        DLOG("%s: %s\n", file->path, o);

        if (stat(o, &st) != 0 || file->st.st_mtime > st.st_mtime) {
            if (create_base_directory(o) == -1) {
                return false;
            }
            args[Config.num_c_flags + 2] = file->path;
            args[Config.num_c_flags + 4] = o;
            for (size_t i = 0; i < ARRAY_SIZE(args) - 1; i++) {
                LOG("%s ", args[i]);
            }
            LOG("\n");

            pid = fork();
            if (pid == 0) {
                if (execvp(Config.cc, args) < 0) {
                    LOG("execvp: %s\n", strerror(errno));
                    exit(1);
                }
            } else {
                waitpid(pid, &wstatus, 0);
                if (WEXITSTATUS(wstatus) != 0) {
                    return false;
                }
            }
        } else {
            DLOG("nothing to be done\n");
        }
        add_file((file->flags & (FILE_SOURCE | FILE_TESTS)) | FILE_OBJECTS |
                (object_has_main(o) ? FILE_HAS_MAIN : 0), o);
        free(o);
    }
    return true;
}

static void get_objects(unsigned flags, char ***pobjects, size_t *pnum)
{
    char **objects = NULL;
    size_t num = 0;
    for (size_t i = 0; i < Files.num; i++) {
        struct file *const file = &Files.ptr[i];
        if ((file->flags ^ FILE_OBJECTS) != flags) {
            continue;
        }
        objects = sreallocarray(objects, num + 1, sizeof(*objects));
        objects[num++] = file->path;
    }
    *pobjects = objects;
    *pnum = num;
}

bool run_tests_and_compile_binaries(void)
{
    char *ext, *ext2;
    char *o, *exe;
    char **objects;
    size_t num_objects;
    char *path_data, *path_input, *path_output;
    FILE *data;
    int pid;
    int wstatus;
    char **main_objects;
    size_t num_main_objects;
    char *main_object;

    DLOG("run tests\n");

    get_objects(FILE_SOURCE, &objects, &num_objects);
    /* gcc C_FLAGS OBJECTS MAIN_OBJECT -o DESTINATION C_LIBS NULL */
    char *args[1 + Config.num_c_flags + num_objects + 3 + Config.num_c_libs + 1];
    size_t argi = 0;

    args[argi++] = (char*) "gcc";
    for (size_t i = 0; i < Config.num_c_flags; i++) {
        args[argi++] = Config.c_flags[i];
    }
    for (size_t i = 0; i < num_objects; i++) {
        args[argi++] = objects[i];
    }
    args[argi++] = "<main object>";
    args[argi++] = "-o";
    args[argi++] = "<destination>";
    for (size_t i = 0; i < Config.num_c_libs; i++) {
        args[argi++] = Config.c_libs[i];
    }
    args[argi] = NULL;
    free(objects);

    for (size_t i = 0; i < Files.num; i++) {
        struct file *const file = &Files.ptr[i];

        if (!(file->flags & FILE_TESTS)) {
            continue;
        }
        ext = strrchr(file->path, '.');
        if (ext == NULL || ext[1] != 'c' || ext[2] != '\0') {
            continue;
        }

        path_data = NULL;
        path_input = NULL;
        for (size_t j = 0; j < Files.num; j++) {
            struct file *const file2 = &Files.ptr[i];
            if (!(file2->flags & FILE_TESTS)) {
                continue;
            }
            ext2 = strrchr(file2->path, '.');
            if (ext2 == NULL || ext - file->path != ext2 - file2->path) {
                continue;
            }
            if (memcmp(file->path, file2->path, ext - file->path) != 0) {
                continue;
            }
            ext2++;
            if (strcmp(ext2, "data") == 0) {
                path_data = file2->path;
            } else if (strcmp(ext2, "") == 0) {
                path_input = file2->path;
            } else {
                continue;
            }
        }

        o = change_file(file->path, Config.build, "o");
        exe = change_file(file->path, Config.build, NULL);
        DLOG("%s : %s : %s\n", file->path, o, exe);

        args[1 + Config.num_c_flags + num_objects] = o;
        args[1 + Config.num_c_flags + num_objects + 2] = exe;
        for (size_t i = 0; i < ARRAY_SIZE(args) - 1; i++) {
            LOG("%s ", args[i]);
        }
        LOG("\n");
        pid = fork();
        if (pid == -1) {
            LOG("fork: %s\n", strerror(errno));
            free(exe);
            free(o);
            return false;
        }
        if (pid == 0) {
            execvp("gcc", args);
        } else {
            waitpid(pid, &wstatus, 0);
            if (WEXITSTATUS(wstatus) != 0) {
                free(exe);
                free(o);
                return false;
            }
        }

        pid = fork();
        if (pid == -1) {
            LOG("fork: %s\n", strerror(errno));
            free(exe);
            free(o);
            return false;
        }
        if (pid == 0) {
            path_output = change_file(exe, NULL, "output");
            if (freopen(path_output, "wb", stdout) == NULL) {
                free(path_output);
                LOG("freopen: %s\n", strerror(errno));
                exit(1);
            }
            free(path_output);
            if (path_input != NULL) {
                if (freopen(path_input, "rb", stdin) == NULL) {
                    LOG("freopen: %s\n", strerror(errno));
                    exit(1);
                }
            }
            char *args[2];
            args[0] = (char*) exe;
            args[1] = NULL;
            if (execv(exe, args) == -1) {
                LOG("execv: %s\n", strerror(errno));
                exit(1);
            }
        } else {
            waitpid(pid, &wstatus, 0);
            if (WEXITSTATUS(wstatus) != 0) {
                free(exe);
                free(o);
                return false;
            }
        }
        (void) data;
        (void) path_data;
        free(exe);
        free(o);
    }

    /* link main program */
    get_objects(FILE_SOURCE | FILE_HAS_MAIN,
            &main_objects, &num_main_objects);
    if (num_main_objects == 0) {
        LOG("no main objects\n");
        return false;
    }
    if (num_main_objects > 1) {
        LOG("multiple main objects: ");
        for (size_t i = 0; i < num_main_objects; i++) {
            LOG("%s ", main_objects[i]);
        }
        LOG("\n");
        free(main_objects);
        return false;
    }
    main_object = main_objects[0];
    free(main_objects);
    exe = change_file(main_object, NULL, NULL);
    args[1 + Config.num_c_flags + num_objects] = main_object;
    args[1 + Config.num_c_flags + num_objects + 2] = exe;
    for (size_t i = 0; i < ARRAY_SIZE(args) - 1; i++) {
        LOG("%s ", args[i]);
    }
    LOG("\n");
    pid = fork();
    if (pid == -1) {
        LOG("fork: %s\n", strerror(errno));
        free(exe);
        return false;
    }
    if (pid == 0) {
        execvp("gcc", args);
    } else {
        waitpid(pid, &wstatus, 0);
        if (WEXITSTATUS(wstatus) != 0) {
            free(exe);
            return false;
        }
    }
    free(exe);
    return true;
}
