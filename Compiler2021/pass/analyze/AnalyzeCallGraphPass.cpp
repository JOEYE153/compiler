//
// Created by hujin on 2021/7/3.
//

#include "AnalyzeCallGraphPass.h"

bool AnalyzeCallGraphPass::analyze() {
    const auto &functions = md.getFunctionVecDictOrder();
    for (auto fn : functions) {
        callGraph[fn] = {};
        for (auto bb : fn->getBasicBlockSetPtrOrder()) {
            auto vf = extractCalledFunctions(bb);
            callGraph[fn].insert(vf.begin(), vf.end());
        }
    }
    return true;
}

void AnalyzeCallGraphPass::invalidate() {
    AnalyzePass::invalidate();
    callGraph.clear();
}

vector<Function *> AnalyzeCallGraphPass_HIR::extractCalledFunctions(BasicBlock *basicBlock) {
    vector<Function *> ret = {};
    for (auto &hir : basicBlock->hirTable) {
        auto call_hir = dynamic_cast<CallHIR *>(hir.get());
        if (call_hir != nullptr) {
            ret.push_back(md.getFunctionByName(call_hir->func_name));
        }
    }
    return std::move(ret);
}

vector<Function *> AnalyzeCallGraphPass_MIR::extractCalledFunctions(BasicBlock *basicBlock) {
    vector<Function *> ret = {};
    for (auto &mir : basicBlock->mirTable) {
        auto call_mir = dynamic_cast<CallMIR *>(mir.get());
        if (call_mir != nullptr) {
            ret.push_back(call_mir->func);
        }
    }
    return ret;
}

vector<Function *> AnalyzeCallGraphPass_LIR::extractCalledFunctions(BasicBlock *basicBlock) {
    vector<Function *> ret = {};
    for (auto &lir : basicBlock->lirTable) {
        auto call_mir = dynamic_cast<BranchAndLinkInst *>(lir.get());
        if (call_mir != nullptr) {
            ret.push_back(call_mir->func);
        }
    }
    return ret;
}
