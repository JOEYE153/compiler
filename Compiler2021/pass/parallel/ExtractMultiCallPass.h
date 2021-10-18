//
// Created by 陈思言 on 2021/8/13.
//

#ifndef COMPILER2021_EXTRACTMULTICALLPASS_H
#define COMPILER2021_EXTRACTMULTICALLPASS_H

#include "../parallel/AnalyzeParallelLoopPass.h"
#include "../../utils/FunctionUtils.h"

class ExtractMultiCallPass : public ModulePass {
public:
    ExtractMultiCallPass(Module &md, AnalyzeModulePasses &dependency, ostream &out = null_out)
            : ModulePass(md, out), analyzeModulePasses(dependency) {}

    bool run() override;

private:
    // 提取循环作为函数（并行）
    Function *extractLoopFunc(Module *md, Function *src, Loop *loop, AnalyzeFunctionPasses &functionPasses,
                              AnalyzeParallelLoopPass &parallelLoopPass);

    void extractLoopCallee(Function *new_fn, vector<BasicBlock *> &etrBlocks, vector<pair<Assignment *, bool>> &scalars,
                          vector<pair<Assignment *, bool>> &arrays, BasicBlock *prevBlock, BasicBlock *oldExitBlock, Loop *loop,
                          AnalyzeFunctionPasses &functionPasses);

private:
    AnalyzeModulePasses &analyzeModulePasses;
    vector<unique_ptr<MIR>> toErase;
};


#endif //COMPILER2021_EXTRACTMULTICALLPASS_H
