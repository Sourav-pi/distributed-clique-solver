# Sequential versus MPI benchmark

Both executables use the same search engine in `src/solver.hpp`. Sequential and one-rank MPI call the same root traversal, bounds, candidate operations, and incumbent updates.

Measured on Apple M1 with 8 logical CPUs, Apple Clang, C++17 `-O3`, and Open MPI 5.0.9. Graph: 400 vertices, 39890 edges, density parameter 0.5, budget 500, seed 42, profits/costs in 1–100. One warmup and five measured trials per configuration, with MPI rank counts 1, 2, 4, and 8. All MPI runs explicitly use `--oversubscribe`.

| Implementation | Total ranks | Median search (s) | Median wall (s) | Search speedup vs seq | Wall speedup vs seq |
| --- | ---: | ---: | ---: | ---: | ---: |
| sequential | 1 | 0.318238 | 0.323844 | 1.00× | 1.00× |
| mpi | 1 | 0.317928 | 0.401806 | 1.00× | 0.81× |
| mpi | 2 | 0.330246 | 0.420448 | 0.96× | 0.77× |
| mpi | 4 | 0.115635 | 0.228112 | 2.75× | 1.42× |
| mpi | 8 | 0.070713 | 0.194423 | 4.50× | 1.67× |

![End-to-end runtime and MPI scaling](scaling.png)

Search timing excludes input/output, launch, MPI initialization/finalization, and graph broadcast. For multiple ranks it includes coordinator scheduling, worker search, incumbent communication, and completion of pending updates. Wall time includes the entire subprocess execution. MPI-one-rank wall time can exceed sequential even with the identical search because of MPI startup overhead.

All 25 measured outputs passed feasibility validation and agreed on profit 876. This graph is not independently checked for optimality. Tests compare both runners against an independent exhaustive oracle on small graphs.

MPI with P>1 uses a dedicated coordinator and P-1 search workers. Search work can vary with scheduling and incumbent arrival order. Eight ranks had the lowest measured end-to-end median among the tested configurations. These measurements are one-machine, one-seed evidence, not a guarantee of general or multi-node scaling.

Reproduce:

```sh
make
python3 scripts/benchmark.py benchmarks/seq-vs-mpi/input.txt \
  --processes 1 2 4 8 --mpi-flags='--oversubscribe' --repeats 5 --warmups 1 \
  --graph-metadata benchmarks/seq-vs-mpi/generation.json \
  --output-dir benchmarks/local
MPLCONFIGDIR=/tmp/clique-mpl python3 scripts/plot.py \
  benchmarks/local/summary.csv --output benchmarks/local/scaling.png
```

Raw trial timings are in [trials.csv](trials.csv), medians and end-to-end scaling metrics in [summary.csv](summary.csv), and compiler/environment details, commands, and source/binary hashes in [metadata.json](metadata.json).
