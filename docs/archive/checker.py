with open('input.txt', 'r') as file:
    content = file.read().splitlines()

N, E, B = map(int, content[0].split())


p = []
c = []
for i in range(1, N + 1):
    p_val, c_val = map(int, content[i].split())
    p.append(p_val)
    c.append(c_val)


adj = [[False] * N for _ in range(N)]
for i in range(N + 1, N + 1 + E):
    u, v = map(int, content[i].split())
    adj[u][v] = True
    adj[v][u] = True

with open('output.txt', 'r') as file:
    content = file.read().splitlines()

P_max = int(content[0])
best_clique = list(map(int, content[1].split()))

for i in best_clique:
    for j in best_clique:
        if i != j and not adj[i][j]:
            print("Not a clique")
            exit(0)

total_profit = sum(p[i] for i in best_clique)
total_cost = sum(c[i] for i in best_clique)

if total_profit != P_max:
    print("Incorrect profit")
    print(f"Expected: {P_max}, Found: {total_profit}")
    exit(0)

if total_cost > B:
    print("Budget exceeded")
    print(f"Total cost: {total_cost}, Budget: {B}")
    exit(0)

print("Correct solution")