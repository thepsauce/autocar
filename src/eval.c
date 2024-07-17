#include "args.h"
#include "file.h"
#include "conf.h"
#include "cmd.h"
#include "salloc.h"
#include "macros.h"

#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/wait.h>

#define STATE_ERROR (-1)
#define STATE_REGULAR 0
#define STATE_EQUAL 1
#define STATE_APPEND 2
#define STATE_SUBTRACT 3
#define STATE_REDIRECT 4
#define STATE_SYSTEM 5
#define STATE_EXEC_SYSTEM 6

struct state {
    int state;
    char **args;
    size_t num_args;
    char *line;
};

static void advance_state(struct state *st)
{
    int c;

    size_t arg_a = 10;
    char *arg, *a;
    size_t arg_len;

    bool esc;
    char quot;

    const char *start;
    struct config_entry *entry;

    bool has_arg;

    char old;
    size_t n;
    size_t index;

    arg = smalloc(arg_a);

beg:
    arg_len = 0;
    has_arg = false;
    esc = false;
    quot = '\0';
    while (isblank(st->line[0])) {
        st->line++;
    }
    if (st->line[0] == '\0' || ((c == ';' || c == '#') && !esc && quot == '\0')) {
        free(arg);
        return;
    }
    for (; c = st->line[0], c != '\0'; st->line++) {
        if ((c == ';' || c == '#') && !esc && quot == '\0') {
            break;
        }
        switch (c) {
        case '\\':
            esc = !esc;
            if (esc) {
                continue;
            }
            break;

        case '\"':
        case '\'':
            if (!esc) {
                if (quot == c) {
                    quot = '\0';
                } else if (quot == '\0') {
                    quot = c;
                    has_arg = true;
                }
                continue;
            }
            break;

        case ' ':
        case '\t':
            if (!esc && quot == '\0' && st->state < STATE_REDIRECT) {
                goto end;
            }
            break;

        case '+':
        case '-':
        case ':':
            if (st->state != STATE_REGULAR || st->num_args > 1) {
                break;
            }
            if (!esc && quot == '\0') {
                if (st->line[1] == '=') {
                    st->state = c == '+' ? STATE_APPEND :
                        c == '-' ? STATE_SUBTRACT :
                        STATE_SYSTEM;
                    st->line += 2;
                    goto end;
                }
                break;
            }
            break;

        case '=':
            if (st->state != STATE_REGULAR || st->num_args > 1) {
                break;
            }
            if (!esc && quot == '\0') {
                st->state = STATE_EQUAL;
                st->line++;
                goto end;
            }
            break;

        case '>':
            if (st->state != STATE_REGULAR || st->num_args > 1) {
                break;
            }
            if (!esc && quot == '\0') {
                st->state = STATE_REDIRECT;
                st->line++;
                goto end;
            }
            break;

        case '$':
            if (esc || quot == '\'') {
                break;
            }
            if (isdigit(st->line[1])) {
                st->line++;
                index = strtoull(st->line, &st->line, 0);
                if (index == 0 || index - 1 >= Files.num) {
                    printf("file index is out of range\n");
                    goto err;
                }
                index--;
                has_arg = true;
                n = strlen(Files.ptr[index]->path);
                if (arg_len + n > arg_a) {
                    arg_a *= 2;
                    arg = srealloc(arg, arg_a);
                }
                memcpy(&arg[arg_len], Files.ptr[index]->path, n);
                arg_len += n;
                st->line--;
                continue;
            } else if (st->state >= STATE_SYSTEM) {
                break;
            }
            st->line++;
            start = st->line;
            while (!isblank(st->line[0]) && st->line[0] != quot &&
                    st->line[0] != '\0') {
                st->line++;
            }
            old = st->line[0];
            st->line[0] = '\0';
            entry = get_conf(start, NULL);
            st->line[0] = old;
            if (entry == NULL) {
                printf("variable '%.*s' is unset\n",
                        (int) (st->line - start), start);
                goto err;
            }

            if (has_arg && entry->num_values > 0) {
                n = strlen(entry->values[0]);
                if (arg_len + n > arg_a) {
                    arg_a *= 2;
                    arg = srealloc(arg, arg_a);
                }
                memcpy(&arg[arg_len], entry->values[0], n);
                arg_len += n;
            } else {
                st->args = sreallocarray(st->args,
                        st->num_args + entry->num_values,
                        sizeof(*st->args));
                for (size_t i = 0; i < entry->num_values; i++) {
                    st->args[st->num_args++] = sstrdup(entry->values[i]);
                }
            }
            st->line--;
            continue;

        default:
            if (esc && quot != '\'') {
                switch (c) {
                case 'a':
                    c = '\a';
                    break;
                case 'b':
                    c = '\b';
                    break;
                case 'e':
                    c = '\x1b';
                    break;
                case 'f':
                    c = '\f';
                    break;
                case 'n':
                    c = '\n';
                    break;
                case 'v':
                    c = '\v';
                    break;
                case 'r':
                    c = '\r';
                    break;
                case 't':
                    c = '\t';
                    break;
                }
            }
        }
        if (arg_len + 1 > arg_a) {
            arg_a *= 2;
            arg = srealloc(arg, arg_a);
        }
        arg[arg_len++] = c;
        has_arg = true;
        esc = false;
    }

end:
    if (!has_arg) {
        goto beg;
    }
    a = smalloc(arg_len + 1);
    memcpy(a, arg, arg_len);
    a[arg_len] = '\0';
    st->args = sreallocarray(st->args, st->num_args + 1, sizeof(*st->args));
    st->args[st->num_args++] = a;
    goto beg;

err:
    st->state = STATE_ERROR;
    free(arg);
}

static int read_process(const char *cmd, char ***pargs, size_t *pnum_args)
{
    int pipe_fd[2];
    pid_t pid;
    FILE *pp;
    char *shell;
    char **args = NULL;
    size_t num_args = 0;
    char *s, *a;
    size_t s_a, s_i;
    bool sp;
    int c;

    if (pipe(pipe_fd) == -1) {
        printf("pipe: %s\n", strerror(errno));
        return -1;
    }

    pid = fork();
    if (pid == -1) {
        printf("fork: %s\n", strerror(errno));
        return -1;
    }

    if (pid == 0) {
        close(pipe_fd[0]);
        dup2(pipe_fd[1], STDOUT_FILENO);
        close(pipe_fd[1]);

        shell = getenv("SHELL");
        execl(shell, shell, "-c", cmd, NULL);
        printf("execl: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    } else {
        close(pipe_fd[1]);
        pp = fdopen(pipe_fd[0], "r");
        if (pp == NULL) {
            printf("fdopen: %s\n", strerror(errno));
            return -1;
        }

        s_a = 256;
        s = smalloc(s_a);
        s_i = 0;
        while (c = fgetc(pp), c != EOF) {
            sp = false;
            while (isspace(c)) {
                c = fgetc(pp);
                sp = true;
            }
            if (s_i > 0 && (c == EOF || sp)) {
                args = sreallocarray(args, num_args + 1, sizeof(*args));
                a = smalloc(s_i + 1);
                memcpy(a, s, s_i);
                a[s_i] = '\0';
                args[num_args++] = a;
                s_i = 0;
            }
            if (c == EOF) {
                break;
            }
            s[s_i++] = c;
            if (s_i + 1 >= s_a) {
                s_a *= 2;
                s = srealloc(s, s_a);
            }
        }
        free(s);
        fclose(pp);
        waitpid(pid, NULL, 0);
        *pargs = args;
        *pnum_args = num_args;
    }
    return 0;
}

int run_command_line(char *s)
{
    int result = 0;

    struct state state;

    int cmd;
    size_t pref;

    FILE *redir;

    char **p_args;
    size_t p_num_args;

    DLOG("running cmd: %s\n", s);

    state.line = s;

next_segment:
    if (state.line[0] == ':') {
        state.state = STATE_EXEC_SYSTEM;
        state.line++;
    } else {
        state.state = STATE_REGULAR;
    }
    state.args = NULL;
    state.num_args = 0;

    advance_state(&state);

    if (state.num_args == 0) {
        goto end;
    }

    DLOG("got args in state %d:", state);
    for (size_t i = 0; i < state.num_args; i++) {
        DLOG(" %s", state.args[i]);
    }
    DLOG("\n");

    redir = stdout;

    switch (state.state) {
    case STATE_ERROR:
        result = -1;
        break;

    case STATE_REDIRECT:
        if (state.num_args == 1) {
            printf("need redirect output name after '>'\n");
            break;
        }
        redir = fopen(state.args[state.num_args - 1], "w");
        if (redir == NULL) {
            printf("fopen: %s\n", strerror(errno));
            result = -1;
            break;
        }
        /* fall through */
    case STATE_REGULAR:
        for (cmd = 0; cmd < (int) ARRAY_SIZE(Commands); cmd++) {
            for (pref = 0; state.args[0][pref] != '\0'; pref++) {
                if (Commands[cmd].name[pref] != state.args[0][pref]) {
                    break;
                }
            }
            if (state.args[0][pref] == '\0') {
                break;
            }
        }
        if (cmd == (int) ARRAY_SIZE(Commands)) {
            printf("command '%s' not found\n", state.args[0]);
            result = -1;
        } else {
            result = run_command(cmd, &state.args[1], state.num_args - 1, redir);
        }
        if (redir != stdout) {
            fclose(redir);
        }
        break;

    case STATE_EQUAL:
    case STATE_APPEND:
    case STATE_SUBTRACT:
        result = set_conf(state.args[0], (const char**) &state.args[1],
                state.num_args - 1, state.state - 1);
        break;

    case STATE_EXEC_SYSTEM:
        system(state.args[0]);
        break;

    case STATE_SYSTEM:
        if (state.num_args == 1) {
            set_conf(state.args[0], NULL, 0, SET_CONF_MODE_SET);
            break;
        }
        result = read_process(state.args[1], &p_args, &p_num_args);
        if (result == 0) {
            result = set_conf(state.args[0], (const char**) p_args,
                    p_num_args, SET_CONF_MODE_SET);
        }
        for (size_t i = 0; i < p_num_args; i++) {
            free(p_args[i]);
        }
        free(p_args);
        break;
    }

    for (size_t i = 0; i < state.num_args; i++) {
        free(state.args[i]);
    }
    free(state.args);

end:
    while (isblank(state.line[0])) {
        state.line++;
    }
    if (state.line[0] == ';') {
        state.line++;
        if (result == 0) {
            goto next_segment;
        }
    }
    return result;
}

int eval_file(FILE *fp)
{
    char *line = NULL;
    size_t a;
    ssize_t len;

    while ((len = getline(&line, &a, fp)) > 0) {
        if (line[len - 1] == '\n') {
            len--;
            line[len] = '\0';
        }
        if (run_command_line(line) != 0) {
            free(line);
            return false;
        }
    }
    free(line);
    if (errno != 0) {
        fprintf(stderr, "getline: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

