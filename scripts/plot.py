#!/usr/bin/env python3
import argparse
import csv
from pathlib import Path
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

parser = argparse.ArgumentParser(description='Plot benchmark medians and MPI-one-rank scaling.')
parser.add_argument('summary', type=Path)
parser.add_argument('--output', type=Path, default=Path('benchmarks/scaling.png'))
args = parser.parse_args()
with args.summary.open() as f:
    rows = list(csv.DictReader(f))
mpi = sorted((r for r in rows if r['implementation'] == 'mpi'), key=lambda r: int(r['processes']))
p = [int(r['processes']) for r in mpi]
seq = float(next(r['median_seconds'] for r in rows if r['implementation'] == 'sequential'))
fig, axes = plt.subplots(1, 3, figsize=(13, 4))
axes[0].plot(p, [float(r['median_seconds']) for r in mpi], 'o-', label='MPI')
axes[0].axhline(seq, linestyle='--', label='Sequential')
axes[0].set_ylabel('Median wall time (s)')
axes[0].legend()
axes[1].plot(p, [float(r['mpi1_speedup']) for r in mpi], 'o-', label='Measured')
axes[1].plot(p, p, '--', label='Ideal')
axes[1].set_ylabel('Speedup vs MPI P=1')
axes[1].legend()
axes[2].plot(p, [float(r['mpi1_efficiency']) for r in mpi], 'o-')
axes[2].set_ylabel('Efficiency (speedup / P)')
for ax in axes:
    ax.set_xlabel('Total MPI ranks (P)')
    ax.set_xticks(p)
    ax.grid(alpha=.25)
fig.suptitle('End-to-end timings; rank 0 coordinates and searches when P > 1')
fig.tight_layout()
args.output.parent.mkdir(parents=True, exist_ok=True)
fig.savefig(args.output, dpi=180)
