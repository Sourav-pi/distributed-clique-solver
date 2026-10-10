CXX = c++
MPICXX ?= mpic++
CXXFLAGS ?= -O3 -std=c++17 -Wall -Wextra -Wpedantic
PYTHON ?= python3

all: sequential mpi
sequential: build/clique-seq
mpi: build/clique-mpi

build:
	mkdir -p build
build/clique-seq: src/seq.cpp src/solver.hpp | build
	$(CXX) $(CXXFLAGS) $< -o $@
build/clique-mpi: src/mpi.cpp src/solver.hpp | build
	$(MPICXX) $(CXXFLAGS) $< -o $@
test: all
	$(PYTHON) tests/test_correctness.py
clean:
	rm -rf build
.PHONY: all sequential mpi test clean
