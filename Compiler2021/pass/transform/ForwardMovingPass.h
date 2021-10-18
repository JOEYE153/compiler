//
// Created by hujin on 2021/7/31.
//

#ifndef COMPILER2021_FORWARDMOVINGPASS_H
#define COMPILER2021_FORWARDMOVINGPASS_H

#include "../analyze/AnalyzeDomTreePass.h"
#include "../analyze/AnalyzeActivityPass.h"
#include "../analyze/AnalyzeArrayAccessPass.h"
#include "../analyze/AnalyzeSideEffectPass.h"

class ForwardMovingPass : public FunctionPass {
public:
    ForwardMovingPass(Function &fn, AnalyzeFunctionPasses &dependency, ostream &out = null_out)
            : FunctionPass(fn, out),
              analyzeDomTreePass(*dependency.analyzeDomTreePass),
              analyzeActivityPass(*dynamic_cast<AnalyzeActivityPass_MIR *>(dependency.analyzeActivityPass)),
              analyzeCFGPass(*dependency.analyzeCFGPass),
              analyzeArrayAccessPass(*dependency.analyzeArrayAccessPass),
              analyzeSideEffectPass(*dependency.analyzeModulePasses->analyzeSideEffectPass) {}

    bool run() override;

private:
    void dfs_operate(DomTreeNode *node);

    bool canMoveForward(ArrayRead *read, BasicBlock *curBlock);

private:
    AnalyzeDomTreePass &analyzeDomTreePass;
    AnalyzeActivityPass_MIR &analyzeActivityPass;
    AnalyzeCFGPass &analyzeCFGPass;
    AnalyzeSideEffectPass &analyzeSideEffectPass;
    AnalyzeArrayAccessPass &analyzeArrayAccessPass;
    std::map<ArrayWrite *, BasicBlock *> writePos;
    bool modified = false;
};


#endif //COMPILER2021_FORWARDMOVINGPASS_H
