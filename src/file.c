#include "file.h"

struct file *add_file(struct file *parent, const char *name)
{
    struct file *new;
    struct file *child;

    new = malloc(sizeof(*new));

    if (strncpy(new->name, sizeof(new->name), name) ==
            new->name + sizeof(new->name)) {
        return NULL;
    }

    child = file->child;
    if (child == NULL) {
        file
    }
    while (child->next != NULL) {
        child = child->next;
    }
}

struct file *find_file(struct file *file, const char *name)
{
}

struct file *add_path(const char *path)
{
    struct file *cur = Current;

    if (*path == '~') {
        path++;
        cur = Home;
    } else if (*path == '/') {
        path++;
        cur = Current;
    }
    for (; *path != '\0'; path++) {
        char *const sep = strchr(path, '/');
        if (sep - path > MAX_NAME) {
            fprintf(stderr, "file component too long: exceeds '%zu'\n", MAX_NAME);
            return -1;
        }
        struct file *const child = find_file(path, sep - path);
        if (sep == NULL) {
            if (child == NULL) {
                child = add_file(cur, path);
                if (child == NULL) {
                    return -1;
                }
                cur = child;
            }
            break;
        }
        if (child == NULL) {
            return -1;
        }
        cur = child;
        path = sep + 1;
    }
    return cur;
}
