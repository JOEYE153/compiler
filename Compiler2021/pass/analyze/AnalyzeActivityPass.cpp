//
// Created by 陈思言 on 2021/8/3.
//

#include "AnalyzeActivityPass.h"

using std::make_pair;

bool AnalyzeActivityPass::analyze() {
    analyzeCFGPass.run();
    for (auto bb : analyzeCFGPass.dfsSequence) {
        collectDefUsePos(bb);
    }
    return true;
}

void AnalyzeActivityPass_MIR::invalidate() {
    AnalyzePass::invalidate();
    assignActivityMap.clear();
    blockActivityMap.clear();
}

void AnalyzeActivityPass_MIR::printAssignActivityMap() const {
    for (auto &item : assignActivityMap) {
        out << item.first->getNameAndIdString() << ":\n";
        out << "\tdef = " << item.second.def.first->getName() << " @" << item.second.def.second << '\n';
        out << "\tuse =\n";
        for (auto &item_use : item.second.use) {
            out << "\t\t" << item_use.first->getName() << " @" << item.second.def.second << '\n';
        }
        out << "\tuseByPhi =\n";
        for (auto &item_useByPhi : item.second.useByPhi) {
            out << "\t\t" << item_useByPhi.first->getName() << " @" << item.second.def.second << '\n';
        }
        out << '\n';
    }
}

void AnalyzeActivityPass_MIR::printBlockActivityMap() const {
    for (auto &item : blockActivityMap) {
        out << item.first->getName() << ":\n";
        out << "\tdef =\n";
        for (auto &item_def : item.second.def) {
            out << "\t\t" << item_def->getNameAndIdString() << '\n';
        }
        out << "\tuse =\n";
        for (auto &item_use : item.second.use) {
            out << "\t\t" << item_use->getNameAndIdString() << '\n';
        }
        out << "\tuseAfterDef =\n";
        for (auto &item_useAfterDef : item.second.useAfterDef) {
            out << "\t\t" << item_useAfterDef->getNameAndIdString() << '\n';
        }
        out << "\tuseByPhi =\n";
        for (auto &item_useByPhi : item.second.useByPhi) {
            out << "\t\t" << item_useByPhi.first->getNameAndIdString() << '\n';
        }
        out << '\n';
    }
}

void AnalyzeActivityPass_MIR::collectDefUsePos(BasicBlock *bb) {
    struct MyVisitor : MIR::Visitor {
        AnalyzeActivityPass_MIR &pass;
        BasicBlock *bb;
        int id = 0;

        explicit MyVisitor(AnalyzeActivityPass_MIR &pass, BasicBlock *bb)
                : pass(pass), bb(bb) {}

        void addDef(Assignment *dst) {
            pass.assignActivityMap[dst].def = make_pair(bb, id);
            pass.blockActivityMap[bb].def.insert(dst);
        }

        void addUse(MIR *mir, Assignment *src) {
            pass.assignActivityMap[src].use.emplace(bb, mir);
            pass.assignActivityMap[src].usePos.emplace(bb, id);
            if (pass.blockActivityMap[bb].def.find(src) == pass.blockActivityMap[bb].def.end()) {
                pass.blockActivityMap[bb].use.insert(src);
            } else {
                pass.blockActivityMap[bb].useAfterDef.insert(src);
            }
        }

        void visit(UninitializedMIR *mir) override {
            addDef(mir);
        }

        void visit(LoadConstantMIR *mir) override {
            addDef(mir);
        }

        void visit(LoadVariableMIR *mir) override {
            addDef(mir);
        }

        void visit(LoadPointerMIR *mir) override {
            addDef(mir);
            addUse(mir, mir->src);
        }

        void visit(StoreVariableMIR *mir) override {
            addUse(mir, mir->src);
        }

        void visit(StorePointerMIR *mir) override {
            addUse(mir, mir->dst);
            addUse(mir, mir->src);
        }

        void visit(MemoryCopyMIR *mir) override {
            addUse(mir, mir->dst);
            addUse(mir, mir->src);
        }

        void visit(MemoryFillMIR *mir) override {
            addUse(mir, mir->dst);
            addUse(mir, mir->src);
        }

        void visit(UnaryMIR *mir) override {
            addDef(mir);
            addUse(mir, mir->src);
        }

        void visit(BinaryMIR *mir) override {
            addDef(mir);
            addUse(mir, mir->src1);
            addUse(mir, mir->src2);
        }

        void visit(ValueAddressingMIR *mir) override {
            addDef(mir);
        }

        void visit(ArrayAddressingMIR *mir) override {
            addDef(mir);
            addUse(mir, mir->offset);
            auto base_ptr = dynamic_cast<Assignment *>(mir->base);
            if (base_ptr != nullptr) {
                addUse(mir, base_ptr);
            }
        }

        void visit(SelectMIR *mir) override {
            addDef(mir);
            addUse(mir, mir->cond);
            addUse(mir, mir->src1);
            addUse(mir, mir->src2);
        }

        void visit(BranchMIR *mir) override {
            addUse(mir, mir->cond);
        }

        void visit(CallMIR *mir) override {
            for (auto arg : mir->args) {
                addUse(mir, arg);
            }
        }

        void visit(CallWithAssignMIR *mir) override {
            visit(static_cast<CallMIR *>(mir));
            addDef(mir);
        }

        void visit(MultiCallMIR *mir) override {
            visit(static_cast<CallMIR *>(mir));
        }

        void visit(ReturnMIR *mir) override {
            if (mir->val != nullptr) {
                addUse(mir, mir->val);
            }
        }

        void visit(PhiMIR *mir) override {
            addDef(mir);
            auto &blockUseByPhi = pass.blockActivityMap[bb].useByPhi;
            for (auto &incoming : mir->incomingTable) {
                pass.assignActivityMap[incoming.second].usePos.emplace(bb, id);
                auto iter1 = blockUseByPhi.find(incoming.second);
                if (iter1 == blockUseByPhi.end()) {
                    std::set<BasicBlock *> blockSet = {incoming.first};
                    blockUseByPhi.emplace(incoming.second, blockSet);
                } else {
                    iter1->second.insert(incoming.first);
                }
                auto &useByPhi = pass.assignActivityMap[incoming.second].useByPhi;
                auto iter2 = useByPhi.find(bb);
                if (iter2 == useByPhi.end()) {
                    std::set<BasicBlock *> blockSet = {incoming.first};
                    useByPhi.emplace(bb, blockSet);
                } else {
                    iter2->second.insert(incoming.first);
                }
            }
        }

        void visit(AtomicLoopCondMIR *mir) override {
            addDef(mir);
            addUse(mir, mir->step);
            addUse(mir, mir->border);
        }
    };

    MyVisitor visitor(*this, bb);
    for (auto &mir : bb->mirTable) {
        mir->accept(visitor);
        visitor.id++;
    }
}

void AnalyzeActivityPass_LIR::invalidate() {
    AnalyzePass::invalidate();
    assignActivityMapCoreReg.clear();
    blockActivityMapCoreReg.clear();
    assignActivityMapCoreMem.clear();
    blockActivityMapCoreMem.clear();
    assignActivityMapNeonReg.clear();
    blockActivityMapNeonReg.clear();
    assignActivityMapNeonMem.clear();
    blockActivityMapNeonMem.clear();
}

template<typename T>
static void LIR_reg_printAssignActivityMap(ostream &out, const std::map<T *, AssignActivity<LIR>> &assignActivityMap) {
    for (auto &item : assignActivityMap) {
        out << item.first->getRegString(RegAssign::Format::VREG) << ":\n";
        out << "\tdef = " << item.second.def.first->getName() << " @" << item.second.def.second << '\n';
        out << "\tuse =\n";
        for (auto &item_use : item.second.use) {
            out << "\t\t" << item_use.first->getName() << " @" << item.second.def.second << '\n';
        }
        out << "\tuseByPhi =\n";
        for (auto &item_useByPhi : item.second.useByPhi) {
            out << "\t\t" << item_useByPhi.first->getName() << " @" << item.second.def.second << '\n';
        }
        out << '\n';
    }
}

template<typename T>
static void LIR_mem_printAssignActivityMap(ostream &out, const std::map<T *, AssignActivity<LIR>> &assignActivityMap) {
    for (auto &item : assignActivityMap) {
        out << item.first->getMemString() << ":\n";
        out << "\tdef = " << item.second.def.first->getName() << " @" << item.second.def.second << '\n';
        out << "\tuse =\n";
        for (auto &item_use : item.second.use) {
            out << "\t\t" << item_use.first->getName() << " @" << item.second.def.second << '\n';
        }
        out << "\tuseByPhi =\n";
        for (auto &item_useByPhi : item.second.useByPhi) {
            out << "\t\t" << item_useByPhi.first->getName() << " @" << item.second.def.second << '\n';
        }
        out << '\n';
    }
}

void AnalyzeActivityPass_LIR::printAssignActivityMap() const {
    LIR_reg_printAssignActivityMap(out, assignActivityMapCoreReg);
//    LIR_reg_printAssignActivityMap(out, assignActivityMapNeonReg);
    LIR_reg_printAssignActivityMap(out, assignActivityMapStatusReg);
    LIR_mem_printAssignActivityMap(out, assignActivityMapCoreMem);
//    LIR_mem_printAssignActivityMap(out, assignActivityMapNeonMem);
}

template<typename T>
static void
LIR_reg_printBlockActivityMap(ostream &out, const std::map<BasicBlock *, BlockActivity<T>> &blockActivityMap) {
    for (auto &item : blockActivityMap) {
        out << item.first->getName() << ":\n";
        out << "\tdef =\n";
        for (auto &item_def : item.second.def) {
            out << "\t\t" << item_def->getRegString(RegAssign::Format::VREG) << '\n';
        }
        out << "\tuse =\n";
        for (auto &item_use : item.second.use) {
            out << "\t\t" << item_use->getRegString(RegAssign::Format::VREG) << '\n';
        }
        out << "\tuseAfterDef =\n";
        for (auto &item_useAfterDef : item.second.useAfterDef) {
            out << "\t\t" << item_useAfterDef->getRegString(RegAssign::Format::VREG) << '\n';
        }
        out << "\tuseByPhi =\n";
        for (auto &item_useByPhi : item.second.useByPhi) {
            out << "\t\t" << item_useByPhi.first->getRegString(RegAssign::Format::VREG) << '\n';
        }
        out << '\n';
    }
}

template<typename T>
static void
LIR_mem_printBlockActivityMap(ostream &out, const std::map<BasicBlock *, BlockActivity<T>> &blockActivityMap) {
    for (auto &item : blockActivityMap) {
        out << item.first->getName() << ":\n";
        out << "\tdef =\n";
        for (auto &item_def : item.second.def) {
            out << "\t\t" << item_def->getMemString() << '\n';
        }
        out << "\tuse =\n";
        for (auto &item_use : item.second.use) {
            out << "\t\t" << item_use->getMemString() << '\n';
        }
        out << "\tuseAfterDef =\n";
        for (auto &item_useAfterDef : item.second.useAfterDef) {
            out << "\t\t" << item_useAfterDef->getMemString() << '\n';
        }
        out << "\tuseByPhi =\n";
        for (auto &item_useByPhi : item.second.useByPhi) {
            out << "\t\t" << item_useByPhi.first->getMemString() << '\n';
        }
        out << '\n';
    }
}

void AnalyzeActivityPass_LIR::printBlockActivityMap() const {
    LIR_reg_printBlockActivityMap(out, blockActivityMapCoreReg);
//    LIR_reg_printBlockActivityMap(out, blockActivityMapNeonReg);
    LIR_reg_printBlockActivityMap(out, blockActivityMapStatusReg);
    LIR_mem_printBlockActivityMap(out, blockActivityMapCoreMem);
//    LIR_mem_printBlockActivityMap(out, blockActivityMapNeonMem);
}

void AnalyzeActivityPass_LIR::collectDefUsePos(BasicBlock *bb) {
    struct MyVisitor : LIR::Visitor {
        AnalyzeActivityPass_LIR &pass;
        BasicBlock *bb;
        int id = 0;

        explicit MyVisitor(AnalyzeActivityPass_LIR &pass, BasicBlock *bb)
                : pass(pass), bb(bb) {}

        void addCoreRegDef(CoreRegAssign *dst) {
            pass.assignActivityMapCoreReg[dst].def = make_pair(bb, id);
            pass.blockActivityMapCoreReg[bb].def.insert(dst);
        }

        void addCoreRegUse(LIR *lir, CoreRegAssign *src) {
            pass.assignActivityMapCoreReg[src].use.emplace(bb, lir);
            pass.assignActivityMapCoreReg[src].usePos.emplace(bb, id);
            if (pass.blockActivityMapCoreReg[bb].def.find(src) == pass.blockActivityMapCoreReg[bb].def.end()) {
                pass.blockActivityMapCoreReg[bb].use.insert(src);
            } else {
                pass.blockActivityMapCoreReg[bb].useAfterDef.insert(src);
            }
        }

        void addNeonRegDef(NeonRegAssign *dst) {
            pass.assignActivityMapNeonReg[dst].def = make_pair(bb, id);
            pass.blockActivityMapNeonReg[bb].def.insert(dst);
        }

        void addNeonRegUse(LIR *lir, NeonRegAssign *src) {
            pass.assignActivityMapNeonReg[src].use.emplace(bb, lir);
            pass.assignActivityMapNeonReg[src].usePos.emplace(bb, id);
            if (pass.blockActivityMapNeonReg[bb].def.find(src) == pass.blockActivityMapNeonReg[bb].def.end()) {
                pass.blockActivityMapNeonReg[bb].use.insert(src);
            } else {
                pass.blockActivityMapNeonReg[bb].useAfterDef.insert(src);
            }
        }

        void addStatusRegDef(StatusRegAssign *dst) {
            pass.assignActivityMapStatusReg[dst].def = make_pair(bb, id);
            pass.blockActivityMapStatusReg[bb].def.insert(dst);
        }

        void addStatusRegUse(LIR *lir, StatusRegAssign *src) {
            pass.assignActivityMapStatusReg[src].use.emplace(bb, lir);
            pass.assignActivityMapStatusReg[src].usePos.emplace(bb, id);
            if (pass.blockActivityMapStatusReg[bb].def.find(src) == pass.blockActivityMapStatusReg[bb].def.end()) {
                pass.blockActivityMapStatusReg[bb].use.insert(src);
            } else {
                pass.blockActivityMapStatusReg[bb].useAfterDef.insert(src);
            }
        }

        void addCoreMemDef(CoreMemAssign *dst) {
            pass.assignActivityMapCoreMem[dst].def = make_pair(bb, id);
            pass.blockActivityMapCoreMem[bb].def.insert(dst);
        }

        void addCoreMemUse(LIR *lir, CoreMemAssign *src) {
            pass.assignActivityMapCoreMem[src].use.emplace(bb, lir);
            pass.assignActivityMapCoreMem[src].usePos.emplace(bb, id);
            if (pass.blockActivityMapCoreMem[bb].def.find(src) == pass.blockActivityMapCoreMem[bb].def.end()) {
                pass.blockActivityMapCoreMem[bb].use.insert(src);
            } else {
                pass.blockActivityMapCoreMem[bb].useAfterDef.insert(src);
            }
        }

        void addNeonMemDef(NeonMemAssign *dst) {
            pass.assignActivityMapNeonMem[dst].def = make_pair(bb, id);
            pass.blockActivityMapNeonMem[bb].def.insert(dst);
        }

        void addNeonMemUse(LIR *lir, NeonMemAssign *src) {
            pass.assignActivityMapNeonMem[src].use.emplace(bb, lir);
            pass.assignActivityMapNeonMem[src].usePos.emplace(bb, id);
            if (pass.blockActivityMapNeonMem[bb].def.find(src) == pass.blockActivityMapNeonMem[bb].def.end()) {
                pass.blockActivityMapNeonMem[bb].use.insert(src);
            } else {
                pass.blockActivityMapNeonMem[bb].useAfterDef.insert(src);
            }
        }

        void visitCond(CondInst *lir) {
            if (lir->cpsr != nullptr) {
                addStatusRegUse(lir, lir->cpsr);
            }
            if (lir->rd != nullptr) {
                addCoreRegUse(lir, lir->rd);
            }
        }

        void visit(UnaryRegInst *lir) override {
            addCoreRegDef(lir);
            addCoreRegUse(lir, lir->rm);
            visitCond(lir);
        }

        void visit(BinaryRegInst *lir) override {
            addCoreRegDef(lir);
            addCoreRegUse(lir, lir->rn);
            addCoreRegUse(lir, lir->rm);
            visitCond(lir);
        }

        void visit(CompareRegInst *lir) override {
            addCoreRegUse(lir, lir->rn);
            addCoreRegUse(lir, lir->rm);
            visitCond(lir);
        }

        void visit(UnaryShiftInst *lir) override {
            addCoreRegDef(lir);
            addCoreRegUse(lir, lir->rm);
            addCoreRegUse(lir, lir->rs);
            visitCond(lir);
        }

        void visit(BinaryShiftInst *lir) override {
            addCoreRegDef(lir);
            addCoreRegUse(lir, lir->rn);
            addCoreRegUse(lir, lir->rm);
            addCoreRegUse(lir, lir->rs);
            visitCond(lir);
        }

        void visit(CompareShiftInst *lir) override {
            addCoreRegUse(lir, lir->rn);
            addCoreRegUse(lir, lir->rm);
            addCoreRegUse(lir, lir->rs);
            visitCond(lir);
        }

        void visit(UnaryImmInst *lir) override {
            addCoreRegDef(lir);
            visitCond(lir);
        }

        void visit(BinaryImmInst *lir) override {
            addCoreRegDef(lir);
            addCoreRegUse(lir, lir->rn);
            visitCond(lir);
        }

        void visit(CompareImmInst *lir) override {
            addCoreRegUse(lir, lir->rn);
            visitCond(lir);
        }

        void visit(SetFlagInst *lir) override {
            addStatusRegDef(lir);
        }

        void visit(LoadRegInst *lir) override {
            addCoreRegDef(lir);
            addCoreRegUse(lir, lir->rn);
            addCoreRegUse(lir, lir->rm);
            visitCond(lir);
        }

        void visit(LoadImmInst *lir) override {
            addCoreRegDef(lir);
            addCoreRegUse(lir, lir->rn);
            visitCond(lir);
        }

        void visit(StoreRegInst *lir) override {
            addCoreRegUse(lir, lir->rt);
            addCoreRegUse(lir, lir->rn);
            addCoreRegUse(lir, lir->rm);
            visitCond(lir);
        }

        void visit(StoreImmInst *lir) override {
            addCoreRegUse(lir, lir->rt);
            addCoreRegUse(lir, lir->rn);
            visitCond(lir);
        }

        void visit(WriteBackAddressInst *lir) override {
            addCoreRegDef(lir);
        }

        void visit(MultiplyInst *lir) override {
            addCoreRegDef(lir);
            addCoreRegUse(lir, lir->rn);
            addCoreRegUse(lir, lir->rm);
            if (lir->ra != nullptr) {
                addCoreRegUse(lir, lir->ra);
            }
            visitCond(lir);
        }

        void visit(Multiply64GetHighInst *lir) override {
            addCoreRegDef(lir);
            addCoreRegUse(lir, lir->rn);
            addCoreRegUse(lir, lir->rm);
            visitCond(lir);
        }

        void visit(DivideInst *lir) override {
            addCoreRegDef(lir);
            addCoreRegUse(lir, lir->rn);
            addCoreRegUse(lir, lir->rm);
            visitCond(lir);
        }

        void visit(MovwInst *lir) override {
            addCoreRegDef(lir);
            visitCond(lir);
        }

        void visit(MovtInst *lir) override {
            addCoreRegDef(lir);
            visitCond(lir);
        }

        void visit(BranchInst *lir) override {
            visitCond(lir);
        }

        void visit(BranchAndLinkInst *lir) override {
            visitCond(lir);
        }

        void visit(BranchAndExchangeInst *lir) override {
            visitCond(lir);
            addCoreRegUse(lir, lir->rm);
        }

        void visit(LoadImm32LIR *lir) override {
            addCoreRegDef(lir);
        }

        void visit(ValueAddressingLIR *lir) override {
            addCoreRegDef(lir);
        }

        void visit(GetArgumentLIR *lir) override {
            addCoreRegDef(lir);
        }

        void visit(SetArgumentLIR *lir) override {
            addCoreRegUse(lir, lir->src);
        }

        void visit(GetReturnValueLIR *lir) override {
            addCoreRegDef(lir);
        }

        void visit(ReturnLIR *lir) override {
            if (lir->val != nullptr) {
                addCoreRegUse(lir, lir->val);
            }
        }

        void visit(LoadScalarLIR *lir) override {
            addCoreRegDef(lir);
            addCoreMemUse(lir, lir->src);
        }

        void visit(StoreScalarLIR *lir) override {
            addCoreMemDef(lir);
            addCoreRegUse(lir, lir->src);
        }

//        void visit(LoadVectorLIR *lir) override {
//            addNeonRegDef(lir);
//            addNeonMemUse(lir, lir->src);
//        }
//
//        void visit(StoreVectorLIR *lir) override {
//            addNeonMemDef(lir);
//            addNeonRegUse(lir, lir->src);
//        }

        void visit(UninitializedCoreReg *lir) override {
            addCoreRegDef(lir);
        }

//        void visit(UninitializedNeonReg *lir) override {
//            addNeonRegDef(lir);
//        }

        void visit(CoreRegPhiLIR *lir) override {
            addCoreRegDef(lir);
            auto &blockUseByPhi = pass.blockActivityMapCoreReg[bb].useByPhi;
            for (auto &incoming : lir->incomingTable) {
                pass.assignActivityMapCoreReg[incoming.second].usePos.emplace(bb, id);
                auto iter1 = blockUseByPhi.find(incoming.second);
                if (iter1 == blockUseByPhi.end()) {
                    std::set<BasicBlock *> blockSet = {incoming.first};
                    blockUseByPhi.emplace(incoming.second, blockSet);
                } else {
                    iter1->second.insert(incoming.first);
                }
                auto &useByPhi = pass.assignActivityMapCoreReg[incoming.second].useByPhi;
                auto iter2 = useByPhi.find(bb);
                if (iter2 == useByPhi.end()) {
                    std::set<BasicBlock *> blockSet = {incoming.first};
                    useByPhi.emplace(bb, blockSet);
                } else {
                    iter2->second.insert(incoming.first);
                }
            }
        }

//        void visit(NeonRegPhiLIR *lir) override {
//            addNeonRegDef(lir);
//            auto &blockUseByPhi = pass.blockActivityMapNeonReg[bb].useByPhi;
//            for (auto &incoming : lir->incomingTable) {
//                pass.assignActivityMapNeonReg[incoming.second].usePos.emplace(bb, id);
//                auto iter1 = blockUseByPhi.find(incoming.second);
//                if (iter1 == blockUseByPhi.end()) {
//                    std::set<BasicBlock *> blockSet = {incoming.first};
//                    blockUseByPhi.emplace(incoming.second, blockSet);
//                } else {
//                    iter1->second.insert(incoming.first);
//                }
//                auto &useByPhi = pass.assignActivityMapNeonReg[incoming.second].useByPhi;
//                auto iter2 = useByPhi.find(bb);
//                if (iter2 == useByPhi.end()) {
//                    std::set<BasicBlock *> blockSet = {incoming.first};
//                    useByPhi.emplace(bb, blockSet);
//                } else {
//                    iter2->second.insert(incoming.first);
//                }
//            }
//        }

        void visit(CoreMemPhiLIR *lir) override {
            addCoreMemDef(lir);
            auto &blockUseByPhi = pass.blockActivityMapCoreMem[bb].useByPhi;
            for (auto &incoming : lir->incomingTable) {
                pass.assignActivityMapCoreMem[incoming.second].usePos.emplace(bb, id);
                auto iter1 = blockUseByPhi.find(incoming.second);
                if (iter1 == blockUseByPhi.end()) {
                    std::set<BasicBlock *> blockSet = {incoming.first};
                    blockUseByPhi.emplace(incoming.second, blockSet);
                } else {
                    iter1->second.insert(incoming.first);
                }
                auto &useByPhi = pass.assignActivityMapCoreMem[incoming.second].useByPhi;
                auto iter2 = useByPhi.find(bb);
                if (iter2 == useByPhi.end()) {
                    std::set<BasicBlock *> blockSet = {incoming.first};
                    useByPhi.emplace(bb, blockSet);
                } else {
                    iter2->second.insert(incoming.first);
                }
            }
        }

//        void visit(NeonMemPhiLIR *lir) override {
//            addNeonMemDef(lir);
//            auto &blockUseByPhi = pass.blockActivityMapNeonMem[bb].useByPhi;
//            for (auto &incoming : lir->incomingTable) {
//                pass.assignActivityMapNeonMem[incoming.second].usePos.emplace(bb, id);
//                auto iter1 = blockUseByPhi.find(incoming.second);
//                if (iter1 == blockUseByPhi.end()) {
//                    std::set<BasicBlock *> blockSet = {incoming.first};
//                    blockUseByPhi.emplace(incoming.second, blockSet);
//                } else {
//                    iter1->second.insert(incoming.first);
//                }
//                auto &useByPhi = pass.assignActivityMapNeonMem[incoming.second].useByPhi;
//                auto iter2 = useByPhi.find(bb);
//                if (iter2 == useByPhi.end()) {
//                    std::set<BasicBlock *> blockSet = {incoming.first};
//                    useByPhi.emplace(bb, blockSet);
//                } else {
//                    iter2->second.insert(incoming.first);
//                }
//            }
//        }

        void visit(AtomicLoopCondLIR *lir) override {
            addCoreRegDef(lir);
            addCoreRegUse(lir, lir->atomic_var_ptr);
            addCoreRegUse(lir, lir->step);
            addCoreRegUse(lir, lir->border);
            addCoreRegUse(lir, lir->tmp);
        }
    };

    MyVisitor visitor(*this, bb);
    for (auto &lir : bb->lirTable) {
        lir->accept(visitor);
        visitor.id++;
    }
}
