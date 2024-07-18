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

## Config

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
| IGNORE\_HEADER\_CHANGE | if header files should be checked for changes | false |
| INTERVAL | re-check interval in milliseconds | 100 |
| ERR\_FILE | where errors of the compiler should go | stderr |
| PROMPT | customize the prompt of the cli | >>>  |

## Arguments

| Flag | Purpose |
| ---- | ------- |
|--help\|-h | shows all arguments |
|--verbose\|-v [arg] | enable verbose output (`-vdebug` for maximum verbosity) |
|--config\|-c \<name\> | specify a config (default: `autocar.conf`) |
|--no-config | start without any config; load default options |
|--allow-parent-paths | allows paths to be in a parent directory |

## Tests

Tests are all ran if they have a main object, autocar simply checks if an object
file has a function called `main` and then declares that object as main object.
All main objects result in an executable that is stored in build and has no file
extension. If a .data file is present, this data is used to compare against the
output of the test. If a .input file is present, it is sent as `stdin` into the
test. If neither .input nor .data are present, the test is ignored.

## CLI

The cli allows adding of (test) files/folders and running.

1. `add [files] [-t files] [-r files]` adds given files to the file list, (`-t` for tests and `-r` for recursive directories)
2. `config` show all config options
3. `delete [files]` deletes given files from the file list
4. `echo [args]` prints the expanded arguments to stdout
5. `help` show help
6. `list` list all files
7. `pause` un-/pause the builder
8. `run <name> <args>` run file with given name. Use `run` without any arguments
    to list all main programs. Use `run $<index> <args>` for convenience.
9. `source [files]` runs all given files as autocar script
10. `quit` quit all


#### Variables

See below on how to set a variable.

Variables are stored internally as upper case values in a sorted list, every
variable has an array of strings as value. Case is ignored for variables:
`a` and `A` are the same variables. Variables have no limit on their name, one
could set a variable with name: `{holy* this/ is a weirdname(* `. But when
setting a variable with this name, all special characters have to be escaped.

#### Command line

A command line may be of these forms:
1. `<command> <args> [> output]`
2. `<name> = <args>`
3. `<name> += <args>`
4. `<name> -= <args>`
5. `<name> := <shell>`
6. `:<shell>`

Any argument, command or name can be quoted and quotation marks as well as
spaces can be escaped. To write multiple commands on a single line, use ';', the
semicolon can also be escaped.

The first one executes the command with given arguments and optionally redirects
the output to a file, note that this only works when using `config`; use this to
write the configuration variables to a file. The second one sets the config
value called `<name>` to given arguments. `+=` and `-=` also modify a variable
but `+=` appends and `-=` subtracts to/from the values. `:=` runs given shell
command and sets the variable to that; it is read as space separated array.
`:<shell>` simply runs given shell command.

Note on `<shell>`:
Sequences like `$1` are not sent to the shell but are still interpreted by the
command parser.

#### Dollar $ expansion

There are two interpretations the command parser does:
1. `$[0-9]+`
2. `$[^ \t]+`

The first expands to the the file path of the file at given index starting from
1.

The second expands to the variable value like this:
```sh
A=1 2 3
echo $A
1 2 3
echo a$A
a1
B=$A $A
echo $B
1 2 3 1 2 3
```

If any file should be rebuild, the best way is to remove the object
file/executable from the file system.

## Todo

- cleanup
- build script
