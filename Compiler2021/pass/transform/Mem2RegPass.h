//
// Created by 陈思言 on 2021/6/13.
//

#ifndef COMPILER2021_MEM2REGPASS_H
#define COMPILER2021_MEM2REGPASS_H

#include "../analyze/AnalyzeDomTreePass.h"

class Mem2RegPass : public FunctionPass {
public:
    Mem2RegPass(Function &fn, AnalyzeFunctionPasses &dependency, ostream &out = null_out)
            : FunctionPass(fn, out),
              analyzeDomTreePass(*dependency.analyzeDomTreePass),
              analyzeCFGPass(*dependency.analyzeCFGPass) {}

    bool run() override;

private:
    std::set<BasicBlock *> getAssignSet(Variable *variable);

    void renameVariable(BasicBlock *block, BasicBlock *prev, Variable *variable, Assignment *ssa);

private:
    AnalyzeDomTreePass &analyzeDomTreePass;
    AnalyzeCFGPass &analyzeCFGPass;
    std::map<BasicBlock *, PhiMIR *> phiTable;
    std::map<Assignment *, Assignment *> replaceTable;
    std::set<BasicBlock *> visited;
    vector<unique_ptr<MIR>> toErase;
    size_t currentId = 0;
};


class Mem2RegGlobalPass : public FunctionPass {
public:
    Mem2RegGlobalPass(Function &fn, AnalyzeFunctionPasses &dependency, ostream &out = null_out)
            : FunctionPass(fn, out),
              analyzeDomTreePass(*dependency.analyzeDomTreePass),
              analyzeCFGPass(*dependency.analyzeCFGPass),
              analyzeSideEffectPass(*dependency.analyzeModulePasses->analyzeSideEffectPass) {}

    bool run() override;

private:
    std::set<BasicBlock *> getAssignSet(Variable *variable);

    void renameVariable(BasicBlock *block, BasicBlock *prev, Variable *variable, Assignment *ssa);

private:
    AnalyzeDomTreePass &analyzeDomTreePass;
    AnalyzeSideEffectPass &analyzeSideEffectPass;
    AnalyzeCFGPass &analyzeCFGPass;
    std::map<BasicBlock *, PhiMIR *> phiTable;
    std::map<Assignment *, Assignment *> replaceTable;
    std::set<BasicBlock *> visited;
    vector<unique_ptr<MIR>> toErase;
    size_t currentId = 0;
    int read_cnt_orig = 0, write_cnt_orig = 0;
    int read_cnt = 0, write_cnt = 0;
};

#endif //COMPILER2021_MEM2REGPASS_H
