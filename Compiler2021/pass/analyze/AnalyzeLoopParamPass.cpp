//
// Created by hujin on 2021/8/12.
//

#include <cassert>
#include "AnalyzeLoopParamPass.h"

void AnalyzeLoopParamPass::invalidate() {
    AnalyzePass::invalidate();
    result.clear();
}

bool AnalyzeLoopParamPass::linkVarBranch(Loop *loop, BranchMIR *branchMir, Assignment *cur,
                                         std::set<Assignment *> &visited) {
    auto *phi = dynamic_cast<PhiMIR *> (cur);
    if (phi != nullptr) {
        if (result[loop].entrancePhi.count(phi)) {
            if (result[loop].varMap.count(phi)) {
                result[loop].varMap[phi].exitCond[branchMir->cond] = branchMir;
            }
            return true;
        }
    }
    visited.insert(cur);
    struct Visitor : MIR::Visitor {
        std::set<Assignment *> next = {};

        void visit(UnaryMIR *mir) override {
            next.insert(mir->src);
        }

        void visit(BinaryMIR *mir) override {
            next.insert(mir->src1);
            next.insert(mir->src2);
        }

        void visit(SelectMIR *mir) override {
            next.insert(mir->cond);
            next.insert(mir->src1);
            next.insert(mir->src2);
        }

        void visit(PhiMIR *mir) override {
            for (auto &in : mir->incomingTable) {
                next.insert(in.second);
            }
        }
    };
    Visitor v{};
    cur->castToMIR()->accept(v);
    bool ans = true;
    for (auto *n : v.next) {
        if (visited.count(n)) {
            return false;
        }
        ans = linkVarBranch(loop, branchMir, n, visited) || ans;
    }
    return ans;
}

bool AnalyzeLoopParamPass::analyze() {
    analyzeLoopPass.run();
    for (auto &loop: analyzeLoopPass.loops) {
        result[&loop] = {};
        for (auto *b : loop.entrances) {
            for (auto &mir: b->mirTable) {
                auto phi = dynamic_cast<PhiMIR *>(mir.get());
                if (phi != nullptr) {
                    result[&loop].entrancePhi.insert(phi);
                    bool canReduce = true;
                    Assignment *next = nullptr, *init = nullptr;
                    for (auto &x : phi->incomingTable) {
                        if (!loop.nodes.count(x.first)) {
                            if (init != nullptr && x.second != init) {
                                canReduce = false;
                                break;
                            } else init = x.second;
                        } else {
                            if (next != nullptr && x.second != next) {
                                canReduce = false;
                                break;
                            } else next = x.second;
                        }
                    }
                    if (canReduce) {
                        result[&loop].varMap[phi] = {init, next, phi};
                    }
                } else break;
            }
        }
        for (auto *b : loop.exits) {
            auto branch = dynamic_cast<BranchMIR *>(b->mirTable.back().get());
            std::set<Assignment *> visited = {};
            if (branch != nullptr) {
                result[&loop].exitBranch[branch] = !loop.nodes.count(branch->block1);
                linkVarBranch(&loop, branch, branch->cond, visited);
            } else
                assert(false);
        }
    }
    return true;
}
