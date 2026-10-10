#!/usr/bin/env python3
import argparse
import random
from pathlib import Path


def generate(n, density, max_profit, max_cost, budget, seed):
    if not 0 <= n <= 1200 or not 0 <= density <= 1 or not 1 <= max_profit <= 10**9 or not 1 <= max_cost <= 10**9 or not 0 <= budget <= 10**9:
        raise ValueError('parameters outside supported ranges')
    rng = random.Random(seed)
    profits = [rng.randint(1, max_profit) for _ in range(n)]
    costs = [rng.randint(1, max_cost) for _ in range(n)]
    edges = [(u, v) for u in range(n) for v in range(u+1, n) if rng.random() < density]
    return '\n'.join([f'{n} {len(edges)} {budget}'] +
                     [f'{p} {c}' for p, c in zip(profits, costs)] +
                     [f'{u} {v}' for u, v in edges]) + '\n'


def main():
    parser = argparse.ArgumentParser(description='Generate a seeded random budgeted-clique instance.')
    parser.add_argument('--vertices', type=int, default=100)
    parser.add_argument('--density', type=float, default=0.5)
    parser.add_argument('--max-profit', type=int, default=100)
    parser.add_argument('--max-cost', type=int, default=100)
    parser.add_argument('--budget', type=int, default=500)
    parser.add_argument('--seed', type=int, default=42)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    try:
        content = generate(args.vertices, args.density, args.max_profit, args.max_cost, args.budget, args.seed)
        if args.output:
            args.output.write_text(content)
        else:
            print(content, end='')
    except (ValueError, OSError) as error:
        parser.error(str(error))


if __name__ == '__main__':
    main()
