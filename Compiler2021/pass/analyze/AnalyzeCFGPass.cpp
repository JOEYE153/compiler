//
// Created by 陈思言 on 2021/6/9.
//

#include "AnalyzeCFGPass.h"
#include <stdexcept>
#include <queue>
#include <algorithm>

using std::logic_error;

bool AnalyzeCFGPass::analyze() {
    if (fn.entryBlock == nullptr) {
        throw logic_error(string("Function has no entry: ") + fn.getName());
    }
    remainBlocks = fn.getBasicBlockSetPtrOrder();
    dfs(fn.entryBlock, nullptr);
    return true;
}

void AnalyzeCFGPass::invalidate() {
    AnalyzePass::invalidate();
    result.clear();
    remainBlocks.clear();
    dfsSequence.clear();
}

void AnalyzeCFGPass::dfs(BasicBlock *block, BasicBlock *father) {
    auto iter = remainBlocks.find(block);
    if (iter == remainBlocks.end()) {
        return;
    }
    result[block].block = block;
    result[block].father = father;
    remainBlocks.erase(iter);
    dfsSequence.push_back(block);
    auto rearVec = getRear(block);
    result[block].rear = rearVec;
    for (auto rear : rearVec) {
        dfs(rear, block);
        result[rear].prev.insert(block);
    }
}


std::map<BasicBlock *, size_t> AnalyzeCFGPass::getDfnMap() const {
    std::map<BasicBlock *, size_t> dfnMap;
    for (size_t dfn = 0; dfn < dfsSequence.size(); dfn++) {
        dfnMap[dfsSequence[dfn]] = dfn;
    }
    return std::move(dfnMap);
}

size_t AnalyzeCFGPass::calculateCriticalPathLength() const {
    if (dfsSequence.empty()) {
        return 0;
    }
    auto dfnMap = getDfnMap();
    std::queue<size_t> Q;
    vector<size_t> T(dfsSequence.size(), 0);
    Q.push(0);
    while (!Q.empty()) {
        size_t i = Q.front();
        const auto &rearVec = result.at(dfsSequence[Q.front()]).rear;
        for (auto rear: rearVec) {
            size_t j = dfnMap[rear];
            if (T[j] < T[i] + 1) {
                T[j] = T[i] + 1;
                Q.push(j);
            }
        }
        Q.pop();
    }
    return *std::max_element(T.begin(), T.end());
}

vector<BasicBlock *> AnalyzeCFGPass_HIR::getRear(BasicBlock *block) {
    if (block->hirTable.empty()) {
        out << "\terror: empty\n";
        return {};
    }
    auto hir = block->hirTable.back().get();
    auto jump_hir = dynamic_cast<JumpHIR *>(hir);
    if (jump_hir != nullptr) {
        return {jump_hir->block};
    }
    auto branch_hir = dynamic_cast<BranchHIR *>(hir);
    if (branch_hir != nullptr) {
        return {branch_hir->block1, branch_hir->block2};
    }
    return {};
}

vector<BasicBlock *> AnalyzeCFGPass_MIR::getRear(BasicBlock *block) {
    if (block->mirTable.empty()) {
        out << "\terror: empty\n";
        return {};
    }
    auto mir = block->mirTable.back().get();
    auto jump_mir = dynamic_cast<JumpMIR *>(mir);
    if (jump_mir != nullptr) {
        return {jump_mir->block};
    }
    auto branch_mir = dynamic_cast<BranchMIR *>(mir);
    if (branch_mir != nullptr) {
        return {branch_mir->block1, branch_mir->block2};
    }
    auto atomic_mir = dynamic_cast<AtomicLoopCondMIR *>(mir);
    if (atomic_mir != nullptr) {
        return {atomic_mir->body, atomic_mir->exit};
    }
    return {};
}

vector<BasicBlock *> AnalyzeCFGPass_LIR::getRear(BasicBlock *block) {
    if (block->lirTable.empty()) {
        out << "\terror: empty\n";
        return {};
    }
    vector<BasicBlock *> ret;
    for (auto &lir : block->lirTable) {
        auto branch_lir = dynamic_cast<BranchInst *>(lir.get());
        if (branch_lir != nullptr) {
            ret.insert(ret.begin(), branch_lir->block);
            continue;
        }
        auto atomic_mir = dynamic_cast<AtomicLoopCondLIR *>(lir.get());
        if (atomic_mir != nullptr) {
            return {atomic_mir->body, atomic_mir->exit};
        }
    }
    return ret;
}
