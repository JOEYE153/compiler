//
// Created by 陈思言 on 2021/7/8.
//

#include "HIRAssemblyGenerator.h"

void HIRAssemblyGenerator::runOnFunction(Function &fn) {
    out << "\tpush {lr}\n";

    auto constantTable = fn.getLocalConstantVecDictOrder();
    auto variableTable = fn.getLocalVariableVecDictOrder();

    // calculate space remain for called func arg
    int maxCalledFuncArgSize = 0;
    AnalyzeCallGraphPass_HIR analyzeCallGraphPassHir(md, out);
    analyzeCallGraphPassHir.run();
    for (auto callfunc : analyzeCallGraphPassHir.callGraph[&fn]) {
        maxCalledFuncArgSize = callfunc->args.size() > maxCalledFuncArgSize ?
                               (int) callfunc->args.size() : maxCalledFuncArgSize;
    }
    maxCalledFuncArgSize = maxCalledFuncArgSize > 4 ? maxCalledFuncArgSize - 4 : 0;

    int constantSize = 0;
    for (auto constant: constantTable) {
        if (constant->getType()->getId() == Type::ID::ARRAY) {
            constantSize += (int) constant->getValue<vector<int>>().size();
        }
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

    int argSize = fn.args.size() > 4 ? 4 : (int) fn.args.size();
    frameSize = (constantSize + variableSize + argSize + maxCalledFuncArgSize) * 4;
    allocateFrame("r11");
    if (fn.args.size() > 4) {
        int argOffset = frameSize + 4; // value remain space and lr
        for (int i = 4; i < fn.args.size(); i++) {
            valueOffsetTable.emplace(fn.args[i], argOffset);
            argOffset += 4;
        }
    }
    int offset = maxCalledFuncArgSize * 4;
    for (int i = 0; i < 4 && i < fn.args.size(); i++) {
        valueOffsetTable.emplace(fn.args[i], offset);
        string res = preTreatStackOffset(offset, "r11");
        out << "\tstr r" << i << ", [" << res << "]\n";
        offset += 4;
    }
    for (auto constant: constantTable) {
        valueOffsetTable.emplace(constant, offset);
        // only constant array should be stored on stack, or mov immediate directly to register
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
    AnalyzeCFGPass_HIR analyzeCFGPass(fn);
    analyzeCFGPass.run();
    dfsSequence = analyzeCFGPass.dfsSequence;
    for (auto i = 0; i < dfsSequence.size(); i++) {
        runOnBasicBlock(*dfsSequence[i], i);
    }
    valueOffsetTable.clear();
}

void HIRAssemblyGenerator::translateCommonBinaryHIR(BinaryHIR *hir, const string &op) {
    load(hir->src1, "r1");
    load(hir->src2, "r2");
    out << "\t" << op << " r0, r1, r2\n";
    store(hir->dst, "r0");
}

void HIRAssemblyGenerator::translateCompareHIR(BinaryHIR *hir, const string &op) {
    load(hir->src1, "r1");
    load(hir->src2, "r2");
    out << "\tmov r0, #0\n";
    out << "\tcmp r1, r2\n";
    out << "\t" << op << " r0, #1\n";
    store(hir->dst, "r0");
}

void HIRAssemblyGenerator::runOnBasicBlock(BasicBlock &bb, size_t dfn) {
    struct MyVisitor : HIR::Visitor {
        HIRAssemblyGenerator &pass;
        BasicBlock &bb;
        std::ostream &out;
        size_t dfn;

        MyVisitor(HIRAssemblyGenerator &pass, BasicBlock &bb, std::ostream &out, size_t dfn)
                : pass(pass), bb(bb), out(out), dfn(dfn) {}

        void visit(UnaryHIR *hir) override {
            switch (hir->op) {
                case UnaryHIR::Operator::ASSIGN: {
                    if (hir->src->getType()->getId() == Type::ID::ARRAY) {
                        auto values = ((Constant *) hir->src)->getValue<vector<int>>();
                        int offset = 0;
                        for (auto value : values) {
                            pass.loadImmediate(value, "r0");
                            // out << "mov r0, " << value << "\n";
                            if (hir->dst->getLocation() == Value::Location::STATIC) {
                                pass.loadLabel(hir->dst->getName(), "r11");
                                if (offset <= 4095) {
                                    out << "\tstr r0, [r11, #" << offset << "]\n";
                                } else {
                                    out << "\tmovw r1, #:lower16:" << offset << "\n";
                                    out << "\tmovt r1, #:upper16:" << offset << "\n";
                                    out << "\tstr r0, [r11, r1]\n";
                                }
                            } else {
                                auto variable = hir->dst;
                                int dstOffset = pass.valueOffsetTable.find(variable)->second;
                                string ref = pass.preTreatStackOffset(dstOffset, "r11");
                                if (variable->isReference()) {
                                    out << "\tldr r11, [" << ref << "]\n";
                                    pass.loadImmediate(offset, "r1");
                                    out << "\tstr r0, [r11, r1]\n";
                                } else {
                                    string res = pass.preTreatStackOffset(offset + dstOffset, "r11");
                                    out << "\tstr r0, [" << res << "]\n";
                                }
                            }
                            offset += 4;
                        }
                    } else {
                        pass.load(hir->src, "r0");
                        pass.store(hir->dst, "r0");
                    }
                    break;
                }
                case UnaryHIR::Operator::NEG:
                    pass.load(hir->src, "r1");
                    out << "\trsb r0, r1, #0\n";
                    pass.store(hir->dst, "r0");
                    break;
                case UnaryHIR::Operator::NOT:
                    // if src == 0 dst = 1
                    // else dst = 0
                    pass.load(hir->src, "r1");
                    out << "\tmov r0, #0\n";
                    out << "\tcmp r1, #0\n";
                    out << "\tmoveq r0, #1\n";
                    pass.store(hir->dst, "r0");
                    break;
                case UnaryHIR::Operator::REV:
                    // int dst = ~src
                    pass.load(hir->src, "r1");
                    out << "\tmvn r0, r1\n";
                    pass.store(hir->dst, "r0");
                    break;
            }
        }

        void visit(BinaryHIR *hir) override {
            switch (hir->op) {
                case BinaryHIR::Operator::ADD:
                    pass.translateCommonBinaryHIR(hir, "add");
                    break;
                case BinaryHIR::Operator::SUB:
                    pass.translateCommonBinaryHIR(hir, "sub");
                    break;
                case BinaryHIR::Operator::MUL:
                    pass.translateCommonBinaryHIR(hir, "mul");
                    break;
                case BinaryHIR::Operator::DIV:
                    pass.translateCommonBinaryHIR(hir, "sdiv");
                    break;
                case BinaryHIR::Operator::MOD:
                    // r0 = r1 % r2 = r1 - (r1/r2) * r2
                    pass.load(hir->src1, "r1");
                    pass.load(hir->src2, "r2");
                    out << "\tsdiv r0, r1, r2\n";
                    out << "\tmul r0, r0, r2\n";
                    out << "\tsub r0, r1, r0\n";
                    pass.store(hir->dst, "r0");
                    break;
                case BinaryHIR::Operator::AND:
                    pass.translateCommonBinaryHIR(hir, "and");
                    break;
                case BinaryHIR::Operator::OR:
                    pass.translateCommonBinaryHIR(hir, "orr");
                    break;
                case BinaryHIR::Operator::XOR:
                    pass.translateCommonBinaryHIR(hir, "eor");
                    break;
                case BinaryHIR::Operator::LSL:
                    pass.translateCommonBinaryHIR(hir, "lsl");
                    break;
                case BinaryHIR::Operator::LSR:
                    pass.translateCommonBinaryHIR(hir, "lsr");
                    break;
                case BinaryHIR::Operator::ASR:
                    pass.translateCommonBinaryHIR(hir, "asr");
                    break;
                case BinaryHIR::Operator::CMP_EQ:
                    pass.translateCompareHIR(hir, "moveq");
                    break;
                case BinaryHIR::Operator::CMP_NE:
                    pass.translateCompareHIR(hir, "movne");
                    break;
                case BinaryHIR::Operator::CMP_GT:
                    pass.translateCompareHIR(hir, "movgt");
                    break;
                case BinaryHIR::Operator::CMP_LT:
                    pass.translateCompareHIR(hir, "movlt");
                    break;
                case BinaryHIR::Operator::CMP_GE:
                    pass.translateCompareHIR(hir, "movge");
                    break;
                case BinaryHIR::Operator::CMP_LE:
                    pass.translateCompareHIR(hir, "movle");
                    break;
                case BinaryHIR::Operator::ADDRESSING: {
                    auto arr = hir->src1;
                    auto iter = pass.valueOffsetTable.find(arr);
                    if (iter == pass.valueOffsetTable.end()) {
                        pass.loadLabel(hir->src1->getName(), "r0");
                    } else {
                        auto variable = dynamic_cast<Variable *>(arr);
                        if (variable != nullptr && variable->isReference()) {
                            string res = pass.preTreatStackOffset(iter->second, "r11");
                            out << "\tldr r0, [" << res << "]\n";
                        } else {
                            pass.loadImmediate(iter->second, "r11");
                            out << "\tadd r0, sp, r11\n";
                        }
                    }
                    pass.load(hir->src2, "r1");
                    // get next dim size
                    auto dst = hir->dst;
                    if (dst->getType()->getId() == Type::ID::ARRAY) {
                        int typeSize = (int) dynamic_cast<ArrayType *>(arr->getType().get())->getElementType()->getSizeOfType();
                        pass.loadImmediate(typeSize, "r2");
                        out << "\tmul r1, r1, r2\n";
                        out << "\tadd r0, r0, r1\n";
                    } else {
                        out << "\tadd r0, r1, lsl #2\n";
                    }
                    // address stored in r0
                    int dstOffset = pass.valueOffsetTable.find(hir->dst)->second;
                    string res = pass.preTreatStackOffset(dstOffset, "r11");
                    out << "\tstr r0, [" << res << "]\n";
                }
            }
        }

        void visit(JumpHIR *hir) override {
            auto nextBlock = hir->block;
            if (dfn + 1 == pass.dfsSequence.size() || pass.dfsSequence[dfn + 1] != nextBlock) {
                out << "\tb " << nextBlock->getName() << "\n";
            }
        }

        void visit(BranchHIR *hir) override {
            pass.load(hir->cond, "r0");
            out << "\tcmp r0, #0\n";
            out << "\tbeq " << hir->block2->getName() << "\n";
            auto nextBlock = hir->block1;
            if (dfn + 1 == pass.dfsSequence.size() || pass.dfsSequence[dfn + 1] != nextBlock) {
                out << "\tb " << nextBlock->getName() << "\n";
            }
        }

        void visit(CallHIR *hir) override {
            int argOffset = 0;
            auto args = hir->args;
            if (args.size() > 4) {
                for (int i = 4; i < args.size(); i++) {
                    pass.loadArg(args[i], "r4");
                    string res = pass.preTreatStackOffset(argOffset, "r11");
                    out << "\tstr r4, [" << res << "]\n";
                    argOffset += 4;
                }
            }
            if (!args.empty()) pass.loadArg(args[0], "r0");
            if (args.size() > 1) pass.loadArg(args[1], "r1");
            if (args.size() > 2) pass.loadArg(args[2], "r2");
            if (args.size() > 3) pass.loadArg(args[3], "r3");
            out << "\tbl " << hir->func_name << "\n";
            pass.store(hir->ret, "r0");
        }

        void visit(ReturnHIR *hir) override {
            pass.load(hir->val, "r0");
            pass.freeFrame("r11");
            out << "\tpop {pc}\n";
        }
    };

    out << bb.getName() << ":\n";
    MyVisitor visitor(*this, bb, out, dfn);
    for (auto &hir: bb.hirTable) {
        out << "\t@ " << hir->toString() << "\n";
        hir->accept(visitor);
    }
}
