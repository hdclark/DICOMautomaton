#!/usr/bin/env bash

function command_exists () {
    type "$1" &> /dev/null ;
}

CC="g++" # triSYCL
command_exists syclcc && CC="syclcc" # hipSYCL or whenever a wrapper exists.

$CC test.cpp -o test -pthread

