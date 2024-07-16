#ifndef CLI_H
#define CLI_H

#include <pthread.h>
#include <stdbool.h>

extern volatile bool CliRunning;
extern volatile bool CliWantsPause;

bool run_cli(void);

#endif

