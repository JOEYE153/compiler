//
// Created by 陈思言 on 2021/8/13.
//

#ifndef COMPILER2021_ERASEUSELESSFUNCTIONPASS_H
#define COMPILER2021_ERASEUSELESSFUNCTIONPASS_H

#include "../analyze/AnalyzeCallGraphPass.h"

class EraseUselessFunctionPass : public ModulePass {
public:
    EraseUselessFunctionPass(Module &md, AnalyzeModulePasses &dependency, ostream &out = null_out)
            : ModulePass(md, out),
              analyzeCallGraphPass(*dependency.analyzeCallGraphPass) {}

    bool run() override;

private:
    AnalyzeCallGraphPass &analyzeCallGraphPass;
};


#endif //COMPILER2021_ERASEUSELESSFUNCTIONPASS_H
