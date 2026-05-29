#!/usr/bin/env bash

set -eux

printf 'Compiling example now...\n'
g++ --std=c++17 \
  ../Structs.cc \
  ../Alignment*cc \
  ../Tables.cc \
  ../Metadata.cc \
  ../Dose_Meld.cc \
  ../Regex_Selectors.cc \
  \
  ./gen-cpp/DCMA_constants.cc \
  ./gen-cpp/DCMA_types.cc \
  ./gen-cpp/Receiver.cc \
  Serialization.cc \
  Receiver_server.impl.cc \
  \
  -o rpc_example_server \
  `pkg-config --cflags --libs thrift` \
  -lygor \
  -lboost_system \
  -pthread
#  -I./gen-cpp/ \


g++ --std=c++17 \
  ../Structs.cc \
  ../Alignment*cc \
  ../Tables.cc \
  ../Metadata.cc \
  ../Dose_Meld.cc \
  ../Regex_Selectors.cc \
  \
  ./gen-cpp/DCMA_constants.cc \
  ./gen-cpp/DCMA_types.cc \
  ./gen-cpp/Receiver.cc \
  Serialization.cc \
  Receiver_client.impl.cc \
  \
  -o rpc_example_client \
  `pkg-config --cflags --libs thrift` \
  -lygor \
  -lboost_system \
  -pthread
#  -I./gen-cpp/ \


# Note: can replace pkg-config thrift with thrift-z, but then you have to manually link zlib for some reason...

#  generated/Receiver_server.skeleton.cpp \

