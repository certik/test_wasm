#!/bin/bash

set -ex
clang++ -W -Wall -Werror parse_macho.cpp && ./a.out
