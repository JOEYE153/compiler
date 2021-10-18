//
// Created by 陈思言 on 2021/7/5.
//

#ifndef COMPILER2021_CONSTANTFOLDINGPASS_H
#define COMPILER2021_CONSTANTFOLDINGPASS_H

#include "../analyze/AnalyzeDomTreePass.h"
#include "../analyze/AnalyzeSideEffectPass.h"
#include "AnalyzeUnwrittenGlobalValuePass.h"
#include "../analyze/AnalyzeArrayAccessPass.h"

class ConstantFoldingPass : public FunctionPass {
public:
    ConstantFoldingPass(Function &fn, AnalyzeFunctionPasses &dependency, ostream &out = null_out)
            : FunctionPass(fn, out),
              analyzeSideEffectPass(*dependency.analyzeModulePasses->analyzeSideEffectPass),
              analyzeDomTreePass(*dependency.analyzeDomTreePass),
              analyzeUnwrittenGlobalValuePass(*dependency.analyzeModulePasses->analyzeUnwrittenGlobalValuePass),
              analyzeCFGPass(*dependency.analyzeCFGPass),
              analyzePointerPass(*dependency.analyzeModulePasses->analyzePointerPass),
              analyzeArrayAccessPass(*dependency.analyzeArrayAccessPass),
              dependency(dependency), moduleIn(dependency.analyzeModulePasses->md) {}

    bool run() override;

private:
    void fold(DomTreeNode *node, std::map<Assignment *, Assignment *> replaceTable);

    MIR *checkMIR(MIR *mir, std::map<Assignment *, Assignment *> &replaceTable, BasicBlock *block);

    std::map<PhiMIR *, Assignment *> destination;

    Assignment *getPhiDestination(PhiMIR *phiMir);

private:
    Module *moduleIn;
    AnalyzeDomTreePass &analyzeDomTreePass;
    AnalyzeCFGPass &analyzeCFGPass;
    AnalyzeSideEffectPass &analyzeSideEffectPass;
    AnalyzeUnwrittenGlobalValuePass &analyzeUnwrittenGlobalValuePass;
    AnalyzePointerPass &analyzePointerPass;
    AnalyzeArrayAccessPass &analyzeArrayAccessPass;
    AnalyzeFunctionPasses &dependency;
    vector<unique_ptr<MIR>> toErase;
    size_t currentId = 0;
    bool controlModified = false;
    bool pointerModified = false;

    void foldPhi(DomTreeNode *node, std::map<Assignment *, Assignment *> replaceTable);
};


#endif //COMPILER2021_CONSTANTFOLDINGPASS_H
