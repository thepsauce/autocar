#!/bin/python

import os

os.environ["LOL"] = "get outta here"

for name, value in os.environ.items():
    print(f"{name}={value}")
