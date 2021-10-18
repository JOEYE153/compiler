//
// Created by 陈思言 on 2021/6/13.
//

#include <algorithm>
#include "AnalyzeDomTreePass.h"

using std::make_unique;

bool AnalyzeDomTreePass::analyze() {
    analyzeCFGPass.run();
    size_t n = analyzeCFGPass.dfsSequence.size();
    if (n == 0) {
        return false;
    }
    auto dfnMap = analyzeCFGPass.getDfnMap();
    father.resize(n);
    idom.resize(n);
    sdom.resize(n);
    ancestor.resize(n);
    best.resize(n);
    children.resize(n);
    from.resize(n);
    calc.resize(n);
    for (size_t dfn = 0; dfn < analyzeCFGPass.dfsSequence.size(); dfn++) {
        auto block = analyzeCFGPass.dfsSequence[dfn];
        auto father_ptr = analyzeCFGPass.result[block].father;
        father[dfn] = father_ptr == nullptr ? -1 : (int) dfnMap[father_ptr];
        for (auto prev : analyzeCFGPass.result[block].prev) {
            from[dfn].push_back((int) dfnMap[prev]);
        }
    }
    tarjan();
    vector<DomTreeNode *> tree(n);
    for (size_t dfn = 0; dfn < analyzeCFGPass.dfsSequence.size(); dfn++) {
        tree[dfn] = new DomTreeNode;
        tree[dfn]->block = analyzeCFGPass.dfsSequence[dfn];
        tree[dfn]->father = nullptr;
    }
    for (size_t dfn = 0; dfn < analyzeCFGPass.dfsSequence.size(); dfn++) {
        for (int child : children[dfn]) {
            tree[child]->father = tree[dfn];
            tree[dfn]->children.emplace(tree[child]);
        }
    }
    root = tree[0];
    father.clear();
    idom.clear();
    sdom.clear();
    ancestor.clear();
    best.clear();
    from.clear();
    calc.clear();
    children.clear();
    for (auto &node : tree) {
        auto &edgeSet = analyzeCFGPass.result[node->block];
        if (edgeSet.prev.size() >= 2) {
            for (auto prev_block : edgeSet.prev) {
                auto runner = tree[dfnMap[prev_block]];
                while (runner != node->father) {
                    runner->frontier.insert(node);
                    runner = runner->father;
                }
            }
        }
    }
    return true;
}

void AnalyzeDomTreePass::collectDF(std::map<BasicBlock *, std::set<BasicBlock *>> &DF, const DomTreeNode *node) {
    for (const auto &df : node->frontier) {
        DF[node->block].insert(df->block);
    }
    for (const auto &child : node->children) {
        collectDF(DF, child.get());
    }
}

void AnalyzeDomTreePass::invalidate() {
    AnalyzePass::invalidate();
    if (root != nullptr) {
        delete root;
        root = nullptr;
    }
}

int AnalyzeDomTreePass::find(int x) {
    if (ancestor[x] == x) return x;
    int tmp = ancestor[x];
    ancestor[x] = find(ancestor[x]);
    if (sdom[best[tmp]] < sdom[best[x]]) {
        best[x] = best[tmp];
    }
    return ancestor[x];
}

int AnalyzeDomTreePass::getBest(int x) {
    find(x);
    return best[x];
}

void AnalyzeDomTreePass::tarjan() {
    int n = (int) analyzeCFGPass.dfsSequence.size();
    for (int i = 0; i < n; i++) {
        idom[i] = 0;
        sdom[i] = ancestor[i] = best[i] = i;
    }
    for (int i = n - 1; i > 0; i--) {
        for (unsigned j = 0; j < from[i].size(); j++) {
            int s = sdom[getBest(from[i][j])];
            if (sdom[i] > s) {
                sdom[i] = s;
            }
        }
        calc[sdom[i]].push_back(i);
        ancestor[i] = father[i];
        int u = father[i];
        for (unsigned j = 0; j < calc[u].size(); j++) {
            int t = getBest(calc[u][j]);
            if (sdom[t] == u) {
                idom[calc[u][j]] = u;
            } else {
                idom[calc[u][j]] = t;
            }
        }
        calc[u].clear();
    }
    children.assign(n, {});
    for (int i = 1; i < n; i++) {
        if (sdom[i] != idom[i]) {
            idom[i] = idom[idom[i]];
        }
        children[idom[i]].push_back(i);
    }
}

void AnalyzeDomTreePass::printTree(DomTreeNode *node, int tab_num) {
    for (int i = 0; i < tab_num; i++) {
        out << '\t';
    }
    out << node->block->getName() << " ( df: ";
    for (auto df : node->frontier) {
        out << df->block->getName() << ' ';
    }
    out << ")\n";
    for (auto &child : node->children) {
        printTree(child.get(), tab_num + 1);
    }
}

bool compareDomTreeNode(const DomTreeNode *left, const DomTreeNode *right) {
    return left->block->getName() == right->block->getName() ? left < right :
           left->block->getName() < right->block->getName();
}


std::vector<DomTreeNode *> getDomChildrenBlockNameOrder(DomTreeNode *n) {
    std::vector<DomTreeNode *> ret;
    for (auto &c: n->children) {
        ret.push_back(c.get());
    }
#ifndef RANDOM_TEST
    std::sort(ret.begin(), ret.end(), compareDomTreeNode);
#endif
    return std::move(ret);
}
