//
// Created by hujin on 2021/8/12.
//

#include <cassert>
#include "LoopUnrollingPass.h"
#include "../../utils/IRUtils.h"
#include "../../utils/BasicBlockUtils.h"

const int MAX_LOOP_UNROLLING_SIZE = 1000;

void replacePhiBlocks(BasicBlock *fromOld, BasicBlock *fromNew, BasicBlock *to) {
    for (auto &mir : to->mirTable) {
        auto *phi = dynamic_cast<PhiMIR *>(mir.get());
        if (phi != nullptr && phi->incomingTable.count(fromOld)) {
            phi->incomingTable[fromNew] = phi->incomingTable[fromOld];
            phi->incomingTable.erase(fromOld);
        } else break;
    }
}

void reversePhiEditing(BasicBlock *b, std::map<Assignment *, Assignment *> &replace) {
    std::map<Assignment *, Assignment *> rev;
    for (auto x : replace)rev[x.second] = x.first;
    for (auto &mir : b->mirTable) {
        auto *phi = dynamic_cast<PhiMIR *>(mir.get());
        if (phi != nullptr) {
            phi->doReplacement(rev);
        } else break;
    }
}

void doPhiEditing(BasicBlock *b, std::map<Assignment *, Assignment *> &replace) {
    for (auto &mir : b->mirTable) {
        auto *phi = dynamic_cast<PhiMIR *>(mir.get());
        if (phi != nullptr) {
            phi->doReplacement(replace);
        } else break;
    }
}

void eraseControlFlow(BasicBlock *from, BasicBlock *to) {
    auto branch = dynamic_cast<BranchMIR *> (from->mirTable.back().get());
    if (branch != nullptr) {
        assert(branch->block1 == to || branch->block2 == to);
        BasicBlock *jumpTo = branch->block1 == to ? branch->block2 : branch->block1;
        from->mirTable[from->mirTable.size() - 1] = std::make_unique<JumpMIR>(jumpTo);
    } else
        assert(false);
}

//from --> to_old, from_old-->to_new ==> from --> to_new
void redirectControlFlow(BasicBlock *from, BasicBlock *to_old, BasicBlock *to_new) {
    auto branch = dynamic_cast<BranchMIR *> (from->mirTable.back().get());
    if (branch != nullptr) {
        assert(branch->block1 == to_old || branch->block2 == to_old);
        if (branch->block1 == to_old) branch->block1 = to_new;
        else branch->block2 = to_new;
    }
    auto jump = dynamic_cast<JumpMIR *> (from->mirTable.back().get());
    if (jump != nullptr) {
        jump->block = to_new;
    }
}

bool LoopUnrollingPass::run() {
    analyzeLoopPass.run();
    analyzeLoopParamPass.run();
    int total_mir_cnt = 0;
    for (auto *b: fn.getBasicBlockSetPtrOrder()) {
        total_mir_cnt += b->mirTable.size();
    }
    bool modified = false;
    std::map<Loop *, std::map<Assignment *, Assignment *>> replaceTableLoopVarAll = {};
    std::map<Loop *, std::set<BasicBlock *>> expanded = {};

    for (auto it = analyzeLoopPass.loops.rbegin(); it != analyzeLoopPass.loops.rend(); it++) {
        Loop &l = *it;
        int step = 0;
        if (canExpandAll(&l, 1, step)) {
            modified = true;
            out << "Expand: " << (*l.entrances.begin())->getName() << "--step:--" << step << std::endl;
            BasicBlock *entrance = *l.entrances.begin();
            BasicBlock *exit = *l.exits.begin();
            BasicBlock *loopBack = nullptr;
            for (auto *b : analyzeLoopPass.analyzeCFGPass.result[entrance].prev) {
                if (l.nodes.count(b))loopBack = b;
            }
            assert(loopBack != nullptr);
            auto *branch = dynamic_cast<BranchMIR *>(exit->mirTable.back().get());
            assert(branch != nullptr);
            bool &exitTrue = analyzeLoopParamPass.result[&l].exitBranch[branch];
            BasicBlock *next = exitTrue ? branch->block1 : branch->block2;

            std::map<BasicBlock *, BasicBlock *> table = {};
            std::map<Assignment *, Assignment *> tableA = {};
            std::map<Assignment *, Assignment *> replaceTableLoopVar = {};
            for (auto *phi : analyzeLoopParamPass.result[&l].entrancePhi) {
                replaceTableLoopVar[phi] = phi;
            }
            for (auto *x : analyzeLoopPass.analyzeCFGPass.result[entrance].prev) {
                table[x] = x;
            }
            std::set<BasicBlock *> lastLoopNodes = l.nodes;
            for (int i = 0; i < step; i++) {
                std::map<BasicBlock *, BasicBlock *> blockTable = table;
                blockTable[next] = next;
                std::map<Assignment *, Assignment *> replaceTable = tableA;
                auto newBlocks = cloneSetOfBlocks(&fn, lastLoopNodes, blockTable, replaceTable,
                                                  "loop_" + (*l.entrances.begin())->getName() + std::to_string(i + 1),
                                                  i > 0);

                lastLoopNodes.clear();
                for (auto b : newBlocks) {
                    expanded[&l].insert(b.second);
                    lastLoopNodes.insert(b.second);
                    total_mir_cnt += b.second->mirTable.size();
                }

                if (i == 0) reversePhiEditing(newBlocks[entrance], replaceTable);
                tableA = replaceTable;
                replacePhiBlocks(newBlocks[loopBack], loopBack, newBlocks[entrance]);
                table[loopBack] = newBlocks[loopBack];
                redirectControlFlow(loopBack, entrance, newBlocks[entrance]);
                eraseControlFlow(exit, next);
                doPhiEditing(next, replaceTable);
                replacePhiBlocks(exit, newBlocks[exit], next);

                exit = newBlocks[exit];
                entrance = newBlocks[entrance];
                loopBack = newBlocks[loopBack];
//                for (auto &x : replaceTableLoopVar) {
//                    x.second = replaceTable[x.second];
//                }
                if (i == 0) replaceTableLoopVar = replaceTable;
                else
                    for (auto &x : replaceTableLoopVar) {
                        x.second = replaceTable[x.second];
                    }

            }
            branch = dynamic_cast<BranchMIR *>(exit->mirTable.back().get());
            assert(branch != nullptr);
            BasicBlock *exitIn = exitTrue ? branch->block2 : branch->block1;
            eraseControlFlow(exit, exitIn);
            replaceTableLoopVarAll[&l] = replaceTableLoopVar;
        }
        if (total_mir_cnt > MAX_LOOP_UNROLLING_SIZE) break;
    }
    if (!modified) {
        return false;
    }
    for (auto *b : fn.getBasicBlockSetPtrOrder()) {
        for (auto &mir : b->mirTable) {
            for (auto &l : replaceTableLoopVarAll) {
                if (l.first->nodes.count(b) || expanded[l.first].count(b))continue;
                mir->doReplacement(l.second);
            }
        }
    }
    return true;
}



bool LoopUnrollingPass::canExpandAll(Loop *loop, int count_expanded, int &stepCnt) {
    if (loop->exits.size() > 1 || loop->entrances.size() > 1 || !analyzeLoopPass.loopTree[loop].empty() ||
        analyzeLoopParamPass.result[loop].entrancePhi.size() != analyzeLoopParamPass.result[loop].varMap.size()) {
        return false;
    }

    stepCnt = 0;
    std::map<Assignment *, optional<int>> curValue;
    for (auto &x : analyzeLoopParamPass.result[loop].varMap) {
        auto *cnst = dynamic_cast<LoadConstantMIR * >(x.second.init);
        if (cnst == nullptr) {
            return false;
        }
        curValue[x.first] = cnst->src->getValue<int>();
    }
    int sum_mir = 0;
    for (auto *x : loop->nodes) {
        sum_mir += x->mirTable.size();
    }
    if (sum_mir > MAX_LOOP_UNROLLING_SIZE) {
        return false;
    }
    do {
        for (auto exit : analyzeLoopParamPass.result[loop].exitBranch) {
            BranchMIR *b = exit.first;
            std::map<Assignment *, optional<int>> visit = {};
            std::map<Assignment *, optional<int>> copy = curValue;
            auto cond = calculate(b->cond, copy, visit);
            if (!cond.has_value())return false;
            if (cond != 0 && !loop->nodes.count(b->block1)) {
                return true;
            }
            if (cond == 0 && !loop->nodes.count(b->block2)) {
                return true;
            }
        }
        stepCnt++;
        for (auto &x : analyzeLoopParamPass.result[loop].varMap) {
            std::map<Assignment *, optional<int>> visit = {};
            std::map<Assignment *, optional<int>> copy = curValue;
            const optional<int> &v = calculate(x.second.update, copy, visit);
            if (!v.has_value()) continue;
            //return false;
            curValue[x.first] = v.value();
        }
        if (stepCnt * sum_mir > MAX_LOOP_UNROLLING_SIZE) return false;
    } while (true);

}
