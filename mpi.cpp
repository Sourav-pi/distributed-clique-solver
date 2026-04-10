#include <bits/stdc++.h>
#include <mpi.h>

using namespace std;

// ── Constants ─────────────────────────────────────────────────────────────────
const int MAXN = 1200;

const int TAG_WORK        = 1;  
const int TAG_RESULT      = 2;  
const int TAG_DONE        = 3;  
const int TAG_PMAX        = 4;  
const int TAG_UPDATE_PMAX = 5;  

const int CHECK_INTERVAL = 500;

const int WORK_BUF = 2;
const int RES_BUF  = 2 + MAXN;  

// ── Data ──────────────────────────────────────────────────────────────────────
struct Node {
    int profit, cost, index;
};

int N, B, E;
bitset<MAXN> adj[MAXN];
Node nodes[MAXN];
int Pmax = 0;
vector<int> best_clique;

int color_max_p[MAXN];
bitset<MAXN> forbidden_colors[MAXN]; 

// FAST CACHE OPTIMIZATION: Flat integer array for O(1) rank lookups
int profit_rank[MAXN];

static int call_count = 0;
static bool is_worker = false;
static int task_own_pmax = 0;  

// ── Helpers ───────────────────────────────────────────────────────────────────
bool ratio_cmp(const Node& a, const Node& b) {
    return (double)a.profit / a.cost > (double)b.profit / b.cost;
}

void drain_pmax_updates() {
    int flag, np;
    do {
        MPI_Iprobe(0, TAG_PMAX, MPI_COMM_WORLD, &flag, MPI_STATUS_IGNORE);
        if (flag) {
            MPI_Recv(&np, 1, MPI_INT, 0, TAG_PMAX, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            if (np > Pmax) Pmax = np;
        }
    } while (flag);
}

void worker_update_pmax(int new_pmax, const vector<int>& clique) {
    Pmax = new_pmax;
    task_own_pmax = new_pmax;  
    best_clique = clique;
    if (is_worker) {
        MPI_Send(&Pmax, 1, MPI_INT, 0, TAG_UPDATE_PMAX, MPI_COMM_WORLD);
    }
}

// ── Bounds ────────────────────────────────────────────────────────────────────
int get_greedy_bound(const int* cand, int cand_sz) {
    if (cand_sz == 0) return 0;
    
    int sorted_cand[MAXN];
    memcpy(sorted_cand, cand, cand_sz * sizeof(int));
    
    // CACHE OPTIMIZATION: Sort using the L1-friendly integer array
    sort(sorted_cand, sorted_cand + cand_sz, [](int a, int b) {
        return profit_rank[a] < profit_rank[b];
    });

    int max_c = -1; 
    
    for (int i = 0; i < cand_sz; ++i) {
        int u = sorted_cand[i];
        int c = 0;
        
        while (forbidden_colors[c].test(u)) {
            c++;
        }
        
        forbidden_colors[c] |= adj[u];
        
        if (c > max_c) {
            max_c = c;
            color_max_p[c] = nodes[u].profit;
        } else {
            color_max_p[c] = max(color_max_p[c], nodes[u].profit);
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

int get_knapsack_bound(const int* cand, int cand_sz, int rem) {
    long long total = 0;
    for (int i = 0; i < cand_sz; ++i) {
        int v = cand[i];
        if (nodes[v].cost <= rem) {
            total += nodes[v].profit;
            rem -= nodes[v].cost;
        } else {
            total += (1LL * nodes[v].profit * rem) / nodes[v].cost;
            break;
        }
    }
    return (int)total;
}

// ── Branch and Bound ──────────────────────────────────────────────────────────
void findClique(const int* cand, int cand_sz, vector<int>& cur, int pc, int wc) {
    if (is_worker && (++call_count % CHECK_INTERVAL == 0))
        drain_pmax_updates();

    if (cand_sz == 0) {
        if (pc > Pmax) {
            worker_update_pmax(pc, cur);
        }
        return;
    }
    
    // Original required ordering: Greedy, then Knapsack
    if (pc + get_greedy_bound(cand, cand_sz) <= Pmax) return;
    if (pc + get_knapsack_bound(cand, cand_sz, B - wc) <= Pmax) return;

    for (int i = 0; i < cand_sz; ++i) {
        int v = cand[i];
        if (wc + nodes[v].cost <= B) {
            int nc[MAXN], nc_sz = 0;
            
            // Fast Neighbor Collection
            const auto& v_adj = adj[v];
            for (int j = i + 1; j < cand_sz; ++j) {
                int u = cand[j];
                bool is_connected = v_adj.test(u);
                nc[nc_sz] = u;
                nc_sz += is_connected; 
            }
            
            cur.push_back(v);
            if (pc + nodes[v].profit > Pmax)
                worker_update_pmax(pc + nodes[v].profit, cur);
            findClique(nc, nc_sz, cur, pc + nodes[v].profit, wc + nodes[v].cost);
            cur.pop_back();
        }
    }
}

// ── I/O ───────────────────────────────────────────────────────────────────────
vector<int> read_and_broadcast(const char* path, int rank) {
    int profits[MAXN] = {}, costs[MAXN] = {};

    if (rank == 0) {
        FILE* f = fopen(path, "r");
        if (!f) { fprintf(stderr, "Cannot open input file: %s\n", path); MPI_Abort(MPI_COMM_WORLD, 1); }
        fscanf(f, "%d %d %d", &N, &E, &B);
        for (int i = 0; i < N; i++)
            fscanf(f, "%d %d", &profits[i], &costs[i]);
        for (int i = 0; i < E; i++) {
            int u, v;
            fscanf(f, "%d %d", &u, &v);
            adj[u].set(v);
            adj[v].set(u);
        }
        fclose(f);
    }

    int hdr[3] = {N, E, B};
    MPI_Bcast(hdr, 3, MPI_INT, 0, MPI_COMM_WORLD);
    N = hdr[0]; E = hdr[1]; B = hdr[2];

    MPI_Bcast(profits, N, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(costs, N, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(adj, N * sizeof(adj[0]), MPI_BYTE, 0, MPI_COMM_WORLD);

    vector<Node> tmp(N);
    for (int i = 0; i < N; i++) {
        tmp[i] = {profits[i], costs[i], i};
    }
    
    // POPULATE PROFIT RANKS
    vector<Node> tmp_rank = tmp;
    sort(tmp_rank.begin(), tmp_rank.end(), [](const Node& a, const Node& b) {
        if (a.profit != b.profit) return a.profit > b.profit;
        return a.index < b.index;
    });
    for (int i = 0; i < N; i++) {
        profit_rank[tmp_rank[i].index] = i; 
    }

    sort(tmp.begin(), tmp.end(), ratio_cmp);
    vector<int> cand;
    for (int i = 0; i < N; i++) {
        nodes[tmp[i].index] = tmp[i];
        cand.push_back(tmp[i].index);
    }
    return cand;
}

static void build_sub(int root, const vector<int>& initial_cand, int* sub, int& sub_sz) {
    sub_sz = 0;
    const auto& root_adj = adj[root];
    for (int u : initial_cand) {
        if (u == root) break;
        // Fast Neighbor Collection
        bool is_connected = root_adj.test(u);
        sub[sub_sz] = u;
        sub_sz += is_connected;
    }
}

void write_output(const char* path) {
    sort(best_clique.begin(), best_clique.end());
    ofstream out(path);
    out << Pmax << '\n';
    for (int id : best_clique) out << id << ' ';
    out << '\n';
}

// ── Main ──────────────────────────────────────────────────────────────────────
int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);
    double t0 = MPI_Wtime();

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (argc < 3) {
        MPI_Finalize();
        return -1;
    }

    vector<int> initial_cand = read_and_broadcast(argv[1], rank);

    if (size == 1) {
        int sub[MAXN], sub_sz;
        for (int root : initial_cand) {
            int pc = nodes[root].profit, wc = nodes[root].cost;
            build_sub(root, initial_cand, sub, sub_sz);
            if (pc > Pmax) worker_update_pmax(pc, {root});
            vector<int> cur = {root};
            findClique(sub, sub_sz, cur, pc, wc);
        }
        write_output(argv[2]);
        fprintf(stderr, "Time: %.3f s\n", MPI_Wtime() - t0);
        MPI_Finalize();
        return 0;
    }

    int total = (int)initial_cand.size();

    // ── MASTER (rank 0) ───────────────────────────────────────────────────────
    if (rank == 0) {
        int sent = 0, active = 0;
        int workers = size - 1;
        int wbuf[WORK_BUF], rbuf[RES_BUF];
        set<int> active_set;

        auto send_task = [&](int w, int ti) {
            wbuf[0] = Pmax;
            wbuf[1] = initial_cand[ti];
            MPI_Send(wbuf, WORK_BUF, MPI_INT, w, TAG_WORK, MPI_COMM_WORLD);
            active_set.insert(w);
        };

        for (int w = 1; w <= workers; w++) {
            if (sent < total) {
                send_task(w, sent++);
                active++;
            } else {
                MPI_Send(nullptr, 0, MPI_INT, w, TAG_DONE, MPI_COMM_WORLD);
            }
        }

        while (active > 0) {
            MPI_Status st;
            MPI_Recv(rbuf, RES_BUF, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &st);

            int w = st.MPI_SOURCE;

            if (st.MPI_TAG == TAG_UPDATE_PMAX) {
                int wpmax = rbuf[0];
                if (wpmax > Pmax) {
                    Pmax = wpmax;
                    for (int aw : active_set) {
                        if (aw != w) MPI_Send(&Pmax, 1, MPI_INT, aw, TAG_PMAX, MPI_COMM_WORLD);
                    }
                }
                continue; 
            }

            int wpmax = rbuf[0], csz = rbuf[1];
            int old_pmax = Pmax;
            if (wpmax > Pmax) Pmax = wpmax;
            if (csz > 0 && wpmax == Pmax) {
                best_clique.assign(rbuf + 2, rbuf + 2 + csz);
            }
            if (Pmax > old_pmax) {
                for (int aw : active_set) {
                    if (aw != w) MPI_Send(&Pmax, 1, MPI_INT, aw, TAG_PMAX, MPI_COMM_WORLD);
                }
            }

            active--;
            active_set.erase(w);

            if (sent < total) {
                send_task(w, sent++);
                active++;
            } else {
                MPI_Send(nullptr, 0, MPI_INT, w, TAG_DONE, MPI_COMM_WORLD);
            }
        }

        write_output(argv[2]);
        
    // ── WORKER (rank > 0) ─────────────────────────────────────────────────────
    } else {
        is_worker = true;
        int wbuf[WORK_BUF], rbuf[RES_BUF];
        
        while (true) {
            MPI_Status st;
            MPI_Recv(wbuf, WORK_BUF, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &st);
            
            if (st.MPI_TAG == TAG_DONE) break;
            
            if (st.MPI_TAG == TAG_PMAX) {
                Pmax = max(Pmax, wbuf[0]);
                continue;
            }
            
            Pmax = max(Pmax, wbuf[0]);
            task_own_pmax = 0;   
            best_clique.clear();
            int root = wbuf[1];
            int pc = nodes[root].profit;
            int wc = nodes[root].cost;

            int sub[MAXN], sub_sz;
            build_sub(root, initial_cand, sub, sub_sz);

            if (pc > Pmax) {
                worker_update_pmax(pc, {root});
            }

            call_count = 0;
            vector<int> cur = {root};
            findClique(sub, sub_sz, cur, pc, wc);

            rbuf[0] = task_own_pmax;
            rbuf[1] = (int)best_clique.size();
            for (int i = 0; i < (int)best_clique.size(); i++)
                rbuf[2 + i] = best_clique[i];
            MPI_Send(rbuf, 2 + (int)best_clique.size(), MPI_INT, 0, TAG_RESULT, MPI_COMM_WORLD);
        }
    }
    
    if(rank==0) fprintf(stderr, "MPI Time: %.3f s\n", MPI_Wtime() - t0);
    MPI_Finalize();
    return 0;
}