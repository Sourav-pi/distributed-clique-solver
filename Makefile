CXX      = g++
MPICXX   = mpic++
CXXFLAGS = -O3 -std=gnu++11
all: seq main

seq: seq.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

main: mpi.cpp
	$(MPICXX) $(CXXFLAGS) $< -o $@

clean:
	rm -f seq main out_*.txt test_*.txt

.PHONY: all clean
