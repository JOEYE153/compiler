//
// Created by 陈思言 on 2021/6/13.
//

#include "Mem2RegPass.h"
#include "../analyze/AnalyzeSideEffectPass.h"
#include <queue>

bool Mem2RegPass::run() {
    analyzeDomTreePass.run();
    auto variableVec = fn.getLocalVariableVecDictOrder();
    std::queue<BasicBlock *> workList;
    std::map<BasicBlock *, Variable *> inserted;
    std::map<BasicBlock *, Variable *> inWorkList;
    std::map<BasicBlock *, std::set<BasicBlock *>> DF;
    analyzeDomTreePass.collectDF(DF, analyzeDomTreePass.root);
    currentId = 0;
    for (auto block : analyzeCFGPass.dfsSequence) {
        for (auto &mir : block->mirTable) {
            auto mir_assignment = dynamic_cast<Assignment *>(mir.get());
            if (mir_assignment != nullptr && mir_assignment->id > currentId) {
                currentId = mir_assignment->id;
            }
        }
    }
    currentId++;
    for (auto block : analyzeCFGPass.dfsSequence) {
        inserted[block] = nullptr;
        inWorkList[block] = nullptr;
    }
    for (auto x : variableVec) {
        if (x->isReference() || x->getType()->getId() == Type::ID::ARRAY) {
            continue;
        }
        auto assignSet = getAssignSet(x);
        for (auto block : assignSet) {
            inWorkList[block] = x;
            workList.push(block);
        }
        while (!workList.empty()) {
            auto n = workList.front();
            workList.pop();
            for (auto m : DF[n]) {
                if (inserted[m] != x) {
                    phiTable[m] = new PhiMIR(x->getType(), x->getName(), currentId++);
                    inserted[m] = x;
                    if (inWorkList[m] != x) {
                        inWorkList[m] = x;
                        workList.push(m);
                    }
                }
            }
        }
        if (x->isArgument()) {
            auto ld = new LoadVariableMIR(x->getType(), x->getName(), currentId++, x);
            renameVariable(fn.entryBlock, nullptr, x, ld);
            fn.entryBlock->mirTable.emplace(fn.entryBlock->mirTable.begin(), ld);
        } else {
            auto un = new UninitializedMIR(x->getType(), x->getName(), currentId++);
            renameVariable(fn.entryBlock, nullptr, x, un);
            fn.entryBlock->mirTable.emplace(fn.entryBlock->mirTable.begin(), un);
        }
        phiTable.clear();
        visited.clear();
        for (auto block : analyzeCFGPass.dfsSequence) {
            for (auto &mir : block->mirTable) {
                mir->doReplacement(replaceTable);
            }
        }
    }
    return !toErase.empty();
}

std::set<BasicBlock *> Mem2RegPass::getAssignSet(Variable *variable) {
    std::set<BasicBlock *> result;
    for (auto block : analyzeCFGPass.dfsSequence) {
        for (auto &mir : block->mirTable) {
            auto mir_store_var = dynamic_cast<StoreVariableMIR *>(mir.get());
            if (mir_store_var != nullptr && mir_store_var->dst == variable) {
                result.insert(block);
                break;
            }
        }
    }
    return std::move(result);
}

void Mem2RegPass::renameVariable(BasicBlock *block, BasicBlock *prev, Variable *variable, Assignment *ssa) {
    auto phi = phiTable.find(block);
    if (visited.find(block) != visited.end()) {
        if (phi != phiTable.end()) {
            phi->second->addIncoming(prev, ssa);
        }
        return;
    }
    if (phi != phiTable.end()) {
        phi->second->addIncoming(prev, ssa);
        block->mirTable.emplace(block->mirTable.begin(), phi->second);
        ssa = phi->second;
    }
    vector<unique_ptr<MIR>> mirTable;
    for (auto &mir : block->mirTable) {
        auto mir_load_var = dynamic_cast<LoadVariableMIR *>(mir.get());
        if (mir_load_var != nullptr && mir_load_var->src == variable) {
            replaceTable[mir_load_var] = ssa;
            toErase.emplace_back(std::move(mir));
            continue;
        }
        auto mir_store_var = dynamic_cast<StoreVariableMIR *>(mir.get());
        if (mir_store_var != nullptr && mir_store_var->dst == variable) {
            ssa = mir_store_var->src;
            toErase.emplace_back(std::move(mir));
            continue;
        }
        mirTable.emplace_back(std::move(mir));
    }
    block->mirTable.swap(mirTable);
    visited.insert(block);
    const auto &edgeSet = analyzeCFGPass.result[block];
    for (auto rear : edgeSet.rear) {
        renameVariable(rear, block, variable, ssa);
    }
}

bool Mem2RegGlobalPass::run() {
    analyzeDomTreePass.run();
    analyzeSideEffectPass.run();
    auto variableVec = fn.getLocalVariableVecDictOrder();
    std::queue<BasicBlock *> workList;
    std::map<BasicBlock *, Variable *> inserted;
    std::map<BasicBlock *, Variable *> inWorkList;
    std::map<BasicBlock *, std::set<BasicBlock *>> DF;
    analyzeDomTreePass.collectDF(DF, analyzeDomTreePass.root);
    currentId = 0;
    for (auto block : analyzeCFGPass.dfsSequence) {
        for (auto &mir : block->mirTable) {
            auto mir_assignment = dynamic_cast<Assignment *>(mir.get());
            if (mir_assignment != nullptr && mir_assignment->id > currentId) {
                currentId = mir_assignment->id;
            }
        }
    }
    currentId++;
    for (auto block : analyzeCFGPass.dfsSequence) {
        inserted[block] = nullptr;
        inWorkList[block] = nullptr;
    }
    for (auto x : analyzeSideEffectPass.sideEffects[&fn]->writeGlobalVariable) {
        if (x->isReference() || x->getType()->getId() == Type::ID::ARRAY) {
            continue;
        }
        write_cnt = write_cnt_orig = read_cnt = read_cnt_orig = 0;
        auto assignSet = getAssignSet(x);
        if (write_cnt + read_cnt + 2 > read_cnt_orig + write_cnt_orig) continue;
        for (auto block : assignSet) {
            inWorkList[block] = x;
            workList.push(block);
        }
        while (!workList.empty()) {
            auto n = workList.front();
            workList.pop();
            for (auto m : DF[n]) {
                if (inserted[m] != x) {
                    phiTable[m] = new PhiMIR(x->getType(), x->getName(), currentId++);
                    inserted[m] = x;
                    if (inWorkList[m] != x) {
                        inWorkList[m] = x;
                        workList.push(m);
                    }
                }
            }
        }

        auto ld = new LoadVariableMIR(x->getType(), x->getName(), currentId++, x);
        renameVariable(fn.entryBlock, nullptr, x, ld);
        fn.entryBlock->mirTable.emplace(fn.entryBlock->mirTable.begin(), ld);

        phiTable.clear();
        visited.clear();
        for (auto block : analyzeCFGPass.dfsSequence) {
            for (auto &mir : block->mirTable) {
                mir->doReplacement(replaceTable);
            }
        }
    }
    return !toErase.empty();
}

std::set<BasicBlock *> Mem2RegGlobalPass::getAssignSet(Variable *variable) {
    std::set<BasicBlock *> result;
    for (auto block : analyzeCFGPass.dfsSequence) {
        for (auto &mir : block->mirTable) {
            auto mir_store_var = dynamic_cast<StoreVariableMIR *>(mir.get());
            if (mir_store_var != nullptr && mir_store_var->dst == variable) {
                result.insert(block);
                write_cnt_orig++;
            }
            auto mir_load_var = dynamic_cast<LoadVariableMIR *>(mir.get());
            if (mir_load_var != nullptr && mir_load_var->src == variable) {
                read_cnt_orig++;
            }
            auto call = dynamic_cast<CallMIR *>(mir.get());
            if (call != nullptr) {
                if (analyzeSideEffectPass.sideEffects[call->func]->readGlobalVariable.count(variable)) {
                    read_cnt++;
                }
                if (analyzeSideEffectPass.sideEffects[call->func]->writeGlobalVariable.count(variable)) {
                    result.insert(block);
                    write_cnt++;
                }
            }
            auto mir_return = dynamic_cast<ReturnMIR *>(mir.get());
            if (mir_return != nullptr) {
                write_cnt++;
            }
        }
    }
    return std::move(result);
}

void Mem2RegGlobalPass::renameVariable(BasicBlock *block, BasicBlock *prev, Variable *variable, Assignment *ssa) {
    auto phi = phiTable.find(block);
    if (visited.find(block) != visited.end()) {
        if (phi != phiTable.end()) {
            phi->second->addIncoming(prev, ssa);
        }
        return;
    }
    if (phi != phiTable.end()) {
        phi->second->addIncoming(prev, ssa);
        block->mirTable.emplace(block->mirTable.begin(), phi->second);
        ssa = phi->second;
    }
    vector<unique_ptr<MIR>> mirTable;
    for (auto &mir : block->mirTable) {
        auto mir_load_var = dynamic_cast<LoadVariableMIR *>(mir.get());
        if (mir_load_var != nullptr && mir_load_var->src == variable) {
            replaceTable[mir_load_var] = ssa;
            toErase.emplace_back(std::move(mir));
            continue;
        }
        auto mir_store_var = dynamic_cast<StoreVariableMIR *>(mir.get());
        if (mir_store_var != nullptr && mir_store_var->dst == variable) {
            ssa = mir_store_var->src;
            toErase.emplace_back(std::move(mir));
            continue;
        }
        auto mir_call = dynamic_cast<CallMIR *>(mir.get());
        if (mir_call != nullptr) {
            if (analyzeSideEffectPass.sideEffects[mir_call->func]->readGlobalVariable.count(variable)) {
                auto ld = new StoreVariableMIR(variable, ssa);
                mirTable.emplace_back(ld);
            }
            mirTable.emplace_back(std::move(mir));
            if (analyzeSideEffectPass.sideEffects[mir_call->func]->writeGlobalVariable.count(variable)) {
                auto ld = new LoadVariableMIR(variable->getType(), variable->getName(), currentId++, variable);
                mirTable.emplace_back(ld);
                ssa = ld;
            }
            continue;
        }
        auto mir_return = dynamic_cast<ReturnMIR *>(mir.get());
        if (mir_return != nullptr) {
            auto ld = new StoreVariableMIR(variable, ssa);
            mirTable.emplace_back(ld);
        }
        mirTable.emplace_back(std::move(mir));
    }
    block->mirTable.swap(mirTable);
    visited.insert(block);
    const auto &edgeSet = analyzeCFGPass.result[block];
    for (auto rear : edgeSet.rear) {
        renameVariable(rear, block, variable, ssa);
    }
}
