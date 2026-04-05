#!/usr/bin/env python3
"""
Testcase generator for Budgeted Maximum Weight Clique.

Usage:
    python3 gen.py <n> <density> <max_p> <max_c> <budget> [seed]

Args:
    n        : number of vertices
    density  : edge probability between any pair (0.0 - 1.0)
    max_p    : maximum profit per vertex (uniform [1, max_p])
    max_c    : maximum cost per vertex   (uniform [1, max_c])
    budget   : global budget B
    seed     : random seed (default: 42)

Output format (stdout):
    Line 1       : N M B
    Next N lines : p(v) c(v)
    Next M lines : u v
"""

import random
import sys


def main():
    if len(sys.argv) < 6:
        print(__doc__)
        sys.exit(1)

    n       = int(sys.argv[1])
    density = float(sys.argv[2])
    max_p   = int(sys.argv[3])
    max_c   = int(sys.argv[4])
    budget  = int(sys.argv[5])
    seed    = int(sys.argv[6]) if len(sys.argv) > 6 else 42

    rng = random.Random(seed)

    profits = [rng.randint(1, max_p) for _ in range(n)]
    costs   = [rng.randint(1, max_c) for _ in range(n)]

    edges = []
    for u in range(n):
        for v in range(u + 1, n):
            if rng.random() < density:
                edges.append((u, v))

    m = len(edges)
    print(n, m, budget)
    for i in range(n):
        print(profits[i], costs[i])
    for u, v in edges:
        print(u, v)


if __name__ == "__main__":
    main()
