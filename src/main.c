#include "args.h"
#include "watch.h"

#include <unistd.h>

int main(int argc, char **argv)
{
    if (!parse_args(argc, argv)) {
        return 1;
    }

    if (Args.needs_help) {
        usage(stdout, argv[0]);
        return 1;
    }

    init_watch();
    for (size_t i = 0; i < Args.num_files; i++) {
        watch_file_or_directory(Args.files[i]);
    }

    while (1) {
        sleep(1);
    }

    return 0;
}

