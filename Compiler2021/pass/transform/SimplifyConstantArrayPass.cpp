//
// Created by 陈思言 on 2021/8/14.
//

#include "SimplifyConstantArrayPass.h"
#include "../erase/EraseUselessValuePass.h"

bool SimplifyConstantArrayPass::run() {
    bool hasAnyOptimize = false;
    auto fnVec = md.getFunctionVecDictOrder();
    for (auto fn : fnVec) {
        if (fn->isExternal) {
            continue;
        }
        auto bbVec = fn->getBasicBlockVecDictOrder();
        std::map<Assignment *, Assignment *> replaceTable;
        bool hasOptimize = false;
        for (auto bb : bbVec) {
            hasOptimize = runOnBasicBlock(bb, replaceTable) || hasOptimize;
        }
        if (hasOptimize) {
            for (auto bb : bbVec) {
                for (auto &mir : bb->mirTable) {
                    mir->doReplacement(replaceTable);
                }
            }
            EraseUselessLocalValuePass_MIR(*fn, std::cerr).run();
            hasAnyOptimize = true;
        }
    }
    return hasAnyOptimize;
}

bool SimplifyConstantArrayPass::runOnBasicBlock(BasicBlock *bb, std::map<Assignment *, Assignment *> &replaceTable) {
    bool hasOptimize = false;
    vector<unique_ptr<MIR>> mirTable;
    mirTable.swap(bb->mirTable);
    for (auto &mir : mirTable) {
        auto load_ptr_mir = dynamic_cast<LoadPointerMIR *>(mir.get());
        if (load_ptr_mir != nullptr) {
            auto arr_addr_mir = dynamic_cast<ArrayAddressingMIR *>(load_ptr_mir->src);
            if (arr_addr_mir != nullptr) {
                auto const_base = dynamic_cast<Constant *>(arr_addr_mir->base);
                auto base_ptr_mir = dynamic_cast<ValueAddressingMIR *>(arr_addr_mir);
                if (base_ptr_mir != nullptr && base_ptr_mir->base->isConstant()) {
                    const_base = dynamic_cast<Constant *>(base_ptr_mir->base);
                }
                if (const_base == nullptr) {
                    bb->mirTable.emplace_back(std::move(mir));
                    continue;
                }
                auto iter = md.constantArrayCache.find(const_base);
                if (iter == md.constantArrayCache.end()) {
                    iter = md.constantArrayCache.emplace(const_base, ArrLoad2Op(const_base)).first;
                }
                if (iter->second != nullptr) {
                    toErase.emplace_back(std::move(mir));
                    auto new_ssa = (*iter->second)(arr_addr_mir->offset, &md, bb);
                    replaceTable.emplace(load_ptr_mir, new_ssa);
                    hasOptimize = true;
                    out << "SimplifyConstant: " << const_base->getName() << '\n';
                    continue;
                }
            }
        }
        bb->mirTable.emplace_back(std::move(mir));
    }
    return hasOptimize;
}
