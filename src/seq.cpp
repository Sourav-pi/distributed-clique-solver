#include "solver.hpp"
#include <chrono>

int main(int argc, char** argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: clique-seq INPUT OUTPUT\n");
        return 1;
    }
    try {
        auto start = chrono::steady_clock::now();
        read_graph(argv[1]);
        auto search_start = chrono::steady_clock::now();
        solve_all();
        fprintf(stderr, "Search seconds: %.6f\n",
                chrono::duration<double>(chrono::steady_clock::now() - search_start).count());
        write_output(argv[2]);
        fprintf(stderr, "Solver seconds: %.6f\n",
                chrono::duration<double>(chrono::steady_clock::now() - start).count());
        return 0;
    } catch (const exception& error) {
        fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
