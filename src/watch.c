#include "watch.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/inotify.h>

int NotifyFd;

pthread_t WatchThreadId;

struct file_list Files;

static struct file *find_file_by_watch(int wd)
{
    for (size_t i = 0; i < Files.n; i++) {
        if (Files.p[i].wd == wd) {
            return &Files.p[i];
        }
    }
    return NULL;
}

void *watch_thread(void *unused)
{
    (void) unused;

    char buf[4096];
    ssize_t len;
    struct inotify_event *ie;

    while (1) {
        len = read(NotifyFd, buf, sizeof(buf));
        pthread_mutex_lock(&Files.lock);
        for (char *b = buf; b < buf + len; b += sizeof(*ie) + ie->len) {
            ie = (struct inotify_event*) b;
            struct file *const file = find_file_by_watch(ie->wd);
            char *const ext = strrchr(ie->name, '.');
            if (ext == NULL) {
                continue;
            }
            fprintf(stderr, "log: something happened to [%s]\n", ext);
            if (ie->mask & IN_CREATE) {
                fprintf(stderr, "log: file created: %s\n", ext);
            }
            if (ie->mask & IN_DELETE) {
                fprintf(stderr, "log: file deleted: %s\n", ext);
            }
            if (ie->mask & IN_MOVED_FROM) {
                fprintf(stderr, "log: file moved away: %s\n", ext);
            }
            if (ie->mask & IN_MOVED_TO) {
                fprintf(stderr, "log: file moved to: %s\n", ext);
            }
            if (ie->mask & IN_MOVE_SELF) {
                fprintf(stderr, "log: I moved away: %s\n", ext);
            }
            if (ie->mask & IN_DELETE_SELF) {
                fprintf(stderr, "log: I was deleted: %s\n", ext);
            }
        }
        pthread_mutex_unlock(&Files.lock);
    }
}

int init_watch(void)
{
    NotifyFd = inotify_init();
    if (NotifyFd < 0) {
        fprintf(stderr, "error: could not init inotify: %s\n",
                strerror(errno));
        return -1;
    }
    const int err = pthread_create(&WatchThreadId, 0, watch_thread, NULL);
    if (err != 0) {
        fprintf(stderr, "error: could not create watch thread: %s\n",
                strerror(err));
        return -1;
    }
    return 0;
}

static struct file *search_file(const char *path, size_t *pIndex)
{
    size_t l, r;

    l = 0;
    r = Files.n;
    while (l < r) {
        const size_t m = (l + r) / 2;

        const int cmp = strcmp(Files.p[m].path, path);
        if (cmp == 0) {
            if (pIndex != NULL) {
                *pIndex = m;
            }
            return &Files.p[m];
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

static struct file *add_file(const char *path)
{
    struct file *p;

    p = reallocarray(Files.p, Files.n + 1, sizeof(*Files.p));
    if (p == NULL) {
        return NULL;
    }
    Files.p = p;
    p += Files.n;
    p->path = strdup(path);
    if (p->path == NULL) {
        return NULL;
    }
    Files.n++;
    return p;
}

int watch_file_or_directory(const char *path)
{
    struct file *file;
    int wd;
    size_t index;

    file = search_file(path, &index);
    if (file != NULL) {
        return 1;
    }

    wd = inotify_add_watch(NotifyFd, path,
            IN_CREATE | IN_DELETE | IN_MOVE |
            IN_ATTRIB | IN_MODIFY);
    if (wd == -1) {
        fprintf(stderr, "%s\n", strerror(errno));
        return -1;
    }

    pthread_mutex_lock(&Files.lock);
    file = add_file(path);
    if (file == NULL) {
        return -1;
    }
    file->wd = wd;
    pthread_mutex_unlock(&Files.lock);
    return 0;
}
