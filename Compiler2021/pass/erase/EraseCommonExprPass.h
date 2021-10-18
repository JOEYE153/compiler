//
// Created by 陈思言 on 2021/7/16.
//

#ifndef COMPILER2021_ERASECOMMONEXPRPASS_H
#define COMPILER2021_ERASECOMMONEXPRPASS_H

#include "../analyze/AnalyzeDomTreePass.h"
#include "../analyze/AnalyzeSideEffectPass.h"
#include "../analyze/AnalyzeArrayAccessPass.h"

class EraseCommonExprPass : public FunctionPass {
public:
    EraseCommonExprPass(Function &fn, AnalyzeFunctionPasses &dependency, ostream &out = null_out)
            : FunctionPass(fn, out),
              analyzeDomTreePass(*dependency.analyzeDomTreePass),
              analyzeCFGPass(*dependency.analyzeCFGPass),
              analyzeArrayAccessPass(*dependency.analyzeArrayAccessPass),
              analyzeSideEffectPass(*dependency.analyzeModulePasses->analyzeSideEffectPass) {}

    bool run() override;

private:
    void runOnNode(DomTreeNode *node, std::map<Assignment *, Assignment *> replaceTable);

    void runOnBlock(BasicBlock *block, std::map<Assignment *, Assignment *> &replaceTable);

private:
    AnalyzeDomTreePass &analyzeDomTreePass;
    AnalyzeCFGPass &analyzeCFGPass;
    AnalyzeSideEffectPass &analyzeSideEffectPass;
    AnalyzeArrayAccessPass &analyzeArrayAccessPass;
    vector<unique_ptr<MIR>> toErase;
    size_t currentId = 0;
};


#endif //COMPILER2021_ERASECOMMONEXPRPASS_H
