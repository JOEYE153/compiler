//
// Created by 陈思言 on 2021/8/14.
//

#ifndef COMPILER2021_ANALYZEPARALLELFUNCTIONPASS_H
#define COMPILER2021_ANALYZEPARALLELFUNCTIONPASS_H

#include "../analyze/AnalyzeArrayAccessPass.h"
#include "../analyze/AnalyzeActivityPass.h"

class AnalyzeParallelFunctionPass : public AnalyzeFunctionPass {
public:
    AnalyzeParallelFunctionPass(Function &fn, AnalyzeFunctionPasses &dependency, ostream &out = null_out)
            : AnalyzeFunctionPass(fn, out),
              analyzeCFGPass(*dependency.analyzeCFGPass),
              analyzeArrayAccessPass(*dependency.analyzeArrayAccessPass),
              analyzeActivityPass(*dynamic_cast<AnalyzeActivityPass_MIR *>(dependency.analyzeActivityPass)),
              analyzeSideEffectPass(*dependency.analyzeModulePasses->analyzeSideEffectPass) {}

protected:
    bool analyze() override;

public:
    void invalidate() override;

private:
    void runOnBasicBlock(BasicBlock *bb);

    size_t getLatestSyncTime(BasicBlock *bb, size_t callIndex);

public:
    std::map<BasicBlock *, vector<std::pair<size_t, size_t>>> callSyncTimeVecMap;

private:
    AnalyzeCFGPass &analyzeCFGPass;
    AnalyzeArrayAccessPass &analyzeArrayAccessPass;
    AnalyzeActivityPass_MIR &analyzeActivityPass;
    AnalyzeSideEffectPass &analyzeSideEffectPass;
};


#endif //COMPILER2021_ANALYZEPARALLELFUNCTIONPASS_H
