#include <bits/stdc++.h>
#include <mpi.h>
using namespace std;

struct node {
    int idx;
    int p;
    int c;
};

bool check(const set<int>& st, const unordered_set<int>& neis) {
    for (auto nei : neis)
        if (st.count(nei)) return false;
    return true;
}

int greedyBound(vector<node> nodes, vector<unordered_set<int>>& adj) {
    sort(nodes.begin(), nodes.end(), [](const node& a, const node& b) { return a.p > b.p; });
    vector<set<int>> colorset;
    vector<int> max_elem;
    for (node& nd : nodes) {
        int found = 0;
        for (int color = 0; color < (int)colorset.size(); color++) {
            if (check(colorset[color], adj[nd.idx])) {
                found = 1;
                colorset[color].insert(nd.idx);
                max_elem[color] = max(max_elem[color], nd.p);
                break;
            }
        }
        if (!found) {
            set<int> s;
            s.insert(nd.idx);
            colorset.push_back(s);
            max_elem.push_back(nd.p);
        }
    }
    int sm = 0;
    for (auto x : max_elem) sm += x;
    return sm;
}

int resourceBound(vector<node> nodes, int b) {
    sort(nodes.begin(), nodes.end(), [](const node& a, const node& b) {
        return (double)a.p / (double)a.c > (double)b.p / (double)b.c;
    });
    int left = b, profit = 0, partial = -1;
    for (int i = 0; i < (int)nodes.size(); i++) {
        node& nd = nodes[i];
        if (left - nd.c >= 0) {
            left -= nd.c;
            profit += nd.p;
        } else {
            partial = i;
            break;
        }
    }
    if (partial != -1)
        profit += (1LL * left * nodes[partial].p + (nodes[partial].c - 1)) / (1LL * nodes[partial].c);
    return profit;
}

vector<node> inters(vector<node>& Ccand, unordered_set<int>& neis) {
    vector<node> opt;
    for (auto cnd : Ccand)
        if (neis.count(cnd.idx)) opt.push_back(cnd);
    return opt;
}

static int Pmax = 0;
static vector<node> max_cliche;
static vector<node> cur_clique;

void findCliche(vector<node> Ccand, int Pcur, int Wcur, vector<unordered_set<int>>& adj, int b) {
    // greedyBound and resourceBound now take by value, no need for explicit copy
    int Ugreedy = greedyBound(Ccand, adj);
    if (Pcur + Ugreedy <= Pmax) return;
    int Uresource = resourceBound(Ccand, b - Wcur);
    if (Pcur + Uresource <= Pmax) return;
    while (Ccand.size() != 0) {
        node nd = Ccand[Ccand.size() - 1];
        Ccand.pop_back();
        if (Wcur + nd.c <= b) {
            if (Pmax < Pcur + nd.p) {
                Pmax = Pcur + nd.p;
                max_cliche = cur_clique;
                max_cliche.push_back(nd);
            }
            auto Cnext = inters(Ccand, adj[nd.idx]);
            cur_clique.push_back(nd);
            findCliche(Cnext, Pcur + nd.p, Wcur + nd.c, adj, b);
            cur_clique.pop_back();
        }
    }
}

int main(int argc, char* argv[]) {
    MPI_Init(&argc, &argv);
    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (argc != 3) { MPI_Finalize(); return -1; }

    string inpath = argv[1], outpath = argv[2];

    // All ranks read the same input file
    ifstream infile(inpath);
    if (!infile.is_open()) { MPI_Finalize(); return -1; }

    int n, m, b;
    infile >> n >> m >> b;
    vector<node> nodes(n);
    vector<unordered_set<int>> adj(n);
    for (int i = 0; i < n; i++) {
        nodes[i].idx = i;
        infile >> nodes[i].p >> nodes[i].c;
    }
    for (int i = 0; i < m; i++) {
        int u, v; infile >> u >> v;
        adj[u].insert(v);
        adj[v].insert(u);
    }
    infile.close();


    int local_lb = 0;
    for (auto& nd : nodes)
        if (nd.c <= b) local_lb = max(local_lb, nd.p);
    MPI_Allreduce(&local_lb, &Pmax, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD);

 
    for (int i = n - 1; i >= 0; i--) {
        if (i % size != rank) continue;

        node nd = nodes[i];
        if (nd.c > b) continue;
        if (Pmax < nd.p) {
            Pmax = nd.p;
            max_cliche = {nd};
        }

        vector<node> sub_Ccand;
        for (int j = 0; j < i; j++)
            if (adj[nd.idx].count(nodes[j].idx))
                sub_Ccand.push_back(nodes[j]);

        cur_clique.push_back(nd);
        findCliche(sub_Ccand, nd.p, nd.c, adj, b);
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
        ofstream outfile(outpath);
        if (!outfile.is_open()) { MPI_Finalize(); return -1; }
        outfile << global_Pmax << '\n';
        for (int id : clique_ids) outfile << id << " ";
        outfile << '\n';
    }

    MPI_Finalize();
    return 0;
}
