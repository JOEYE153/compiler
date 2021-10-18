//
// Created by hujin on 2021/7/27.
//

#ifndef COMPILER2021_ANALYZELOOPPASS_H
#define COMPILER2021_ANALYZELOOPPASS_H


#include "../Pass.h"
#include "AnalyzeCFGPass.h"

struct Loop {
    std::set<BasicBlock *> entrances = {}; //循环中的入口
    std::set<BasicBlock *> exits = {}; //循环中的出口
    std::set<BasicBlock *> nodes = {}; //循环包含的全部节点
};

/*
 * 每轮找强连通分量后，每个节点数大于1的分量为一个循环，删除进入循环的第一个节点，并将该节点的后继节点与首节点相连
 * 重复上述过程直到找不到循环
 * 复杂度O(V * 循环层数)
 */
class AnalyzeLoopPass : public AnalyzeFunctionPass {
public:
    AnalyzeLoopPass(Function &fn, AnalyzeFunctionPasses &dependency, ostream &out = null_out)
            : AnalyzeFunctionPass(fn, out),
              analyzeCFGPass(*dependency.analyzeCFGPass) {}

    std::map<BasicBlock *, std::vector<int>> loopIn;
    std::vector<Loop> loops;
    std::map<Loop *, std::set<Loop *> > loopTree;
    std::map<BasicBlock *, int> newLoop;
    AnalyzeCFGPass &analyzeCFGPass;

    void invalidate() override;

protected:
    bool analyze() override;

private:
    std::map<BasicBlock *, std::set<BasicBlock *> > G;
    std::map<BasicBlock *, std::set<BasicBlock *> > G_rev;

    int buildCycle(std::set<BasicBlock *> cycle);
};


#endif //COMPILER2021_ANALYZELOOPPASS_H
