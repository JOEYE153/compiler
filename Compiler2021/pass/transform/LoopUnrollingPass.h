//
// Created by hujin on 2021/8/12.
//

#ifndef COMPILER2021_LOOPUNROLLINGPASS_H
#define COMPILER2021_LOOPUNROLLINGPASS_H


#include "../Pass.h"
#include "../analyze/AnalyzeLoopPass.h"
#include "../analyze/AnalyzeLoopParamPass.h"

class LoopUnrollingPass : public FunctionPass {

public:
    LoopUnrollingPass(Module &md, Function &fn, AnalyzeFunctionPasses &dependency, ostream &out = null_out)
            : FunctionPass(fn, out),
              analyzeLoopPass(*dependency.analyzeLoopPass),
              analyzeLoopParamPass(*dependency.analyzeLoopParamPass),
              md(md) {}

    bool run() override;

public:
    Module &md;
    AnalyzeLoopPass &analyzeLoopPass;
    AnalyzeLoopParamPass &analyzeLoopParamPass;

    bool canExpandAll(Loop *loop, int count_expanded, int &stepCnt);
};


#endif //COMPILER2021_LOOPUNROLLINGPASS_H
