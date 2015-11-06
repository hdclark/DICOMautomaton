#!/bin/bash


g++ -std=c++11 -Wall Parse.cc -o parse -lygor

exit

g++ -std=c++11 Dump.cc  -o dump  -lygor

g++ -std=c++11 Parse.cc -o parse -lygor

