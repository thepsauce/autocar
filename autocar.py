#!/bin/python

import os

os.environ["CC"] = "gcc"
os.environ["C_FLAGS"] = "  -std=gnu99    -Wall -Wextra -Werror -Wpedantic -g -fsanitize=address  "
os.environ["C_LIBS"] = "-lm -lbfd"
os.environ["SOURCES"] = "src///"
os.environ["TESTS"] = "tests/"
os.environ["BUILD"] = "bulid"
os.environ["INTERVAL"] = "100"

for name, value in os.environ.items():
    print(f"{name}={value}")
