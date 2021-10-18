//
// Created by 陈思言 on 2021/5/23.
//

#ifndef COMPILER2021_PRINTIRPASS_H
#define COMPILER2021_PRINTIRPASS_H

#include "../analyze/AnalyzeCFGPass.h"

class PrintFunctionPass final : public FunctionPass {
public:
    PrintFunctionPass(Function &fn, BasicBlock::LevelOfIR level,
                      AnalyzeFunctionPasses &dependency, ostream &out = std::cout)
            : FunctionPass(fn, out), level(level),
              analyzeCFGPass(*dependency.analyzeCFGPass) {}

    bool run() override;

private:
    AnalyzeCFGPass &analyzeCFGPass;
    BasicBlock::LevelOfIR level;
};

class PrintModulePass final : public ModulePass {
public:
    PrintModulePass(Module &md, BasicBlock::LevelOfIR level, ostream &out = std::cout)
            : ModulePass(md, out), level(level) {}

    bool run() override;

private:
    BasicBlock::LevelOfIR level;
};


#endif //COMPILER2021_PRINTIRPASS_H
