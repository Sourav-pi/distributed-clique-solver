import subprocess
import re

INPUT_FILE = "input.txt"
OUTPUT_FILE = "output.txt"
PROCESS_COUNTS = [1, 2,3, 4,5,6,7, 8,9,10,11,12,13,14,15,16]
CSV_FILE = "results.csv"

# Step 1: Compile
print("Compiling...")
proc = subprocess.Popen(["mpic++", "-O3", "-std=c++17", "mpi.cpp", "-o", "main"], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
stdout, stderr = proc.communicate()
if proc.returncode != 0:
    print("Compilation failed:")
    print(stderr)
    exit(1)
print("Compilation successful.\n")

# Step 2: Run and collect
times = []
for np_count in PROCESS_COUNTS:
    print("Running with -np %d ... " % np_count)
    p = subprocess.Popen(
        ["mpirun", "-np", str(np_count), "./main", INPUT_FILE, OUTPUT_FILE],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE
    )
    stdout, stderr = p.communicate()
    output = stdout.decode("utf-8", "replace") + stderr.decode("utf-8", "replace")
    match = re.search(r"MPI Time:\s*([\d.]+) s", output)
    if match:
        t = float(match.group(1))
        times.append(t)
        print("  %.4fs" % t)
    else:
        print("  Could not parse time. Output:")
        print(output)
        times.append(None)

# Step 3: Save CSV
f = open(CSV_FILE, "w")
f.write("processes,time\n")
for np_count, t in zip(PROCESS_COUNTS, times):
    if t is not None:
        f.write("%d,%.6f\n" % (np_count, t))
f.close()
print("\nResults saved to %s" % CSV_FILE)