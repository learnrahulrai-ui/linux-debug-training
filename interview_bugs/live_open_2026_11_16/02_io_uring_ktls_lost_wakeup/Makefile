CXX ?= g++
CC ?= cc
CXXFLAGS ?= -std=c++20 -O1 -g -Wall -Wextra -Werror -pedantic
CFLAGS ?= -O2 -g -Wall -Wextra
.PHONY: all exercise upstream-repro probe clean
all: exercise
exercise: source
source: source.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<
upstream-repro: upstream_repro.c
	$(CC) $(CFLAGS) -o $@ $< -luring -pthread
probe:
	sh ./probe_readonly.sh
clean:
	rm -f source upstream-repro
