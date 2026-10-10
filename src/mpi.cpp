#include "solver.hpp"
#include <mpi.h>

const int TAG_WORK        = 1;  
const int TAG_RESULT      = 2;  
const int TAG_DONE        = 3;  
const int TAG_PMAX        = 4;  
const int TAG_UPDATE_PMAX = 5;  



const int WORK_BUF = 2;
const int RES_BUF  = 2 + MAXN;  

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

void report_pmax() { MPI_Send(&Pmax, 1, MPI_INT, 0, TAG_UPDATE_PMAX, MPI_COMM_WORLD); }

void read_and_broadcast(const char* path, int rank) {
    if (rank == 0) read_graph(path);
    MPI_Bcast(&N, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&E, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&B, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(nodes, N * sizeof(Node), MPI_BYTE, 0, MPI_COMM_WORLD);
    MPI_Bcast(adj, MAXN * sizeof(adj[0]), MPI_BYTE, 0, MPI_COMM_WORLD);
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

    try {
    read_and_broadcast(argv[1], rank);

    double search_start = MPI_Wtime();
    if (size == 1) {
        solve_all();
        fprintf(stderr, "Search seconds: %.6f\n", MPI_Wtime() - search_start);
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
        // Stable per-send buffers prevent cyclic blocking and buffer reuse races.
        struct PendingUpdate { int value; MPI_Request request; };
        list<PendingUpdate> pending;
        auto send_bound = [&](int worker) {
            for (auto it = pending.begin(); it != pending.end();) {
                int complete = 0;
                MPI_Test(&it->request, &complete, MPI_STATUS_IGNORE);
                if (complete) it = pending.erase(it); else ++it;
            }
            pending.push_back({Pmax, MPI_REQUEST_NULL});
            auto& update = pending.back();
            MPI_Isend(&update.value, 1, MPI_INT, worker, TAG_PMAX,
                      MPI_COMM_WORLD, &update.request);
        };

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
                        if (aw != w) send_bound(aw);
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
                    if (aw != w) send_bound(aw);
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

        for (auto& update : pending) MPI_Wait(&update.request, MPI_STATUS_IGNORE);
        fprintf(stderr, "Search seconds: %.6f\n", MPI_Wtime() - search_start);
        write_output(argv[2]);
        
    // ── WORKER (rank > 0) ─────────────────────────────────────────────────────
    } else {
        is_worker = true;
        poll_updates = drain_pmax_updates;
        report_improvement = report_pmax;
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
            if (wc > B) {
                rbuf[0] = 0; rbuf[1] = 0;
                MPI_Send(rbuf, 2, MPI_INT, 0, TAG_RESULT, MPI_COMM_WORLD);
                continue;
            }

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
    } catch (const exception& error) {
        fprintf(stderr, "Rank %d: %s\n", rank, error.what());
        MPI_Abort(MPI_COMM_WORLD, 1);
        return 1;
    }
}