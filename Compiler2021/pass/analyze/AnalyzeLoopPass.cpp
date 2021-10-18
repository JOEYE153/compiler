//
// Created by hujin on 2021/7/27.
//

#include <stack>
#include <queue>
#include <ostream>
#include "AnalyzeLoopPass.h"

struct Tarjan_SCC_Cycle {
    typedef BasicBlock *F;
    std::map<F, std::set<F>> &G;
    std::map<int, std::set<F>> SCC = {};

    std::set<F> inStack = {};
    std::map<F, int> dfn = {}, low = {}, belong = {};
    std::stack<F> stack = {};
    int index, scc;

    void tarjan(F u) {
        low[u] = dfn[u] = ++index;
        stack.push(u);
        inStack.insert(u);
        for (F v : G[u]) {
            if (!dfn.count(v)) {
                tarjan(v);
                if (low[u] > low[v])low[u] = low[v];
            } else if (inStack.count(v) && low[u] > dfn[v])
                low[u] = dfn[v];
        }
        if (low[u] == dfn[u]) {
            scc++;
            SCC[scc] = {};
            F v;
            do {
                v = stack.top();
                stack.pop();
                inStack.erase(v);
                belong[v] = scc;
                SCC[scc].insert(v);
            } while (v != u);
        }
    }

    void solve() {
        index = scc = 0;
        for (auto &p : G) {
            if (!dfn.count(p.first))
                tarjan(p.first);
        }
    }

};

int AnalyzeLoopPass::buildCycle(std::set<BasicBlock *> cycle) {
    Loop c = {};
    int cc = loops.size();
    for (auto *b :cycle) {
        loopIn[b].push_back(cc);
    }
    c.nodes = std::move(cycle);
    loops.push_back({c});
    return cc;
}


//#define TEST


bool AnalyzeLoopPass::analyze() {
    analyzeCFGPass.run();
    BasicBlock E0("E0");
#ifdef TEST
    std::map<BasicBlock*, int> ids;
    int pos = 0;
#endif
    for (auto &x : analyzeCFGPass.result) {
#ifdef TEST
        ids[x.first] = pos++;
#endif
        for (auto r : x.second.rear) {
            G[x.first].insert(r);
            G_rev[r].insert(x.first);
        }
    }
#ifdef TEST
    for(auto &x : G){
        out<<ids[x.first]<<':'<<x.first->getName()<<std::endl;
        for(auto y : x.second){
            out<<ids[x.first]<<"->"<<ids[y]<<std::endl;
        }
    }
#endif
    G_rev[fn.entryBlock].insert(&E0);
    G[&E0].insert(fn.entryBlock);
    bool haveCycle;
    std::queue<BasicBlock *> entrances = {};
    do {
        while (!entrances.empty()) {
            auto ent = entrances.front();
            for (auto *b : G[ent]) {
                G_rev[b].erase(ent);
                G_rev[b].insert(&E0);
                G[&E0].insert(b);
            }
            for (auto *b : G_rev[ent]) {
                G[b].erase(ent);
            }
            G.erase(ent);
            G_rev.erase(ent);
            entrances.pop();
        }
        haveCycle = false;
        Tarjan_SCC_Cycle tarjan = Tarjan_SCC_Cycle({G});
        tarjan.solve();
        std::map<int, int> sccCycle;
        for (auto &x: tarjan.SCC) {
            if (x.second.size() > 1) {
                haveCycle = true;
                sccCycle[x.first] = buildCycle(x.second);
            }
        }
        for (auto &x :G) {
            for (auto &y:G[x.first]) {
                int scc1 = tarjan.belong[x.first];
                int scc2 = tarjan.belong[y];
                if (scc1 != scc2) {
                    if (sccCycle.count(scc2)) {
                        loops[sccCycle[scc2]].entrances.insert(y);
                        newLoop[y] = sccCycle[scc2];
                        entrances.push(y);
                    }
                    if (sccCycle.count(scc1)) {
                        loops[sccCycle[scc1]].exits.insert(x.first);
                    }
                }
            }
            if (G[x.first].empty()) {
                int scc1 = tarjan.belong[x.first];
                if (sccCycle.count(scc1)) {
                    loops[sccCycle[scc1]].exits.insert(x.first);
                }
            }
        }
    } while (haveCycle);
    for (auto &x :G) {
        BasicBlock *b = x.first;
        if (G[b].count(b)) {
            int c = buildCycle({b});
            loops[c].entrances = {b};
            newLoop[b] = c;
            loops[c].exits = {b};
        }
    }
    G.clear();
    G_rev.clear();
    for (auto &x: loopIn) {
        std::vector<int> &second = x.second;
        for (int i = 0; i < second.size(); ++i) {
            if (i + 1 < second.size())
                loopTree[&loops[second[i]]].insert(&loops[second[i + 1]]);
            if (!loopTree.count(&loops[second[i]]))
                loopTree[&loops[second[i]]] = {};
        }
    }

#ifdef TEST
    for(auto &c : loops){
        out<<"entrance:";
        for(auto *b : c.entrances){
            out<<ids[b]<<' ';
        }out<<std::endl;
        out<<"exits:";
        for(auto *b : c.exits){
            out<<ids[b]<<' ';
        }out<<std::endl;
        out<<"node:";
        for(auto *b : c.nodes){
            out<<ids[b]<<' ';
        }out<<std::endl;
    }
#endif
    loopIn.erase(&E0);
    return true;
}


void AnalyzeLoopPass::invalidate() {
    loops.clear();
    loopIn.clear();
    loopTree.clear();
    newLoop.clear();
    AnalyzePass::invalidate();
}