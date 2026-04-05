#!/usr/bin/env bash
# =============================================================================
# bench.sh — compile, generate tests, run seq vs mpi, compare & time
# =============================================================================
# Usage:  bash bench.sh
#
# Output:
#   bench_results.csv   — timing table (import into Excel/Python to plot)
#   test_*.txt          — generated input files
#   out_seq_*.txt       — sequential outputs
#   out_mpi_*_np*.txt   — mpi outputs per process count
# =============================================================================

set -euo pipefail

# ── Configuration ─────────────────────────────────────────────────────────────

# MPI process counts to test (for the report: 1 to 16)
MPI_PROCS=(1 2 4 8)

# Test cases: "name n density max_p max_c budget seed"
# density 0.5 near the clique threshold → hardest for B&B
# Adjust n upward if runs finish too quickly on your machine.
declare -a TESTS=(
    "tiny        15  0.60  100  20   40   42"   # sanity / correctness check
    "small_dense 25  0.50  100  20   60   123"  # medium difficulty
    "med_dense   35  0.50  100  20   80   456"  # main timing test
    "large_dense 45  0.55  100  20  100   789"  # heavier
    "sparse      80  0.20  100  20  150   321"  # sparse graph
)

SEQ_BIN="./seq"
MPI_BIN="./main"

# Extra mpirun flags (add --oversubscribe if you want more procs than cores)
MPIRUN_FLAGS=""

# ── Helpers ───────────────────────────────────────────────────────────────────

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; NC='\033[0m'

# run_timed <output_var> cmd [args...]
# Runs cmd, stores wall-clock seconds (3 decimal places) in $output_var.
run_timed() {
    local _var=$1; shift
    local _t
    _t=$(python3 - "$@" <<'EOF'
import time, subprocess, sys
t = time.perf_counter()
r = subprocess.run(sys.argv[1:])
print(f"{time.perf_counter() - t:.3f}")
if r.returncode != 0:
    sys.exit(r.returncode)
EOF
)
    printf -v "$_var" '%s' "$_t"
}

# get_profit <file>  → first line of output file (the profit)
get_profit() { head -1 "$1" 2>/dev/null || echo "ERR"; }

# verify_clique <input_file> <output_file> <expected_profit>
# Returns 0 if the reported clique is valid and profit matches.
verify_clique() {
    python3 - "$1" "$2" "$3" <<'EOF'
import sys

in_f, out_f, expected = sys.argv[1], sys.argv[2], int(sys.argv[3])

with open(in_f) as f:
    n, m, b = map(int, f.readline().split())
    profits, costs = [], []
    for _ in range(n):
        p, c = map(int, f.readline().split())
        profits.append(p); costs.append(c)
    adj = [set() for _ in range(n)]
    for _ in range(m):
        u, v = map(int, f.readline().split())
        adj[u].add(v); adj[v].add(u)

with open(out_f) as f:
    reported_profit = int(f.readline().strip())
    ids_line = f.readline().strip()
    clique = list(map(int, ids_line.split())) if ids_line else []

# Check profit matches expected
if reported_profit != expected:
    print(f"PROFIT_MISMATCH reported={reported_profit} expected={expected}")
    sys.exit(1)

# Check budget
total_cost = sum(costs[v] for v in clique)
if total_cost > b:
    print(f"BUDGET_EXCEEDED cost={total_cost} budget={b}")
    sys.exit(1)

# Check it is actually a clique
for i, u in enumerate(clique):
    for v in clique[i+1:]:
        if v not in adj[u]:
            print(f"NOT_A_CLIQUE missing edge ({u},{v})")
            sys.exit(1)

# Check profit value
actual_profit = sum(profits[v] for v in clique)
if actual_profit != reported_profit:
    print(f"PROFIT_WRONG actual={actual_profit} reported={reported_profit}")
    sys.exit(1)

print("OK")
EOF
}

# ── Compile ───────────────────────────────────────────────────────────────────

echo "═══════════════════════════════════════════════════════"
echo " Compiling"
echo "═══════════════════════════════════════════════════════"

g++    -O3 -std=c++17 seq.cpp -o seq  && echo "  seq.cpp  → seq  [OK]"
mpic++ -O3 -std=c++17 mpi.cpp -o main && echo "  mpi.cpp  → main [OK]"
echo ""

# ── Header ────────────────────────────────────────────────────────────────────

# Build CSV header
CSV="bench_results.csv"
csv_header="test,n,density,seq_time"
for np in "${MPI_PROCS[@]}"; do csv_header+=",mpi_np${np}"; done
for np in "${MPI_PROCS[@]}"; do csv_header+=",speedup_np${np}"; done
echo "$csv_header" > "$CSV"

# Print table header
col_w=12
printf "\n%-16s %${col_w}s" "Test" "seq (s)"
for np in "${MPI_PROCS[@]}"; do printf " %${col_w}s" "mpi-${np} (s)"; done
printf " %${col_w}s  %s\n" "status" ""

sep="────────────────$(printf '─%.0s' $(seq 1 $((col_w * (${#MPI_PROCS[@]} + 2) + 4))))"
echo "$sep"

# ── Run tests ─────────────────────────────────────────────────────────────────

echo "═══════════════════════════════════════════════════════"
echo " Running benchmarks"
echo "═══════════════════════════════════════════════════════"

for entry in "${TESTS[@]}"; do
    read -r name n density max_p max_c budget seed <<< "$entry"

    in_file="test_${name}.txt"
    out_seq="out_seq_${name}.txt"

    # Generate input
    python3 gen.py "$n" "$density" "$max_p" "$max_c" "$budget" "$seed" > "$in_file"

    echo ""
    echo "  ── $name  (n=$n, density=$density, budget=$budget) ──"

    # Run sequential
    seq_time=""
    run_timed seq_time $SEQ_BIN "$in_file" "$out_seq"
    seq_profit=$(get_profit "$out_seq")
    echo "    seq          : ${seq_time}s  (profit=$seq_profit)"

    csv_row="${name},${n},${density},${seq_time}"
    mpi_times=()
    all_ok=true
    status_msg=""

    for np in "${MPI_PROCS[@]}"; do
        out_mpi="out_mpi_${name}_np${np}.txt"

        mpi_time=""
        # shellcheck disable=SC2086
        run_timed mpi_time mpirun $MPIRUN_FLAGS -np "$np" $MPI_BIN "$in_file" "$out_mpi"

        mpi_profit=$(get_profit "$out_mpi")
        mpi_times+=("$mpi_time")

        # Correctness: profit must match seq, clique must be valid
        verify_result=$(verify_clique "$in_file" "$out_mpi" "$seq_profit" 2>&1)
        if [[ "$verify_result" == "OK" ]]; then
            status="${GREEN}OK${NC}"
        else
            status="${RED}FAIL: $verify_result${NC}"
            all_ok=false
            status_msg+=" np${np}:${verify_result}"
        fi

        speedup=$(python3 -c "print(f'{float(\"$seq_time\")/float(\"$mpi_time\"):.2f}')")
        echo -e "    mpi -np $np     : ${mpi_time}s  (profit=$mpi_profit, speedup=${speedup}x)  $status"
    done

    # CSV row
    for t in "${mpi_times[@]}"; do csv_row+=",${t}"; done
    for i in "${!mpi_times[@]}"; do
        sp=$(python3 -c "print(f'{float(\"$seq_time\")/float(\"${mpi_times[$i]}\"):.2f}')")
        csv_row+=",${sp}"
    done
    echo "$csv_row" >> "$CSV"

    # Table row
    printf "%-16s %${col_w}s" "$name" "$seq_time"
    for t in "${mpi_times[@]}"; do printf " %${col_w}s" "$t"; done
    if $all_ok; then
        printf -v status_col "  %b" "${GREEN}PASS${NC}"
    else
        printf -v status_col "  %b" "${RED}FAIL${NC}"
    fi
    echo -e "$status_col"
done

# ── Summary ───────────────────────────────────────────────────────────────────

echo ""
echo "═══════════════════════════════════════════════════════"
echo " Summary"
echo "═══════════════════════════════════════════════════════"
echo "  Timing results → $CSV"
echo ""
echo "  To plot speedup in Python:"
echo "    import pandas as pd, matplotlib.pyplot as plt"
echo "    df = pd.read_csv('bench_results.csv')"
echo "    for _, row in df.iterrows():"
echo "        plt.plot([1,2,4,8], [row.speedup_np1, row.speedup_np2, row.speedup_np4, row.speedup_np8], marker='o', label=row.test)"
echo "    plt.xlabel('# processes'); plt.ylabel('speedup'); plt.legend(); plt.show()"
echo ""
