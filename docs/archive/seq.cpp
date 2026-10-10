#include <iostream>
#include <vector>
#include <algorithm>
#include <chrono>
#include <bitset>
#include <cstdio>

using namespace std;

const int MAXN = 1200;

struct Node {
    int profit;
    int cost;
    int index;
};

int N, B, E;
bitset<MAXN> adj[MAXN];
Node nodes[MAXN];
int Pmax = 0;
vector<int> best_clique;

// Pre-allocated buffers to avoid repeated allocation in recursion
int color_max_p[MAXN]; 
int node_colors[MAXN];

// Heuristic: Higher profit-to-cost ratio nodes explored first to find better Pmax quickly
bool ratio_comparator(const Node& a, const Node& b) {
    return (double)a.profit / a.cost > (double)b.profit / b.cost;
}

// Optimized Greedy Coloring using bitset and array
int get_greedy_bound(const vector<int>& cand) {
    if (cand.empty()) return 0;

    int max_c = 0;
    for (int i = 0; i < cand.size(); ++i) {
        int u = cand[i];
        bitset<MAXN> used_colors;
        
        for (int j = 0; j < i; ++j) {
            if (adj[u].test(cand[j])) {
                used_colors.set(node_colors[j]);
            }
        }

        int c = 0;
        while (used_colors.test(c)) c++;
        node_colors[i] = c;
        
        if (c > max_c) {
            max_c = c;
            color_max_p[c] = nodes[u].profit;
        } else {
            color_max_p[c] = max(color_max_p[c], nodes[u].profit);
        }
    }

    int bound = 0;
    for (int i = 0; i <= max_c; ++i) {
        bound += color_max_p[i];
        color_max_p[i] = 0; // Clean up for next call
    }
    return bound;
}

// Fast Fractional Knapsack Upper Bound
int get_knapsack_bound(const vector<int>& cand, int rem_B) {
    double total_p = 0;
    for (int idx : cand) {
        if (nodes[idx].cost <= rem_B) {
            total_p += nodes[idx].profit;
            rem_B -= nodes[idx].cost;
        } else {
            total_p += (double)nodes[idx].profit / nodes[idx].cost * rem_B;
            break;
        }
    }
    return (int)total_p;
}

void findClique_seq(vector<int> cand, vector<int>& current_clique, int pcurr, int wcurr) {
    if (cand.empty()) {
        if (pcurr > Pmax) {
            Pmax = pcurr;
            best_clique = current_clique;
        }
        return;
    }

    // Bound 1: Knapsack (Fast)
    if (pcurr + get_knapsack_bound(cand, B - wcurr) <= Pmax) return;

    // Bound 2: Coloring (Tight)
    if (pcurr + get_greedy_bound(cand) <= Pmax) return;

    while (!cand.empty()) {
        int v_idx = cand.back();
        cand.pop_back();

        if (wcurr + nodes[v_idx].cost <= B) {
            vector<int> next_cand;
            next_cand.reserve(cand.size());
            
            for (int u_idx : cand) {
                if (adj[v_idx].test(u_idx)) {
                    next_cand.push_back(u_idx);
                }
            }

            current_clique.push_back(v_idx);
            findClique_seq(next_cand, current_clique, pcurr + nodes[v_idx].profit, wcurr + nodes[v_idx].cost);
            current_clique.pop_back();
        }
    }
}

int main(int argc, char** argv) {
    auto start = chrono::steady_clock::now();
    if (argc < 2) return 1;

    freopen(argv[1], "r", stdin);
    if (scanf("%d %d %d", &N, &E, &B) != 3) return 1;

    vector<Node> temp_nodes(N);
    for (int i = 0; i < N; i++) {
        scanf("%d %d", &temp_nodes[i].profit, &temp_nodes[i].cost);
        temp_nodes[i].index = i;
    }

    for (int i = 0; i < E; i++) {
        int u, v;
        scanf("%d %d", &u, &v);
        adj[u].set(v);
        adj[v].set(u);
    }

    // Sort globally by density to improve pruning efficiency
    sort(temp_nodes.begin(), temp_nodes.end(), ratio_comparator);
    
    vector<int> initial_cand;
    for (int i = 0; i < N; i++) {
        nodes[temp_nodes[i].index] = temp_nodes[i];
        initial_cand.push_back(temp_nodes[i].index);
    }

    vector<int> current_clique;
    findClique_seq(initial_cand, current_clique, 0, 0);

    freopen("output_sequential.txt", "w", stdout);
    printf("%d\n", Pmax);
    for (int i = 0; i < best_clique.size(); i++) {
        printf("%d%c", best_clique[i], (i == best_clique.size() - 1 ? '\n' : ' '));
    }

    auto duration = chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - start);
    fprintf(stderr, "Elapsed time: %lld ms\n", (long long)duration.count());
    return 0;
}