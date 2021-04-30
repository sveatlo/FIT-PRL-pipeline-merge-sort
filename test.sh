#!/bin/bash

set -e

BINARY_NAME="pms"
DATA_FILENAME="numbers"
N=16
PC=5 #log_2(N)+1

dd if=/dev/urandom of="$DATA_FILENAME" bs=1 count="$N" >/dev/null 2>&1

#compile
mpic++ -std=c++11 -Wall -Wextra -pedantic -O0 pms.cpp -o "$BINARY_NAME" >/dev/null
#run
mpirun -v -np $PC pms

# cleanup
rm -f "$BINARY_NAME" "$DATA_FILENAME"
