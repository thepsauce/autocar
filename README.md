# autocar

**auto**mated<br>
**c**ompiling<br>
**a**nd<br>
**r**unning<br>

(Also a pun because Auto is German for car.)

All starts with a project directory in which lies an autocar config. Simply run
autocar and it will start by searching all sources, then build them and (run
them). It does not run everything as that would be annoying for interactive
programs, that is why the cli includes a `run` command to run any executable
that was built. However test files are all run if either a corresponding .data
or .input file exists for them.

# Config

| Variable name | Purpose | Default |
| ------------- | ------- | ------- |
| CC | The compiler to use | gcc |
| DIFF | The diff program to use | diff |
| C\_FLAGS | flags to send the compiler | -g -fsanitize=address -Wall -Wextra -Werror |
| C\_LIBS | flags to send the linker | |
| BUILD | output directory for all building | build |
| EXT\_SOURCE | file extensions of source files, multiple can be specified separated by \| | .c |
| EXT\_HEADER | file extensions of header files | .h |
| EXT\_BUILD | file extensions of build files | .o |
| INTERVAL | re-check interval in milliseconds | 100 |
| ERR\_FILE | where errors of the compiler should go | stderr |
| PROMPT | customize the prompt of the cli | >>>  |

# Arguments

| Flag | Purpose |
| ---- | ------- |
|--help\|-h | shows all arguments |
|--verbose\|-v [arg] | enable verbose output (`-vdebug` for maximum verbosity) |
|--config\|-c \<name\> | specify a config (default: `autocar.conf`) |
|--no-config | start without any config; load default options |
|--allow-parent-paths | allows paths to be in a parent directory |

# Tests

Tests are all ran if they have a main object, autocar simply checks if an object
file has a function called `main` and then declares that object as main object.
All main objects result in an executable that is stored in build and has no file
extension. If a .data file is present, this data is used to compare against the
output of the test. If a .input file is present, it is sent as `stdin` into the
test. If neither .input nor .data are present, the test is ignored.

# CLI

The cli allows adding of (test) files/folders and running.

1. `add [files] [-t files] [-r files]` adds given files to the file list, (`-t` for tests and `-r` for recursive directories)
2. `delete [files]` deletes given files from the file list
3. `config` show all config options
4. `help` show help
5. `execute [files]` runs all given files as autocar config file
6. `list` list all files
7. `pause` un-/pause the builder
8. `run [number]` run given index, use `run` without any arguments to list all
   main programs
9. `quit` quit all

A command line may be of two forms:
1. `<command> <args>`
2. `<name> = <args>`

Any argument, command or name can be quoted and quotation marks as well as
spaces can be escaped. To write multiple commands on a single line, use ';', the
semicolon can also be escaped.

The first one executes the command with given arguments. The second one sets the
config value called \<name\> to given arguments.

If any file should be rebuild, the best way is to remove the object
file/executable from the file system.

# Todo

- cleanup
- build script
