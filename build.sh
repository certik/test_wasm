#!/bin/bash

set -ex

as exit.asm -o exit.o
ld exit.o -L/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/lib/ -lSystem -o test.x

#clang f.c -o test.x
#clang g.c -o test.x
clang++ -g -W -Wall -Werror parse_macho.cpp -o parse_macho && ./parse_macho
clang++ -g -W -Wall -Werror write_macho.cpp -o write_macho && ./write_macho
