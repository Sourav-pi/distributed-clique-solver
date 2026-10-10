#!/usr/bin/env python3
"""Repeated end-to-end timings, validated outputs, and explicit baselines."""
import argparse
import csv
import hashlib
import json
import os
import re
from pathlib import Path
import platform
import shlex
import statistics
import subprocess
import sys
import time
from common import read_graph, validate, brute_force

ROOT = Path(__file__).resolve().parents[1]


def version(command):
    try:
        p = subprocess.run(command, capture_output=True, text=True, timeout=10)
        return (p.stdout + p.stderr).strip()
    except (OSError, subprocess.TimeoutExpired) as error:
        return str(error)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input', type=Path)
    parser.add_argument('--processes', type=int, nargs='+', default=[1, 2, 4])
    parser.add_argument('--repeats', type=int, default=3)
    parser.add_argument('--warmups', type=int, default=1)
    parser.add_argument('--timeout', type=float, default=120)
    parser.add_argument('--mpirun', default='mpirun')
    parser.add_argument('--mpi-flags', default='', help='quoted launcher flags, e.g. "--oversubscribe"')
    parser.add_argument('--seq-bin', type=Path, default=ROOT/'build/clique-seq')
    parser.add_argument('--mpi-bin', type=Path, default=ROOT/'build/clique-mpi')
    parser.add_argument('--output-dir', type=Path, default=ROOT/'benchmarks/local')
    parser.add_argument('--graph-metadata', type=Path, help='JSON file with generation parameters')
    args = parser.parse_args()
    if args.repeats < 1 or args.warmups < 0 or args.timeout <= 0 or any(p < 1 for p in args.processes):
        parser.error('invalid trial/process settings')
    counts = sorted(set([1] + args.processes))
    graph = read_graph(args.input)
    expected = brute_force(graph) if graph[0] <= 22 else None
    args.output_dir.mkdir(parents=True, exist_ok=True)
    configurations = [('sequential', 1, [str(args.seq_bin.resolve())])]
    configurations += [('mpi', p, [args.mpirun] + shlex.split(args.mpi_flags) +
                         ['-np', str(p), str(args.mpi_bin.resolve())]) for p in counts]
    rows, commands = [], []
    for implementation, processes, prefix in configurations:
        output = args.output_dir/f'{implementation}-{processes}.txt'
        command = prefix + [str(args.input.resolve()), str(output.resolve())]
        commands.append(command)
        for trial in range(-args.warmups, args.repeats):
            output.unlink(missing_ok=True)
            start = time.perf_counter()
            run = subprocess.run(command, capture_output=True, text=True, timeout=args.timeout)
            elapsed = time.perf_counter()-start
            if run.returncode:
                raise RuntimeError(f'Command failed: {shlex.join(command)}\n{run.stderr}')
            match = re.search(r'Search seconds:\s*([\d.]+)', run.stderr)
            if not match:
                raise ValueError('missing search timing diagnostic')
            search_seconds = float(match.group(1))
            profit = validate(graph, output)
            if expected is None:
                expected = profit
            if profit != expected:
                raise ValueError('objective mismatch between runs/oracle')
            if trial >= 0:
                rows.append(dict(implementation=implementation, processes=processes,
                                 trial=trial+1, seconds=elapsed, search_seconds=search_seconds, profit=profit))
        print(f'{implementation} P={processes}: median {statistics.median(r["seconds"] for r in rows if r["implementation"] == implementation and r["processes"] == processes):.6f}s')
    def median(implementation, p):
        return statistics.median(r['seconds'] for r in rows if r['implementation'] == implementation and r['processes'] == p)
    seq, mpi1 = median('sequential', 1), median('mpi', 1)
    summaries = []
    for implementation, p, _ in configurations:
        elapsed = median(implementation, p)
        summaries.append(dict(implementation=implementation, processes=p, median_seconds=elapsed,
                              median_search_seconds=statistics.median(r['search_seconds'] for r in rows if r['implementation']==implementation and r['processes']==p),
                              sequential_speedup=seq/elapsed,
                              mpi1_speedup=mpi1/elapsed if implementation == 'mpi' else '',
                              mpi1_efficiency=(mpi1/elapsed)/p if implementation == 'mpi' else ''))
    for name, records in [('trials.csv', rows), ('summary.csv', summaries)]:
        with (args.output_dir/name).open('w', newline='') as f:
            writer = csv.DictWriter(f, fieldnames=list(records[0]))
            writer.writeheader()
            writer.writerows(records)
    metadata = dict(timestamp_utc=time.strftime('%Y-%m-%dT%H:%M:%SZ', time.gmtime()),
                    platform=platform.platform(), machine=platform.machine(), processor=platform.processor(),
                    cpu_count=os.cpu_count(), python=sys.version,
                    hardware=version(['sysctl', '-n', 'machdep.cpu.brand_string']) if sys.platform == 'darwin' else platform.processor(),
                    source_sha256={str(path.relative_to(ROOT)): hashlib.sha256(path.read_bytes()).hexdigest()
                                   for path in sorted((ROOT/'src').glob('*')) if path.is_file()},
                    makefile_sha256=hashlib.sha256((ROOT/'Makefile').read_bytes()).hexdigest(),
                    compiler=version(['c++', '--version']), mpi_compiler=version(['mpic++', '--showme']),
                    mpi=version([args.mpirun, '--version']), git_revision=version(['git', '-C', str(ROOT), 'rev-parse', 'HEAD']),
                    git_dirty=bool(version(['git', '-C', str(ROOT), 'status', '--porcelain']).strip()),
                    input=str(args.input.resolve()), input_sha256=hashlib.sha256(args.input.read_bytes()).hexdigest(),
                    vertices=graph[0], edges=sum(map(len, graph[4]))//2, budget=graph[1],
                    generation=json.loads(args.graph_metadata.read_text()) if args.graph_metadata else None,
                    repeats=args.repeats, warmups=args.warmups, commands=commands,
                    timing_scope='Python subprocess wall time: launch, MPI initialization, input, broadcast, search, output, finalization; excludes generation, build, validation.',
                    optimality='exhaustive oracle' if graph[0] <= 22 else 'feasibility and agreement only')
    for name, binary in [('seq_binary_sha256', args.seq_bin), ('mpi_binary_sha256', args.mpi_bin)]:
        metadata[name] = hashlib.sha256(binary.read_bytes()).hexdigest()
    # Use repository-relative paths in published metadata.
    metadata = json.loads(json.dumps(metadata).replace(str(ROOT) + '/', ''))
    (args.output_dir/'metadata.json').write_text(json.dumps(metadata, indent=2)+'\n')


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, RuntimeError, subprocess.TimeoutExpired) as error:
        print(f'Benchmark failed: {error}', file=sys.stderr)
        sys.exit(1)
