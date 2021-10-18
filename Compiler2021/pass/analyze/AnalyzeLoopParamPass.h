//
// Created by hujin on 2021/8/12.
//

#ifndef COMPILER2021_ANALYZELOOPPARAMPASS_H
#define COMPILER2021_ANALYZELOOPPARAMPASS_H


#include "../Pass.h"
#include "AnalyzeLoopPass.h"

struct InductionVar {
    Assignment *init;
    Assignment *update;
    PhiMIR *phi;
    std::map<Assignment *, BranchMIR *> exitCond;
};

struct LoopParam {
    std::set<PhiMIR *> entrancePhi;
    std::map<BranchMIR *, bool> exitBranch;
    std::map<PhiMIR *, InductionVar> varMap;
};

class AnalyzeLoopParamPass : public AnalyzeFunctionPass {
public:
    AnalyzeLoopParamPass(Function &fn, AnalyzeFunctionPasses &dependency, ostream &out = null_out)
            : AnalyzeFunctionPass(fn, out),
              analyzeLoopPass(*dependency.analyzeLoopPass) {}

    void invalidate() override;

private:
    bool analyze() override;

public:
    AnalyzeLoopPass &analyzeLoopPass;
    std::map<Loop *, LoopParam> result;

    bool linkVarBranch(Loop *loop, BranchMIR *branchMir, Assignment *cur, std::set<Assignment *> &visited);
};


#endif //COMPILER2021_ANALYZELOOPPARAMPASS_H
