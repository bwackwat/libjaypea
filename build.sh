#!/bin/bash

clang++ -std=c++11 -g -O0 -Weverything -Wextra -Wall -pedantic \
./src/server.cpp -o server

clang++ -std=c++11 -g -O0 -Weverything -Wextra -Wall -pedantic \
./src/client.cpp -o client
