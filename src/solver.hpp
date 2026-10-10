#pragma once
#include <algorithm>
#include <bitset>
#include <climits>
#include <cstdio>
#include <fstream>
#include <list>
#include <set>
#include <vector>

#include <stdexcept>

using namespace std;

constexpr int MAXN = 1201;
constexpr int CHECK_INTERVAL = 500;
// ── Data ──────────────────────────────────────────────────────────────────────
struct Node {
    int profit, cost, index; // 'index' holds the input vertex ID for output
};

int N, B, E;
bitset<MAXN> adj[MAXN]; // Adjacency matrix using NEW sorted indices
Node nodes[MAXN];       // Nodes sorted by ratio: nodes[0] is the best ratio
int Pmax = 0;
vector<int> best_clique; // Stores NEW sorted indices

int color_max_p[MAXN];
bitset<MAXN> forbidden_colors[MAXN]; 

static int call_count = 0;
static bool is_worker = false;
static int task_own_pmax = 0;  

// Transport hooks are used only by MPI workers.
void (*poll_updates)() = nullptr;
void (*report_improvement)() = nullptr;

void worker_update_pmax(int new_pmax, const vector<int>& clique) {
    Pmax = new_pmax;
    task_own_pmax = new_pmax;  
    best_clique = clique;
    if (is_worker) {
        report_improvement();
    }
}

// Use fast bitset iteration with GCC and a fallback with other libraries.
int first_candidate(const bitset<MAXN>& cand) {
#ifdef __GLIBCXX__
    return cand._Find_first();
#else
    for (int v = 0; v < N; ++v) if (cand.test(v)) return v;
    return MAXN;
#endif
}
int next_candidate(const bitset<MAXN>& cand, int previous) {
#ifdef __GLIBCXX__
    return cand._Find_next(previous);
#else
    for (int v = previous + 1; v < N; ++v) if (cand.test(v)) return v;
    return MAXN;
#endif
}

// ── Bounds ────────────────────────────────────────────────────────────────────
int ksbound(const bitset<MAXN>& cand, int rem) {
    long long total = 0;
    for (int v = first_candidate(cand); v < MAXN; v = next_candidate(cand, v)) {
        if (nodes[v].cost > rem) {
            total += (1LL * nodes[v].profit * rem) / nodes[v].cost;
            break;
        } else {
            total += nodes[v].profit;
            rem -= nodes[v].cost;
        }
    }
    return (int)total;
}


int greedybound(const bitset<MAXN>& cand) {
    if (cand.none()) return 0;
    
    // Extract remaining candidates to sort them by absolute profit
    int sorted_cand[MAXN];
    int cand_sz = 0;
    for (int v = first_candidate(cand); v < MAXN; v = next_candidate(cand, v)) {
        sorted_cand[cand_sz++] = v;
    }
    
    // Sort by absolute profit for greedy coloring
    sort(sorted_cand, sorted_cand + cand_sz, [](int a, int b) {
        return nodes[a].profit > nodes[b].profit;
    });

    int max_c = -1; 
    for (int i = 0; i < cand_sz; ++i) {
        int u = sorted_cand[i];
        int c = 0;
        
        while (forbidden_colors[c].test(u)) c++;
        
        forbidden_colors[c] =  forbidden_colors[c] | adj[u];
        
        if (c <= max_c) {
            color_max_p[c] = max(color_max_p[c], nodes[u].profit);
        } else {
            max_c = c;
            color_max_p[c] = nodes[u].profit;
        }
    }
    
    int bound = 0;
    for (int c = 0; c <= max_c; ++c) {
        bound += color_max_p[c];
        color_max_p[c] = 0;       
        forbidden_colors[c].reset(); 
    }
    
    return bound;
}

// ── Branch and Bound ──────────────────────────────────────────────────────────
// Pass 'cand' by value so we can mutate it locally without affecting backtracking
void findClique(bitset<MAXN> cand, vector<int>& cur, int pc, int wc) {
    if (is_worker && (++call_count % CHECK_INTERVAL == 0))
        poll_updates();

    if (cand.none()) {
        if (pc > Pmax) worker_update_pmax(pc, cur);
        return;
    }
    
    if (pc + greedybound(cand) <= Pmax) return;
    if (pc + ksbound(cand, B - wc) <= Pmax) return;

    // GCC Hardware intrinsics to instantly jump to the next set bit (best ratio first)
    for (int v = first_candidate(cand); v < MAXN; v = next_candidate(cand, v)) {
        
        // Remove 'v' from future siblings to break symmetry
        cand.reset(v);

        if (wc + nodes[v].cost <= B) {
            // THE MAGIC: Generate all valid next candidates in ONE clock cycle
            bitset<MAXN> nc = cand & adj[v];
            
            cur.push_back(v);
            if (pc + nodes[v].profit > Pmax)
                worker_update_pmax(pc + nodes[v].profit, cur);
            
            findClique(nc, cur, pc + nodes[v].profit, wc + nodes[v].cost);
            cur.pop_back();
        }
    }
}

void read_graph(const char* path) {
    struct OrigNode { int p, c, id; };
    vector<OrigNode> tmp;
    vector<pair<int, int>> edge_list;

    {
        FILE* f = fopen(path, "r");
        if (!f) { fprintf(stderr, "Error opening file\n"); throw runtime_error("Input/output failure"); }
        if (fscanf(f, "%d %d %d", &N, &E, &B) != 3 ||
            N < 0 || N > 1200 || E < 0 || E > 1LL * N * (N - 1) / 2 ||
            B < 0 || B > 1000000000) {
            fprintf(stderr, "Invalid input header\n"); throw runtime_error("Input/output failure");
        }
        long long total_profit = 0;
        
        tmp.resize(N);
        for (int i = 0; i < N; i++) {
            if (fscanf(f, "%d %d", &tmp[i].p, &tmp[i].c) != 2 ||
                tmp[i].p < 0 || tmp[i].p > 1000000000 ||
                tmp[i].c <= 0 || tmp[i].c > 1000000000) {
                fprintf(stderr, "Invalid vertex\n"); throw runtime_error("Input/output failure");
            }
            total_profit += tmp[i].p;
            if (total_profit > INT_MAX) {
                fprintf(stderr, "Total profit exceeds 32-bit solver capacity\n");
                throw runtime_error("Input/output failure");
            }
            tmp[i].id = i;
        }
        
        edge_list.resize(E);
        for (int i = 0; i < E; i++) {
            if (fscanf(f, "%d %d", &edge_list[i].first, &edge_list[i].second) != 2 ||
                edge_list[i].first < 0 || edge_list[i].first >= N ||
                edge_list[i].second < 0 || edge_list[i].second >= N ||
                edge_list[i].first == edge_list[i].second) {
                fprintf(stderr, "Invalid edge\n"); throw runtime_error("Input/output failure");
            }
        }
        char extra;
        if (fscanf(f, " %c", &extra) == 1) {
            fprintf(stderr, "Unexpected trailing input\n"); throw runtime_error("Input/output failure");
        }
        fclose(f);

        // Sort by Ratio (Best to Worst)
        sort(tmp.begin(), tmp.end(), [](const OrigNode& a, const OrigNode& b) {
            return 1LL * a.p * b.c > 1LL * b.p * a.c;
        });

        // RE-INDEX THE GRAPH
        vector<int> orig_to_new(MAXN);
        for (int i = 0; i < N; i++) {
            nodes[i].profit = tmp[i].p;
            nodes[i].cost = tmp[i].c;
            nodes[i].index = tmp[i].id; // Save input vertex ID
            orig_to_new[tmp[i].id] = i;
        }

        // Build Adjacency Matrix using NEW indices
        for (auto& e : edge_list) {
            int u = orig_to_new[e.first];
            int v = orig_to_new[e.second];
            if (adj[u].test(v)) {
                fprintf(stderr, "Duplicate edge\n"); throw runtime_error("Input/output failure");
            }
            adj[u].set(v);
            adj[v].set(u);
        }
    }


}

void write_output(const char* path) {
    // Map the internal optimal clique back to input vertex IDs
    vector<int> original_clique;
    for (int id : best_clique) {
        original_clique.push_back(nodes[id].index);
    }
    sort(original_clique.begin(), original_clique.end());
    
    ofstream out(path);
    if (!out) { fprintf(stderr, "Cannot open output\n"); throw runtime_error("Input/output failure"); }
    out << Pmax << '\n';
    for (int id : original_clique) out << id << ' ';
    out << '\n';
    out.flush();
    if (!out) { fprintf(stderr, "Output write failed\n"); throw runtime_error("Input/output failure"); }
}


void solve_all() {
        for (int root = 0; root < N; ++root) {
            int pc = nodes[root].profit, wc = nodes[root].cost;
            if (wc > B) continue;
            bitset<MAXN> sub;
            // Only add valid neighbors that occur AFTER root in sorted order
            for (int u = root + 1; u < N; ++u) {
                if (adj[root].test(u)) sub.set(u);
            }
            
            if (pc > Pmax) worker_update_pmax(pc, {root});
            vector<int> cur = {root};
            findClique(sub, cur, pc, wc);
        }
}
