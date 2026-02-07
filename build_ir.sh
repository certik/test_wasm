#!/bin/bash

set -ex

clang++ -g -W -Wall -Werror write_macho_from_ir.cpp -o write_macho_from_ir
./write_macho_from_ir
chmod +x test_ir.x
./test_ir.x
