#!/bin/bash

export PATH="/usr/local/opt/llvm/bin:$PATH"

clang++ -g -O3 -std=c++0x \
   $(llvm-config --cxxflags --ldflags --system-libs --libs core native) \
   ../src/lang.cc
