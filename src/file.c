#include "args.h"
#include "salloc.h"
#include "macros.h"
#include "file.h"
#include "conf.h"
#include "util.h"

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
 * @brief Gets the extension of given path.
 *
 * The extension is the '.<text>' at the end where <text> may be empty. If there
 * is no '.' in the last entry of the path, then a pointer to the
 * `NULL`-terminator is returned.
 *
 * @return Extension of the path.
 */
static char *get_extension(char *path)
{
    char *end, *ext;

    end = path + strlen(path);
    ext = end;
    while (ext != path) {
        ext--;
        if (ext[0] == '.') {
            break;
        }
        if (ext[0] == '/') {
            ext = end;
            break;
        }
    }
    return ext;
}

/**
 * @brief Translates a file extension to an integer.
 *
 * @see conf.h
 *
 * @return Extension type integer (`EXT_TYPE_*`).
 */
static int get_extension_type(/* const */ char *e)
{
    char *ext, *end;
    bool repl;
    int cmp;

    e = get_extension(e);
    for (int i = 0; i < EXT_TYPE_FOLDER; i++) {
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
    return EXT_TYPE_OTHER;
}

/**
 * @brief Finds a file corresponding to given parameters.
 *
 * The `pindex` parameter can be used to insert the next element, the file list
 * is sorted at all times and adding a new file not already contained at the
 * index will keep it sorted.
 *
 * @param name Name to search for.
 * @param pindex Pointer to store the index in, may be `NULL`.
 *
 * @return NULL if the file was not found, otherwise a pointer to the file.
 */
static struct file *search_file(const char *path, size_t *pindex)
{
    size_t l, m, r;
    int cmp;
    struct file *file;

    l = 0;
    r = Files.num;
    while (l < r) {
        m = (l + r) / 2;

        file = Files.ptr[m];
        cmp = strcmp(file->path, path);
        if (cmp == 0) {
            if (pindex != NULL) {
                *pindex = m;
            }
            return file;
        }
        if (cmp < 0) {
            l = m + 1;
        } else {
            r = m;
        }
    }
    if (pindex != NULL) {
        *pindex = r;
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
    int s;

    s = stat(file->path, &st);
    if (s == 0 && (file->type != EXT_TYPE_EXECUTABLE ||
                (st.st_mode & S_IXUSR))) {
        file->flags |= FLAG_EXISTS;
        file->st = st;
    } else {
        file->flags &= ~FLAG_EXISTS;
    }
    if (s == 0 && S_ISDIR(st.st_mode)) {
        file->type = EXT_TYPE_FOLDER;
    }
}

struct file *add_file(char *path, int type, int flags)
{
    size_t index;
    struct file *file;

    DLOG("adding file: %s\n", path);

    path = get_relative_path(path);
    if (path == NULL) {
        return NULL;
    }

    file = search_file(path, &index);
    if (file != NULL) {
        DLOG("file already existed\n");
        free(path);
        return file;
    }

    file = scalloc(1, sizeof(*file));
    file->path = path;
    file->type = type == -1 ? get_extension_type(path) : type;
    file->flags = flags | FLAG_IS_FRESH;
    file->ext = get_extension(path);
    Files.ptr = sreallocarray(Files.ptr, Files.num + 1, sizeof(*Files.ptr));
    memmove(&Files.ptr[index + 1], &Files.ptr[index],
            sizeof(*Files.ptr) * (Files.num - index));
    Files.ptr[index] = file;
    Files.num++;
    DLOG("file: '%s' added with type %d and flags %d\n",
            file->path, file->type, file->flags);
    return file;
}

bool collect_files(void)
{
    struct file *file;
    DIR *dir;
    struct dirent *ent;
    char *path;

    for (size_t i = 0; i < Files.num; i++) {
        file = Files.ptr[i];
        if (file->type != EXT_TYPE_FOLDER || !(file->flags & FLAG_EXISTS)) {
            continue;
        }
        DLOG("collect sources from: '%s'\n", file->path);

        dir = opendir(file->path);
        if (dir == NULL) {
            LOG("opendir: %s\n", strerror(errno));
            return false;
        }
        while (ent = readdir(dir), ent != NULL) {
            if (ent->d_type != DT_REG) {
                continue;
            }
            path = sasprintf("%s/%s", file->path, ent->d_name);
            add_file(path, -1, file->flags & FLAG_IS_TEST);
            free(path);
        }

        closedir(dir);
    }
    for (size_t i = 0; i < Files.num; i++) {
        file = Files.ptr[i];
        stat_file(file);
    }
    return true;
}

/**
 * @brief Checks if the given object file has a function called 'main'.
 *
 * This is the only function that uses the bfd library and does magic.
 *
 * @param o Path of the object file.
 *
 * @return Whether the object file has a 'main' function.
 */
static bool object_has_main(const char *o)
{
    bfd *b;
    long size_needed;
    asymbol **symbol_table;
    long num_symbols;
    asymbol *symbol;

    b = bfd_openr(o, NULL);
    if (b == NULL) {
        LOG("bfd_openr: %s\n", bfd_errmsg(bfd_get_error()));
        return false;
    }

    if (!bfd_check_format(b, bfd_object)) {
        LOG("'%s' is not an object file\n", o);
        return false;
    }

    size_needed = bfd_get_symtab_upper_bound(b);
    if (size_needed > 0) {
        symbol_table = smalloc(size_needed);
        num_symbols = bfd_canonicalize_symtab(b, symbol_table);
        for (long i = 0; i < num_symbols; i++) {
            symbol = symbol_table[i];
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

/**
 * @brief Rebuilds a source file.
 *
 * Constructs a command like: `gcc <flags> -c src -o obj` and runs it.
 * Also sets the `FLAG_HAS_MAIN` flag for the object if it includes a main
 * function.
 *
 * @param src The source file to rebuild.
 * @param obj The destination object file.
 *
 * @return Whether building was successful.
 */
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
    if (create_recursive_directory(obj->path) == -1) {
        return false;
    }
    if (run_executable(args, Config.err_file, NULL) != 0) {
        obj->flags &= ~FLAG_EXISTS;
        return false;
    }
    if (object_has_main(obj->path)) {
        obj->flags |= FLAG_HAS_MAIN;
    } else {
        obj->flags &= ~FLAG_HAS_MAIN;
    }
    obj->flags |= FLAG_EXISTS;
    return true;
}

static inline char *get_object_path(char *path, char *ext)
{
    bool dot;
    char *e;

    if (ext[0] == '.') {
        dot = true;
        ext[0] = '\0';
    } else {
        dot = false;
    }
    e = Config.exts[EXT_TYPE_OBJECT];
    if (e == NULL || e[0] == '\0') {
        e = sasprintf("%s/%s", Config.build, path);
    } else {
        e = sasprintf("%s/%s%s", Config.build, path, e);
    }
    if (dot) {
        ext[0] = '.';
    }
    return e;
}

bool build_objects(void)
{
    struct file *obj, *file;
    char *o;

    for (size_t i = 0; i < Files.num; i++) {
        file = Files.ptr[i];
        if (file->type == EXT_TYPE_SOURCE) {
            o = get_object_path(file->path, file->ext);
            obj = add_file(o, EXT_TYPE_OBJECT, file->flags & FLAG_IS_TEST);
            free(o);
            stat_file(obj);
            if (!(obj->flags & FLAG_EXISTS) ||
                    file->st.st_mtime > obj->st.st_mtime) {
                if (!rebuild_object(file, obj)) {
                    return false;
                }
            } else if (obj->flags & FLAG_IS_FRESH) {
                if (object_has_main(obj->path)) {
                    obj->flags |= FLAG_HAS_MAIN;
                } else {
                    obj->flags &= ~FLAG_HAS_MAIN;
                }
            }
            obj->flags &= ~FLAG_IS_FRESH;
        }
    }
    return true;
}

/**
 * @brief Links object files and libraries to create and executable.
 *
 * Runs a command line like: `gcc <flags> <objects> -o <main_object> <libs>`.
 *
 * @param exec The resulting executable file.
 * @param objects The objects to link.
 * @param num_objects The number of objects to link.
 * @param main_object The main objects.
 *
 * @return Whether linking was successful.
 */
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
    if (create_recursive_directory(exec->path) == -1) {
        return false;
    }
    if (run_executable(args, Config.err_file, NULL) != 0) {
        return false;
    }
    exec->flags |= FLAG_EXISTS;
    return true;
}

static char *get_exec_path(char *path, char *ext)
{
    bool dot;
    char *e;

    if (ext[0] == '.') {
        dot = true;
        ext[0] = '\0';
    } else {
        dot = false;
    }
    e = Config.exts[EXT_TYPE_EXECUTABLE];
    if (e == NULL || e[0] == '\0') {
        e = sasprintf("%s", path);
    } else {
        e = sasprintf("%s.%s", path, e);
    }
    if (dot) {
        ext[0] = '.';
    }
    return e;
}

bool link_executables(void)
{
    struct file *file;
    struct file **objects = NULL;
    size_t num_objects = 0;
    time_t latest_time = 0;
    char *e;
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
            e = get_exec_path(file->path, file->ext);
            exec = add_file(e, EXT_TYPE_EXECUTABLE, file->flags & FLAG_IS_TEST);
            free(e);
            stat_file(exec);
            if (!(exec->flags & FLAG_EXISTS) ||
                    MAX(latest_time, file->st.st_mtime) > exec->st.st_mtime) {
                if (!relink_executable(exec, objects, num_objects, file)) {
                    free(objects);
                    return false;
                }
            }
        }
    }

    free(objects);
    return true;
}

bool run_tests(void)
{
    struct file *file, *other, *input, *output, *data;
    bool update;
    char *args[4];
    int c;
    FILE *fp;
    char *name, *n;
    bool dot, d;
    char *output_path;

    for (size_t i = 0; i < Files.num; i++) {
        file = Files.ptr[i];
        if (file->type != EXT_TYPE_EXECUTABLE ||
                (file->flags & (FLAG_IS_TEST | FLAG_EXISTS)) !=
                (FLAG_IS_TEST | FLAG_EXISTS)) {
            continue;
        }

        update = false;

        name = strrchr(file->path, '/');
        if (name == NULL) {
            name = file->path;
        } else {
            name++;
        }
        if (file->ext[0] == '.') {
            dot = true;
            file->ext[0] = '\0';
        } else {
            dot = false;
        }
        input = NULL;
        data = NULL;
        output = NULL;
        for (size_t j = 0; j < Files.num; j++) {
            other = Files.ptr[j];
            if (other->type != EXT_TYPE_OTHER) {
                continue;
            }
            n = strrchr(other->path, '/');
            if (n == NULL) {
                n = other->path;
            } else {
                n++;
            }
            if (other->ext[0] == '.') {
                d = true;
                other->ext[0] = '\0';
            } else {
                d = false;
            }
            c = strcmp(name, n);
            if (d) {
                other->ext[0] = '.';
            }
            if (c != 0) {
                continue;
            }
            if (strcmp(other->ext, ".input") == 0) {
                input = other;
            } else if (strcmp(other->ext, ".data") == 0) {
                data = other;
            } else if (strcmp(other->ext, ".output") == 0) {
                output = other;
            }
        }

        if (input == NULL && data == NULL) {
            /* ignore this test */
            continue;
        }

        if (output == NULL) {
            output_path = sasprintf("%s.output", file->path);
            output = add_file(output_path, EXT_TYPE_OTHER, FLAG_IS_TEST);
            free(output_path);
            stat_file(output);
            update = true;
        }
        if (output->st.st_mtime < file->st.st_mtime) {
            update = true;
        }
        if (dot) {
            file->ext[0] = '.';
        }

        if (input != NULL && output->st.st_mtime < input->st.st_mtime) {
            update = true;
        }

        if (data != NULL && output->st.st_mtime < data->st.st_mtime) {
            update = true;
        }

        if (!update) {
            continue;
        }

        args[0] = file->path;
        args[1] = NULL;
        if (run_executable(args, output->path, input == NULL ?
                    "/dev/null" : input->path) != 0) {
            return false;
        }

        fprintf(stderr, "| %s |\n", output->path);
        if (data != NULL) {
            args[0] = Config.diff;
            args[1] = data->path;
            args[2] = output->path;
            args[3] = NULL;
            if (run_executable(args, NULL, NULL) != 0) {
                return false;
            }
        } else {
            fp = fopen(output->path, "rb");
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
    }
    return true;
}
