#!/usr/bin/env python3
"""Integration tests against an independent exhaustive oracle."""
import argparse
from pathlib import Path
import random
import shlex
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'scripts'))
from common import read_graph, validate, brute_force
from generate import generate

parser = argparse.ArgumentParser()
parser.add_argument('--sequential-only', action='store_true')
parser.add_argument('--mpi-flags', default='')
ARGS = parser.parse_args()


class Correctness(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.folder = Path(self.temp.name)

    def run_case(self, text, processes=(1, 2, 4)):
        source, output = self.folder/'input.txt', self.folder/'output.txt'
        source.write_text(text)
        graph = read_graph(source)
        expected = brute_force(graph)
        commands = [[str(ROOT/'build/clique-seq')]]
        if not ARGS.sequential_only:
            commands += [['mpirun'] + shlex.split(ARGS.mpi_flags) +
                         ['-np', str(p), str(ROOT/'build/clique-mpi')] for p in processes]
        for command in commands:
            with self.subTest(command=command, text=text):
                output.unlink(missing_ok=True)
                run = subprocess.run(command + [str(source), str(output)], capture_output=True, text=True, timeout=30)
                self.assertEqual(run.returncode, 0, run.stderr)
                self.assertEqual(validate(graph, output), expected)

    def test_regressions_and_edge_cases(self):
        cases = [
            '0 0 0\n',
            '1 0 0\n100 1\n',  # over-budget roots and empty optimum
            '1 0 1\n100 1\n',
            '2 1 5\n10 5\n1 1\n0 1\n',  # feasible prefix with infeasible extension
            '3 0 10\n7 1\n50 11\n8 2\n',
            '3 3 3\n10 1\n20 1\n30 1\n0 1\n0 2\n1 2\n',
            '3 3 3\n0 1\n0 1\n0 1\n0 1\n0 2\n1 2\n',
            '3 3 1000000000\n700000000 1\n700000000 1\n700000000 1\n0 1\n0 2\n1 2\n',
            '3 3 1000000000\n999999999 999999998\n1000000000 999999999\n1 1\n0 1\n0 2\n1 2\n',
            (ROOT/'examples/tiny.txt').read_text(),
        ]
        for text in cases:
            self.run_case(text)

    def test_random_oracle_comparison(self):
        for seed in range(24):
            rng = random.Random(seed)
            n = rng.randrange(2, 13)
            self.run_case(generate(n, rng.choice([0, .2, .5, .8, 1]), 30, 12,
                                   rng.randrange(0, 45), seed), processes=(1, 3))

    def test_vertex_limit(self):
        source, output = self.folder/'limit.txt', self.folder/'out.txt'
        source.write_text('1200 0 1\n' + '1 1\n'*1199 + '2 1\n')
        graph = read_graph(source)
        commands = [[str(ROOT/'build/clique-seq')]]
        if not ARGS.sequential_only:
            commands.append(['mpirun'] + shlex.split(ARGS.mpi_flags) + ['-np', '4', str(ROOT/'build/clique-mpi')])
        for command in commands:
            run = subprocess.run(command+[str(source), str(output)], capture_output=True, text=True, timeout=30)
            self.assertEqual(run.returncode, 0, run.stderr)
            self.assertEqual(validate(graph, output), 2)

    def test_invalid_inputs_fail(self):
        invalid = ['3 0 1\n1000000000 1\n1000000000 1\n1000000000 1\n', '1201 0 0\n', '1 0 -1\n1 1\n', '1 0 1\n1 0\n',
                   '2 1 1\n1 1\n1 1\n0 2\n', '1 0 1\n',
                   '2 2 1\n1 1\n1 1\n0 1\n0 1\n',
                   '3 2 1\n1 1\n1 1\n1 1\n0 1\n1 0\n',
                   '1 0 1\n1 1\nextra\n']
        source = self.folder/'bad.txt'
        for text in invalid:
            source.write_text(text)
            commands = [[str(ROOT/'build/clique-seq')]]
            if not ARGS.sequential_only:
                commands.append(['mpirun'] + shlex.split(ARGS.mpi_flags) + ['-np', '2', str(ROOT/'build/clique-mpi')])
            for command in commands:
                run = subprocess.run(command+[str(source), str(self.folder/'out.txt')], capture_output=True, text=True, timeout=30)
                self.assertNotEqual(run.returncode, 0)

    def test_checker_rejects_invalid_and_suboptimal_outputs(self):
        graph = read_graph(ROOT/'examples/tiny.txt')
        output = self.folder/'bad-output.txt'
        for text in ['16\n0 0\n', '0\n-1\n', '0\n4\n', '13\n2 3\n', '24\n0 1 2\n', '99\n0 1\n', '0\n\nextra\n']:
            output.write_text(text)
            with self.assertRaises(ValueError):
                validate(graph, output)
            run = subprocess.run([sys.executable, str(ROOT/'scripts/check.py'), str(ROOT/'examples/tiny.txt'), str(output)], capture_output=True)
            self.assertNotEqual(run.returncode, 0)
        output.write_text('8\n0\n')
        self.assertEqual(validate(graph, output), 8)
        run = subprocess.run([sys.executable, str(ROOT/'scripts/check.py'), str(ROOT/'examples/tiny.txt'), str(output), '--optimal'], capture_output=True)
        self.assertNotEqual(run.returncode, 0)

    def test_generator_reproducibility(self):
        self.assertEqual(generate(12, .5, 30, 12, 20, 42), generate(12, .5, 30, 12, 20, 42))
        self.assertNotEqual(generate(12, .5, 30, 12, 20, 42), generate(12, .5, 30, 12, 20, 43))


if __name__ == '__main__':
    unittest.main(argv=[sys.argv[0]], verbosity=2)
