#include<bits/stdc++.h>
#include<mpi.h>
using namespace std;

struct node {
    int idx;
    int p;
    int c;
};

static int      NWORDS;
static vector<vector<uint64_t>> adj;

inline bool isAdj(int u, int v) {
    return (adj[u][v >> 6] >> (v & 63)) & 1;
}
inline void addEdge(int u, int v) {
    adj[u][v >> 6] |= 1ULL << (v & 63);
    adj[v][u >> 6] |= 1ULL << (u & 63);
}

int greedyBound(vector<node>& nodes) {
    sort(nodes.begin(), nodes.end(),
         [](const node& a, const node& b){ return a.p > b.p; });

    vector<vector<uint64_t>> colorBits;
    vector<int>              maxP;

    for (const node& nd : nodes) {
        bool placed = false;
        for (int c = 0; c < (int)colorBits.size(); c++) {
            bool indep = true;
            for (int w = 0; w < NWORDS; w++) {
                if (adj[nd.idx][w] & colorBits[c][w]) { indep = false; break; }
            }
            if (indep) {
                colorBits[c][nd.idx >> 6] |= 1ULL << (nd.idx & 63);
                maxP[c] = max(maxP[c], nd.p);
                placed = true;
                break;
            }
        }
        if (!placed) {
            colorBits.emplace_back(NWORDS, 0ULL);
            colorBits.back()[nd.idx >> 6] |= 1ULL << (nd.idx & 63);
            maxP.push_back(nd.p);
        }
    }

    int sm = 0;
    for (int x : maxP) sm += x;
    return sm;
}

int resourceBound(vector<node>& nodes, int b) {
    sort(nodes.begin(), nodes.end(),
         [](const node& a, const node& b){
             return (double)a.p / a.c > (double)b.p / b.c;
         });
    int left = b, profit = 0, partial = -1;
    for (int i = 0; i < (int)nodes.size(); i++) {
        if (left - nodes[i].c >= 0) {
            left   -= nodes[i].c;
            profit += nodes[i].p;
        } else {
            partial = i;
            break;
        }
    }
    if (partial != -1)
        profit += (1LL * left * nodes[partial].p + nodes[partial].c - 1)
                  / nodes[partial].c;
    return profit;
}

vector<node> inters(const vector<node>& Ccand, int nd_idx) {
    vector<node> opt;
    opt.reserve(Ccand.size());
    for (const auto& cnd : Ccand)
        if (isAdj(nd_idx, cnd.idx))
            opt.push_back(cnd);
    return opt;
}

static int          Pmax = 0;
static vector<node> max_cliche;
static vector<node> cur_clique;

void findCliche(vector<node> Ccand, int Pcur, int Wcur, int b) {
    int Ugreedy = greedyBound(Ccand);
    if (Pcur + Ugreedy   <= Pmax) return;
    int Uresource = resourceBound(Ccand, b - Wcur);
    if (Pcur + Uresource <= Pmax) return;

    while (!Ccand.empty()) {
        node nd = Ccand.back();
        Ccand.pop_back();
        if (Wcur + nd.c <= b) {
            if (Pmax < Pcur + nd.p) {
                Pmax       = Pcur + nd.p;
                max_cliche = cur_clique;
                max_cliche.push_back(nd);
            }
            auto Cnext = inters(Ccand, nd.idx);
            cur_clique.push_back(nd);
            findCliche(std::move(Cnext), Pcur + nd.p, Wcur + nd.c, b);
            cur_clique.pop_back();
        }
    }
}


int main(int argc, char* argv[]) {
    MPI_Init(&argc, &argv);
    int rank, size;
    double st = MPI_Wtime();
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (argc != 3) { MPI_Finalize(); return -1; }

    ifstream infile(argv[1]);
    if (!infile.is_open()) { MPI_Finalize(); return -1; }

    int n, m, b;
    infile >> n >> m >> b;

    NWORDS = (n + 63) / 64;
    adj.assign(n, vector<uint64_t>(NWORDS, 0ULL));

    vector<node> nodes(n);
    for (int i = 0; i < n; i++) {
        nodes[i].idx = i;
        infile >> nodes[i].p >> nodes[i].c;
    }
    for (int i = 0; i < m; i++) {
        int u, v; infile >> u >> v;
        addEdge(u, v);
    }
    infile.close();

    int local_lb = 0;
    for (const auto& nd : nodes)
        if (nd.c <= b) local_lb = max(local_lb, nd.p);
    MPI_Allreduce(&local_lb, &Pmax, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD);

    for (int i = n - 1; i >= 0; i--) {
        if (i % size != rank) continue;

        node nd = nodes[i];
        if (nd.c > b) continue;

        if (Pmax < nd.p) {
            Pmax       = nd.p;
            max_cliche = {nd};
        }

        vector<node> sub_Ccand;
        sub_Ccand.reserve(i);
        for (int j = 0; j < i; j++)
            if (isAdj(nd.idx, nodes[j].idx))
                sub_Ccand.push_back(nodes[j]);

        cur_clique.push_back(nd);
        findCliche(std::move(sub_Ccand), nd.p, nd.c, b);
        cur_clique.pop_back();
    }

    int global_Pmax;
    MPI_Allreduce(&Pmax, &global_Pmax, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD);

    int my_candidate = (Pmax == global_Pmax) ? rank : size;
    int winner_rank;
    MPI_Allreduce(&my_candidate, &winner_rank, 1, MPI_INT, MPI_MIN, MPI_COMM_WORLD);

    int clique_size = (rank == winner_rank) ? (int)max_cliche.size() : 0;
    MPI_Bcast(&clique_size, 1, MPI_INT, winner_rank, MPI_COMM_WORLD);

    vector<int> clique_ids(clique_size);
    if (rank == winner_rank)
        for (int i = 0; i < clique_size; i++) clique_ids[i] = max_cliche[i].idx;
    MPI_Bcast(clique_ids.data(), clique_size, MPI_INT, winner_rank, MPI_COMM_WORLD);

    if (rank == 0) {
        sort(clique_ids.begin(), clique_ids.end());
        ofstream outfile(argv[2]);
        if (!outfile.is_open()) { MPI_Finalize(); return -1; }
        outfile << global_Pmax << '\n';
        for (int id : clique_ids) outfile << id << ' ';
        outfile << '\n';
    }

    MPI_Finalize();

    cout << "MPI Time : " << MPI_Wtime()-st << " s\n";
    return 0;
}
