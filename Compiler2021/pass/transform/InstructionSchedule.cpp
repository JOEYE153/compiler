//
// Created by Joeye on 2021/8/14.
//

#include <queue>
#include "InstructionSchedule.h"

bool InstructionSchedule::run() {
    analyzeCFGPass.invalidate();
    analyzeDomTreePass.invalidate();
    analyzeDomTreePass.run();
    schedule(analyzeDomTreePass.root, {});
    analyzeCFGPass.invalidate();
    analyzeDomTreePass.invalidate();
    return false;
}

void InstructionSchedule::schedule(DomTreeNode *domTreeNode, std::map<CoreRegAssign *, DAGNode *> assignMapNode) {
    // 构建DAG
    std::set<DAGNode *> nodes;

    std::map<int, DAGNode *> statusDef; // compare and bl def
    std::map<int, DAGNode *> statusUse; // setFlag and conditional instr use



    vector<std::pair<BranchAndLinkInst *, DAGNode *>> blMapNode;
    vector<std::pair<MovtInst *, DAGNode *>> movtMapNode;
    vector<std::pair<LIR *, DAGNode *>> nodePair; // 保证原指令序
    std::map<std::pair<LIR *, int>, DAGNode *> loadMapNode;
    std::map<std::pair<LIR *, int>, DAGNode *> storeMapNode;
    std::map<DAGNode *, LIR *> nodeMapLIR;
    vector<unique_ptr<LIR>> end; // Branch指令，bx指令，还原栈空间指令，Pop指令
    vector<unique_ptr<LIR>> lirTable;

    std::map<int, regDefUse> regActivity;
    for (int i = 0; i < 15; i++) {
        regDefUse defUse;
        regActivity[i] = defUse;
    }

    for (auto &iter:assignMapNode) {
        nodes.insert(iter.second); // 保证使用前序基本块内的assign时在获取拓扑序时有起始点
    }

    int idx = 0;
    for (auto &lir_ptr: domTreeNode->block->lirTable) {
        auto lir = lir_ptr.release();
        auto sp = dynamic_cast<BinaryImmInst *>(lir);
        if (sp != nullptr && sp->pReg.value() == 13) {
            if (sp->op == ArmInst::BinaryOperator::ADD) {
                end.emplace_back(lir); // add sp, sp, #imm
            } else {
                lirTable.emplace_back(lir); // sub sp, sp, #imm
            }
            continue;
        }
        auto push = dynamic_cast<PushInst *>(lir);
        if (push != nullptr) {
            lirTable.emplace_back(lir);
            continue;
        }
        auto pop = dynamic_cast<PopInst *>(lir);
        if (pop != nullptr) {
            end.emplace_back(lir);
            continue;
        }
        auto branchInst = dynamic_cast<BranchInst *>(lir);
        if (branchInst != nullptr) {
            end.emplace_back(lir);
            continue;
        }
        auto branchExInst = dynamic_cast<BranchAndExchangeInst *>(lir);
        if (branchExInst != nullptr) {
            end.emplace_back(lir);
            continue;
        }


        auto lastBL = blMapNode.empty() ? nullptr : blMapNode.rbegin()->second;
        auto lastMovt = movtMapNode.empty() ? nullptr : movtMapNode.rbegin()->second;
        auto node = createDAGNode(lir, assignMapNode, nodePair, statusDef, statusUse, regActivity, idx, lastBL,
                                  lastMovt);
        nodePair.emplace_back(std::make_pair(lir, node));
        idx++;
        auto armInst = dynamic_cast<ArmInst *>(lir);
        if (armInst == nullptr) continue;
        nodes.insert(node);
        nodeMapLIR[node] = lir;

        auto bl = dynamic_cast<BranchAndLinkInst *>(lir);
        if (bl != nullptr) {
            blMapNode.emplace_back(std::make_pair(bl, node));
            continue;
        }

        auto movt = dynamic_cast<MovtInst *>(lir);
        if (movt != nullptr) {
            movtMapNode.emplace_back(std::make_pair(movt, node));
            continue;
        }

        auto loadReg = dynamic_cast<LoadRegInst *>(lir);
        auto loadImm = dynamic_cast<LoadImmInst *>(lir);
        if (loadReg != nullptr || loadImm != nullptr) {
            loadMapNode[std::make_pair(lir, idx)] = node;
            continue;
        }

        auto storeReg = dynamic_cast<StoreRegInst *>(lir);
        auto storeImm = dynamic_cast<StoreImmInst *>(lir);
        if (storeReg != nullptr || storeImm != nullptr) {
            storeMapNode[std::make_pair(lir, idx)] = node;
            continue;
        }

        // 建立bl返回值与bl的关联
        if (idx > 1 && dynamic_cast<GetReturnValueLIR *>(nodePair[idx - 2].first) != nullptr) {
            auto mov = dynamic_cast<UnaryRegInst *>(lir);
            if (mov != nullptr && mov->rm->pReg.value() == 0) {
                auto last = blMapNode.rbegin();
                last->second->father.insert(node);
                node->son.insert(last->second);
            }
        }
    }
    for (auto &iter:regActivity) {
        // use-def
        for (auto &use: iter.second.use) {
            for (auto &def: iter.second.def) {
                if (use.first < def.first) {
                    use.second->father.insert(def.second);
                    def.second->son.insert(use.second);
                }
            }
        }
    }

    for (auto &iter:regActivity) {
        // def-use
        for (auto &def: iter.second.def) {
            for (auto &use: iter.second.use) {
                if (use.first > def.first) {
                    use.second->son.insert(def.second);
                    def.second->father.insert(use.second);
                }
            }
        }
    }

    // cpsr use def
    for (auto &def:statusDef) {
        for (auto &use:statusUse) {
            if (use.first > def.first) {
                def.second->father.insert(use.second);
                use.second->son.insert(def.second);
            } else {
                use.second->father.insert(def.second);
                def.second->son.insert(use.second);
            }
        }
    }

    // cpsr def def
    for (auto &def: statusDef) {
        for (auto &cmp: statusDef) {
            if (cmp.first > def.first) {
                def.second->father.insert(cmp.second);
                cmp.second->son.insert(def.second);
            }
        }
    }

    // 读写加边
    for (auto &load:loadMapNode) {
        for (auto &store:storeMapNode) {
            if (load.first.second < store.first.second) {
                load.second->father.insert(store.second);
                store.second->son.insert(load.second);
            } else {
                load.second->son.insert(store.second);
                store.second->father.insert(load.second);
            }
        }
    }

    // 获取拓扑序
    auto topOrder = getTopologicalOrder(nodes);
    for (auto &node:topOrder) {
        auto iter = nodeMapLIR.find(node);
        if (iter != nodeMapLIR.end()) {
            lirTable.emplace_back(iter->second);
        }
    }
    for (auto &lir:end) {
        lirTable.emplace_back(std::move(lir));
    }
    domTreeNode->block->lirTable.swap(lirTable);
    for (auto &child: getDomChildrenBlockNameOrder(domTreeNode)) {
        schedule(child, assignMapNode);
    }
}


DAGNode *InstructionSchedule::createDAGNode(LIR *lir, std::map<CoreRegAssign *, DAGNode *> &assignMapNode,
                                            vector<std::pair<LIR *, DAGNode *>> &nodePair,
                                            std::map<int, DAGNode *> &statusDef, std::map<int, DAGNode *> &statusUse,
                                            std::map<int, regDefUse> &regActivity, int idx, DAGNode *lastBL,
                                            DAGNode *lastMovt) {
    struct MyVisitor : LIR::Visitor {
        DAGNode *node;
        std::map<CoreRegAssign *, DAGNode *> &assignMapNode;
        vector<std::pair<LIR *, DAGNode *>> &nodePair;
        std::map<int, regDefUse> &regActivity;
        int idx;
        DAGNode *lastBL;
        DAGNode *lastMovt;
        std::map<int, DAGNode *> &statusDef;
        std::map<int, DAGNode *> &statusUse;

        explicit MyVisitor(DAGNode *node, std::map<CoreRegAssign *, DAGNode *> &assignMapNode,
                           vector<std::pair<LIR *, DAGNode *>> &nodePair, std::map<int, regDefUse> &regActivity,
                           std::map<int, DAGNode *> &statusDef, std::map<int, DAGNode *> &statusUse, int idx,
                           DAGNode *lastBL, DAGNode *lastMovt)
                : node(node), assignMapNode(assignMapNode), nodePair(nodePair), statusDef(statusDef),
                  regActivity(regActivity), idx(idx), lastBL(lastBL), lastMovt(lastMovt), statusUse(statusUse) {}

        void addUse(int pReg) {
            if (pReg == 13) return;
            regActivity[pReg].use[idx] = node;
        }

        void addDef(int pReg) {
            if (pReg == 13) return;
            regActivity[pReg].def[idx] = node;
        }

        void addSon(CoreRegAssign *src) {
            if (src == nullptr) return;
            auto iter = assignMapNode.find(src);
            if (iter == assignMapNode.end()) {
                // 初始loadArg时的r0~r3, sp寄存器
                return;
            }
            iter->second->father.insert(node);
            node->son.insert(iter->second);
        }

        void setSelf(CoreRegAssign *self) {
            auto iter = assignMapNode.find(self);
            if (iter != assignMapNode.end()) {
                std::cerr << "[InstructionSchedule]!!!!!Set Self!!!!!\n";
                return;
            }
            assignMapNode[self] = node;
        }

        void addStatusUse(StatusRegAssign *cpsr) {
            if (cpsr == nullptr) return;
            statusUse[idx] = node;
        }

        void visit(UnaryRegInst *lir) override {
            addSon(lir->rm);
            setSelf(lir);
            addStatusUse(lir->cpsr);
            addSon(lir->rd);
            node->weight = 1;

            addDef(lir->pReg.value());
            addUse(lir->rm->pReg.value());
        }

        void visit(BinaryRegInst *lir) override {
            addSon(lir->rm);
            addSon(lir->rn);
            setSelf(lir);
            addStatusUse(lir->cpsr);
            addSon(lir->rd);
            node->weight = 1;

            addDef(lir->pReg.value());
            addUse(lir->rm->pReg.value());
            addUse(lir->rn->pReg.value());
        }

        void visit(CompareRegInst *lir) override {
            addSon(lir->rm);
            addSon(lir->rn);
            statusDef[idx] = node;
            node->weight = 1;

            addUse(lir->rm->pReg.value());
            addUse(lir->rn->pReg.value());
        }

        void visit(UnaryShiftInst *lir) override {
            addSon(lir->rm);
            addSon(lir->rs);
            setSelf(lir);
            addStatusUse(lir->cpsr);
            addSon(lir->rd);
            node->weight = 1;

            addDef(lir->pReg.value());
            addUse(lir->rm->pReg.value());
            addUse(lir->rs->pReg.value());
        }

        void visit(BinaryShiftInst *lir) override {
            addSon(lir->rm);
            addSon(lir->rn);
            addSon(lir->rs);
            setSelf(lir);
            addStatusUse(lir->cpsr);
            addSon(lir->rd);
            node->weight = 1;

            addDef(lir->pReg.value());
            addUse(lir->rm->pReg.value());
            addUse(lir->rn->pReg.value());
            addUse(lir->rs->pReg.value());
        }

        void visit(CompareShiftInst *lir) override {
            addSon(lir->rm);
            addSon(lir->rn);
            addSon(lir->rs);
            statusDef[idx] = node;
            node->weight = 1;

            addUse(lir->rm->pReg.value());
            addUse(lir->rn->pReg.value());
            addUse(lir->rs->pReg.value());
        }

        void visit(UnaryImmInst *lir) override {
            setSelf(lir);
            addStatusUse(lir->cpsr);
            addSon(lir->rd);
            node->weight = 1;

            addDef(lir->pReg.value());
        }

        void visit(BinaryImmInst *lir) override {
            addSon(lir->rn);
            setSelf(lir);
            addStatusUse(lir->cpsr);
            addSon(lir->rd);
            node->weight = 1;

            addDef(lir->pReg.value());
            addUse(lir->rn->pReg.value());
        }

        void visit(CompareImmInst *lir) override {
            addSon(lir->rn);
            statusDef[idx] = node;
            node->weight = 1;

            addUse(lir->rn->pReg.value());
        }

        void visit(SetFlagInst *lir) override {
            addStatusUse(lir);
            node->weight = 1;
        }

        void visit(LoadRegInst *lir) override {
            addSon(lir->rm);
            addSon(lir->rn);
            setSelf(lir);
            addStatusUse(lir->cpsr);
            addSon(lir->rd);
            node->weight = 3;

            addDef(lir->pReg.value());
            addUse(lir->rm->pReg.value());
            addUse(lir->rn->pReg.value());
        }

        void visit(LoadImmInst *lir) override {
            addSon(lir->rn);
            setSelf(lir);
            addStatusUse(lir->cpsr);
            addSon(lir->rd);
            node->weight = 3;

            addDef(lir->pReg.value());
            addUse(lir->rn->pReg.value());
        }

        void visit(StoreRegInst *lir) override {
            addSon(lir->rm);
            addSon(lir->rn);
            addSon(lir->rt);
            addStatusUse(lir->cpsr);
            node->weight = 3;

            addUse(lir->rt->pReg.value());
            addUse(lir->rm->pReg.value());
            addUse(lir->rn->pReg.value());
        }

        void visit(StoreImmInst *lir) override {
            addSon(lir->rt);
            addSon(lir->rn);
            addStatusUse(lir->cpsr);
            node->weight = 3;

            addUse(lir->rt->pReg.value());
            addUse(lir->rn->pReg.value());
        }

        void visit(WriteBackAddressInst *lir) override {
            for (auto rIter = nodePair.rbegin(); rIter != nodePair.rend(); rIter++) {
                if (rIter->first == lir->lastInst) {
                    rIter->second->father.insert(node);
                    node->son.insert(rIter->second);
                    break;
                }
            }
            setSelf(lir);
            node->weight = 1;
        }

        void visit(MultiplyInst *lir) override {
            addSon(lir->rm);
            addSon(lir->rn);
            setSelf(lir);
            addStatusUse(lir->cpsr);
            addSon(lir->rd);
            node->weight = 3;

            addDef(lir->pReg.value());
            addUse(lir->rm->pReg.value());
            addUse(lir->rn->pReg.value());
            if (lir->ra != nullptr) {
                addUse(lir->ra->pReg.value());
            }
        }

        void visit(Multiply64GetHighInst *lir) override {
            addSon(lir->rm);
            addSon(lir->rn);
            setSelf(lir);
            addStatusUse(lir->cpsr);
            addSon(lir->rd);
            node->weight = 3;

            addDef(lir->pReg.value());
            addUse(lir->rm->pReg.value());
            addUse(lir->rn->pReg.value());
        }

        void visit(DivideInst *lir) override {
            addSon(lir->rm);
            addSon(lir->rn);
            setSelf(lir);
            addStatusUse(lir->cpsr);
            addSon(lir->rd);
            node->weight = 9;

            addDef(lir->pReg.value());
            addUse(lir->rm->pReg.value());
            addUse(lir->rn->pReg.value());
        }

        void visit(MovwInst *lir) override {
            setSelf(lir);
            addStatusUse(lir->cpsr);
            addSon(lir->rd);
            node->weight = 1;

            addDef(lir->pReg.value());

            if (lastMovt != nullptr) {
                lastMovt->father.insert(node);
                node->son.insert(lastMovt);
            }
        }

        void visit(MovtInst *lir) override {
            setSelf(lir);
            addStatusUse(lir->cpsr);
            addSon(lir->rd);
            node->weight = 1;

            addDef(lir->pReg.value());
        }

        void visit(BranchInst *lir) override {
            // 在基本块的最后
        }

        void visit(BranchAndLinkInst *lir) override {
            addDef(0);
            addDef(1);
            addDef(2);
            addDef(3);
            addDef(12);
            addDef(14);

            addUse(0);
            addUse(1);
            addUse(2);
            addUse(3);
            addUse(12);
            addUse(14);

            statusDef[idx] = node;

            node->weight = 2;
            // bl前传递参数
            int id = (int) nodePair.size() - 1;
            bool hasArg = false;
            for (auto rIter = nodePair.rbegin(); rIter != nodePair.rend(); rIter++, id--) {
                auto setArg = dynamic_cast<SetArgumentLIR *>(rIter->first);
                if (setArg != nullptr && setArg->index == 0) {
                    hasArg = true;
                    break;
                }
            }
            if (!hasArg) return;

            for (int i = id; i < nodePair.size(); i++) {
                auto armInst = dynamic_cast<ArmInst *>(nodePair[i].first);
                if (armInst != nullptr) {
                    if (lastBL != nullptr) {
                        lastBL->father.insert(nodePair[i].second);
                        nodePair[i].second->son.insert(lastBL);
                    }
                    nodePair[i].second->father.insert(node);
                    node->son.insert(nodePair[i].second);
                }
            }
        }

        void visit(BranchAndExchangeInst *lir) override {
            // 在基本块的最后
        }

        void visit(PushInst *lir) override {
            // 在基本块的开始
        }

        void visit(PopInst *lir) override {
            // 在基本块的最后
        }
    };

    auto node = new DAGNode;
    node->father = {};
    node->son = {};
    node->weight = 0;
    MyVisitor visitor(node, assignMapNode, nodePair, regActivity, statusDef, statusUse, idx, lastBL, lastMovt);
    lir->accept(visitor);
    return node;
}

vector<DAGNode *> InstructionSchedule::getTopologicalOrder(std::set<DAGNode *> &nodes) {
    vector<DAGNode *> res;
    std::queue<DAGNode *> queue;
    std::set<DAGNode *> visit;
    for (auto &node:nodes) {
        // 可能存在前序定义但在本块中没有使用的孤立点
        if (node->son.empty()) {
            queue.push(node);
        }
    }
    while (!queue.empty()) {
        auto first = queue.front();
        queue.pop();
        if (visit.find(first) != visit.end()) continue;
        res.emplace_back(first);
        visit.insert(first);
        for (auto &suc: first->father) {
            suc->son.erase(first);
            if (suc->son.empty()) {
                queue.push(suc);
            }
        }
    }
    return res;
}


