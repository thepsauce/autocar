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
| INIT | run this at the start of the cli |  |
| PROMPT | customize the prompt of the cli | >>>  |

# Tests

Tests are all ran if they have a main object, autocar simply checks if an object
file has a function called `main` and then declares that object as main object.
All main objects result in an executable that is stored in build and has no file
extension. If a .data file is present, this data is used to compare against the
output of the test. If a .input file is present, it is sent as `stdin` into the
test.

# CLI

The cli allows adding of (test) files/folders and running.

1. `add [files] -t [files]` adds given files to the file list, (`-t` for tests)
2. `delete [files]` deletes given files from the file list
3. `help` show help
4. `list` list all files
5. `run [number]` run given index, use `run` without any arguments to list all
   main programs
6. `pause` un-/pause the builder
7. `quit` quit all

# Todo

- recursion
- more cli commands
- cleanup
- build script

