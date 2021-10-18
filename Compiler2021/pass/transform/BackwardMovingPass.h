//
// Created by hujin on 2021/8/2.
//

#ifndef COMPILER2021_BACKWARDMOVINGPASS_H
#define COMPILER2021_BACKWARDMOVINGPASS_H

#include "../analyze/AnalyzeActivityPass.h"
#include "../analyze/AnalyzeLoopPass.h"
#include "../analyze/AnalyzeDomTreePass.h"
#include "../analyze/AnalyzeArrayAccessPass.h"

class BackwardMovingPass : FunctionPass {
public:
    BackwardMovingPass(Function &fn, AnalyzeFunctionPasses &dependency, ostream &out = null_out)
            : FunctionPass(fn, out),
              analyzeDomTreePass(*dependency.analyzeDomTreePass),
              analyzeCyclePass(*dependency.analyzeLoopPass),
              analyzeSinglePathPass(*dependency.analyzeRegionPass),
              analyzeActivityPass(*dynamic_cast<AnalyzeActivityPass_MIR *>(dependency.analyzeActivityPass)),
              analyzeCFGPass(*dependency.analyzeCFGPass),
              analyzeSideEffectPass(*dependency.analyzeModulePasses->analyzeSideEffectPass),
              analyzeArrayAccessPass(*dependency.analyzeArrayAccessPass) {}

    bool run() override;

private:

    void dfs_dfn(DomTreeNode *n);

    void dfs_operate(DomTreeNode *n);

    void dfs_init_best_target(DomTreeNode *n);

    void eraseUse(BasicBlock *basicBlock, MIR *ir);

    void insertUse(BasicBlock *basicBlock, MIR *ir);

    BasicBlock *find_best_target(BasicBlock *def, BasicBlock *use, MIR *mir, bool highest);

    void cut_child(std::set<BasicBlock *> &usage);

    void moveCMP(BasicBlock *b);

    std::set<BasicBlock *> getUseBlock(Assignment *assignment);

    std::set<BasicBlock *> getBoundaries(Assignment *assignment, std::set<BasicBlock *> &ret);

    void initArrReadUse();

private:
    AnalyzeDomTreePass &analyzeDomTreePass;
    AnalyzeLoopPass &analyzeCyclePass;
    AnalyzeActivityPass_MIR &analyzeActivityPass;
    AnalyzeArrayAccessPass &analyzeArrayAccessPass;
    AnalyzeCFGPass &analyzeCFGPass;
    AnalyzeSideEffectPass &analyzeSideEffectPass;
    AnalyzeRegionPass &analyzeSinglePathPass;
    std::map<BasicBlock *, std::pair<int, int>> dfn;
    std::map<std::pair<int, int>, BasicBlock *> dfnBlocks;
    std::map<std::pair<BasicBlock *, BasicBlock *>, std::pair<BasicBlock *, BasicBlock *>> bestTarget;
    std::map<BasicBlock *, std::vector<unique_ptr<MIR>>> mirNew_children;
    std::map<BasicBlock *, std::map<Assignment *, Assignment *>> replaceTable;
    std::map<ArrayWrite *, BasicBlock *> writePos;
    std::map<ArrayWrite *, std::set<BasicBlock *>> updateSet;
    int dfnpos = 0;
    int currentId = 0;
    bool modified = false;
};


#endif //COMPILER2021_BACKWARDMOVINGPASS_H
