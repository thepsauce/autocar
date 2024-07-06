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
    while (1) {
        sleep(1);
    }

    return 0;
}

