//
// Created by Joeye on 2021/8/14.
//

#ifndef COMPILER2021_INSTRUCTIONSCHEDULE_H
#define COMPILER2021_INSTRUCTIONSCHEDULE_H

#include "../analyze/AnalyzeCallGraphPass.h"
#include "../analyze/AnalyzeActivityPass.h"
#include "../analyze/AnalyzeDataFlowPass.h"
#include "../analyze/AnalyzeDomTreePass.h"
#include <algorithm>


struct DAGNode {
    std::set<DAGNode *> father;
    std::set<DAGNode *> son;
    int weight;   // 0:无需构建DAGNode的指令(伪指令, branch, bx, push, pop),
    // 1:运算指令(Unary, Binary, Compare, Movw, Movt, SetFlag, WriteBack),
    // 3:内存相关指令(Load, Store),
    // 3:乘法相关指令(Multi, Multi64),
    // 9:除法指令
    // 2:bl指令
};

struct regDefUse {
    std::map<int, DAGNode *> def;
    std::map<int, DAGNode *> use;
};

struct statusDefUse {
    std::map<int, DAGNode *> def; // compare and bl def
    std::map<int, DAGNode *> use; // setFlag and conditional instr use
};

class InstructionSchedule : public FunctionPass {
public:
    InstructionSchedule(Function &fn, AnalyzeFunctionPasses &dependency, ostream &out = std::cout)
            : FunctionPass(fn, out), analyzeCFGPass(*dependency.analyzeCFGPass),
              analyzeDomTreePass(*dependency.analyzeDomTreePass) {}

    bool run() override;

private:
    void schedule(DomTreeNode *domTreeNode, std::map<CoreRegAssign *, DAGNode *> assignMapNode);

    DAGNode *createDAGNode(LIR *lir, std::map<CoreRegAssign *, DAGNode *> &assignMapNode,
                           vector<std::pair<LIR *, DAGNode *>> &nodePair,
                           std::map<int, DAGNode *> &statusDef, std::map<int, DAGNode *> &statusUse,
                           std::map<int, regDefUse> &regActivity,
                           int idx, DAGNode *lastBL, DAGNode *lastMovt);

    static vector<DAGNode *> getTopologicalOrder(std::set<DAGNode *> &nodes);

private:
    AnalyzeCFGPass &analyzeCFGPass;
    AnalyzeDomTreePass &analyzeDomTreePass;

};


#endif //COMPILER2021_INSTRUCTIONSCHEDULE_H
