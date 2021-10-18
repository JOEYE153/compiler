//
// Created by Joeye on 2021/8/10.
//

#include "InstructionFusion.h"
#include "../../utils/AssemblyUtils.h"
#include "../../utils/BasicBlockUtils.h"

bool InstructionFusion::run() {
    analyzeCFGPass.invalidate();
    analyzeDomTreePass.invalidate();
    analyzeActivityPass.invalidate();
    analyzeDataFlowPass.invalidate();
    analyzeDomTreePass.run();
    analyzeDataFlowPass.run();
    fuse(analyzeDomTreePass.root, {});
    out << fn.getName() << " fusion before: " << originInstrCnt << " after: " << instrCnt << " new: " << newInstrCnt
        << "\n";
    analyzeCFGPass.invalidate();
    analyzeActivityPass.invalidate();
    analyzeDataFlowPass.invalidate();
    analyzeDomTreePass.invalidate();
    return newInstrCnt == 0;
}

void InstructionFusion::fuse(DomTreeNode *node, std::map<CoreRegAssign *, CoreRegAssign *> replaceTable) {
    // 物理寄存器的def,use分析
    auto &bb = *node->block;
    std::map<int, PRegActivity> regActivity;
    for (int i = 0; i < 15; i++) {
        PRegActivity pRegActivity;
        regActivity[i] = pRegActivity;
    }
    collectDefUsePos(bb, regActivity, replaceTable);
    auto outIter = analyzeDataFlowPass.dataFlowMapCoreReg.find(&bb);
    if (outIter != analyzeDataFlowPass.dataFlowMapCoreReg.end()) {
        for (auto assign:outIter->second.out) {
            regActivity[assign->pReg.value()].use.insert((int) bb.lirTable.size());
        }
    }
    // 判断可以融合的指令对
    auto fusionInstr = getFusionInstruction(bb, regActivity);

    // 指令融合
    std::map<LIR *, vector<LIR *>> fusionRes;
    for (auto &iter:fusionInstr) {
        auto def = iter.first;
        auto use = iter.second;
        // Unary, Binary与Binary指令的融合
        auto biRegUse = dynamic_cast<BinaryRegInst *>(use);
        if (biRegUse != nullptr) {
            auto uRegDef = dynamic_cast<UnaryRegInst *>(def);
            if (uRegDef != nullptr && uRegDef->shiftImm == 0 && biRegUse->cond == uRegDef->cond &&
                biRegUse->cpsr == uRegDef->cpsr && uRegDef->op == ArmInst::UnaryOperator::MOV) {
                // def:mov reg, use:Binary, mov无移位, Binary可以有移位
                if (uRegDef->pReg.value() == biRegUse->rm->pReg.value() &&
                    uRegDef->pReg.value() == biRegUse->rn->pReg.value()) {
                    biRegUse->rm = uRegDef->rm;
                    biRegUse->rn = uRegDef->rm;
                } else if (uRegDef->pReg.value() == biRegUse->rm->pReg.value()) {
                    biRegUse->rm = uRegDef->rm;
                } else {
                    biRegUse->rn = uRegDef->rm;
                }
                fusionRes[def] = {};
                continue;
            }
            if (uRegDef != nullptr && biRegUse->shiftImm == 0 && biRegUse->cond == uRegDef->cond &&
                biRegUse->cpsr == uRegDef->cpsr && uRegDef->op == ArmInst::UnaryOperator::MOV) {
                // def:mov reg, use:Binary, mov有移位, Binary无移位
                BinaryRegInst *res;
                if (uRegDef->pReg.value() == biRegUse->rm->pReg.value() &&
                    uRegDef->pReg.value() == biRegUse->rn->pReg.value()) {
                    res = new BinaryRegInst(biRegUse->op, 18373808, uRegDef->rm, uRegDef->rm, uRegDef->shiftOp,
                                            uRegDef->shiftImm, biRegUse->cond, biRegUse->cpsr, biRegUse->rd);
                } else if (uRegDef->pReg.value() == biRegUse->rm->pReg.value()) {
                    res = new BinaryRegInst(biRegUse->op, 18373808, biRegUse->rn, uRegDef->rm, uRegDef->shiftOp,
                                            uRegDef->shiftImm, biRegUse->cond, biRegUse->cpsr, biRegUse->rd);
                } else {
                    res = new BinaryRegInst(biRegUse->op, 18373808, biRegUse->rm, uRegDef->rm, uRegDef->shiftOp,
                                            uRegDef->shiftImm, biRegUse->cond, biRegUse->cpsr, biRegUse->rd);
                }
                res->pReg = biRegUse->pReg;
                fusionRes[def] = {res};
                replaceTable[biRegUse] = res;
                continue;
            }
            auto uImmDef = dynamic_cast<UnaryImmInst *>(def);
            if (uImmDef != nullptr && biRegUse->shiftImm == 0 && biRegUse->cond == uImmDef->cond &&
                biRegUse->cpsr == uImmDef->cpsr && uImmDef->op == ArmInst::UnaryOperator::MOV) {
                // def:mov imm, use: Binary, mov 无移位, Binary无移位
                if (uImmDef->pReg.value() == biRegUse->rm->pReg.value() &&
                    uImmDef->pReg.value() == biRegUse->rn->pReg.value()) {
                    auto imm = getBinaryCalculateRes(biRegUse->op, uImmDef->imm12, uImmDef->imm12);
                    if (checkImmediate((int) imm)) {
                        auto res = new UnaryImmInst(ArmInst::UnaryOperator::MOV, 18373808, imm, biRegUse->cond,
                                                    biRegUse->cpsr, biRegUse->rd);
                        res->pReg = biRegUse->pReg;
                        fusionRes[def] = {res};
                        replaceTable[biRegUse] = res;
                    }
                } else if (uImmDef->pReg.value() == biRegUse->rm->pReg.value()) {
                    auto res = new BinaryImmInst(biRegUse->op, 18373808, biRegUse->rn, uImmDef->imm12,
                                                 biRegUse->cond, biRegUse->cpsr, biRegUse->rd);
                    res->pReg = biRegUse->pReg;
                    fusionRes[def] = {res};
                    replaceTable[biRegUse] = res;
                } else {
                    auto res = new BinaryImmInst(biRegUse->op, 18373808, biRegUse->rm, uImmDef->imm12,
                                                 biRegUse->cond, biRegUse->cpsr, biRegUse->rd);
                    res->pReg = biRegUse->pReg;
                    fusionRes[def] = {res};
                    replaceTable[biRegUse] = res;
                }
                continue;
            }
        }
        auto biImmUse = dynamic_cast<BinaryImmInst *>(use);
        if (biImmUse != nullptr) {
            auto uRegDef = dynamic_cast<UnaryRegInst *>(def);
            if (uRegDef != nullptr && uRegDef->shiftImm == 0 && biImmUse->cond == uRegDef->cond &&
                biImmUse->cpsr == uRegDef->cpsr && uRegDef->op == ArmInst::UnaryOperator::MOV) {
                biImmUse->rn = uRegDef->rm;
                fusionRes[def] = {};
                continue;
            }
            auto uImmDef = dynamic_cast<UnaryImmInst *>(def);
            if (uImmDef != nullptr && biImmUse->cond == uImmDef->cond && biImmUse->cpsr == uImmDef->cpsr
                && uImmDef->op == ArmInst::UnaryOperator::MOV) {
                auto res = getBinaryCalculateRes(biImmUse->op, uImmDef->imm12, biImmUse->imm12);
                if (checkImmediate((int) res)) {
                    auto mov = new UnaryImmInst(ArmInst::UnaryOperator::MOV, 18373808, res,
                                                biImmUse->cond, biImmUse->cpsr, biImmUse->rd);
                    mov->pReg = biImmUse->pReg;
                    fusionRes[def] = {mov};
                    replaceTable[biImmUse] = mov;
                    continue;
                }
            }
            // todo 连加窥孔
//            auto biImmDef = dynamic_cast<BinaryImmInst *>(def);
//            if (biImmDef != nullptr && biImmDef->cond == biImmUse->cond && biImmDef->cpsr == biImmUse->cpsr) {
//                auto defOp = biImmDef->op;
//                auto useOp = biImmUse->op;
//                uint32_t res = 0;
//                bool need = true;
//                if (defOp == ArmInst::BinaryOperator::ADD && useOp == ArmInst::BinaryOperator::ADD) {
//                    res = biImmDef->imm12 + biImmUse->imm12;
//                } else if (defOp == ArmInst::BinaryOperator::ADD && useOp == ArmInst::BinaryOperator::SUB) {
//                    res = biImmDef->imm12 - biImmUse->imm12;
//                } else if (defOp == ArmInst::BinaryOperator::SUB && useOp == ArmInst::BinaryOperator::ADD) {
//                    res = biImmUse->imm12 - biImmDef->imm12;
//                } else if (defOp == ArmInst::BinaryOperator::SUB && useOp == ArmInst::BinaryOperator::SUB) {
//                    res = -biImmDef->imm12 - biImmUse->imm12;
//                } else {
//                    need = false;
//                }
//                int abs = (int) res;
//                ArmInst::BinaryOperator op = ArmInst::BinaryOperator::ADD;
//                if (abs < 0) {
//                    abs = -abs;
//                    op = ArmInst::BinaryOperator::SUB;
//                }
//                if (need && checkImmediate(abs)) {
//                    auto bi = new BinaryImmInst(op, 18373808, biImmDef->rn, abs, biImmUse->cond, biImmUse->cpsr,
//                                                biImmUse->rd);
//                    bi->pReg = biImmUse->pReg;
//                    fusionRes[def] = {bi};
//                    continue;
//                }
//            }
        }
        // Unary与Compare指令的融合
        auto cmpRegUse = dynamic_cast<CompareRegInst *>(use);
        if (cmpRegUse != nullptr) {
            auto uRegDef = dynamic_cast<UnaryRegInst *>(def);
            if (uRegDef != nullptr && uRegDef->cond == CondInst::Cond::AL &&
                uRegDef->op == ArmInst::UnaryOperator::MOV) {
                if (uRegDef->pReg.value() == cmpRegUse->rn->pReg.value()) {
                    cmpRegUse->rn = uRegDef->rm;
                } else {
                    cmpRegUse->rm = uRegDef->rm;
                }
                fusionRes[def] = {};
                continue;
            }
            auto uImmDef = dynamic_cast<UnaryImmInst *>(def);
            if (uImmDef != nullptr && uImmDef->cond == CondInst::Cond::AL &&
                uImmDef->op == ArmInst::UnaryOperator::MOV) {
                CompareImmInst *res;
                if (uImmDef->pReg.value() == cmpRegUse->rn->pReg.value()) {
                    res = new CompareImmInst(cmpRegUse->op, cmpRegUse->rm, uImmDef->imm12, cmpRegUse->cond,
                                             cmpRegUse->cpsr);
                } else {
                    res = new CompareImmInst(cmpRegUse->op, cmpRegUse->rn, uImmDef->imm12, cmpRegUse->cond,
                                             cmpRegUse->cpsr);
                }
                fusionRes[def] = {res};
                continue;
            }
        }
        auto cmpImmUse = dynamic_cast<CompareImmInst *>(use);
        if (cmpImmUse != nullptr) {
            auto uRegDef = dynamic_cast<UnaryRegInst *>(def);
            if (uRegDef != nullptr && uRegDef->cond == CondInst::Cond::AL &&
                uRegDef->op == ArmInst::UnaryOperator::MOV) {
                cmpImmUse->rn = uRegDef->rm;
                fusionRes[def] = {};
                continue;
            }
        }
        // 乘加融合
        auto mulDef = dynamic_cast<MultiplyInst *>(def);
        if (mulDef != nullptr) {
            auto regUse = dynamic_cast<BinaryRegInst *>(use);
            if (regUse != nullptr && mulDef->cond == regUse->cond && regUse->op == ArmInst::BinaryOperator::ADD
                && regUse->cpsr == mulDef->cpsr) {
                MultiplyInst *mla;
                if (mulDef->pReg.value() == regUse->rm->pReg.value()) {
                    mla = new MultiplyInst(18373808, mulDef->rn, mulDef->rm, MultiplyInst::Operator::MLA, regUse->rn,
                                           regUse->cond, regUse->cpsr, regUse->rd);
                } else {
                    mla = new MultiplyInst(18373808, mulDef->rn, mulDef->rm, MultiplyInst::Operator::MLA, regUse->rm,
                                           regUse->cond, regUse->cpsr, regUse->rd);
                }
                mla->pReg = regUse->pReg;
                fusionRes[def] = {mla};
                replaceTable[regUse] = mla;
                continue;
            }
        }
        // todo smlal
    }

    vector<LIR *> fusionUse;
    fusionUse.reserve(fusionRes.size());
    for (auto &iter: fusionRes) {
        // 替换的集合是新生成指令的集合，为空时说明在原指令上进行的修改，集合大小非0即1
        if (!iter.second.empty()) {
            fusionUse.emplace_back(fusionInstr.find(iter.first)->second);
        } else {
            newInstrCnt++;
            std::cerr << "fuse in " << bb.getName() << "\n";
        }
    }

    vector<unique_ptr<LIR>> lirTable;
    for (auto &lir: bb.lirTable) {
        auto fuseIter = fusionRes.find(lir.get());
        if (fuseIter != fusionRes.end()) {
            for (auto &newLir:fuseIter->second) {
                newInstrCnt++;
                std::cerr << "fuse in " << bb.getName() << "\n";
                lirTable.emplace_back(newLir);
            }
        } else if (std::find(fusionUse.begin(), fusionUse.end(), lir.get()) == fusionUse.end()) {
            lirTable.emplace_back(std::move(lir));
        } else {
            toErase.emplace_back(std::move(lir));
        }
    }

    originInstrCnt += (int) bb.lirTable.size();
    bb.lirTable.swap(lirTable);
    instrCnt += (int) bb.lirTable.size();
    for (auto &lir:bb.lirTable) {
        lir->doReplacement(replaceTable, {}, {});
    }
    auto rearVec = analyzeCFGPass.result[&bb].rear;
    for (auto rear:rearVec) {
        replacePhiIncoming(rear, replaceTable, {}, {});
    }
    for (auto &child: getDomChildrenBlockNameOrder(node)) {
        fuse(child, replaceTable);
    }
//    std::cerr << "\n\n";
//    for (auto &lir:bb.lirTable) {
//        std::cerr << lir->toString(RegAssign::Format::PREG) << std::endl;
//    }
}

uint32_t InstructionFusion::getBinaryCalculateRes(ArmInst::BinaryOperator op, uint32_t x, uint32_t y) {
    uint32_t res;
    if (op == ArmInst::BinaryOperator::AND) res = x & y;
    else if (op == ArmInst::BinaryOperator::EOR) res = x ^ y;
    else if (op == ArmInst::BinaryOperator::SUB) res = x - y;
    else if (op == ArmInst::BinaryOperator::RSB) res = y - x;
    else if (op == ArmInst::BinaryOperator::ADD) res = x + y;
    else if (op == ArmInst::BinaryOperator::ORR) res = x | y;
    else res = x & (~y);
    return res;
}

std::map<LIR *, LIR *>
InstructionFusion::getFusionInstruction(BasicBlock &bb, std::map<int, PRegActivity> &regActivity) {
    std::map<LIR *, LIR *> fusionInstr;
    for (int i = 0; i < bb.lirTable.size(); i++) {
        auto assign = dynamic_cast<CoreRegAssign *>(bb.lirTable[i].get());
        if (assign == nullptr) continue;
        int defReg = assign->pReg.value();
        int defPos = i;
        int nextDefPos = (int) bb.lirTable.size();
        auto defIter = regActivity.find(defReg);
        if (defIter == regActivity.end()) continue;
        for (auto defIdx: defIter->second.def) {
            if (defIdx > defPos) {
                nextDefPos = defIdx;
                break;
            }
        }
        vector<int> uses;
        for (auto useIdx: defIter->second.use) {
            if (useIdx > defPos && useIdx <= nextDefPos) {
                uses.emplace_back(useIdx);
            }
        }
        if (uses.size() == 1 && uses[0] != bb.lirTable.size()) {
            auto def = bb.lirTable[i].get();
            auto loadImm = dynamic_cast<LoadImmInst *>(def);
            if (loadImm != nullptr) continue;
            auto loadReg = dynamic_cast<LoadRegInst *>(def);
            if (loadReg != nullptr) continue;
            auto uRegDef = dynamic_cast<UnaryRegInst *>(def);
            if (uRegDef != nullptr) {
                auto movSrcIter = regActivity.find(uRegDef->rm->pReg.value());
                if (movSrcIter == regActivity.end()) continue;
                bool redefine = false;
                for (auto defIdx: movSrcIter->second.def) {
                    if (defIdx > defPos && defIdx < uses[0]) {
                        redefine = true;
                    }
                }
                if (redefine) continue;
            }
            auto use = bb.lirTable[uses[0]].get();
            auto biRegUse = dynamic_cast<BinaryRegInst *>(use);
            int anotherReg = -1;
            if (biRegUse != nullptr) {
                anotherReg = biRegUse->rm->pReg.value() == defReg ? biRegUse->rn->pReg.value() :
                             biRegUse->rm->pReg.value();
            }
            auto cmpRegUse = dynamic_cast<CompareRegInst *>(use);
            if (cmpRegUse != nullptr) {
                anotherReg = cmpRegUse->rm->pReg.value() == defReg ? cmpRegUse->rn->pReg.value()
                                                                   : cmpRegUse->rm->pReg.value();
            }
            if (anotherReg != -1) {
                auto anotherDefIter = regActivity.find(anotherReg);
                if (anotherDefIter == regActivity.end()) continue;
                bool anotherRedefine = false;
                for (auto defIdx: anotherDefIter->second.def) {
                    if (defIdx > defPos && defIdx < uses[0]) {
                        anotherRedefine = true;
                        break;
                    }
                }
                if (anotherRedefine) continue;
            }
            fusionInstr[def] = use;

        }
    }
    return fusionInstr;
}

void InstructionFusion::collectDefUsePos(BasicBlock &bb, std::map<int, PRegActivity> &regActivity,
                                         std::map<CoreRegAssign *, CoreRegAssign *> &replaceTable) {
    struct MyVisitor : LIR::Visitor {
        InstructionFusion &pass;
        std::map<int, PRegActivity> &regActivity;
        BasicBlock &bb;
        int id = 0;

        explicit MyVisitor(BasicBlock &bb, InstructionFusion &pass, std::map<int, PRegActivity> &regActivity)
                : bb(bb), pass(pass), regActivity(regActivity) {}

        void addUse(int pReg) {
            if (pReg == 13) return;
            regActivity[pReg].use.insert(id);
        }

        void addDef(int pReg) {
            if (pReg == 13) return;
            regActivity[pReg].def.insert(id);
        }

        void visit(UnaryRegInst *lir) override {
            addDef(lir->pReg.value());
            addUse(lir->rm->pReg.value());
        }

        void visit(BinaryRegInst *lir) override {
            addDef(lir->pReg.value());
            addUse(lir->rm->pReg.value());
            addUse(lir->rn->pReg.value());
        }

        void visit(CompareRegInst *lir) override {
            addUse(lir->rm->pReg.value());
            addUse(lir->rn->pReg.value());
        }

        void visit(UnaryShiftInst *lir) override {
            addDef(lir->pReg.value());
            addUse(lir->rm->pReg.value());
            addUse(lir->rs->pReg.value());
        }

        void visit(BinaryShiftInst *lir) override {
            addDef(lir->pReg.value());
            addUse(lir->rm->pReg.value());
            addUse(lir->rn->pReg.value());
            addUse(lir->rs->pReg.value());
        }

        void visit(CompareShiftInst *lir) override {
            addUse(lir->rm->pReg.value());
            addUse(lir->rn->pReg.value());
            addUse(lir->rs->pReg.value());
        }

        void visit(UnaryImmInst *lir) override {
            addDef(lir->pReg.value());
        }

        void visit(BinaryImmInst *lir) override {
            addDef(lir->pReg.value());
            addUse(lir->rn->pReg.value());
        }

        void visit(CompareImmInst *lir) override {
            addUse(lir->rn->pReg.value());
        }

        void visit(LoadRegInst *lir) override {
            addDef(lir->pReg.value());
            addUse(lir->rm->pReg.value());
            addUse(lir->rn->pReg.value());
        }

        void visit(LoadImmInst *lir) override {
            addDef(lir->pReg.value());
            addUse(lir->rn->pReg.value());
        }

        void visit(StoreRegInst *lir) override {
            addUse(lir->rt->pReg.value());
            addUse(lir->rm->pReg.value());
            addUse(lir->rn->pReg.value());
        }

        void visit(StoreImmInst *lir) override {
            addUse(lir->rt->pReg.value());
            addUse(lir->rn->pReg.value());
        }

        void visit(MultiplyInst *lir) override {
            addDef(lir->pReg.value());
            addUse(lir->rm->pReg.value());
            addUse(lir->rn->pReg.value());
            if (lir->ra != nullptr) {
                addUse(lir->ra->pReg.value());
            }
        }

        void visit(Multiply64GetHighInst *lir) override {
            addDef(lir->pReg.value());
            addUse(lir->rm->pReg.value());
            addUse(lir->rn->pReg.value());
        }

        void visit(DivideInst *lir) override {
            addDef(lir->pReg.value());
            addUse(lir->rm->pReg.value());
            addUse(lir->rn->pReg.value());
        }

        void visit(MovwInst *lir) override {
            addDef(lir->pReg.value());
        }

        void visit(MovtInst *lir) override {
            addDef(lir->pReg.value());
        }

        void visit(BranchAndLinkInst *lir) override {
            // 函数调用，临时寄存器会改变
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
        }

        void visit(BranchAndExchangeInst *lir) override {
            if (lir->rm->pReg.value() != 14) {
                addUse(lir->rm->pReg.value());
            }
        }

        void visit(AtomicLoopCondLIR *lir) override {
            addUse(lir->atomic_var_ptr->pReg.value());
            addUse(lir->step->pReg.value());
            addUse(lir->border->pReg.value());
        }
    };

    MyVisitor visitor(bb, *this, regActivity);
    for (auto &lir : bb.lirTable) {
        lir->doReplacement(replaceTable, {}, {});
        lir->accept(visitor);
        visitor.id++;
    }
}



