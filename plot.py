import matplotlib.pyplot as plt
import csv

# Read results.csv
procs = []
ts = []
with open("results.csv", "r") as f:
    reader = csv.reader(f)
    next(reader)  # skip header
    for row in reader:
        procs.append(int(row[0]))
        ts.append(float(row[1]))

speedup = [ts[0] / t for t in ts]

fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(12, 5))

ax1.plot(procs, ts, "o-", color="#2563eb", linewidth=2, markersize=8)
for p, t in zip(procs, ts):
    ax1.annotate("%.2fs" % t, (p, t), textcoords="offset points", xytext=(0, 10), ha="center", fontsize=9)
ax1.set_xlabel("Number of Processes", fontsize=12)
ax1.set_ylabel("Execution Time (s)", fontsize=12)
ax1.set_title("Execution Time vs Number of Processes", fontsize=13, fontweight="bold")
ax1.set_xticks(procs)
ax1.grid(True, linestyle="--", alpha=0.5)

ax2.plot(procs, speedup, "s-", color="#dc2626", linewidth=2, markersize=8, label="Measured")
ax2.plot(procs, procs, "k--", linewidth=1.5, alpha=0.5, label="Ideal")
for p, s in zip(procs, speedup):
    ax2.annotate("%.2fx" % s, (p, s), textcoords="offset points", xytext=(0, 10), ha="center", fontsize=9)
ax2.set_xlabel("Number of Processes", fontsize=12)
ax2.set_ylabel("Speedup", fontsize=12)
ax2.set_title("Speedup vs Number of Processes", fontsize=13, fontweight="bold")
ax2.set_xticks(procs)
ax2.grid(True, linestyle="--", alpha=0.5)
ax2.legend(fontsize=11)

plt.tight_layout()
plt.savefig("time_vs_procs.png", dpi=300, bbox_inches="tight")
plt.savefig("time_vs_procs.pdf", bbox_inches="tight")
print("Saved: time_vs_procs.png and time_vs_procs.pdf")