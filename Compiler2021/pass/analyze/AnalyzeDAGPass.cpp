//
// Created by 陈思言 on 2021/7/20.
//

#include "AnalyzeDAGPass.h"

bool AnalyzeDAGPass::analyze() {
    if (run_floyd) {
        for (size_t k = 0; k < exprNum; k++) {
            for (size_t i = 0; i < k; i++) {
                if (getEdgeAt(i, k) == 0) {
                    continue;
                }
                for (size_t j = k; j < exprNum; j++) {
                    if (getEdgeAt(k, j) == 0) {
                        continue;
                    }
                    int tmp = getEdgeAt(i, k) + getEdgeAt(k, j);
                    if (getEdgeAt(i, j) == 0 || tmp < getEdgeAt(i, j)) {
                        edgeMat[i * exprNum + j] = tmp;
                        typeMat[i * exprNum + j] |= INDIRECT;
                    }
                }
            }
        }
    }
    for (size_t i = 0; i < exprNum; i++) {
        for (size_t j = 0; j < i; j++) {
            edgeMat[i * exprNum + j] = -edgeMat[j * exprNum + i];
            typeMat[i * exprNum + j] = typeMat[j * exprNum + i];
        }
    }
//    for (size_t i = 0; i < exprNum; i++) {
//        for (size_t j = 0; j < exprNum; j++) {
//            printf("(%d, %x) ", getEdgeAt(i, j), getTypeAt(i, j));
//        }
//        puts("");
//    }
//    puts("");
    return true;
}

void AnalyzeDAGPass::invalidate() {
    AnalyzePass::invalidate();
    edgeMat.clear();
    typeMat.clear();
    phiNum = 0;
    exprNum = 0;
}

void AnalyzeDAGPass::print() const {
    for (size_t i = 0; i < exprNum; i++) {
        for (size_t j = 0; j < exprNum; j++) {
            printf("%04d ", getEdgeAt(i, j));
        }
        puts("");
    }
    puts("");
    for (size_t i = 0; i < exprNum; i++) {
        for (size_t j = 0; j < exprNum; j++) {
            printf("%04x ", getTypeAt(i, j));
        }
        puts("");
    }
    puts("");
}

bool AnalyzeDAGPass_LIR::analyze() {
    struct MyVisitor : LIR::Visitor {
        AnalyzeDAGPass_LIR &pass;
        size_t currentIndex = 0;
        std::map<RegAssign *, size_t> regAssignIndexMap;
        std::map<LIR *, size_t> loadIndexMap;
        std::map<LIR *, size_t> storeIndexMap;
        std::map<CallMIR *, size_t> callIndexMap;

        explicit MyVisitor(AnalyzeDAGPass_LIR &pass) : pass(pass) {}

        void addDef(RegAssign *dst) {
            regAssignIndexMap[dst] = currentIndex;
        }

        void addUse(RegAssign *src) {
            auto iter = regAssignIndexMap.find(src);
            if (iter != regAssignIndexMap.end()) {
                pass.addAt(iter->second, currentIndex, DEF_USE);
            }
        }

        void visitCond(CondInst *lir) {
            if (lir->cpsr != nullptr) {
                addUse(lir->cpsr);
            }
            if (lir->rd != nullptr) {
                addUse(lir->rd);
            }
        }

        void visit(UnaryRegInst *lir) override {
            addDef(lir);
            addUse(lir->rm);
            visitCond(lir);
        }

        void visit(BinaryRegInst *lir) override {
            addDef(lir);
            addUse(lir->rn);
            addUse(lir->rm);
            visitCond(lir);
        }

        void visit(CompareRegInst *lir) override {
            addUse(lir->rn);
            addUse(lir->rm);
            visitCond(lir);
        }

        void visit(UnaryShiftInst *lir) override {
            addDef(lir);
            addUse(lir->rm);
            addUse(lir->rs);
            visitCond(lir);
        }

        void visit(BinaryShiftInst *lir) override {
            addDef(lir);
            addUse(lir->rn);
            addUse(lir->rm);
            addUse(lir->rs);
            visitCond(lir);
        }

        void visit(CompareShiftInst *lir) override {
            addUse(lir->rn);
            addUse(lir->rm);
            addUse(lir->rs);
            visitCond(lir);
        }

        void visit(UnaryImmInst *lir) override {
            addDef(lir);
            visitCond(lir);
        }

        void visit(BinaryImmInst *lir) override {
            addDef(lir);
            addUse(lir->rn);
            visitCond(lir);
        }

        void visit(CompareImmInst *lir) override {
            addUse(lir->rn);
            visitCond(lir);
        }

        void visit(SetFlagInst *lir) override {
            addDef(lir);
            pass.addAt(currentIndex - 1, currentIndex, DEF_USE);
        }

        void visit(LoadRegInst *lir) override {
            addDef(lir);
            addUse(lir->rn);
            addUse(lir->rm);
            visitCond(lir);
            for (auto item : storeIndexMap) {
                pass.addAt(item.second, currentIndex, STORE_LOAD);
            }
            loadIndexMap[lir] = currentIndex;
        }

        void visit(LoadImmInst *lir) override {
            addDef(lir);
            addUse(lir->rn);
            visitCond(lir);
            for (auto item : storeIndexMap) {
                pass.addAt(item.second, currentIndex, STORE_LOAD);
            }
            loadIndexMap[lir] = currentIndex;
        }

        void visit(StoreRegInst *lir) override {
            addUse(lir->rt);
            addUse(lir->rn);
            addUse(lir->rm);
            visitCond(lir);
            for (auto item : loadIndexMap) {
                pass.addAt(item.second, currentIndex, LOAD_STORE);
            }
            for (auto item : storeIndexMap) {
                pass.addAt(item.second, currentIndex, STORE_STORE);
            }
            storeIndexMap[lir] = currentIndex;
        }

        void visit(StoreImmInst *lir) override {
            addUse(lir->rt);
            addUse(lir->rn);
            visitCond(lir);
            for (auto item : loadIndexMap) {
                pass.addAt(item.second, currentIndex, LOAD_STORE);
            }
            for (auto item : storeIndexMap) {
                pass.addAt(item.second, currentIndex, STORE_STORE);
            }
            storeIndexMap[lir] = currentIndex;
        }

        void visit(WriteBackAddressInst *lir) override {
            addDef(lir);
            pass.addAt(currentIndex - 1, currentIndex, DEF_USE);
        }

        void visit(MultiplyInst *lir) override {
            addDef(lir);
            addUse(lir->rn);
            addUse(lir->rm);
            if (lir->ra != nullptr) {
                addUse(lir->ra);
            }
            visitCond(lir);
        }

        void visit(Multiply64GetHighInst *lir) override {
            addDef(lir);
            addUse(lir->rn);
            addUse(lir->rm);
            visitCond(lir);
        }

        void visit(DivideInst *lir) override {
            addDef(lir);
            addUse(lir->rn);
            addUse(lir->rm);
            visitCond(lir);
        }

        void visit(MovwInst *lir) override {
            addDef(lir);
            visitCond(lir);
        }

        void visit(MovtInst *lir) override {
            addDef(lir);
            visitCond(lir);
        }

        void visit(BranchInst *lir) override {
            visitCond(lir);
            for (size_t i = 0; i < currentIndex; i++) {
                pass.addAt(i, currentIndex, CONTROL_FLOW);
            }
        }

        void visit(BranchAndLinkInst *lir) override {
            visitCond(lir);
            for (size_t i = 0; i < currentIndex; i++) {
                pass.addAt(i, currentIndex, SYSTEM_CALL);
            }
        }

        void visit(LoadImm32LIR *lir) override {
            addDef(lir);
        }

        void visit(ValueAddressingLIR *lir) override {
            addDef(lir);
        }

        void visit(GetArgumentLIR *lir) override {
            addDef(lir);
        }

        void visit(SetArgumentLIR *lir) override {
            addUse(lir->src);
        }

        void visit(GetReturnValueLIR *lir) override {
            addDef(lir);
        }

        void visit(ReturnLIR *lir) override {
            if (lir->val != nullptr) {
                addUse(lir->val);
            }
            for (size_t i = 0; i < currentIndex; i++) {
                pass.addAt(i, currentIndex, CONTROL_FLOW);
            }
        }

        void visit(LoadScalarLIR *lir) override {
            addDef(lir);
            for (auto item : storeIndexMap) {
                pass.addAt(item.second, currentIndex, STORE_LOAD);
            }
            loadIndexMap[lir] = currentIndex;
        }

        void visit(StoreScalarLIR *lir) override {
            addUse(lir->src);
            for (auto item : loadIndexMap) {
                pass.addAt(item.second, currentIndex, LOAD_STORE);
            }
            for (auto item : storeIndexMap) {
                pass.addAt(item.second, currentIndex, STORE_STORE);
            }
            storeIndexMap[lir] = currentIndex;
        }

        void visit(UninitializedCoreReg *lir) override {
            addDef(lir);
        }

        void visit(CoreRegPhiLIR *lir) override {
            addDef(lir);
            pass.phiNum++;
        }

        void visit(AtomicLoopCondLIR *lir) override {
            addDef(lir);
            addUse(lir->atomic_var_ptr);
            addUse(lir->step);
            addUse(lir->border);
            addUse(lir->tmp);
        }
    };

    exprNum = bb.lirTable.size();
    edgeMat.resize(exprNum * exprNum, 0);
    typeMat.resize(exprNum * exprNum, NONE);
    MyVisitor visitor(*this);
    for (auto &lir : bb.lirTable) {
        for (auto item : visitor.callIndexMap) {
            addAt(item.second, visitor.currentIndex, SYSTEM_CALL); // FAKE
        }
        lir->accept(visitor);
        visitor.currentIndex++;
    }
    return AnalyzeDAGPass::analyze();
}
