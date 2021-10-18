//
// Created by tcyhost on 2021/8/9.
//

#include <algorithm>
#include <fstream>

#include "InlineFunctionPass.h"
#include "../../utils/FunctionUtils.h"
#include "../analyze/AnalyzeCFGPass.h"
#include "../erase/JumpMergePass.h"
#include "../erase/EraseUselessBlockPass.h"
#include "../debug/ResetIRIDPass.h"

bool InlineFunctionPass::run() {
//    std::ofstream mir_out("MIR_bb.txt");
//    PrintModulePass(md, BasicBlock::LevelOfIR::MEDIUM, mir_out).run();
//    mir_out.close();
    dependency->analyzeSideEffectPass->run();
    auto &funcSeq = dependency->analyzeSideEffectPass->funcTopoSeq;
    bool flag = false;
    for (auto iter = funcSeq.rbegin(); iter != funcSeq.rend(); iter++) {
        auto fn = *iter;
        if (fn->isExternal) continue;
        if (inlineAll(fn)) {
            auto *analyzePasses = new AnalyzeFunctionPasses();
            analyzePasses->analyzeCFGPass = new AnalyzeCFGPass_MIR(*fn, out);
            JumpMergePass(*fn, *analyzePasses, out).run();
            EraseUselessBlockPass(*fn, *analyzePasses, out).run();
            delete analyzePasses->analyzeCFGPass;
            delete analyzePasses;
            flag = true;
        }
    }
    ResetModulePass(md, BasicBlock::LevelOfIR::MEDIUM).run();
//    std::ofstream mir2_out("MIR_b.txt");
//    PrintModulePass(md, BasicBlock::LevelOfIR::MEDIUM, mir2_out).run();
//    mir2_out.close();
    return flag;
}

bool InlineFunctionPass::inlineAll(Function *fn) {
    bool flag = false;
    auto blocks = fn->getBasicBlockVecDictOrder();
    std::reverse(blocks.begin(), blocks.end());
    while (!blocks.empty()) {
        auto *bb = blocks.back();
        blocks.pop_back();
        for (int i = 0; i < bb->mirTable.size(); i++) {
            auto call_mir = dynamic_cast<CallMIR *>(bb->mirTable[i].get());
            if (call_mir != nullptr && dependency->analyzeCallGraphPass->callGraph[call_mir->func].empty() &&
                call_mir->func->getName() != fn->getName() && canInline(call_mir->func)) {
                out << "inline func " + call_mir->func->getName() << " in " << fn->getName() << ": "
                    << bb->getName() << "\n";
                auto nbb = inlineFunction(fn, bb, i);
                blocks.emplace_back(nbb);
                flag = true;
                break;
            }
        }
    }
    return flag;
}

bool InlineFunctionPass::canInline(Function *call_fn) const {
    if (call_fn->isExternal) return false;
    auto blocks = call_fn->getBasicBlockVecDictOrder();
    if (blocks.size() > max_block_count) return false;
    size_t mir_count = 0;
    for (auto *b : blocks) {
        mir_count += b->mirTable.size();
    }
    if (mir_count > max_mir_count) return false;
    return true;
}


