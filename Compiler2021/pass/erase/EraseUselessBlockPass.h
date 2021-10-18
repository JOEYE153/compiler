//
// Created by 陈思言 on 2021/6/5.
//

#ifndef COMPILER2021_ERASEUSELESSBLOCKPASS_H
#define COMPILER2021_ERASEUSELESSBLOCKPASS_H

#include "../analyze/AnalyzeCFGPass.h"

class EraseUselessBlockPass : public FunctionPass {
public:
    EraseUselessBlockPass(Function &fn, AnalyzeFunctionPasses &dependency, ostream &out = null_out)
            : FunctionPass(fn, out),
              analyzeCFGPass(*dependency.analyzeCFGPass) {}

    bool run() override;

private:
    AnalyzeCFGPass &analyzeCFGPass;
};


#endif //COMPILER2021_ERASEUSELESSBLOCKPASS_H
