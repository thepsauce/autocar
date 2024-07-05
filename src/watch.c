#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/inotify.h>

int NotifyFd;

pthread_t WatchThreadId;

void *watch_thread(void *unused)
{
    (void) unused;

    char buf[4096];
    ssize_t len;
    struct inotify_event *ie;
    struct stat st;

    while (1) {
        len = read(NotifyFd, buf, sizeof(buf));
        for (char *b = buf; b < buf + len; b += sizeof(*ie) + ie->len) {
            ie = (struct inotify_event*) b;
            if (ie->mask & IN_CREATE) {
                fprintf("log: file created: %s\n", ie->name);
            }
        }
    }
}

int init_watch(void)
{
    NotifyFd = inotify_init();
    if (NotifyFd < 0) {
        fprintf(stderr, "error: could not init inotify: %s\n", strerror(errno));
        return -1;
    }
    const int err = pthread_create(&WatchThreadId, 0, watch_thread, NULL);
    if (err != 0) {
        fprintf(stderr, "error: could not create watch thread: %s\n", strerror(err));
        return -1;
    }
    return 0;
}

void watch_file_or_directory(const char *path)
{

    tag.wd = inotify_add_watch(NotifyFd, name,
            IN_CREATE | IN_DELETE | IN_MOVE | IN_ATTRIB);
}
