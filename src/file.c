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

/**
 * @brief Translates a file extension to an integer.
 *
 * @see conf.h
 *
 * @return Extension type integer (`EXT_TYPE_*`)
 */
static int get_extension_type(const char *e)
{
    char *ext, *end;
    bool repl;
    int cmp;

    for (int i = 0; i < EXT_TYPE_MAX; i++) {
        ext = Config.exts[i];
        while (ext[0] != '\0') {
            end = ext;
            while (end[0] != '\0' && end[0] != '|') {
                end++;
            }
            if (end[0] == '|') {
                repl = true;
                end[0] = '\0';
            } else {
                repl = false;
            }
            cmp = strcmp(e, ext);
            if (repl) {
                end[0] = '|';
            }
            if (cmp == 0) {
                return i;
            }
            if (end[0] == '\0') {
                break;
            }
            ext = end + 1;
        }
    }
    return -1;
}

/**
 * @brief Finds a file corresponding to given parameters.
 *
 * The `pIndex` parameter can be used to insert the next element, the file list
 * is sorted at all times and adding a new file not already contained at the
 * index will keep it sorted.
 *
 * @return NULL if file was not found, otherwise a pointer to the file.
 */
static struct file *search_file(int folder, const char *name, int type, size_t *pIndex)
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
            cmp = file->folder - folder;
        }
        if (cmp == 0) {
            cmp = file->type - type;
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

/**
 * @brief Update the `st` member and un-/set FLAG_EXISTS.
 *
 * Sets the `st` member of given file using `stat()` and sets FLAG_EXISTS for
 * all files successfully stat'ed.
 *
 * Files that have no extension (assumed to be executables) must have execute
 * permissions, otherwise they are not seen as existing.
 */
static void stat_file(struct file *file)
{
    struct stat st;

    if (stat(file->path, &st) == 0 && (file->type != EXT_TYPE_EXECUTABLE ||
                (st.st_mode & S_IXUSR))) {
        file->flags |= FLAG_EXISTS;
        file->st = st;
    } else {
        file->flags &= ~FLAG_EXISTS;
    }
}

/**
 * @brief Makes a file object and adds it to the file list.
 *
 * Checks if a file with given parameters already exists and returns this file,
 * it also uses `stat_file()` on it. If it does not exist, it makes a file using
 * given `folder`, `name` and `type`, constructs the `path` and then adds this
 * to the file list.
 *
 * @return The allocated file.
 */
static struct file *add_file(int folder, const char *name, int type)
{
    size_t index;
    struct file *file;
    char *folder_str;
    char *type_str, *end;
    bool repl_bar;
    char *path;

    DLOG("adding file: %s/%s (%d)\n", Config.folders[folder], name, type);

    file = search_file(folder, name, type, &index);
    if (file != NULL) {
        DLOG("file already existed\n");
        stat_file(file);
        return file;
    }

    folder_str = Config.folders[folder];
    type_str = Config.exts[type];
    end = type_str;
    while (end[0] != '|' && end[0] != '\0') {
        end++;
    }
    path = smalloc(strlen(folder_str) + 1 + strlen(name) + strlen(type_str) + 1);
    if (end[0] == '|') {
        end[0] = '\0';
        repl_bar = true;
    } else {
        repl_bar = false;
    }
    sprintf(path, "%s/%s%s", folder_str, name, type_str);
    if (repl_bar) {
        end[0] = '|';
    }

    file = scalloc(1, sizeof(*file));
    file->folder = folder;
    file->type = type;
    file->name = sstrdup(name);
    file->path = path;
    stat_file(file);
    Files.ptr = sreallocarray(Files.ptr, Files.num + 1, sizeof(*Files.ptr));
    memmove(&Files.ptr[index + 1], &Files.ptr[index],
            sizeof(*Files.ptr) * (Files.num - index));
    Files.ptr[index] = file;
    Files.num++;
    return file;
}

bool collect_files(void)
{
    char *folder;
    DIR *dir;
    struct dirent *ent;
    char *ext;
    int type;

    for (int i = 0; i <= FOLDER_TEST; i++) {
        folder = Config.folders[i];
        DLOG("collect sources from: '%s'\n", folder);

        dir = opendir(folder);
        if (dir == NULL) {
            LOG("opendir: %s\n", strerror(errno));
            return false;
        }
        while (ent = readdir(dir), ent != NULL) {
            if (ent->d_type != DT_REG) {
                continue;
            }
            ext = strrchr(ent->d_name, '.');
            if (ext == NULL) {
                continue;
            }
            type = get_extension_type(ext);
            if (type == -1) {
                continue;
            }
            char name[strlen(ent->d_name) + 1];
            memcpy(name, ent->d_name, ext - ent->d_name);
            name[ext - ent->d_name] = '\0';
            add_file(i, name, type);
        }

        closedir(dir);
    }

    return true;
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

static bool run_executable(char **args)
{
    int pid;
    int wstatus;

    for (char **a = args; a[0] != NULL; a++) {
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

static bool rebuild_object(struct file *src, struct file *obj)
{
    char *args[6 + Config.num_c_flags];

    args[0] = Config.cc;
    for (size_t f = 0; f < Config.num_c_flags; f++) {
        args[f + 1] = Config.c_flags[f];
    }
    args[Config.num_c_flags + 1] = (char*) "-c";
    args[Config.num_c_flags + 2] = src->path;
    args[Config.num_c_flags + 3] = (char*) "-o";
    args[Config.num_c_flags + 4] = obj->path;
    args[Config.num_c_flags + 5] = NULL;
    if (!run_executable(args)) {
        return false;
    }
    if (object_has_main(obj->path)) {
        obj->flags |= FLAG_HAS_MAIN;
    } else {
        obj->flags &= ~FLAG_HAS_MAIN;
    }
    return true;
}

bool build_objects(void)
{
    struct file *obj, *file;

    for (size_t i = 0; i < Files.num; i++) {
        file = Files.ptr[i];
        if (file->type == EXT_TYPE_SOURCE) {
            obj = add_file(file->folder + FOLDER_BUILD + 1,
                    file->name, EXT_TYPE_OBJECT);
            if (!(obj->flags & FLAG_EXISTS) ||
                    file->st.st_mtime > obj->st.st_mtime) {
                rebuild_object(file, obj);
            }
        }
    }
    return true;
}

static bool relink_executable(struct file *exec,
        struct file **objects, size_t num_objects,
        struct file *main_object)
{
    char *args[1 + Config.num_c_flags + num_objects + 3 + Config.num_c_libs + 1];
    size_t argi = 0;

    args[argi++] = Config.cc;
    for (size_t i = 0; i < Config.num_c_flags; i++) {
        args[argi++] = Config.c_flags[i];
    }
    for (size_t i = 0; i < num_objects; i++) {
        args[argi++] = objects[i]->path;
    }
    args[argi++] = main_object->path;
    args[argi++] = "-o";
    args[argi++] = exec->path;
    for (size_t i = 0; i < Config.num_c_libs; i++) {
        args[argi++] = Config.c_libs[i];
    }
    args[argi] = NULL;
    if (!run_executable(args)) {
        return false;
    }
    return true;
}

bool link_executables(void)
{
    struct file *file;
    struct file **objects = NULL;
    size_t num_objects = 0;
    time_t latest_time = 0;
    struct file *exec;

    for (size_t i = 0; i < Files.num; i++) {
        file = Files.ptr[i];
        if (file->type == EXT_TYPE_OBJECT && !(file->flags & FLAG_HAS_MAIN)) {
            latest_time = MAX(latest_time, file->st.st_mtime);
            objects = sreallocarray(objects, num_objects + 1, sizeof(*objects));
            objects[num_objects++] = file;
        }
    }

    for (size_t i = 0; i < Files.num; i++) {
        file = Files.ptr[i];
        if (file->flags & FLAG_HAS_MAIN) {
            exec = add_file(file->folder, file->name, EXT_TYPE_EXECUTABLE);
            if (!(exec->flags & FLAG_EXISTS) ||
                    MAX(latest_time, file->st.st_mtime) > exec->st.st_mtime) {
                relink_executable(exec, objects, num_objects, file);
            }
        }
    }

    free(objects);
    return true;
}

bool run_tests(void)
{
    struct file *file;
    char *input_path, *output_path, *data_path;
    int s;
    struct stat st;
    bool update;
    char *args[4];
    int pid;
    int wstatus;
    int c;
    FILE *fp;

    for (size_t i = 0; i < Files.num; i++) {
        file = Files.ptr[i];
        if (file->type != EXT_TYPE_EXECUTABLE || file->folder != FOLDER_BUILD_TEST) {
            continue;
        }

        update = false;

        if (asprintf(&input_path, "%s/%s.input",
                    Config.folders[FOLDER_TEST], file->name) == -1) {
            fprintf(stderr, "asprintf: %s\n", strerror(errno));
            exit(1);
        }
        s = stat(input_path, &st);
        if (s == 0 && st.st_mtime > file->last_input) {
            update = true;
        } else if (s == -1 && file->last_input != 0) {
            free(input_path);
            input_path = NULL;
            file->last_input = 0;
        }

        if (asprintf(&data_path, "%s/%s.data",
                    Config.folders[FOLDER_TEST], file->name) == -1) {
            fprintf(stderr, "asprintf: %s\n", strerror(errno));
            exit(1);
        }
        s = stat(input_path, &st);
        if (s == 0 && st.st_mtime > file->last_input) {
            update = true;
        } else if (s == -1 && file->last_input != 0) {
            free(data_path);
            data_path = NULL;
            file->last_input = 0;
        }

        if (asprintf(&output_path, "%s/%s.output",
                    Config.folders[FOLDER_TEST], file->name) == -1) {
            fprintf(stderr, "asprintf: %s\n", strerror(errno));
            exit(1);
        }
        if (!update) {
            s = stat(output_path, &st);
            update = (s == 0 && st.st_mtime < file->st.st_mtime) ||
                s == -1;
        }

        pid = fork();
        if (pid == -1) {
            LOG("fork: %s\n", strerror(errno));
            goto err;
        }
        if (pid == 0) {
            if (freopen(output_path, "wb", stdout) == NULL) {
                LOG("freopen '%s' stdout: %s\n", output_path, strerror(errno));
                exit(1);
            }
            if (input_path != NULL) {
                printf("%s\n", input_path);
                if (freopen(input_path, "rb", stdin) == NULL) {
                    LOG("freopen '%s' stdin: %s\n", input_path, strerror(errno));
                    exit(1);
                }
            }
            args[0] = file->path;
            args[1] = NULL;
            LOG("run %s\n", file->path);
            if (execv(file->path, args) == -1) {
                LOG("execv: %s\n", strerror(errno));
                exit(1);
            }
        } else {
            waitpid(pid, &wstatus, 0);
            if (WEXITSTATUS(wstatus) != 0) {
                LOG("%s returned: %d\n", file->path, wstatus);
                goto err;
            }
        }

        fprintf(stderr, "| %s |\n", output_path);
        if (data_path != NULL) {
            args[0] = Config.diff;
            args[1] = data_path;
            args[2] = output_path;
            args[3] = NULL;
            if (!run_executable(args)) {
                goto err;
            }
        } else {
            fp = fopen(output_path, "rb");
            if (fp != NULL) {
                while (c = fgetc(fp), c != EOF) {
                    fputc(c, stderr);
                }
                fclose(fp);
                if (c != '\n') {
                    fputc('\n', stderr);
                }
            }
        }
        free(input_path);
        free(data_path);
        free(output_path);
    }
    return true;

err:
    free(input_path);
    free(data_path);
    free(output_path);
    return false;
}
