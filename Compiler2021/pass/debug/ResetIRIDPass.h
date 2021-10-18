//
// Created by Joeye on 2021/7/28.
//

#ifndef COMPILER2021_RESETIRIDPASS_H
#define COMPILER2021_RESETIRIDPASS_H

#include "../analyze/AnalyzeCFGPass.h"

class ResetModulePass final : public ModulePass {
public:
    ResetModulePass(Module &md, BasicBlock::LevelOfIR level, ostream &out = std::cerr)
            : ModulePass(md, out), level(level) {}

    bool run() override;

private:
    BasicBlock::LevelOfIR level;

};

class ResetFunctionPass : public FunctionPass {
public:
    ResetFunctionPass(Function &fn, BasicBlock::LevelOfIR level, AnalyzeFunctionPasses &dependency,
                      ostream &out = std::cerr)
            : FunctionPass(fn, out), dependency(dependency), level(level) {}

    bool run() override;

protected:
    virtual void resetBasicBlock(BasicBlock &bb) {};
    BasicBlock::LevelOfIR level;
    AnalyzeFunctionPasses &dependency;
};

class ResetFunctionPass_MIR final : public ResetFunctionPass {
public:
    ResetFunctionPass_MIR(Function &fn, AnalyzeFunctionPasses &dependency, ostream &out = std::cerr)
            : ResetFunctionPass(fn, BasicBlock::LevelOfIR::MEDIUM, dependency, out) {}

    void resetBasicBlock(BasicBlock &bb) override;

private:
    size_t currentId = 0;
};

class ResetFunctionPass_LIR final : public ResetFunctionPass {
public:
    ResetFunctionPass_LIR(Function &fn, AnalyzeFunctionPasses &dependency, ostream &out = std::cerr)
            : ResetFunctionPass(fn, BasicBlock::LevelOfIR::LOW, dependency, out) {}

    void resetBasicBlock(BasicBlock &bb) override;

private:
    size_t currentVReg = 0;
    size_t currentVMem = 0;
};

#endif //COMPILER2021_RESETIRIDPASS_H
