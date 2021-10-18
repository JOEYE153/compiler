//
// Created by hujin on 2021/7/31.
//

#include <algorithm>
#include <cassert>
#include "ForwardMovingPass.h"

bool ForwardMovingPass::run() {
    analyzeDomTreePass.run();
    analyzeActivityPass.run();
    analyzeArrayAccessPass.run();
    analyzeSideEffectPass.run();
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
    dfs_operate(analyzeDomTreePass.root);
    analyzeActivityPass.invalidate();
    return modified;
}

bool ForwardMovingPass::canMoveForward(ArrayRead *read, BasicBlock *curBlock) {
    return read->useLastWrite != nullptr && writePos[read->useLastWrite] != curBlock;
}

void ForwardMovingPass::dfs_operate(DomTreeNode *node) {
    for (auto *rear :getDomChildrenBlockNameOrder(node)) {
        dfs_operate(rear);
    }
    if (node->father == nullptr)return;
    struct Visitor : MIR::Visitor {
        ForwardMovingPass *pass = nullptr;
        bool canOperate = false;
        BasicBlock *thiz = nullptr;

        void check(Assignment *use) {
            //use没有被向前移动
            canOperate = canOperate && pass->analyzeActivityPass.assignActivityMap[use].def.first != thiz;
        }

        void visit(UninitializedMIR *mir) override {
            canOperate = true;
        }

        void visit(UnaryMIR *mir) override {
            canOperate = true;
            check(mir->src);
        }

        void visit(BinaryMIR *mir) override {
            canOperate = true;
            check(mir->src1);
            check(mir->src2);
        }

        void visit(ValueAddressingMIR *mir) override {
            canOperate = true;
        }

        void visit(LoadConstantMIR *mir) override {
            canOperate = true;
        }

        void visit(ArrayAddressingMIR *mir) override {
            canOperate = true;
            check(mir->offset);
            auto base_ptr = dynamic_cast<Assignment *>(mir->base);
            if (base_ptr != nullptr) {
                check(base_ptr);
            }
        }

        void visit(SelectMIR *mir) override {
            canOperate = true;
            check(mir->cond);
            check(mir->src1);
            check(mir->src2);
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
                for (auto *x : mir->args) {
                    check(x);
                }
            }
        }

        void visit(JumpMIR *mir) override {
            canOperate = false;
        }

        void visit(BranchMIR *mir) override {
            canOperate = false;
        }

        void visit(PhiMIR *mir) override {
            //如果canOperate = true 即子节点在CFG上只有一个入口，不应存在Phi
            //或者进行了控制流操作后未进行Phi化简（Constant Folding Pass)
            canOperate = false;
        }

        void visit(ReturnMIR *mir) override {
            canOperate = false;
        }

        void visit(LoadPointerMIR *mir) override {
            if (pass->analyzeArrayAccessPass.mirReadTable.count(mir)) {
                auto read = pass->analyzeArrayAccessPass.mirReadTable[mir];
                if (read != nullptr) canOperate = canOperate || pass->canMoveForward(read, thiz);
                check(mir->src);
            }
        }

        void visit(StorePointerMIR *mir) override {
            if (pass->analyzeArrayAccessPass.mirWriteTable.count(mir)) {
                auto write = pass->analyzeArrayAccessPass.mirWriteTable[mir];
                check(mir->src);
                check(mir->dst);
            }
        }

        void visit(MemoryFillMIR *mir) override {
            if (pass->analyzeArrayAccessPass.mirWriteTable.count(mir)) {
                auto write = pass->analyzeArrayAccessPass.mirWriteTable[mir];
                check(mir->src);
                check(mir->dst);
            }
        }

        void visit(MemoryCopyMIR *mir) override {
            if (pass->analyzeArrayAccessPass.mirWriteTable.count(mir) &&
                pass->analyzeArrayAccessPass.mirReadTable.count(mir)) {
                auto write = pass->analyzeArrayAccessPass.mirWriteTable[mir];
                auto read = pass->analyzeArrayAccessPass.mirReadTable[mir];
                check(mir->src);
                check(mir->dst);
            }
        }

        void visit(CallMIR *mir) override {
            for (auto *x : mir->args) {
                check(x);
            }
        }

        void visit(MultiCallMIR *mir) override {
            for (auto *x : mir->args) {
                check(x);
            }
        }

        void visit(AtomicLoopCondMIR *mir) override {
            check(mir->step);
            check(mir->border);
        }
    };
    vector<unique_ptr<MIR>> mirTable;
    bool force = analyzeCFGPass.result[node->father->block].rear.size() == 1
                 && analyzeCFGPass.result[node->block].prev.size() == 1;
    for (auto &mir : node->block->mirTable) {
        Visitor visitor;
        visitor.pass = this;
        visitor.thiz = node->block;
        visitor.canOperate = force;
        mir->accept(visitor);
        if (visitor.canOperate) {
            auto *a = dynamic_cast<Assignment *> (mir.get());
            int pos = (int) node->father->block->mirTable.size() - 1;
            analyzeActivityPass.assignActivityMap[a].def = std::make_pair(node->father->block, pos);
            if (analyzeArrayAccessPass.mirWriteTable.count(mir.get())) {
                auto write = analyzeArrayAccessPass.mirWriteTable[mir.get()];
                writePos[write] = node->father->block;
            }
            //父节点的末尾mir一定是jump或branch
            node->father->block->mirTable.insert(node->father->block->mirTable.end() - 1, std::move(mir));
            modified = true;
        } else {
            mirTable.emplace_back(std::move(mir));
        }
    }
    node->block->mirTable.swap(mirTable);
    mirTable.clear();
}
