//
// Created by 陈思言 on 2021/7/16.
//

#include "MIRAssemblyGenerator.h"

void MIRAssemblyGenerator::runOnFunction(Function &fn) {
    // there is no reference value in MIR
    out << "\tpush {lr}\n";

    auto constantTable = fn.getLocalConstantVecDictOrder();
    auto variableTable = fn.getLocalVariableVecDictOrder();

    int maxCalledFuncArgSize = 0;
    AnalyzeCallGraphPass_MIR analyzeCallGraphPassMir(md, out);
    analyzeCallGraphPassMir.run();
    for (auto callfunc : analyzeCallGraphPassMir.callGraph[&fn]) {
        maxCalledFuncArgSize = callfunc->args.size() > maxCalledFuncArgSize ?
                               (int) callfunc->args.size() : maxCalledFuncArgSize;
    }
    maxCalledFuncArgSize = maxCalledFuncArgSize > 4 ? maxCalledFuncArgSize - 4 : 0;

    int argSize = fn.args.size() > 4 ? 4 : (int) fn.args.size();

    int constantSize = 0;
    for (auto constant: constantTable) {
        if (constant->getType()->getId() == Type::ID::ARRAY) {
            constantSize += (int) constant->getValue<vector<int>>().size();
        }
        // if constant is not array, then load constant as immediate directly
    }

    int variableSize = 0;
    for (auto variable: variableTable) {
        if (variable->getType()->getId() == Type::ID::ARRAY && !variable->isReference()
            && dynamic_cast<ArrayType *>(variable->getType().get())->getElementNum().has_value()) {
            variableSize += (int) dynamic_cast<ArrayType *>(variable->getType().get())->getSizeOfType() / 4;
        } else {
            variableSize += 1;
        }
    }

    AnalyzeCFGPass_MIR analyzeCFGPass(fn, std::cerr);
    analyzeCFGPass.run();
    dfsSequence = analyzeCFGPass.dfsSequence;
    int assignmentSize = 0;
    for (auto block : dfsSequence) {
        for (auto &mir : block->mirTable) {
            auto assignment = dynamic_cast<Assignment *>(mir.get());
            if (assignment != nullptr) {
                assignments.emplace_back(assignment);
                assignmentSize++;
            }
        }
    }

    frameSize = (constantSize + variableSize + assignmentSize + argSize + maxCalledFuncArgSize) * 4;
    allocateFrame("r11");

    // allocate frame offset of value on stack
    if (fn.args.size() > 4) {
        int argOffset = frameSize + 4;
        for (int i = 4; i < fn.args.size(); i++) {
            valueOffsetTable.emplace(fn.args[i], argOffset);
            argOffset += 4;
        }
    }
    int offset = maxCalledFuncArgSize * 4;
    for (int i = 0; i < 4 && i < fn.args.size(); i++) {
        // store args from a0-a3 to stack
        valueOffsetTable.emplace(fn.args[i], offset);
        string res = preTreatStackOffset(offset, "r11");
        out << "\tstr r" << i << ", [" << res << "]\n";
        offset += 4;
    }
    for (auto constant: constantTable) {
        valueOffsetTable.emplace(constant, offset);
        // initialize constant array
        if (constant->getType()->getId() == Type::ID::ARRAY) {
            for (auto value : constant->getValue<vector<int>>()) {
                loadImmediate(value, "r0");
                string res = preTreatStackOffset(offset, "r11");
                out << "\tstr r0, [" << res << "]\n";
                offset += 4;
            }
        }
    }
    for (auto variable: variableTable) {
        valueOffsetTable.emplace(variable, offset);
        if (variable->getType()->getId() == Type::ID::ARRAY && !variable->isReference()
            && dynamic_cast<ArrayType *>(variable->getType().get())->getElementNum().has_value()) {
            offset += (int) dynamic_cast<ArrayType *>(variable->getType().get())->getSizeOfType();
        } else {
            offset += 4;
        }
    }
    for (auto assignment: assignments) {
        valueOffsetTable.emplace(assignment, offset);
        offset += 4;
    }

    for (auto i = 0; i < dfsSequence.size(); i++) {
        runOnBasicBlock(*dfsSequence[i], i);
    }
    assignments.clear();
    valueOffsetTable.clear();
}

void MIRAssemblyGenerator::translateCommonBinaryMIR(BinaryMIR *mir, const string &op) {
    load(mir->src1, "r1");
    load(mir->src2, "r2");
    out << "\t" << op << " r0, r1, r2\n";
    store(mir, "r0");
}

void MIRAssemblyGenerator::translateCompareMIR(BinaryMIR *mir, const string &op) {
    load(mir->src1, "r1");
    load(mir->src2, "r2");
    out << "\tmov r0, #0\n";
    out << "\tcmp r1, r2\n";
    out << "\t" << op << " r0, #1\n";
    store(mir, "r0");
}

void MIRAssemblyGenerator::runOnBasicBlock(BasicBlock &bb, size_t dfn) {
    struct MyVisitor : MIR::Visitor {
        MIRAssemblyGenerator &pass;
        BasicBlock &bb;
        std::ostream &out;
        size_t dfn;

        MyVisitor(MIRAssemblyGenerator &pass, BasicBlock &bb, std::ostream &out, size_t dfn)
                : pass(pass), bb(bb), out(out), dfn(dfn) {}

        void visit(UnaryMIR *mir) override {
            switch (mir->op) {
                case UnaryMIR::Operator::NEG:
                    pass.load(mir->src, "r1");
                    out << "\trsb r0, r1, #0\n";
                    pass.store(mir, "r0");
                    break;
                case UnaryMIR::Operator::NOT:
                    pass.load(mir->src, "r1");
                    out << "\tmov r0, #0\n";
                    out << "\tcmp r1, #0\n";
                    out << "\tmoveq r0, #1\n";
                    pass.store(mir, "r0");
                    break;
                case UnaryMIR::Operator::REV:
                    // int dst = ~src
                    pass.load(mir->src, "r1");
                    out << "\tmvn r0, r1\n";
                    pass.store(mir, "r0");
                    break;
                default:
                    std::cerr << "invalid unary MIR type\n";
            }
        }

        void visit(BinaryMIR *mir) override {
            switch (mir->op) {
                case BinaryMIR::Operator::ADD:
                    pass.translateCommonBinaryMIR(mir, "add");
                    break;
                case BinaryMIR::Operator::SUB:
                    pass.translateCommonBinaryMIR(mir, "sub");
                    break;
                case BinaryMIR::Operator::MUL:
                    pass.translateCommonBinaryMIR(mir, "mul");
                    break;
                case BinaryMIR::Operator::DIV:
                    pass.translateCommonBinaryMIR(mir, "sdiv");
                    break;
                case BinaryMIR::Operator::MOD:
                    // r0 = r1 % r2 = r1 - (r1/r2) * r2
                    pass.load(mir->src1, "r1");
                    pass.load(mir->src2, "r2");
                    out << "\tsdiv r0, r1, r2\n";
                    out << "\tmul r0, r0, r2\n";
                    out << "\tsub r0, r1, r0\n";
                    pass.store(mir, "r0");
                    break;
                case BinaryMIR::Operator::AND:
                    pass.translateCommonBinaryMIR(mir, "and");
                    break;
                case BinaryMIR::Operator::OR:
                    pass.translateCommonBinaryMIR(mir, "orr");
                    break;
                case BinaryMIR::Operator::XOR:
                    pass.translateCommonBinaryMIR(mir, "eor");
                    break;
                case BinaryMIR::Operator::LSL:
                    pass.translateCommonBinaryMIR(mir, "lsl");
                    break;
                case BinaryMIR::Operator::LSR:
                    pass.translateCommonBinaryMIR(mir, "lsr");
                    break;
                case BinaryMIR::Operator::ASR:
                    pass.translateCommonBinaryMIR(mir, "asr");
                    break;
                case BinaryMIR::Operator::CMP_EQ:
                    pass.translateCompareMIR(mir, "moveq");
                    break;
                case BinaryMIR::Operator::CMP_NE:
                    pass.translateCompareMIR(mir, "movne");
                    break;
                case BinaryMIR::Operator::CMP_GT:
                    pass.translateCompareMIR(mir, "movgt");
                    break;
                case BinaryMIR::Operator::CMP_LT:
                    pass.translateCompareMIR(mir, "movlt");
                    break;
                case BinaryMIR::Operator::CMP_GE:
                    pass.translateCompareMIR(mir, "movge");
                    break;
                case BinaryMIR::Operator::CMP_LE:
                    pass.translateCompareMIR(mir, "movle");
                    break;
                default:
                    std::cerr << "invalid binary MIR type\n";
            }
        }

        void visit(UninitializedMIR *mir) override {
            //assign arbitrary value in r0
            pass.store(mir, "r0");
        }

        void visit(LoadConstantMIR *mir) override {
            pass.load(mir->src, "r0");
            pass.store(mir, "r0");
        }

        void visit(LoadVariableMIR *mir) override {
            pass.load(mir->src, "r0");
            pass.store(mir, "r0");
        }

        void visit(LoadPointerMIR *mir) override {
            pass.load(mir->src, "r0");
            out << "\tldr r0, [r0]\n";
            pass.store(mir, "r0");
        }

        void visit(StoreVariableMIR *mir) override {
            pass.load(mir->src, "r0");
            pass.store(mir->dst, "r0");
        }

        void visit(StorePointerMIR *mir) override {
            pass.load(mir->src, "r1");
            pass.load(mir->dst, "r0");
            out << "\tstr r1, [r0]\n";
        }

        void visit(MemoryCopyMIR *mir) override {
            // src and dst assignment must be fixed size array
            pass.load(mir->src, "r1");
            pass.load(mir->dst, "r2");
            auto arr = dynamic_cast<ArrayType *>(dynamic_cast<PointerType *>(mir->src->getType().get())->getElementType().get());
            int len = 1;
            unsigned size = 4;
            while (arr != nullptr) {
                if (!arr->getElementNum().has_value()) {
                    break;
                }
                len *= (int) arr->getElementNum().value();
                size = arr->getElementType()->getSizeOfType();
                arr = dynamic_cast<ArrayType *>(arr->getElementType().get());
            }
            out << "\tldr r0, [r1]\n";
            out << "\tstr r0, [r2]\n";
            pass.loadImmediate((int) size, "r3");
            for (int i = 1; i < len; i++) {
                out << "\tadd r1, r3\n";
                out << "\tldr r0, [r1]\n";
                out << "\tadd r2, r3\n";
                out << "\tstr r0, [r2]\n";
            }
        }

        void visit(ValueAddressingMIR *mir) override {
            pass.loadBase(mir, "r0");
            pass.store(mir, "r0");
        }

        void visit(ArrayAddressingMIR *mir) override {
            pass.loadBase(mir, "r0");
            pass.load(mir->offset, "r1");
            auto ptr = dynamic_cast<PointerType *>(mir->getType().get());
            auto arr = dynamic_cast<ArrayType *>(ptr->getElementType().get());
            if (arr != nullptr) {
                pass.loadImmediate((int) arr->getSizeOfType(), "r2");
                out << "\tmul r1, r1, r2\n";
                out << "\tadd r0, r0, r1\n";
            } else {
                out << "\tadd r0, r1, lsl #2\n";
            }
            int offset = pass.valueOffsetTable.find(mir)->second;
            string res = pass.preTreatStackOffset(offset, "r11");
            out << "\tstr r0, [" << res << "]\n";
        }

        void visit(JumpMIR *mir) override {
            auto nextBlock = mir->block;
            pass.generatePhi(bb, nextBlock);
            if (dfn + 1 == pass.dfsSequence.size() || pass.dfsSequence[dfn + 1] != nextBlock) {
                out << "\tb " << nextBlock->getName() << "\n";
            }
        }

        void visit(BranchMIR *mir) override {
            pass.load(mir->cond, "r0");
            out << "\tcmp r0, #0\n";
            string phiIf = pass.generatePhiLabel();
            out << "\tbne " << phiIf << "\n";
            pass.generatePhi(bb, mir->block2);
            out << "\tb " << mir->block2->getName() << "\n";
            out << phiIf << ":\n";
            pass.generatePhi(bb, mir->block1);
            auto nextBlock = mir->block1;
            if (dfn + 1 == pass.dfsSequence.size() || pass.dfsSequence[dfn + 1] != nextBlock) {
                out << "\tb " << nextBlock->getName() << "\n";
            }
        }

        void visit(CallMIR *mir) override {
            pass.callFunction(mir->args, mir->func->getName());
        }

        void visit(CallWithAssignMIR *mir) override {
            visit(static_cast<CallMIR *>(mir));
            pass.store(mir, "r0");
        }

        void visit(MultiCallMIR *mir) override {
            visit(static_cast<CallMIR *>(mir));
        }

        void visit(ReturnMIR *mir) override {
            pass.load(mir->val, "r0");
            pass.freeFrame("r11");
            out << "\tpop {pc}\n";
        }

        void visit(SelectMIR *mir) override {
            pass.load(mir->src1, "r1");
            pass.load(mir->src2, "r2");
            pass.load(mir->cond, "r3");
            out << "\tcmp r3, #0\n";
            out << "\tmoveq r0, r2\n";
            out << "\tmovne r0, r1\n";
            pass.store(mir, "r0");
        }

//        void visit(PhiMIR *mir) override {
        // generate when meeting jump or branch in precursor block
//        }

        void visit(AtomicLoopCondMIR *mir) override {
            pass.load(mir->atomic_var, "r0");
            pass.load(mir->border, "r1");
            out << "\tcmp r0, r1\n";
            switch (mir->compare_op) {
                case BinaryMIR::Operator::CMP_EQ:
                    out << "\tbne " << mir->exit->getName() << "\n";
                    break;
                case BinaryMIR::Operator::CMP_NE:
                    out << "\tbeq " << mir->exit->getName() << "\n";
                    break;
                case BinaryMIR::Operator::CMP_GT:
                    out << "\tble " << mir->exit->getName() << "\n";
                    break;
                case BinaryMIR::Operator::CMP_LT:
                    out << "\tbge " << mir->exit->getName() << "\n";
                    break;
                case BinaryMIR::Operator::CMP_GE:
                    out << "\tblt " << mir->exit->getName() << "\n";
                    break;
                case BinaryMIR::Operator::CMP_LE:
                    out << "\tbgt " << mir->exit->getName() << "\n";
                    break;
                default:
                    std::cerr << "invalid binary MIR type\n";
            }
            pass.load(mir->step, "r2");
            switch (mir->update_op) {
                case BinaryMIR::Operator::ADD:
                    out << "\tadd r3, r0, r2\n";
                    break;
                case BinaryMIR::Operator::SUB:
                    out << "\tsub r3, r0, r2\n";
                    break;
                default:
                    std::cerr << "invalid binary MIR type\n";
            }
            pass.store(mir->atomic_var, "r3");
            pass.store(mir, "r0");
            out << "\tb " << mir->body->getName() << "\n";
        }
    };
    out << bb.getName() << ":\n";
    MyVisitor visitor(*this, bb, out, dfn);
    for (auto &mir: bb.mirTable) {
        out << "\t@ " << mir->toString() << "\n";
        mir->accept(visitor);
    }
}

string MIRAssemblyGenerator::generatePhiLabel() {
    return ".phi" + std::to_string(incrementer++);
}

void MIRAssemblyGenerator::generatePhi(BasicBlock &ori, BasicBlock *bb) {
    std::map<PhiMIR *, bool> phiMap;
    for (auto &mir: bb->mirTable) {
        auto phiMir = dynamic_cast<PhiMIR *>(mir.get());
        if (phiMir == nullptr) {
            break;
        }
        auto iter = phiMir->incomingTable.find(&ori);
        if (iter != phiMir->incomingTable.end()) {
            phiMap[phiMir] = false;
        }
    }
    for (auto phi = phiMap.begin(); phi != phiMap.end(); phi++) {
        if (phi->second) continue;
        std::map<PhiMIR *, bool> conflict;
        conflict[phi->first] = false;
        phi->second = true;
        bool hasConflict = true;
        string dstName;
        while (hasConflict) {
            hasConflict = false;
            for (auto &conflictPhi : conflict) {
                if (!conflictPhi.second) {
                    conflictPhi.second = true;
                    dstName = conflictPhi.first->getName();
                    break;
                }
            }
            for (auto &nextPhi : phiMap) {
                if (!nextPhi.second && nextPhi.first->incomingTable.find(&ori)->second->getName() == dstName) {
                    conflict[nextPhi.first] = false;
                    nextPhi.second = true;
                    hasConflict = true;
                }
            }
        }
        int i = 0;
        for (auto &conflictPhi : conflict) {
            if (i > 11) {
                std::cerr << "conflict phi nodes are more than 12, registers spill\n";
                break;
            }
            load(conflictPhi.first->incomingTable.find(&ori)->second, "r" + std::to_string(i));
            i++;
        }
        i = 0;
        for (auto &conflictPhi : conflict) {
            if (i > 11) break;
            store(conflictPhi.first, "r" + std::to_string(i));
            i++;
        }
    }
}

void MIRAssemblyGenerator::callFunction(vector<Assignment *> args, const string &func) {
    int argOffset = 0;
    if (!args.empty()) loadArg(args[0], "r0");
    if (args.size() > 1) loadArg(args[1], "r1");
    if (args.size() > 2) loadArg(args[2], "r2");
    if (args.size() > 3) loadArg(args[3], "r3");
    if (args.size() > 4) {
        for (int i = 4; i < args.size(); i++) {
            loadArg(args[i], "r4");
            string res = preTreatStackOffset(argOffset, "r11");
            out << "\tstr r4, [" << res << "]\n";
            argOffset += 4;
        }
    }
    out << "\tbl " << func << "\n";
}

void MIRAssemblyGenerator::loadBase(MIR *mir, const string &reg) {
    auto base = dynamic_cast<ValueAddressingMIR *>(mir) == nullptr ? dynamic_cast<ArrayAddressingMIR *>(mir)->base :
                dynamic_cast<ValueAddressingMIR *>(mir)->base;
    auto iter = valueOffsetTable.find(base);
    if (iter == valueOffsetTable.end()) {
        loadLabel(base->getName(), reg);
    } else {
        loadImmediate(iter->second, "r11");
        auto variable = dynamic_cast<Assignment *>(base);
        if (variable != nullptr) {
            // pointer to array
            out << "\tldr " << reg << ", [sp, r11]\n";
        } else {
            out << "\tadd " << reg << ", sp, r11\n";
        }
    }
}


