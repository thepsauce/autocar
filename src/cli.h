#ifndef CLI_H
#define CLI_H

#include <pthread.h>
#include <stdbool.h>

extern volatile bool CliRunning;
extern volatile bool CliWantsPause;

/**
 * @brief Initializes the command line interface and starts a thread waiting for user
 * input.
 *
 * @return Whether initialization was successful.
 */
bool run_cli(void);

#endif

