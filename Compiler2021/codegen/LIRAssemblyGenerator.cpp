//
// Created by Joeye on 2021/8/3.
//

#include "LIRAssemblyGenerator.h"

void LIRAssemblyGenerator::runOnFunction(Function &fn) {
    AnalyzeCFGPass_LIR analyzeCFGPass(fn);
    analyzeCFGPass.run();
    dfsSequence = analyzeCFGPass.dfsSequence;
    for (auto i = 0; i < dfsSequence.size(); i++) {
        runOnBasicBlock(*dfsSequence[i], i);
    }
}

void LIRAssemblyGenerator::runOnBasicBlock(BasicBlock &bb, size_t dfn) {
    ss << bb.getName() << ":\n";
    for (auto &lir :bb.lirTable) {
        if (preTreatLIR(lir.get(), bb, dfn)) {
            ss << "\t" << lir->toString(RegAssign::Format::PREG) << "\n";
        }
    }
    out << ss.str();
    ss.str("");
}

bool LIRAssemblyGenerator::preTreatLIR(LIR *lir, BasicBlock &ori, size_t dfn) {
    struct MyVisitor : LIR::Visitor {
        LIRAssemblyGenerator &gen;
        BasicBlock &ori;
        bool origin = true;
        int inc = 0;
        size_t dfn;

        explicit MyVisitor(LIRAssemblyGenerator &gen, BasicBlock &ori, size_t dfn) : gen(gen), ori(ori), dfn(dfn) {};

        void addStr(LIR *lir) {
            gen.ss << "\t" << lir->toString(RegAssign::Format::PREG) << "\n";
        }

        void visit(SetFlagInst *lir) override {
            origin = false;
            auto cmpImm = dynamic_cast<CompareImmInst *>(lir->lastInst);
            if (cmpImm != nullptr) return;
            auto cmpReg = dynamic_cast<CompareRegInst *>(lir->lastInst);
            if (cmpReg != nullptr) return;
            addStr(lir->lastInst);
        }

        void visit(WriteBackAddressInst *lir) override {
            origin = false;
            addStr(lir->lastInst);
        }

        void visit(UninitializedCoreReg *lir) override {
            origin = false;
        }

        void visit(BranchInst *lir) override {
            if (lir->cond == CondInst::Cond::AL) {
                if (dfn + 1 != gen.dfsSequence.size() && gen.dfsSequence[dfn + 1] == lir->block) {
                    origin = false;
                }
            }
        }

        void visit(CoreRegPhiLIR *lir) override {
            origin = false;
//            gen.ss << "\t@" << lir->toString(RegAssign::Format::PREG) << "\n";
        }

        void visit(CoreMemPhiLIR *lir) override {
            origin = false;
//            gen.ss << "\t@" << lir->toString(RegAssign::Format::PREG) << "\n";
        }

        void visit(LoadImm32LIR *lir) override {
            origin = false;
            gen.ss << "\t@" << lir->toString(RegAssign::Format::PREG) << "\n";
        }

        void visit(ValueAddressingLIR *lir) override {
            origin = false;
            gen.ss << "\t@" << lir->toString(RegAssign::Format::PREG) << "\n";
        }

        void visit(GetArgumentLIR *lir) override {
            origin = false;
            gen.ss << "\t@" << lir->toString(RegAssign::Format::PREG) << "\n";
        }

        void visit(SetArgumentLIR *lir) override {
            origin = false;
            gen.ss << "\t@" << lir->toString(RegAssign::Format::PREG) << "\n";
        }

        void visit(GetReturnValueLIR *lir) override {
            origin = false;
            gen.ss << "\t@" << lir->toString(RegAssign::Format::PREG) << "\n";
        }

        void visit(ReturnLIR *lir) override {
            origin = false;
            gen.ss << "\t@" << lir->toString(RegAssign::Format::PREG) << "\n";
        }

        void visit(LoadScalarLIR *lir) override {
            origin = false;
            gen.ss << "\t@" << lir->toString(RegAssign::Format::PREG) << "\n";
        }

        void visit(StoreScalarLIR *lir) override {
            origin = false;
            gen.ss << "\t@" << lir->toString(RegAssign::Format::PREG) << "\n";
        }

        void visit(AtomicLoopCondLIR *lir) override {
            origin = false;
            auto my_reg = lir->getRegString(RegAssign::Format::PREG);
            auto atomic_var_ptr_reg = lir->atomic_var_ptr->getRegString(RegAssign::Format::PREG);
            auto step_reg = lir->step->getRegString(RegAssign::Format::PREG);
            auto border_reg = lir->border->getRegString(RegAssign::Format::PREG);
            auto tmp_reg = lir->tmp->getRegString(RegAssign::Format::PREG);
            gen.ss << "atomic_" << ori.getName() << ":\n";
            gen.ss << "\tldrex " << my_reg << ", [" << atomic_var_ptr_reg << "]\n";
            gen.ss << "\tcmp " << my_reg << ", " << border_reg << '\n';
            gen.ss << "\tb" << CondInst::getCondString(CondInst::getContrary(lir->cond)) << ' ' << lir->exit->getName() << '\n';
//            gen.ss << "\tpush {" << border_reg << ',' << step_reg << "}\n";
            gen.ss << '\t' << ArmInst::getString(lir->update_op) << ' ' << tmp_reg << ", " << my_reg << ", " << step_reg << '\n';
            gen.ss << "\tstrex ip, " << tmp_reg << ", [" << atomic_var_ptr_reg << "]\n";
            gen.ss << "\tcmp ip, #0\n";
//            gen.ss << "\tpop {" << border_reg << ',' << step_reg << "}\n";
            gen.ss << "\tbne " << "atomic_" << ori.getName() << '\n';
            gen.ss << "\tb " << lir->body->getName() << "\n";
        }
    };
    MyVisitor visitor(*this, ori, dfn);
    lir->accept(visitor);
    return visitor.origin;
}
