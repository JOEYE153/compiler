//
// Created by hujin on 2021/8/2.
//

#include <algorithm>
#include <queue>
#include "BackwardMovingPass.h"
#include "../analyze/AnalyzeSideEffectPass.h"
#include "../analyze/AnalyzeRegionPass.h"
#include "../../utils/IRUtils.h"
#include "../analyze/AnalyzeArrayAccessPass.h"

void merge_after_phi(std::vector<unique_ptr<MIR>> &src, std::vector<unique_ptr<MIR>> &dst) {
    int pos_phi = 0;
    std::vector<unique_ptr<MIR>> tmp(src.size() + dst.size());
    int p = 0;
    for (auto &i : dst) {
        if (dynamic_cast<PhiMIR *>(i.get()) != nullptr) {
            pos_phi++;
            tmp[p++] = std::move(i);
        }
    }
    for (auto &i : src) {
        tmp[p++] = std::move(i);
    }
    for (int i = pos_phi; i < dst.size(); i++) {
        tmp[p++] = std::move(dst[i]);
    }
    dst.swap(tmp);
}

bool BackwardMovingPass::run() {
    for (auto block : analyzeCFGPass.dfsSequence) {
        for (auto &mir : block->mirTable) {
            auto mir_assignment = dynamic_cast<Assignment *>(mir.get());
            if (mir_assignment != nullptr && mir_assignment->id > currentId) {
                currentId = mir_assignment->id;
            }
        }
    }
    currentId++;
    analyzeDomTreePass.run();
    analyzeActivityPass.run();
    analyzeCyclePass.run();
    analyzeSideEffectPass.run();
    analyzeSinglePathPass.run();
    analyzeArrayAccessPass.run();
    initArrReadUse();
    dfn.clear();
    dfnpos = 0;
    dfs_dfn(analyzeDomTreePass.root);
    bestTarget.clear();
    dfs_init_best_target(analyzeDomTreePass.root);
    mirNew_children.clear();
    replaceTable.clear();
    dfs_operate(analyzeDomTreePass.root);

    for (auto &x : mirNew_children) {
        std::reverse(x.second.begin(), x.second.end());
        merge_after_phi(x.second, x.first->mirTable);
        modified = modified || !x.second.empty();
    }
    analyzeActivityPass.invalidate();

    for (auto *b: analyzeCFGPass.dfsSequence) {
        for (auto &mir: b->mirTable) {
            auto *phi = dynamic_cast<PhiMIR *>(mir.get());
            if (phi != nullptr) {
                for (auto &x: phi->incomingTable) {
                    if (replaceTable.count(x.first) && replaceTable[x.first].count(x.second)) {
                        x.second = replaceTable[x.first][x.second];
                    }
                }
                continue;
            }
            if (!replaceTable.count(b))continue;
            mir->doReplacement(replaceTable[b]);
        }
    }
    analyzeActivityPass.run();
    for (auto *b : analyzeCFGPass.dfsSequence) {
        moveCMP(b);
    }
    analyzeActivityPass.invalidate();
    return modified;
}

void BackwardMovingPass::initArrReadUse() {
    for (auto *b : fn.getBasicBlockSetPtrOrder()) {
        for (auto &mir : b->mirTable) {
            if (analyzeArrayAccessPass.mirWriteTable.count(mir.get())) {
                auto write = analyzeArrayAccessPass.mirWriteTable[mir.get()];
                writePos[write] = b;
            }
        }
    }
    for (auto &x : analyzeArrayAccessPass.allArrayAccessPhi) {
        writePos[x.first.get()] = x.second;
    }
    for (auto &x : analyzeArrayAccessPass.allArraySliceWriteByFunction) {
        writePos[x.first.get()] = x.second;
    }
    for (auto &x : analyzeArrayAccessPass.allUninitializedArray) {
        writePos[x.get()] = nullptr;
    }
    for (auto &x : analyzeArrayAccessPass.allInputArrayArgument) {
        writePos[x.get()] = nullptr;
    }


    for (auto x : writePos) {
        auto phi = dynamic_cast<ArrayAccessPhi * >(x.first);
        if (phi != nullptr) {
            for (auto &xx : phi->incomingTable) {
                updateSet[xx.second].insert(xx.first);
            }
            continue;
        }
        auto update = dynamic_cast<ArrayWriteUpdate * >(x.first);
        if (update != nullptr) {
            updateSet[update].insert(x.second);
        }
    }
}

void BackwardMovingPass::dfs_dfn(DomTreeNode *n) {
    int pre = dfnpos++;
    for (auto &c :n->children) {
        dfs_dfn(c.get());
    }
    dfn[n->block] = std::make_pair(pre, dfnpos++);
    dfnBlocks[dfn[n->block]] = n->block;
}

void BackwardMovingPass::eraseUse(BasicBlock *basicBlock, MIR *ir) {
    struct Visitor : MIR::Visitor {
        BasicBlock *bb{};
        MIR *ir{};
        AnalyzeActivityPass_MIR *pass{};

        void eraseUse(Assignment *a) {
            if (pass->assignActivityMap.count(a))
                pass->assignActivityMap[a].use.erase({bb, ir});
        }

        void visit(UnaryMIR *mir) override {
            eraseUse(mir->src);
        }

        void visit(BinaryMIR *mir) override {
            eraseUse(mir->src1);
            eraseUse(mir->src2);
        }

        void visit(ArrayAddressingMIR *mir) override {
            eraseUse(mir->offset);
        }


        void visit(LoadPointerMIR *mir) override {
            eraseUse(mir->src);
        }

        void visit(SelectMIR *mir) override {
            eraseUse(mir->cond);
            eraseUse(mir->src1);
            eraseUse(mir->src2);
        }

        void visit(CallWithAssignMIR *mir) override {
            for (auto *a : mir->args) {
                eraseUse(a);
            }
        }

    };
    Visitor v = {};
    v.pass = &analyzeActivityPass;
    v.ir = ir;
    v.bb = basicBlock;
    ir->accept(v);
}

void BackwardMovingPass::insertUse(BasicBlock *basicBlock, MIR *ir) {
    struct Visitor : MIR::Visitor {
        BasicBlock *bb{};
        MIR *ir{};
        AnalyzeActivityPass_MIR *pass{};

        void insert(Assignment *a) {
            if (pass->assignActivityMap.count(a))
                pass->assignActivityMap[a].use.insert({bb, ir});
        }

        void visit(UnaryMIR *mir) override {
            insert(mir->src);
        }

        void visit(BinaryMIR *mir) override {
            insert(mir->src1);
            insert(mir->src2);
        }

        void visit(ArrayAddressingMIR *mir) override {
            insert(mir->offset);
        }

        void visit(SelectMIR *mir) override {
            insert(mir->cond);
            insert(mir->src1);
            insert(mir->src2);
        }

        void visit(CallWithAssignMIR *mir) override {
            for (auto *a : mir->args) {
                insert(a);
            }
        }


        void visit(LoadPointerMIR *mir) override {
            insert(mir->src);
        }
    };
    Visitor v = {};
    v.pass = &analyzeActivityPass;
    v.ir = ir;
    v.bb = basicBlock;
    ir->accept(v);
}


void BackwardMovingPass::dfs_init_best_target(DomTreeNode *n) {
    DomTreeNode *f = n->father;
    BasicBlock *best_highest = n->block;
    int cycMin = (int) analyzeCyclePass.loopIn[n->block].size() * 1000;
    if (analyzeSinglePathPass.leaves[n->block]->father->father != nullptr &&
        analyzeSinglePathPass.leaves[n->block]->father->father->entrance == n->block)
        cycMin -= analyzeSinglePathPass.leaves[n->block]->father->father->father->depth;
    else cycMin -= analyzeSinglePathPass.leaves[n->block]->father->depth;
    //std::cout<<n->block->getName()<<" cost: "<<cycMin<<std::endl;
    int cycMin2 = cycMin;
    BasicBlock *best_lowest = n->block;
    while (f != nullptr) {
        int cyc = (int) analyzeCyclePass.loopIn[f->block].size() * 1000;
        if (analyzeSinglePathPass.leaves[f->block]->father->father != nullptr &&
            analyzeSinglePathPass.leaves[f->block]->father->father->entrance == f->block)
            cycMin -= analyzeSinglePathPass.leaves[f->block]->father->father->father->depth;
        else cycMin -= analyzeSinglePathPass.leaves[f->block]->father->depth;
        if (cyc <= cycMin) {
            cycMin = cyc;
            best_highest = f->block;
        }
        if (cyc < cycMin2) {
            cycMin2 = cyc;
            best_lowest = f->block;
        }
        bestTarget[{f->block, n->block}] = {best_highest, best_lowest};
        if (f == n) break;
        f = f->father;
    }
    for (auto &c : n->children) {
        dfs_init_best_target(c.get());
    }
}

BasicBlock *BackwardMovingPass::find_best_target(BasicBlock *def, BasicBlock *use, MIR *mir, bool highest) {
    auto m = dynamic_cast<LoadPointerMIR *>(mir);
    if (m != nullptr) {
        BasicBlock *ans = use;
        while (!analyzeCFGPass.result[ans].prev.empty()) {
            if (ans == bestTarget[{def, use}].second) break;
            auto *b = *analyzeCFGPass.result[ans].prev.begin();
            if (analyzeCFGPass.result[b].rear.size() != 1)break;
            ans = b;
        }
        return use;
    }
    if (def == use)
        return use;
    return bestTarget[{def, use}].second;
}

void BackwardMovingPass::dfs_operate(DomTreeNode *n) {
    struct Visitor : MIR::Visitor {
        BackwardMovingPass *pass = nullptr;
        bool canOperate = false;

        void visit(UnaryMIR *mir) override {
            canOperate = true;
        }

        void visit(BinaryMIR *mir) override {
            canOperate = true;
        }

        void visit(ValueAddressingMIR *mir) override {
            canOperate = true;
        }

        void visit(LoadConstantMIR *mir) override {
            canOperate = true;
        }

        void visit(ArrayAddressingMIR *mir) override {
            canOperate = true;
        }

        void visit(SelectMIR *mir) override {
            canOperate = true;
        }

        void visit(LoadPointerMIR *mir) override {
            canOperate = true;
        }

        void visit(CallWithAssignMIR *mir) override {
            FunctionSideEffect *sideEffects = pass->analyzeSideEffectPass.sideEffects[mir->func];
            if (!mir->func->isExternal &&
                mir->func != &pass->fn && //递归不可前移
                sideEffects->writeByPointerArg.empty() &&
                sideEffects->writeGlobalVariable.empty() &&
                sideEffects->readByPointerArg.empty() &&
                sideEffects->readGlobalVariable.empty() &&
                sideEffects->callExternalFunction.empty()) {
                canOperate = true;
            }
        }


    };

    for (auto c : getDomChildrenBlockNameOrder(n)) {
        dfs_operate(c);
    }

    Visitor visitor = {};
    visitor.pass = this;
    std::vector<unique_ptr<MIR>> mirTable = {};
    for (int i = (int) n->block->mirTable.size() - 1; i >= 0; i--) {
        auto &mir = n->block->mirTable[i];
        visitor.canOperate = false;
        mir->accept(visitor);
        if (!visitor.canOperate) {
            mirTable.push_back(std::move(mir));
            continue;
        }
        auto assignment = dynamic_cast<Assignment *> (mir.get());
        bool stay = assignment == nullptr;
        if (stay) {
            mirTable.push_back(std::move(mir));
            continue;
        }

        std::set<BasicBlock *> moveTo;
        std::set<BasicBlock *> usage;
        std::vector<std::pair<int, BasicBlock *>> usagePos;
        stay = false;

        moveTo = getUseBlock(assignment);
        for (auto *b: moveTo) {
            if (b == n->block) {
                stay = true;
                break;
            }
            b = find_best_target(n->block, b, mir.get(), true);
            usagePos.emplace_back(dfn[b].first, b);
        }

        if (!stay) {
            auto bounds = getBoundaries(assignment, moveTo);
            auto posThis = dfn[n->block];
            for (auto *b: bounds) {
                auto posBound = dfn[b];
                if (!(posThis.first <= posBound.first && posBound.second <= posThis.second)) {
                    continue;
                } else usage.insert(find_best_target(n->block, b, mir.get(), true));

            }
        }
        stay = stay || usage.empty();
        if (stay) {
            mirTable.push_back(std::move(mir));
            continue;
        }

        cut_child(moveTo);
        if (moveTo.size() == 1) {
            //具备支配性使用点，采用离使用点最近移动的策略
            usage.clear();
            usage.insert(find_best_target(n->block, *moveTo.begin(), mir.get(), false));
        }


        cut_child(usage);

        std::vector<std::pair<int, BasicBlock *>> dfnseq2(usage.size());
        int pos = 0;
        for (auto *b : usage)dfnseq2[pos++] = {dfn[b].first, b};
        std::sort(dfnseq2.begin(), dfnseq2.end());
        usage.clear();
        moveTo.clear();

        //查找连续区间
        int pUse = 0;
        std::sort(usagePos.begin(), usagePos.end());
        std::map<BasicBlock *, std::vector<BasicBlock *>> newUseDef;
        std::queue<BasicBlock *> tmp;
        int ll = -2, rr = -2;
        int mincyc = 0;
        for (auto &x : dfnseq2) {
            int l = x.first, r = dfn[x.second].second;
            if (l > rr + 1) {
                //新的连续区间
                ll = l;
                rr = r;
                mincyc = analyzeCyclePass.loopIn[x.second].size();
                while (!tmp.empty()) {
                    auto *b = tmp.front();
                    tmp.pop();
                    moveTo.insert(b);
                    while (pUse < usagePos.size() && dfn[usagePos[pUse].second].second <= dfn[b].second) {
                        //out<<mir->toString()<<" use: "<<usagePos[pUse].second->getName()<<" moveTo: "<<b->getName()<<std::endl;
                        newUseDef[b].push_back(usagePos[pUse++].second);
                    }
                }
                tmp.push(x.second);
                continue;
            } else {
                rr = std::max(rr, r);
                mincyc = std::min(mincyc, (int) analyzeCyclePass.loopIn[x.second].size());
                tmp.push(x.second);
            }
            //连续区间填满子树，且节点数>1，且父节点循环层数不增加，合并使用节点
            if (dfnBlocks.count({ll - 1, rr + 1})) {
                BasicBlock *b = dfnBlocks[{ll - 1, rr + 1}];
                if (analyzeCyclePass.loopIn[b].size() > mincyc) continue;
                moveTo.insert(b);
                while (pUse < usagePos.size() && dfn[usagePos[pUse].second].second <= dfn[b].second) {
                    //out<<mir->toString()<<" use: "<<usagePos[pUse].second->getName()<<" moveTo: "<<b->getName()<<std::endl;
                    newUseDef[b].push_back(usagePos[pUse++].second);
                }
                while (!tmp.empty())tmp.pop();
                ll--;
                rr++;
            }
        }
        while (!tmp.empty()) {
            auto *b = tmp.front();
            tmp.pop();
            moveTo.insert(b);
            while (pUse < usagePos.size() && dfn[usagePos[pUse].second].second <= dfn[b].second) {
                //out<<mir->toString()<<" use: "<<usagePos[pUse].second->getName()<<" moveTo: "<<b->getName()<<std::endl;
                newUseDef[b].push_back(usagePos[pUse++].second);
            }
        }

        if (moveTo.count(n->block)) {
            mirTable.push_back(std::move(mir));
            continue;
        }
        bool first = true;
        eraseUse(n->block, mir.get());
        MIR *ir = mir.get();
        for (auto *rear: moveTo) {
            //out<<ir->toString()<<':'<<n->block->getName()<<"-->"<<rear->getName()<<std::endl;
            if (!first) {
                MIR *newIR = copyMIR(ir, currentId);
                mirNew_children[rear].emplace_back(newIR);
                insertUse(rear, mirNew_children[rear].back().get());
                //可移动的mir一定是Assignment
                for (auto *b : newUseDef[rear]) {
                    if (!replaceTable.count(b))replaceTable[b] = {};
                    replaceTable[b][dynamic_cast<Assignment *>(ir)] = dynamic_cast<Assignment *>(newIR);
                }
            } else {
                first = false;
                insertUse(rear, mir.get());
                mirNew_children[rear].push_back(std::move(mir));
            }
        }
    }

    std::reverse(mirTable.begin(), mirTable.end());
    n->block->mirTable.swap(mirTable);
}

//去掉后代Block
void BackwardMovingPass::cut_child(std::set<BasicBlock *> &usage) {
    std::vector<std::pair<int, BasicBlock *>> dfnseq(usage.size());
    int pos = 0;
    for (auto *b : usage)dfnseq[pos++] = {dfn[b].first, b};
    std::sort(dfnseq.begin(), dfnseq.end());
    usage.clear();

    int pos2 = -1;
    BasicBlock *bb = nullptr;
    for (auto &x : dfnseq) {
        int l = x.first, r = dfn[x.second].second;
        if (l > pos2 && bb != nullptr)usage.insert(bb);
        if (pos2 < r) {
            pos2 = r;
            bb = x.second;
        }
    }
    if (bb != nullptr)usage.insert(bb);
}

void BackwardMovingPass::moveCMP(BasicBlock *b) {
    const std::set<BinaryMIR::Operator> cmp = {BinaryMIR::Operator::CMP_EQ, BinaryMIR::Operator::CMP_NE,
                                               BinaryMIR::Operator::CMP_GT, BinaryMIR::Operator::CMP_LT,
                                               BinaryMIR::Operator::CMP_GE, BinaryMIR::Operator::CMP_LE};
    for (int i = 0; i < b->mirTable.size() - 1; i++) {
        auto &mir = b->mirTable[i];
        auto assign = dynamic_cast<Assignment *> (mir.get());
        auto bin = dynamic_cast<BinaryMIR *> (mir.get());
        auto una = dynamic_cast<UnaryMIR *> (mir.get());
        bool isCmp = (bin != nullptr && cmp.count(bin->op)) || (una != nullptr && una->op == UnaryMIR::Operator::NOT);
        if (isCmp && !analyzeActivityPass.assignActivityMap[assign].usePos.empty()) {
            int minPos = INT32_MAX;
            for (auto &u : analyzeActivityPass.assignActivityMap[assign].usePos) {
                auto branch = dynamic_cast<BranchMIR *> (u.first->mirTable[u.second].get());
                auto select = dynamic_cast<SelectMIR *> (u.first->mirTable[u.second].get());
                if (u.first == b && (branch != nullptr || select != nullptr)) {
                    minPos = std::min(u.second, minPos);
                }
            }
            if (minPos != i + 1 && minPos != INT32_MAX) {
                std::swap(b->mirTable[i], b->mirTable[i + 1]);
                modified = true;
            }
        }
    }
}

std::set<BasicBlock *> BackwardMovingPass::getUseBlock(Assignment *assignment) {
    std::set<BasicBlock *> ret;
    for (auto &u : analyzeActivityPass.assignActivityMap[assignment].use) {
        ret.insert(u.first);
    }
    for (auto &u : analyzeActivityPass.assignActivityMap[assignment].useByPhi) {
        ret.insert(u.second.begin(), u.second.end());
    }
    return std::move(ret);
}

std::set<BasicBlock *> BackwardMovingPass::getBoundaries(Assignment *assignment, std::set<BasicBlock *> &ret) {
    if (analyzeArrayAccessPass.mirReadTable.count(assignment->castToMIR())) {
        std::set<BasicBlock *> ret2 = ret;
        auto &w = updateSet[analyzeArrayAccessPass.mirReadTable[assignment->castToMIR()]->useLastWrite];
        ret2.insert(w.begin(), w.end());
        return ret2;
    }
    return ret;
}