#ifndef CMD_H
#define CMD_H

#include <stdio.h>

#define CMD_ADD         0
#define CMD_CONFIG      1
#define CMD_DELETE      2
#define CMD_ECHO        3
#define CMD_HELP        4
#define CMD_LIST        5
#define CMD_PAUSE       6
#define CMD_QUIT        7
#define CMD_RUN         8
#define CMD_SOURCE      9
#define CMD_MAX        10

extern const char *Commands[CMD_MAX];

/**
 * @brief Runs an internal command with given args.
 *
 * @param cmd Then command to run.
 * @param args The arguments to pass to the command.
 * @param num_args The number of arguments to pass.
 * @param out Then file to output to.
 *
 * @return 0 if command succeeded, -1 otherwise.
 */
int run_command(int cmd, char **args, size_t num_args, FILE *out);

/**
 * @brief Run given line.
 *
 * Lines are split at ';' if not escaped, for each segment, there are these
 * operators:
 * += Append to values of a variable.
 * -= Subtract values from variable.
 * = Set variable to values.
 * > Redirect command to file.
 * Only one operator can be applied at a time.
 */
int run_command_line(char *line);

/**
 * @brief Runs each line of given file.
 */
int eval_file(FILE *fp);

/**
 * @brief Opens path and calls `eval_file()`.
 */
int source_path(const char *path);

#endif

