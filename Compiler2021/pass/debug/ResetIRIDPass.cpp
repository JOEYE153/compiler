//
// Created by Joeye on 2021/7/28.
//

#include "ResetIRIDPass.h"

bool ResetModulePass::run() {
    auto functionTable = md.getFunctionVecDictOrder();
    for (auto fn : functionTable) {
        if (fn->isExternal) {
            continue;
        }
        AnalyzeFunctionPasses analyzePasses;
        if (level == BasicBlock::LevelOfIR::MEDIUM) analyzePasses.analyzeCFGPass = new AnalyzeCFGPass_MIR(*fn, out);
        else if (level == BasicBlock::LevelOfIR::LOW) analyzePasses.analyzeCFGPass = new AnalyzeCFGPass_LIR(*fn, out);
        if (analyzePasses.analyzeCFGPass != nullptr) {
            ResetFunctionPass(*fn, level, analyzePasses, out).run();
            delete analyzePasses.analyzeCFGPass;
        }
    }
    return false;
}

bool ResetFunctionPass::run() {
    ResetFunctionPass *pass;
    if (level == BasicBlock::LevelOfIR::MEDIUM) pass = new ResetFunctionPass_MIR(fn, dependency, out);
    else if (level == BasicBlock::LevelOfIR::LOW) pass = new ResetFunctionPass_LIR(fn, dependency, out);
    AnalyzeCFGPass &analyzeCFGPass = *dependency.analyzeCFGPass;
    analyzeCFGPass.run();
    for (auto &block: analyzeCFGPass.dfsSequence) {
        pass->resetBasicBlock(*block);
    }
    return false;
}


void ResetFunctionPass_MIR::resetBasicBlock(BasicBlock &bb) {
    for (auto &mir: bb.mirTable) {
        auto assign = dynamic_cast<Assignment *>(mir.get());
        if (assign != nullptr) {
            assign->id = currentId++;
        }
    }
}

void ResetFunctionPass_LIR::resetBasicBlock(BasicBlock &bb) {
    for (auto &lir: bb.lirTable) {
        auto regAssign = dynamic_cast<RegAssign *>(lir.get());
        if (regAssign != nullptr) {
            regAssign->vReg = currentVReg++;
        } else {
            auto memAssign = dynamic_cast<MemAssign *>(lir.get());
            if (memAssign != nullptr) {
                memAssign->vMem = currentVMem++;
            }
        }
    }
}
