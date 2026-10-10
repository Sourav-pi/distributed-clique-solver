#!/usr/bin/env python3
import argparse
import sys
from common import read_graph, validate, brute_force


def main():
    parser = argparse.ArgumentParser(description='Check feasibility; optionally prove small-instance optimality.')
    parser.add_argument('input')
    parser.add_argument('output')
    parser.add_argument('--optimal', action='store_true', help='compare against exhaustive oracle (N<=22)')
    args = parser.parse_args()
    try:
        graph = read_graph(args.input)
        profit = validate(graph, args.output)
        if args.optimal and profit != brute_force(graph):
            raise ValueError('feasible solution is not optimal')
        print('Feasible and optimal (exhaustive oracle).' if args.optimal else
              'Feasible; optimality has not been checked.')
    except (ValueError, OSError) as error:
        print(f'Validation failed: {error}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
