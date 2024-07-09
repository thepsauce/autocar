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

    LOG("add file: %s\n", path);

    Files.ptr = sreallocarray(Files.ptr,
            Files.num + 1, sizeof(*Files.ptr));
    file = &Files.ptr[Files.num++];
    file->flags = flags;
    file->path = sstrdup(path);
    if (stat(path, &file->st) == -1) {
        fprintf(stderr, "stat '%s': %s\n",
                path, strerror(errno));
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

    LOG("collect sources from: '%s'\n", path);

    dir = opendir(path);
    if (dir == NULL) {
        fprintf(stderr, "opendir: %s\n", strerror(errno));
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
    long num_needed;
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

    num_needed = bfd_get_symtab_upper_bound(b);
    if (num_needed > 0) {
        symbol_table = malloc(num_needed);
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

    LOG("compiling sources/tests\n");

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

        o = get_object_file(file->path);
        LOG("%s: %s\n", file->path, o);
        if (o == NULL) {
            return false;
        }
        if (stat(o, &st) != 0 || file->st.st_mtime > st.st_mtime) {
            if (create_base_directory(o) == -1) {
                return false;
            }
            args[Config.num_c_flags + 2] = file->path;
            args[Config.num_c_flags + 4] = o;
            if (Args.verbose) {
                for (size_t i = 0; i < ARRAY_SIZE(args) - 1; i++) {
                    fprintf(stderr, "%s ", args[i]);
                }
                fprintf(stderr, "\n");
            }

            pid = fork();
            if (pid == 0) {
                if (execvp(Config.cc, args) < 0) {
                    fprintf(stderr, "execvp: %s\n", strerror(errno));
                    return false;
                }
            } else {
                waitpid(pid, NULL, 0);
            }
        } else {
            LOG("nothing to be done\n");
        }
        add_file(FILE_OBJECTS, o);
        free(o);
    }
    return true;
}

bool run_tests_and_compile_binaries(void)
{
    char *ext, *ext2;
    char *o;

    LOG("run tests\n");

    for (size_t i = 0; i < Files.num; i++) {
        struct file *const file = &Files.ptr[i];
        FILE *data = NULL;
        FILE *input = NULL;

        if (!(file->flags & FILE_TESTS)) {
            continue;
        }
        ext = strrchr(file->path, '.');
        if (ext == NULL || ext[1] != 'c' || ext[2] != '\0') {
            continue;
        }

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
                data = fopen(file2->path, "r");
            } else if (strcmp(ext2, "") == 0) {
                input = fopen(file2->path, "r");
            } else {
                continue;
            }
        }

        o = get_object_file(file->path);
        LOG("%s: %s\n", file->path, o);
        if (data != NULL) {
            fclose(data);
        }
        if (input != NULL) {
            fclose(input);
        }
        free(o);
    }
    return true;
}
