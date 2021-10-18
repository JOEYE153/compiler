//
// Created by hujin on 2021/8/15.
//

#ifndef COMPILER2021_ERASEUSELESSARRAYWRITEPASS_H
#define COMPILER2021_ERASEUSELESSARRAYWRITEPASS_H


#include "../Pass.h"
#include "../analyze/AnalyzeArrayAccessPass.h"

class EraseUselessArrayWritePass : public FunctionPass {

public:
    EraseUselessArrayWritePass(Function &fn, AnalyzeFunctionPasses &dependency, ostream &out = null_out)
            : FunctionPass(fn, out), analyzeArrayAccessPass(*dependency.analyzeArrayAccessPass) {}

    bool run() override;

protected:


    AnalyzeArrayAccessPass &analyzeArrayAccessPass;
    std::set<ArrayWrite *> used;

    void use(ArrayWrite *write);
};


#endif //COMPILER2021_ERASEUSELESSARRAYWRITEPASS_H
