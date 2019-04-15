#!/bin/bash
mkdir -p ../test_bin
g++ -std=c++17 -I ../inc/ -o ../test_bin/optgen_test optgen_tests.cpp && ../test_bin/optgen_test
