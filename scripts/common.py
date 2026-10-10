"""Input parsing, feasibility validation, and an independent exhaustive oracle."""
from pathlib import Path


def read_graph(path):
    tokens = list(map(int, Path(path).read_text().split()))
    if len(tokens) < 3:
        raise ValueError('missing header')
    n, m, budget = tokens[:3]
    if not 0 <= n <= 1200 or not 0 <= m <= n*(n-1)//2 or not 0 <= budget <= 10**9:
        raise ValueError('invalid header')
    if len(tokens) != 3 + 2*n + 2*m:
        raise ValueError('incorrect input length')
    profits, costs = tokens[3:3+2*n:2], tokens[4:3+2*n:2]
    if any(not 0 <= p <= 10**9 for p in profits) or any(not 1 <= c <= 10**9 for c in costs):
        raise ValueError('invalid vertex')
    adjacency = [set() for _ in range(n)]
    edges = tokens[3+2*n:]
    for u, v in zip(edges[::2], edges[1::2]):
        if not 0 <= u < n or not 0 <= v < n or u == v or v in adjacency[u]:
            raise ValueError('invalid or duplicate edge')
        adjacency[u].add(v)
        adjacency[v].add(u)
    return n, budget, profits, costs, adjacency


def validate(graph, output):
    n, budget, profits, costs, adjacency = graph
    lines = Path(output).read_text().splitlines()
    # Accept a zero-profit empty result without a vertex line.
    if len(lines) == 1 and lines[0].strip() == '0':
        lines.append('')
    if len(lines) != 2:
        raise ValueError('output must contain exactly two lines')
    profit = int(lines[0])
    ids = list(map(int, lines[1].split()))
    if len(ids) != len(set(ids)) or any(not 0 <= v < n for v in ids):
        raise ValueError('duplicate or out-of-range vertex ID')
    if any(v not in adjacency[u] for i, u in enumerate(ids) for v in ids[i+1:]):
        raise ValueError('vertices do not form a clique')
    if sum(costs[v] for v in ids) > budget:
        raise ValueError('budget exceeded')
    if sum(profits[v] for v in ids) != profit:
        raise ValueError('reported profit does not match vertices')
    return profit


def brute_force(graph):
    n, budget, profits, costs, adjacency = graph
    if n > 22:
        raise ValueError('exhaustive oracle is limited to 22 vertices')
    best = 0
    # Enumerate subsets independently of the solvers' ordering and pruning bounds.
    for mask in range(1 << n):
        ids = [v for v in range(n) if mask & (1 << v)]
        if sum(costs[v] for v in ids) <= budget and all(
                v in adjacency[u] for i, u in enumerate(ids) for v in ids[i+1:]):
            best = max(best, sum(profits[v] for v in ids))
    return best
