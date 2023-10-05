#!/bin/bash

set -ex

as exit.asm -o exit.o
ld exit.o -L/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/lib/ -lSystem -o test.x
clang++ -g -W -Wall -Werror parse_macho.cpp && ./a.out
