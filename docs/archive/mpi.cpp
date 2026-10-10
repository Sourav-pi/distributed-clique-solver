#include <bits/stdc++.h>
#include <mpi.h>

using namespace std;

// ── Constants ─────────────────────────────────────────────────────────────────
const int MAXN = 1201;

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
    int profit, cost, index; // 'index' holds the ORIGINAL node ID for output
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
int ksbound(const bitset<MAXN>& cand, int rem) {
    long long total = 0;
    for (int v = cand._Find_first(); v < cand.size(); v = cand._Find_next(v)) {
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
    for (int v = cand._Find_first(); v < cand.size(); v = cand._Find_next(v)) {
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
        drain_pmax_updates();

    if (cand.none()) {
        if (pc > Pmax) worker_update_pmax(pc, cur);
        return;
    }
    
    if (pc + greedybound(cand) <= Pmax) return;
    if (pc + ksbound(cand, B - wc) <= Pmax) return;

    // GCC Hardware intrinsics to instantly jump to the next set bit (best ratio first)
    for (int v = cand._Find_first(); v < cand.size(); v = cand._Find_next(v)) {
        
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

// ── I/O and Re-Indexing ───────────────────────────────────────────────────────
void read_and_broadcast(const char* path, int rank) {
    struct OrigNode { int p, c, id; };
    vector<OrigNode> tmp;
    vector<pair<int, int>> edge_list;

    if (rank == 0) {
        FILE* f = fopen(path, "r");
        if (!f) { fprintf(stderr, "Error opening file\n"); MPI_Abort(MPI_COMM_WORLD, 1); }
        fscanf(f, "%d %d %d", &N, &E, &B);
        
        tmp.resize(N);
        for (int i = 0; i < N; i++) {
            fscanf(f, "%d %d", &tmp[i].p, &tmp[i].c);
            tmp[i].id = i;
        }
        
        edge_list.resize(E);
        for (int i = 0; i < E; i++) {
            fscanf(f, "%d %d", &edge_list[i].first, &edge_list[i].second);
        }
        fclose(f);

        // Sort by Ratio (Best to Worst)
        sort(tmp.begin(), tmp.end(), [](const OrigNode& a, const OrigNode& b) {
            return (double)a.p / a.c > (double)b.p / b.c;
        });

        // RE-INDEX THE GRAPH
        vector<int> orig_to_new(MAXN);
        for (int i = 0; i < N; i++) {
            nodes[i].profit = tmp[i].p;
            nodes[i].cost = tmp[i].c;
            nodes[i].index = tmp[i].id; // Save original ID
            orig_to_new[tmp[i].id] = i;
        }

        // Build Adjacency Matrix using NEW indices
        for (auto& e : edge_list) {
            int u = orig_to_new[e.first];
            int v = orig_to_new[e.second];
            adj[u].set(v);
            adj[v].set(u);
        }
    }

    MPI_Bcast(&N, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&E, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&B, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(nodes, N * sizeof(Node), MPI_BYTE, 0, MPI_COMM_WORLD);
    MPI_Bcast(adj, MAXN * sizeof(adj[0]), MPI_BYTE, 0, MPI_COMM_WORLD);
}

void write_output(const char* path) {
    // Map the internal optimal clique back to original node IDs
    vector<int> original_clique;
    for (int id : best_clique) {
        original_clique.push_back(nodes[id].index);
    }
    sort(original_clique.begin(), original_clique.end());
    
    ofstream out(path);
    out << Pmax << '\n';
    for (int id : original_clique) out << id << ' ';
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

    read_and_broadcast(argv[1], rank);

    if (size == 1) {
        for (int root = 0; root < N; ++root) {
            int pc = nodes[root].profit, wc = nodes[root].cost;
            bitset<MAXN> sub;
            // Only add valid neighbors that occur AFTER root in sorted order
            for (int u = root + 1; u < N; ++u) {
                if (adj[root].test(u)) sub.set(u);
            }
            
            if (pc > Pmax) worker_update_pmax(pc, {root});
            vector<int> cur = {root};
            findClique(sub, cur, pc, wc);
        }
        write_output(argv[2]);
        fprintf(stderr, "MPI Time: %.3f s\n", MPI_Wtime() - t0);
        MPI_Finalize();
        return 0;
    }

    // ── MASTER (rank 0) ───────────────────────────────────────────────────────
    if (rank == 0) {
        int sent = 0, active = 0;
        int workers = size - 1;
        int wbuf[WORK_BUF], rbuf[RES_BUF];
        set<int> active_set;

        auto send_task = [&](int w, int root_idx) {
            wbuf[0] = Pmax;
            wbuf[1] = root_idx;
            MPI_Send(wbuf, WORK_BUF, MPI_INT, w, TAG_WORK, MPI_COMM_WORLD);
            active_set.insert(w);
        };

        for (int w = 1; w <= workers; w++) {
            if (sent < N) {
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

            if (sent < N) {
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

            bitset<MAXN> sub;
            for (int u = root + 1; u < N; ++u) {
                if (adj[root].test(u)) sub.set(u);
            }

            if (pc > Pmax) {
                worker_update_pmax(pc, {root});
            }

            call_count = 0;
            vector<int> cur = {root};
            findClique(sub, cur, pc, wc);

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