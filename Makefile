CC = gcc
C_FLAGS = -std=gnu99 -Wall -Wextra -Werror -Wpedantic -g -fsanitize=address
C_LIBS = -lm -lbfd -lreadline
BUILD = bulid
OBJECTS = bulid/src/args.o bulid/src/cli.o bulid/src/cmd.o bulid/src/conf.o bulid/src/eval.o bulid/src/file.o bulid/src/salloc.o bulid/src/util.o
MAIN_OBJECTS = bulid/src/main.o bulid/tests/lol.o
MAIN_EXECUTABLES = bulid/src/main bulid/tests/lol

.PHONY: all
all: $(MAIN_EXECUTABLES) $(OBJECTS) $(MAIN_OBJECTS)
	$(foreach exec,$(MAIN_EXECUTABLES),$(info $(exec)))

$(BUILD)/%.o: %.c
	$(shell mkdir -p $(dir $@))
	$(CC) $(C_FLAGS) -c $< -o $@

$(BUILD)/%: $(BUILD)/%.o $(OBJECTS)
	$(shell mkdir -p $(dir $@))
	$(CC) $(C_FLAGS) $(OBJECTS) $< -o $@ $(C_LIBS)

.PHONY: clean
clean:
	rm -rf $(BUILD)
