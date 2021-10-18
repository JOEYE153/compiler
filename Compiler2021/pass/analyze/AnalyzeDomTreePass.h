//
// Created by 陈思言 on 2021/6/13.
//

#ifndef COMPILER2021_ANALYZEDOMTREEPASS_H
#define COMPILER2021_ANALYZEDOMTREEPASS_H

#include "../analyze/AnalyzeCFGPass.h"

struct DomTreeNode {
    BasicBlock *block;
    DomTreeNode *father;
    std::set<unique_ptr<DomTreeNode>> children;
    std::set<DomTreeNode *> frontier;
};

std::vector<DomTreeNode *> getDomChildrenBlockNameOrder(DomTreeNode *n);

class AnalyzeDomTreePass : public AnalyzeFunctionPass {
public:
    AnalyzeDomTreePass(Function &fn, AnalyzeFunctionPasses &dependency, ostream &out = null_out)
            : AnalyzeFunctionPass(fn, out),
              analyzeCFGPass(*dependency.analyzeCFGPass) {}

    bool analyze() override;

    void collectDF(std::map<BasicBlock *, std::set<BasicBlock *>> &DF, const DomTreeNode *node);

public:
    void invalidate() override;

    void printTree(DomTreeNode *node, int tab_num = 0);

private:
    int find(int x);

    int getBest(int x);

    void tarjan();

public:
    DomTreeNode *root = nullptr;

private:
    AnalyzeCFGPass &analyzeCFGPass;
    vector<int> father, idom, sdom, ancestor, best;
    vector<vector<int>> children, from, calc;
};


#endif //COMPILER2021_ANALYZEDOMTREEPASS_H
