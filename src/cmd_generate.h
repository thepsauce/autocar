struct gen_object_list {
    char **sources;
    char **raw_objects;
    char **objects;
    size_t num;

    char **main_sources;
    char **raw_main_objects;
    char **main_objects;
    char **main_executables;
    size_t num_main;
};

static int make_object_list(struct gen_object_list *gol)
{
    struct file *file, *obj;
    char *raw;

    gol->num = 0;
    gol->num_main = 0;

    for (size_t i = 0; i < Files.num; i++) {
        file = Files.ptr[i];
        if (file->type != EXT_TYPE_SOURCE || !(file->flags & FLAG_EXISTS)) {
            continue;
        }
        obj = get_object_file(file);
        if ((obj->flags & FLAG_HAS_MAIN)) {
            gol->num_main++;
        } else {
            gol->num++;
        }
    }

    gol->sources = sreallocarray(NULL, gol->num, sizeof(*gol->sources));
    gol->raw_objects = sreallocarray(NULL, gol->num,
            sizeof(*gol->raw_objects));
    gol->objects = sreallocarray(NULL, gol->num, sizeof(*gol->objects));

    gol->main_sources = sreallocarray(NULL, gol->num_main,
            sizeof(*gol->main_sources));
    gol->raw_main_objects = sreallocarray(NULL, gol->num_main,
            sizeof(*gol->raw_main_objects));
    gol->main_objects = sreallocarray(NULL, gol->num_main,
            sizeof(*gol->main_objects));
    gol->main_executables = sreallocarray(NULL, gol->num_main,
            sizeof(*gol->main_executables));

    for (size_t i = 0, a = 0, b = 0; i < Files.num; i++) {
        file = Files.ptr[i];
        if (file->type != EXT_TYPE_SOURCE || !(file->flags & FLAG_EXISTS)) {
            continue;
        }
        obj = get_object_file(file);
        raw = smalloc(file->ext - file->path + 1);
        memcpy(raw, file->path, file->ext - file->path);
        raw[file->ext - file->path] = '\0';
        if ((obj->flags & FLAG_HAS_MAIN)) {
            gol->main_sources[a] = file->path;
            gol->raw_main_objects[a] = raw;
            gol->main_objects[a] = obj->path;
            gol->main_executables[a] = get_exec_file(obj)->path;
            a++;
        } else {
            gol->sources[b] = file->path;
            gol->raw_objects[b] = raw;
            gol->objects[b] = obj->path;
            b++;
        }
    }
    return 0;
}

static void clear_object_list(struct gen_object_list *gol)
{
    free(gol->sources);
    for (size_t i = 0; i < gol->num; i++) {
        free(gol->raw_objects[i]);
    }
    free(gol->raw_objects);
    free(gol->objects);

    free(gol->main_sources);
    for (size_t i = 0; i < gol->num_main; i++) {
        free(gol->raw_main_objects[i]);
    }
    free(gol->raw_main_objects);
    free(gol->main_objects);
    free(gol->main_executables);
}

/**
 * @brief Prints a string to a file while escaping all shell interpretations.
 *
 * @param fp File to print to.
 */
static void print_shell_escaped(const char *str, FILE *fp)
{
    fputc('\'', fp);
    for (; str[0] != '\0'; str++) {
        if (str[0] == '\'') {
            fputc('\'', fp);
            fputc('\\', fp);
        }
        fputc(str[0], fp);
    }
    fputc('\'', fp);
}

static void print_shell_array(char *const *s, size_t n, FILE *fp)
{
    for (size_t i = 0; i < n; i++) {
        if (i > 0) {
            fputc(' ', fp);
        }
        print_shell_escaped(s[i], fp);
    }
}


/**
 * @brief Prints a string to a file while escaping all make interpretations.
 *
 * @param fp File to print to.
 */
static void print_make_escaped(const char *str, FILE *fp)
{
    for (; str[0] != '\0'; str++) {
        switch (str[0]) {
        case '\r':
            fputs("'\\r'", fp);
            continue;
        case '\n':
            fputs("'\\n'", fp);
            continue;
        case '#':
        case '%':
        case ' ':
            fputc('\\', fp);
            break;
        case '\'':
            fputc('\'', fp);
            fputc('\\', fp);
            break;
        case '$':
            fputc('$', fp);
            break;
        }
        fputc(str[0], fp);
    }
}

static void print_make_array(char *const *s, size_t n, FILE *fp)
{
    for (size_t i = 0; i < n; i++) {
        if (i > 0) {
            fputc(' ', fp);
        }
        print_make_escaped(s[i], fp);
    }
}

static void print_c_escaped(const char *str, FILE *fp)
{
    unsigned char u, h, l;

    fputc('\"', fp);
    for (; str[0] != '\0'; str++) {
        if (isprint(str[0])) {
            if (str[0] == '\"' || str[0] == '\\') {
                fputc('\\', fp);
            }
            fputc(str[0], fp);
        } else {
            fputs("\\x", fp);
            u = str[0];
            h = u >> 4;
            l = u & 0xf;
            fputc(h >= 0xa ? 'a' - 0xa + h : '0' + h, fp);
            fputc(l >= 0xa ? 'a' - 0xa + l : '0' + h, fp);
        }
    }
    fputc('\"', fp);
}

static void print_c_array(char *const *s, size_t n, FILE *fp)
{
    for (size_t i = 0; i < n; i++) {
        if (i > 0) {
            fputs(", ", fp);
        }
        print_c_escaped(s[i], fp);
    }
}

static const char *const shell_code = "\
#!/bin/bash\n\
\n\
set -ex\n\
\n\
for ro in {{{RAW_OBJECTS}}} ; do\n\
    o={{{BUILD}}}/\"$ro\"{{{EXT_OBJECT}}}\n\
    s=\"$ro\"{{{EXT_SOURCE}}}\n\
    mkdir -p \"$(dirname \"$o\")\"\n\
    {{{CC}}} {{{C_FLAGS}}} -c \"$s\" -o \"$o\"\n\
done\n\
\n\
if [ {{{#MAIN_EXECUTABLES}}} = 0 ] ; then\n\
    echo \"no main executables\"\n\
    exit 0\n\
fi\n\
\n\
for ro in {{{RAW_MAIN_OBJECTS}}} ; do\n\
    o={{{BUILD}}}\"/$ro\"{{{EXT_OBJECT}}}\n\
    s=\"$ro\"{{{EXT_SOURCE}}}\n\
    e={{{BUILD}}}/\"$ro\"{{{EXT_EXECUTABLE}}}\n\
    mkdir -p \"$(dirname \"$o\")\"\n\
    {{{CC}}} {{{C_FLAGS}}} -c \"$s\" -o \"$o\"\n\
    {{{CC}}} {{{C_FLAGS}}} {{{OBJECTS}}} \"$o\" -o \"$e\" {{{C_LIBS}}}\n\
done\n\
\n\
set +x\n\
\n\
echo \"run any of the main executables:\"\n\
for o in {{{MAIN_EXECUTABLES}}} ; do\n\
    echo \"./$o\"\n\
done\n";

static const char *const make_code = "\
CC = {{{CC}}}\n\
C_FLAGS = {{{C_FLAGS}}}\n\
C_LIBS = {{{C_LIBS}}}\n\
BUILD = {{{BUILD}}}\n\
OBJECTS = {{{OBJECTS}}}\n\
MAIN_OBJECTS = {{{MAIN_OBJECTS}}}\n\
MAIN_EXECUTABLES = {{{MAIN_EXECUTABLES}}}\n\
\n\
.PHONY: all\n\
all: $(MAIN_EXECUTABLES) $(OBJECTS) $(MAIN_OBJECTS)\n\
\t$(foreach exec,$(MAIN_EXECUTABLES),$(info $(exec)))\n\
\n\
$(BUILD)/%{{{EXT_OBJECT}}}: %{{{EXT_SOURCE}}}\n\
\t$(shell mkdir -p $(dir $@))\n\
\t$(CC) $(C_FLAGS) -c $< -o $@\n\
\n\
$(BUILD)/%: $(BUILD)/%{{{EXT_OBJECT}}} $(OBJECTS)\n\
\t$(shell mkdir -p $(dir $@))\n\
\t$(CC) $(C_FLAGS) $(OBJECTS) $< -o $@ $(C_LIBS)\n\
\n\
.PHONY: clean\n\
clean:\n\
\trm -rf $(BUILD)\n";

static const char *const c_code = "\
#include <errno.h>\n\
#include <stdio.h>\n\
#include <stdlib.h>\n\
#include <string.h>\n\
#include <unistd.h>\n\
\n\
#include <sys/stat.h>\n\
#include <sys/wait.h>\n\
\n\
#define ARRAY_SIZE(a) (sizeof(a)/sizeof(*(a)))\n\
\n\
const char *CC = {{{CC}}};\n\
const char *C_FLAGS[] = { {{{C_FLAGS}}} };\n\
const char *C_LIBS[] = { {{{C_LIBS}}} };\n\
const char *SOURCES[] = { {{{SOURCES}}} };\n\
const char *MAIN_SOURCES[] = { {{{MAIN_SOURCES}}} };\n\
\n\
const char *OBJECTS[] = { {{{OBJECTS}}} };\n\
const char *MAIN_OBJECTS[] = { {{{MAIN_OBJECTS}}} };\n\
\n\
const char *MAIN_EXECUTABLES[] = { {{{MAIN_EXECUTABLES}}} };\n\
\n\
void make_directory(const char *path)\n\
{\n\
    char *cur, *s;\n\
    char p[strlen(path) + 1];\n\
\n\
    strcpy(p, path);\n\
\n\
    cur = p;\n\
    while (s = strchr(cur, '/'), s != NULL) {\n\
        s[0] = '\\0';\n\
        if (mkdir(p, 0755) == -1) {\n\
            if (errno != EEXIST) {\n\
                printf(\"mkdir '%s': %s\\n\", p, strerror(errno));\n\
                abort();\n\
            }\n\
        } else {\n\
            printf(\"mkdir %s\\n\", p);\n\
        }\n\
        s[0] = '/';\n\
        cur = s + 1;\n\
    }\n\
}\n\
\n\
void run_executable(char **args)\n\
{\n\
    int pid;\n\
    int wstatus;\n\
\n\
    for (char **a = args; a[0] != NULL; a++) {\n\
        printf(\"%s \", a[0]);\n\
    }\n\
    printf(\"\\n\");\n\
\n\
    pid = fork();\n\
    if (pid == -1) {\n\
        printf(\"fork: %s\\n\", strerror(errno));\n\
        abort();\n\
    }\n\
    if (pid == 0) {\n\
        if (execvp(args[0], args) < 0) {\n\
            printf(\"execvp: %s\\n\", strerror(errno));\n\
            abort();\n\
        }\n\
    } else {\n\
        waitpid(pid, &wstatus, 0);\n\
        if (WEXITSTATUS(wstatus) != 0) {\n\
            printf(\"`%s` returned: %d\\n\", args[0], wstatus);\n\
            abort();\n\
        }\n\
    }\n\
}\n\
\n\
int main(void)\n\
{\n\
    char *args[1 + ARRAY_SIZE(C_FLAGS) + 1 + ARRAY_SIZE(OBJECTS) + 3 +\n\
        ARRAY_SIZE(C_LIBS) + 1];\n\
\n\
    args[0] = (char*) CC;\n\
    for (size_t i = 0; i < ARRAY_SIZE(C_FLAGS); i++) {\n\
        args[1 + i] = (char*) C_FLAGS[i];\n\
    }\n\
\n\
    args[1 + ARRAY_SIZE(C_FLAGS)] = (char*) \"-c\";\n\
    args[3 + ARRAY_SIZE(C_FLAGS)] = (char*) \"-o\";\n\
    args[5 + ARRAY_SIZE(C_FLAGS)] = (char*) NULL;\n\
    for (size_t i = 0; i < ARRAY_SIZE(SOURCES); i++) {\n\
        args[2 + ARRAY_SIZE(C_FLAGS)] = (char*) SOURCES[i];\n\
        args[4 + ARRAY_SIZE(C_FLAGS)] = (char*) OBJECTS[i];\n\
        make_directory(OBJECTS[i]);\n\
        run_executable(args);\n\
    }\n\
\n\
    for (size_t i = 0; i < ARRAY_SIZE(MAIN_SOURCES); i++) {\n\
        args[2 + ARRAY_SIZE(C_FLAGS)] = (char*) MAIN_SOURCES[i];\n\
        args[4 + ARRAY_SIZE(C_FLAGS)] = (char*) MAIN_OBJECTS[i];\n\
        make_directory(MAIN_OBJECTS[i]);\n\
        run_executable(args);\n\
    }\n\
\n\
    if (ARRAY_SIZE(MAIN_OBJECTS) == 0) {\n\
        printf(\"no main objects\\n\");\n\
        return 0;\n\
    }\n\
\n\
    for (size_t i = 0; i < ARRAY_SIZE(OBJECTS); i++) {\n\
        args[1 + ARRAY_SIZE(C_FLAGS) + i] = (char*) OBJECTS[i];\n\
    }\n\
    args[1 + ARRAY_SIZE(C_FLAGS) + ARRAY_SIZE(OBJECTS) + 1] = (char*) \"-o\";\n\
    for (size_t i = 0; i < ARRAY_SIZE(C_LIBS); i++) {\n\
        args[1 + ARRAY_SIZE(C_FLAGS) + ARRAY_SIZE(OBJECTS) + 3 + i] =\n\
            (char*) C_LIBS[i];\n\
    }\n\
    args[1 + ARRAY_SIZE(C_FLAGS) + ARRAY_SIZE(OBJECTS) + 3 +\n\
        ARRAY_SIZE(C_LIBS)] = NULL;\n\
    for (size_t i = 0; i < ARRAY_SIZE(MAIN_OBJECTS); i++) {\n\
        args[1 + ARRAY_SIZE(C_FLAGS) + ARRAY_SIZE(OBJECTS)] =\n\
            (char*) MAIN_OBJECTS[i];\n\
        args[1 + ARRAY_SIZE(C_FLAGS) + ARRAY_SIZE(OBJECTS) + 2] =\n\
            (char*) MAIN_EXECUTABLES[i];\n\
        run_executable(args);\n\
    }\n\
\n\
    puts(\"run any of the main executables:\");\n\
    for (size_t i = 0; i < ARRAY_SIZE(MAIN_EXECUTABLES); i++) {\n\
        puts(MAIN_EXECUTABLES[i]);\n\
    }\n\
    return 0;\n\
}\n";

int cmd_generate(char **args, size_t num_args, FILE *out)
{
    static const struct generator {
        const char *name;
        void (*print)(char *const *s, size_t n, FILE *fp);
        const char *code;
    } generators[] = {
        { "shell", print_shell_array, shell_code },
        { "make", print_make_array, make_code },
        { "c", print_c_array, c_code },
    };
    static const char *variables[] = {
        "sources", "raw_objects", "objects",
        "main_sources", "raw_main_objects", "main_objects", "main_executables",
    };
    const struct generator *gen;
    struct gen_object_list gol;
    bool num;
    struct config_entry *entry;
    size_t v;
    char **values;
    size_t num_values;

    if (num_args != 1) {
        printf("need one argument for 'generate' (shell, make or c)\n");
        return -1;
    }

    for (size_t g = 0, pref; g < ARRAY_SIZE(generators); g++) {
        for (pref = 0; args[0][pref] != '\0'; pref++) {
            if (generators[g].name[pref] != tolower(args[0][pref])) {
                break;
            }
        }
        if (args[0][pref] == '\0') {
            gen = &generators[g];
            break;
        }
    }
    if (gen == NULL) {
        printf("invalid argument for 'generate',"
                " expected 'shell', 'make' or 'c'\n");
        return -1;
    }

    pthread_mutex_lock(&Files.lock);
    if (check_conf() != 0 || !build_objects() || !link_executables()) {
        pthread_mutex_unlock(&Files.lock);
        return -1;
    }
    make_object_list(&gol);
    pthread_mutex_unlock(&Files.lock);

    for (const char *c = gen->code, *s, *start; c[0] != '\0'; c++) {
        if (c[0] == '{' && c[1] == '{' && c[2] == '{') {
            s = &c[3];
            if (s[0] == '#') {
                num = true;
                s++;
            } else {
                num = false;
            }
            start = s;
            while (isalpha(s[0]) || s[0] == '_') {
                s++;
            }
            if (s[0] == '}' && s[1] == '}' && s[2] == '}') {
                for (v = 0; v < ARRAY_SIZE(variables); v++) {
                    if (strncasecmp(variables[v], start, s - start) == 0 &&
                            variables[v][s - start] == '\0') {
                        break;
                    }
                }
                if (v != ARRAY_SIZE(variables)) {
                    switch (v) {
                    case 0:
                        values = gol.sources;
                        num_values = gol.num;
                        break;
                    case 1:
                        values = gol.raw_objects;
                        num_values = gol.num;
                        break;
                    case 2:
                        values = gol.objects;
                        num_values = gol.num;
                        break;

                    case 3:
                        values = gol.main_sources;
                        num_values = gol.num_main;
                        break;
                    case 4:
                        values = gol.raw_main_objects;
                        num_values = gol.num_main;
                        break;
                    case 5:
                        values = gol.main_objects;
                        num_values = gol.num_main;
                        break;
                    case 6:
                        values = gol.main_executables;
                        num_values = gol.num_main;
                        break;
                    }
                } else {
                    entry = get_conf_l(start, s - start, NULL);
                    if (entry == NULL) {
                        num_values = 0;
                    } else {
                        values = entry->values;
                        num_values = entry->num_values;
                    }
                }
                if (num) {
                    fprintf(out, "%zu", num_values);
                } else if (entry != NULL) {
                    if (num_values == 0) {
                        if (s[3] == ' ') {
                            s++;
                        }
                    } else {
                        gen->print(values, num_values, out);
                    }
                }
                c = &s[2];
                continue;
            }
        }
        fputc(c[0], out);
    }

    clear_object_list(&gol);
    return 0;
}
