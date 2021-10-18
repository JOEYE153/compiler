//
// Created by 陈思言 on 2021/5/23.
//

#include "PrintIRPass.h"

bool PrintFunctionPass::run() {
    if (fn.isExternal) {
        out << "extern function " << fn.getName() << ";\n\n";
        return false;
    }
    out << "function " << fn.getName() << ":\n";
    auto constantTable = fn.getLocalConstantVecDictOrder();
    for (auto constant : constantTable) {
        out << "\tconst " << constant->getType()->toString() << ' ' << constant->getName();
        out << " = " << constant->getValueString() << ";\n";
    }
    auto variableTable = fn.getLocalVariableVecDictOrder();
    for (auto variable : variableTable) {
        out << '\t' << variable->getType()->toString() << ' ' << variable->getName() << ";\n";
    }
    analyzeCFGPass.run();
    for (auto block : analyzeCFGPass.dfsSequence) {
        out << block->toString(level);
    }
    out << "Remain " << analyzeCFGPass.remainBlocks.size() << " unreachable blocks.\n\n";
    return false;
}

bool PrintModulePass::run() {
    out << "--------- module " << md.getName() << " ----------\n\n";
    auto constantTable = md.getGlobalConstantVecDictOrder();
    for (auto constant : constantTable) {
        out << "const " << constant->getType()->toString() << ' ' << constant->getName();
        out << " = " << constant->getValueString() << ";\n";
    }
    auto variableTable = md.getGlobalVariableVecDictOrder();
    for (auto variable : variableTable) {
        out << variable->getType()->toString() << ' ' << variable->getName() << ";\n";
    }
    out << '\n';
    auto functionTable = md.getFunctionVecDictOrder();
    for (auto fn : functionTable) {
        AnalyzeFunctionPasses analyzePasses;
        switch (level) {
            case BasicBlock::LevelOfIR::HIGH:
                analyzePasses.analyzeCFGPass = new AnalyzeCFGPass_HIR(*fn, out);
                break;
            case BasicBlock::LevelOfIR::MEDIUM:
                analyzePasses.analyzeCFGPass = new AnalyzeCFGPass_MIR(*fn, out);
                break;
            case BasicBlock::LevelOfIR::LOW:
                analyzePasses.analyzeCFGPass = new AnalyzeCFGPass_LIR(*fn, out);
                break;
        }
        if (analyzePasses.analyzeCFGPass != nullptr) {
            PrintFunctionPass(*fn, level, analyzePasses, out).run();
            delete analyzePasses.analyzeCFGPass;
        }
    }
    return false;
}
