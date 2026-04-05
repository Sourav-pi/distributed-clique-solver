#include<bits/stdc++.h>
using namespace std;

struct node {
    int idx;
    int p;
    int c;
};

bool check(const set<int>& st, const unordered_set<int>& neis){
    for(auto nei: neis){
        if(st.count(nei)) return false;
    }
    return true;

}

int greedyBound(vector<node>& nodes,vector<unordered_set<int>>& adj) {
    sort(nodes.begin(),nodes.end(),[](const node& node1,const node& node2){return node1.p > node2.p;});
    vector<set<int>> colorset;
    vector<int> max_elem;
    for(node& nd : nodes){
        int found = 0;
        for(int color=0;color<colorset.size();color++){
            if(check(colorset[color],adj[nd.idx])){
                found=1;
                colorset[color].insert(nd.idx);
                max_elem[color]=max(max_elem[color],nd.p);
                break;
            }
        }
        if(!found){
            set<int> s;
            s.insert(nd.idx);
            colorset.push_back(s);
            max_elem.push_back(nd.p);
        } 
    }
    int sm = 0;
    for(auto x:max_elem) sm+=x;
    return sm;
}

int resourceBound(vector<node>& nodes,int b){
    sort(nodes.begin(),nodes.end(),[](const node& node1,const node& node2){return (double)node1.p/(double)node1.c > (double)node2.p/(double)node2.c;});
    int left = b;
    int profit = 0;
    int partial = -1;
    for(int i=0;i<nodes.size();i++){
        node& nd = nodes[i];
        if(left-nd.c >= 0) {
            left-=nd.c;
            profit+=nd.p;
        } else {
            partial=i;
            break;
        }
    }
    if(partial!=-1){
        profit+=(1LL*left*nodes[partial].p+(nodes[partial].c-1))/(1LL*nodes[partial].c);
    }
    return profit;
}

vector<node> inters(vector<node>& Ccand,unordered_set<int> neis){
    vector<node> opt;
    for(auto cnd: Ccand){
        if(neis.count(cnd.idx)) opt.push_back(cnd);
    }
    return opt;

}

static int Pmax = 0;
static vector<node> max_cliche;
static vector<node> cur_clique;

void findCliche(vector<node> Ccand,int Pcur,int Wcur,vector<unordered_set<int>>& adj,int b) {
    int Ugreedy = greedyBound(Ccand,adj);
    if(Pcur+Ugreedy <= Pmax) return;
    int Uresource = resourceBound(Ccand,b-Wcur);
    if(Pcur+Uresource <= Pmax) return;
    while(Ccand.size()!=0) {
        node nd = Ccand[Ccand.size()-1];
        Ccand.pop_back();
        if(Wcur + nd.c <= b) {
            if(Pmax<Pcur+nd.p){
                Pmax=Pcur+nd.p;
                max_cliche = cur_clique;
                max_cliche.push_back(nd);
            }
            auto Cnext = inters(Ccand,adj[nd.idx]);
            cur_clique.push_back(nd);
            findCliche(Cnext,Pcur+nd.p,Wcur+nd.c,adj,b);
            cur_clique.pop_back();
        }
    }
}




int main(int argc, char const *argv[]) {
    if(argc!=3) return -1;
    string inpath = argv[1];
    string outpath = argv[2];

    // Take input
    ifstream infile(inpath);

    if(!infile.is_open()) return -1;
    int n,m,b;
    infile >> n >> m >> b;
    vector<node> nodes(n);

    vector<unordered_set<int>> adj(n);
    for(int i=0;i<n;i++) {
        nodes[i].idx = i;
        infile >> nodes[i].p >> nodes[i].c;
    }
    int u,v;
    for(int i=0;i<m;i++) {
        infile >> u >> v;
        adj[u].insert(v);
        adj[v].insert(u);
    }

    // compute
    findCliche(nodes,0,0,adj,b);
    sort(max_cliche.begin(),max_cliche.end(),[](const node& node1,const node& node2){return node1.idx < node2.idx;});


    // Write output
    ofstream outfile(outpath);
    if(outfile.is_open()){
        outfile << Pmax << '\n';
        for(auto nd:max_cliche){
            outfile << nd.idx << " ";
        }
        outfile << '\n';
    } else {
        return -1;
    }

    return 0;
}
