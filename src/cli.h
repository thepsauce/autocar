#ifndef CLI_H
#define CLI_H

#include <pthread.h>
#include <stdbool.h>

extern volatile bool CliRunning;
extern volatile bool CliWantsPause;
extern pthread_mutex_t CliLock;

bool run_cli(void);

#endif

