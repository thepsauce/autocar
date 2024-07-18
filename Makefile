CC = gcc
BUILD = bulid
C_FLAGS = -std=gnu99 -Wall -Wextra -Werror -Wpedantic -g -fsanitize=address
C_LIBS = -lm -lbfd -lreadline
RAW_OBJECTS = src/args src/cli src/cmd src/conf src/eval src/file src/salloc src/util
RAW_MAIN_OBJECTS = src/main tests/lol

OBJECTS = $(addprefix $(BUILD)/,$(addsuffix .o,$(RAW_OBJECTS)))
MAIN_OBJECTS = $(addprefix $(BUILD)/,$(addsuffix .o,$(RAW_MAIN_OBJECTS)))

.PHONY: all
all: $(MAIN_OBJECTS) $(RAW_MAIN_OBJECTS:%=$(BUILD)/%)

$(BUILD)/%.o: %.c
	$(shell mkdir -p $(dir $@))
	$(CC) $(C_FLAGS) -c $< -o $@

$(BUILD)/%: $(BUILD)/%.o $(OBJECTS)
	$(shell mkdir -p $(dir $@))
	$(CC) $(C_FLAGS) $(OBJECTS) $< -o $@ $(C_LIBS)

.PHONY: clean
clean:
	rm -rf $(BUILD)
