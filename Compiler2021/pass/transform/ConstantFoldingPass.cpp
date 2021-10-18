//
// Created by 陈思言 on 2021/7/5.
//

#include <cassert>
#include "ConstantFoldingPass.h"
#include "../analyze/AnalyzeLoopPass.h"
#include "../analyze/AnalyzeActivityPass.h"
#include "../analyze/AnalyzeRegionPass.h"
#include "../../utils/BasicBlockUtils.h"

using std::make_pair;

bool ConstantFoldingPass::run() {
    analyzeDomTreePass.run();
    analyzeSideEffectPass.run();
    analyzeUnwrittenGlobalValuePass.run();
    analyzePointerPass.run();
    analyzeArrayAccessPass.run();
    currentId = 0;
    for (auto block : analyzeCFGPass.dfsSequence) {
        for (auto &mir : block->mirTable) {
            auto mir_assignment = dynamic_cast<Assignment *>(mir.get());
            if (mir_assignment != nullptr && mir_assignment->id > currentId) {
                currentId = mir_assignment->id;
            }
        }
    }
    currentId++;
    fold(analyzeDomTreePass.root, {});
    if (controlModified) {
        analyzeCFGPass.invalidate();
        analyzeDomTreePass.invalidate();
        dependency.analyzeLoopPass->invalidate();
        dependency.analyzeRegionPass->invalidate();
        dependency.analyzeActivityPass->invalidate();
    }
    if (pointerModified) {
        analyzePointerPass.invalidate();
        analyzeArrayAccessPass.invalidate();
    }
    analyzeCFGPass.run();
    analyzeDomTreePass.run();
    for (auto block : analyzeCFGPass.dfsSequence) {
        for (auto &mir : block->mirTable) {
            auto phiMir = dynamic_cast<PhiMIR *>(mir.get());
            if (phiMir != nullptr) {
                for (auto iter1 = phiMir->incomingTable.begin(); iter1 != phiMir->incomingTable.end();) {
                    BasicBlock *b = iter1->first;
                    if (!analyzeCFGPass.result[block].prev.count(b)) {
                        phiMir->incomingTable.erase(iter1++);
                    } else {
                        iter1++;
                    }
                }
            } else break;
        }
    }

    std::map<Assignment *, Assignment *> replaceTable;

    for (auto block : analyzeCFGPass.dfsSequence) {
        for (auto &mir : block->mirTable) {
            auto fold_phi = dynamic_cast<PhiMIR *>(mir.get());
            if (fold_phi != nullptr) {
                auto *p = getPhiDestination(fold_phi);
                if (p != nullptr) {
                    replaceTable[fold_phi] = p;
                }
            } else break;
        }
    }
    foldPhi(analyzeDomTreePass.root, {});
    for (auto block : analyzeCFGPass.dfsSequence) {
        for (auto &mir : block->mirTable) {
            mir->doReplacement(replaceTable);
        }
    }
    destination.clear();
    return !toErase.empty() || !replaceTable.empty();
}

void ConstantFoldingPass::fold(DomTreeNode *node, std::map<Assignment *, Assignment *> replaceTable) {
    vector<unique_ptr<MIR>> mirTable;
    for (auto &mir : node->block->mirTable) {
        auto new_mir = checkMIR(mir.get(), replaceTable, node->block);
        if (new_mir == mir.get()) {
            mirTable.emplace_back(std::move(mir));
        } else {
            if (new_mir != nullptr) {
                mirTable.emplace_back(new_mir);
            }
            toErase.emplace_back(std::move(mir));
        }
    }
    node->block->mirTable.swap(mirTable);
    auto rearVec = analyzeCFGPass.result[node->block].rear;
    for (auto rear : rearVec) {
        replacePhiIncoming(rear, replaceTable);
    }
    for (auto &child : getDomChildrenBlockNameOrder(node)) {
        fold(child, replaceTable);
    }
}

void ConstantFoldingPass::foldPhi(DomTreeNode *node, std::map<Assignment *, Assignment *> replaceTable) {
    vector<unique_ptr<MIR>> mirTable;
    for (auto &mir : node->block->mirTable) {
        auto *phi = dynamic_cast<PhiMIR *>(mir.get());
        auto *d = phi == nullptr ? nullptr : getPhiDestination(phi);
        if (phi != nullptr && d != nullptr) {
            //phiDestination中的phi一定不会被消除
            assert(!replaceTable.count(d));
            toErase.emplace_back(std::move(mir));
            replaceTable[phi] = d;
        } else {
            if (phi == nullptr) mir->doReplacement(replaceTable);
            mirTable.emplace_back(std::move(mir));
        }
    }
    node->block->mirTable.swap(mirTable);
    auto rearVec = analyzeCFGPass.result[node->block].rear;
    for (auto rear : rearVec) {
        replacePhiIncoming(rear, replaceTable);
    }
    for (auto &child : getDomChildrenBlockNameOrder(node)) {
        foldPhi(child, replaceTable);
    }

}

bool calculate(BinaryMIR::Operator op, int a, int b, int &val) {
    switch (op) {
        case BinaryMIR::Operator::ADD:
            val = a + b;
            return true;
        case BinaryMIR::Operator::SUB:
            val = a - b;
            return true;
        case BinaryMIR::Operator::MUL:
            val = a * b;
            return true;
        case BinaryMIR::Operator::DIV:
            if (b == 0) val = 0, std::cerr << "constant folding : div0" << std::endl;
            else val = a / b;
            return true;
        case BinaryMIR::Operator::MOD:
            if (b == 0) val = 0, std::cerr << "constant folding : mod0" << std::endl;
            else val = a % b;
            return true;
        case BinaryMIR::Operator::AND:
            val = a & b;
            return true;
        case BinaryMIR::Operator::OR:
            val = a | b;
            return true;
        case BinaryMIR::Operator::XOR:
            val = a ^ b;
            return true;
        case BinaryMIR::Operator::LSL:
            val = a << b;
            return true;
        case BinaryMIR::Operator::LSR:
            val = (unsigned int) a >> b;
            return true;
        case BinaryMIR::Operator::ASR:
            val = a >> b;
            return true;
        default:
            return false;
    }
}

bool compare(BinaryMIR::Operator op, int a, int b) {
    switch (op) {
        case BinaryMIR::Operator::CMP_EQ:
            return a == b;
        case BinaryMIR::Operator::CMP_NE:
            return a != b;
        case BinaryMIR::Operator::CMP_GT:
            return a > b;
        case BinaryMIR::Operator::CMP_LT:
            return a < b;
        case BinaryMIR::Operator::CMP_GE:
            return a >= b;
        case BinaryMIR::Operator::CMP_LE:
            return a <= b;
        default:
            return false;
    }
}

MIR *ConstantFoldingPass::checkMIR(MIR *mir, std::map<Assignment *, Assignment *> &replaceTable, BasicBlock *block) {
    struct MyVisitor : MIR::Visitor {
        ConstantFoldingPass &pass;
        std::map<Assignment *, Assignment *> &replaceTable;
        MIR *new_mir{};
        BasicBlock *curBlock{};

        explicit MyVisitor(ConstantFoldingPass &pass, std::map<Assignment *, Assignment *> &replaceTable)
                : pass(pass), replaceTable(replaceTable) {}

        void visit(UnaryMIR *mir) override {
            auto load_const = dynamic_cast<LoadConstantMIR *>(mir->src);
            if (load_const != nullptr) {
                int value = load_const->src->getValue<int>();
                Constant *constant;
                switch (mir->op) {
                    case UnaryMIR::Operator::NEG:
                        constant = pass.moduleIn->declareGlobalImmediateInt(-value);
                        break;
                    case UnaryMIR::Operator::NOT:
                        constant = pass.fn.declareLocalImmediateBool(!value);
                        break;
                    case UnaryMIR::Operator::REV:
                        constant = pass.moduleIn->declareGlobalImmediateInt(~value);
                        break;
                }
                auto load_const0 = new LoadConstantMIR(constant->getType(), "", pass.currentId++, constant);
                new_mir = load_const0;
                replaceTable[mir] = load_const0;
            }
        }

        void visit(BinaryMIR *mir) override {
            auto load_const1 = dynamic_cast<LoadConstantMIR *>(mir->src1);
            auto load_const2 = dynamic_cast<LoadConstantMIR *>(mir->src2);
            if (load_const1 != nullptr) {
                if (load_const2 != nullptr) {
                    // 两个操作数都是常量，可以直接算出结果
                    int value1 = load_const1->src->getValue<int>();
                    int value2 = load_const2->src->getValue<int>();
                    Constant *constant;
                    int ans = 0;
                    if (calculate(mir->op, value1, value2, ans)) {
                        constant = pass.moduleIn->declareGlobalImmediateInt(ans);
                    } else {
                        constant = pass.fn.declareLocalImmediateBool(compare(mir->op, value1, value2));
                    }
                    auto load_const0 = new LoadConstantMIR(
                            constant->getType(), "", pass.currentId++, constant);
                    new_mir = load_const0;
                    replaceTable[mir] = load_const0;
                } else {
                    // 第一个操作是常量，可以转化为一元表达式或进行复制传播
                    int value1 = load_const1->src->getValue<int>();
                    switch (mir->op) {
                        case BinaryMIR::Operator::ADD:
                            if (value1 == 0) {
                                new_mir = nullptr;
                                replaceTable[mir] = mir->src2;
                            }
                            break;
                        case BinaryMIR::Operator::SUB:
                            if (value1 == 0) {
                                auto neg = new UnaryMIR(UnaryMIR::Operator::NEG, mir->src2->getType(), "",
                                                        pass.currentId++, mir->src2);
                                new_mir = neg;
                                replaceTable[mir] = neg;
                            }
                            break;
                        case BinaryMIR::Operator::MUL:
                            if (value1 == 1) {
                                new_mir = nullptr;
                                replaceTable[mir] = mir->src2;
                            } else if (value1 == 0) {
                                Constant *constant = pass.moduleIn->declareGlobalImmediateInt(0);
                                auto load_const0 = new LoadConstantMIR(constant->getType(), "", pass.currentId++,
                                                                       constant);
                                new_mir = load_const0;
                                replaceTable[mir] = load_const0;
                            } else if (value1 == -1) {
                                auto neg = new UnaryMIR(UnaryMIR::Operator::NEG, mir->src2->getType(), "",
                                                        pass.currentId++, mir->src2);
                                new_mir = neg;
                                replaceTable[mir] = neg;
                            }
                            break;
                        case BinaryMIR::Operator::DIV:
                        case BinaryMIR::Operator::MOD:
                            if (value1 == 0) {
                                Constant *constant = pass.moduleIn->declareGlobalImmediateInt(0);
                                auto load_const0 = new LoadConstantMIR(constant->getType(), "", pass.currentId++,
                                                                       constant);
                                new_mir = load_const0;
                                replaceTable[mir] = load_const0;
                                break;
                            }
                        case BinaryMIR::Operator::AND:
                            if (value1 == 0xffffffff) {
                                new_mir = nullptr;
                                replaceTable[mir] = mir->src2;
                            } else if (value1 == 0) {
                                Constant *constant;
                                constant = pass.moduleIn->declareGlobalImmediateInt(0);
                                auto load_const0 = new LoadConstantMIR(
                                        constant->getType(), "", pass.currentId++, constant);
                                new_mir = load_const0;
                                replaceTable[mir] = load_const0;
                            }
                            break;
                        case BinaryMIR::Operator::OR:
                            if (value1 == 0) {
                                new_mir = nullptr;
                                replaceTable[mir] = mir->src2;
                            } else if (value1 == 0xffffffff) {
                                Constant *constant;
                                constant = pass.moduleIn->declareGlobalImmediateInt(0xffffffff);
                                auto load_const0 = new LoadConstantMIR(
                                        constant->getType(), "", pass.currentId++, constant);
                                new_mir = load_const0;
                                replaceTable[mir] = load_const0;
                            }
                            break;
                        case BinaryMIR::Operator::XOR:
                            if (value1 == 0) {
                                new_mir = nullptr;
                                replaceTable[mir] = mir->src2;
                            }
                            break;
                        case BinaryMIR::Operator::LSL:
                            if (value1 == 0) {
                                new_mir = nullptr;
                                replaceTable[mir] = mir->src2;
                            }
                            break;
                        case BinaryMIR::Operator::LSR:
                            if (value1 % 32 == 0) {
                                new_mir = nullptr;
                                replaceTable[mir] = mir->src2;
                            }
                            break;
                        case BinaryMIR::Operator::ASR:
                            if (value1 == 0) {
                                new_mir = nullptr;
                                replaceTable[mir] = mir->src2;
                            }
                            break;
                        case BinaryMIR::Operator::CMP_EQ:
                            break;
                        case BinaryMIR::Operator::CMP_NE:
                            break;
                        case BinaryMIR::Operator::CMP_GT:
                            break;
                        case BinaryMIR::Operator::CMP_LT:
                            break;
                        case BinaryMIR::Operator::CMP_GE:
                            break;
                        case BinaryMIR::Operator::CMP_LE:
                            break;
                    }
                }
            } else {
                if (load_const2 != nullptr) {
                    // 第二个操作是常量，可以转化为赋值或一元表达式或进行复制传播
                    int value2 = load_const2->src->getValue<int>();
                    switch (mir->op) {
                        case BinaryMIR::Operator::ADD:
                        case BinaryMIR::Operator::SUB:
                            if (value2 == 0) {
                                new_mir = nullptr;
                                replaceTable[mir] = mir->src1;
                            }
                            break;
                        case BinaryMIR::Operator::MUL:
                            if (value2 == 1) {
                                new_mir = nullptr;
                                replaceTable[mir] = mir->src1;
                            } else if (value2 == 0) {
                                Constant *constant;
                                constant = pass.moduleIn->declareGlobalImmediateInt(0);
                                auto load_const0 = new LoadConstantMIR(
                                        constant->getType(), "", pass.currentId++, constant);
                                new_mir = load_const0;
                                replaceTable[mir] = load_const0;
                            } else if (value2 == -1) {
                                auto neg = new UnaryMIR(UnaryMIR::Operator::NEG, mir->src2->getType(), "",
                                                        pass.currentId++, mir->src2);
                                new_mir = neg;
                                replaceTable[mir] = neg;
                            }
                            break;
                        case BinaryMIR::Operator::DIV:
                            if (value2 == 1) {
                                new_mir = nullptr;
                                replaceTable[mir] = mir->src1;
                            }
                            break;
                        case BinaryMIR::Operator::MOD:
                            if (value2 == 1) {
                                Constant *constant;
                                constant = pass.moduleIn->declareGlobalImmediateInt(0);
                                auto load_const0 = new LoadConstantMIR(
                                        constant->getType(), "", pass.currentId++, constant);
                                new_mir = load_const0;
                                replaceTable[mir] = load_const0;
                            }
                            break;
                        case BinaryMIR::Operator::AND:
                            if (value2 == 0xffffffff) {
                                new_mir = nullptr;
                                replaceTable[mir] = mir->src1;
                            } else if (value2 == 0) {
                                Constant *constant = pass.moduleIn->declareGlobalImmediateInt(0);
                                auto load_const0 = new LoadConstantMIR(
                                        constant->getType(), "", pass.currentId++, constant);
                                new_mir = load_const0;
                                replaceTable[mir] = load_const0;
                            }
                            break;
                        case BinaryMIR::Operator::OR:
                            if (value2 == 0) {
                                new_mir = nullptr;
                                replaceTable[mir] = mir->src1;
                            } else if (value2 == 0xffffffff) {
                                Constant *constant;
                                constant = pass.moduleIn->declareGlobalImmediateInt(0xffffffff);
                                auto load_const0 = new LoadConstantMIR(
                                        constant->getType(), "", pass.currentId++, constant);
                                new_mir = load_const0;
                                replaceTable[mir] = load_const0;
                            }
                            break;
                        case BinaryMIR::Operator::XOR:
                            if (value2 == 0) {
                                new_mir = nullptr;
                                replaceTable[mir] = mir->src1;
                            }
                            break;
                        case BinaryMIR::Operator::LSL:
                            if (value2 % 32 == 0) {
                                new_mir = nullptr;
                                replaceTable[mir] = mir->src1;
                            }
                            break;
                        case BinaryMIR::Operator::LSR:
                            if (value2 % 32 == 0) {
                                new_mir = nullptr;
                                replaceTable[mir] = mir->src1;
                            }
                            break;
                        case BinaryMIR::Operator::ASR:
                            if (value2 % 32 == 0) {
                                new_mir = nullptr;
                                replaceTable[mir] = mir->src1;
                            }
                            break;
                        case BinaryMIR::Operator::CMP_EQ:
                            break;
                        case BinaryMIR::Operator::CMP_NE:
                            break;
                        case BinaryMIR::Operator::CMP_GT:
                            break;
                        case BinaryMIR::Operator::CMP_LT:
                            break;
                        case BinaryMIR::Operator::CMP_GE:
                            break;
                        case BinaryMIR::Operator::CMP_LE:
                            break;
                    }
                } else if (mir->src1 == mir->src2) {
                    //两个操作数都不是常量，但src相等
                    switch (mir->op) {
                        case BinaryMIR::Operator::ADD:
                            break;
                        case BinaryMIR::Operator::SUB: {
                            Constant *constant = pass.moduleIn->declareGlobalImmediateInt(0);
                            auto load_const0 = new LoadConstantMIR(
                                    constant->getType(), "", pass.currentId++, constant);
                            new_mir = load_const0;
                            replaceTable[mir] = load_const0;
                            break;
                        }
                        case BinaryMIR::Operator::MUL:
                            break;
                        case BinaryMIR::Operator::DIV: {
                            Constant *constant = pass.moduleIn->declareGlobalImmediateInt(1);
                            auto load_const0 = new LoadConstantMIR(
                                    constant->getType(), "", pass.currentId++, constant);
                            new_mir = load_const0;
                            replaceTable[mir] = load_const0;
                            break;
                        }
                        case BinaryMIR::Operator::MOD: {
                            Constant *constant = pass.moduleIn->declareGlobalImmediateInt(0);
                            auto load_const0 = new LoadConstantMIR(
                                    constant->getType(), "", pass.currentId++, constant);
                            new_mir = load_const0;
                            replaceTable[mir] = load_const0;
                            break;
                        }
                        case BinaryMIR::Operator::AND:
                        case BinaryMIR::Operator::OR:
                            new_mir = nullptr;
                            replaceTable[mir] = mir->src1;
                            break;
                        case BinaryMIR::Operator::XOR: {
                            Constant *constant = pass.moduleIn->declareGlobalImmediateInt(0);
                            auto load_const0 = new LoadConstantMIR(
                                    constant->getType(), "", pass.currentId++, constant);
                            new_mir = load_const0;
                            replaceTable[mir] = load_const0;
                            break;
                        }
                        case BinaryMIR::Operator::LSL:
                        case BinaryMIR::Operator::LSR:
                        case BinaryMIR::Operator::ASR:
                            break;
                        case BinaryMIR::Operator::CMP_EQ: {
                            Constant *constant = pass.fn.declareLocalImmediateBool(true);
                            auto load_const0 = new LoadConstantMIR(
                                    constant->getType(), "", pass.currentId++, constant);
                            new_mir = load_const0;
                            replaceTable[mir] = load_const0;
                            break;
                        }
                        case BinaryMIR::Operator::CMP_NE: {
                            Constant *constant = pass.fn.declareLocalImmediateBool(false);
                            auto load_const0 = new LoadConstantMIR(
                                    constant->getType(), "", pass.currentId++, constant);
                            new_mir = load_const0;
                            replaceTable[mir] = load_const0;
                            break;
                        }
                        case BinaryMIR::Operator::CMP_GT: {
                            Constant *constant = pass.fn.declareLocalImmediateBool(false);
                            auto load_const0 = new LoadConstantMIR(
                                    constant->getType(), "", pass.currentId++, constant);
                            new_mir = load_const0;
                            replaceTable[mir] = load_const0;
                            break;
                        }
                        case BinaryMIR::Operator::CMP_LT: {
                            Constant *constant = pass.fn.declareLocalImmediateBool(false);
                            auto load_const0 = new LoadConstantMIR(
                                    constant->getType(), "", pass.currentId++, constant);
                            new_mir = load_const0;
                            replaceTable[mir] = load_const0;
                            break;
                        }
                        case BinaryMIR::Operator::CMP_GE: {
                            Constant *constant = pass.fn.declareLocalImmediateBool(true);
                            auto load_const0 = new LoadConstantMIR(
                                    constant->getType(), "", pass.currentId++, constant);
                            new_mir = load_const0;
                            replaceTable[mir] = load_const0;
                            break;
                        }
                        case BinaryMIR::Operator::CMP_LE: {
                            Constant *constant = pass.fn.declareLocalImmediateBool(true);
                            auto load_const0 = new LoadConstantMIR(
                                    constant->getType(), "", pass.currentId++, constant);
                            new_mir = load_const0;
                            replaceTable[mir] = load_const0;
                            break;
                        }
                    }
                }
            }
        }

        void visit(SelectMIR *mir) override {
            auto *cnst = dynamic_cast<LoadConstantMIR * >(mir->cond);
            if (cnst == nullptr) {
                if (mir->src1 == mir->src2) {
                    new_mir = nullptr;
                    replaceTable[mir] = mir->src1;
                }
                return;
            }
            int val = cnst->src->getValue<int>();
            if (val) {
                new_mir = nullptr;
                replaceTable[mir] = mir->src1;
            } else {
                new_mir = nullptr;
                replaceTable[mir] = mir->src2;
            }

        }

        void visit(BranchMIR *mir) override {
            auto *cnst = dynamic_cast<LoadConstantMIR * >(mir->cond);
            if (cnst == nullptr) return;
            int val = cnst->src->getValue<int>();
            BasicBlock *block;
            if (val) {
                new_mir = new JumpMIR(mir->block1);
                block = mir->block2;
            } else {
                new_mir = new JumpMIR(mir->block2);
                block = mir->block1;
            }
            for (auto &mmir : block->mirTable) {
                auto mir_phi = dynamic_cast<PhiMIR *>(mmir.get());
                if (mir_phi != nullptr) {
                    mir_phi->incomingTable.erase(curBlock);
                } else break;//phi只会连续存在首部
            }
            pass.controlModified = true;
        }


        void visit(MemoryCopyMIR *mir) override {
            if (mir->src == mir->dst) {
                new_mir = nullptr;
                pass.pointerModified = true;
            }
        }

        void visit(LoadVariableMIR *mir) override {
            if (pass.analyzeUnwrittenGlobalValuePass.constantReadMap.count(mir)) {
                auto load_cnst = new LoadConstantMIR(mir->src->getType(), "", pass.currentId++,
                                                     pass.analyzeUnwrittenGlobalValuePass.constantReadMap[mir]);
                new_mir = load_cnst;
                replaceTable[mir] = load_cnst;
            }
        }


        Constant *readArray(ArrayAddressingMIR *ptr, vector<int> &offset) {
            auto offsetCnst = dynamic_cast<LoadConstantMIR *> (ptr->offset);
            if (offsetCnst == nullptr) return nullptr;
            offset.push_back(offsetCnst->src->getValue<int>()); // offset : reverted dim indexes
            auto subarr = dynamic_cast<ArrayAddressingMIR *>(ptr->base);
            auto value = dynamic_cast<Constant *>(ptr->base);
            if (subarr != nullptr) {
                return readArray(subarr, offset);
            } else if (value != nullptr) {
                auto array_type = dynamic_cast<ArrayType *>(ptr->base->getType().get());
                if (array_type == nullptr) std::cerr << "constant folding: wrong array addr type" << std::endl;
                int realOffset = 0;
                for (int i = (int) offset.size() - 1; i >= 0; i--) {
                    if (array_type->getElementNum().has_value()) realOffset *= array_type->getElementNum().value();
                    realOffset += offset[i];
                    if (i > 0) {
                        array_type = dynamic_cast<ArrayType *> (array_type->getElementType().get());
                    }
                }
                if (array_type->getElementType()->getId() == Type::ID::INTEGER)
                    return pass.moduleIn->declareGlobalImmediateInt(value->getValue<vector<int>>()[realOffset]);
                else {
                    return pass.fn.declareLocalImmediateBool(value->getValue<vector<int>>()[realOffset]);
                }
            } else return nullptr;
        }

        bool isCoveredWrite(vector<Assignment *> &write, vector<Assignment *> &read_element) {
            int i = 0;
            for (; i < write.size() && i < read_element.size(); i++) {
                if (write[i] == nullptr || read_element[i] == nullptr) {
                    return true;
                }
                auto w = replaceTable.count(write[i]) ? replaceTable[write[i]] : write[i];
                auto r = replaceTable.count(read_element[i]) ? replaceTable[read_element[i]] : read_element[i];
                auto *cnst1 = dynamic_cast<LoadConstantMIR * >(w);
                auto *cnst2 = dynamic_cast<LoadConstantMIR * >(r);
                if (cnst1 == nullptr || cnst2 == nullptr) {
                    return true;
                }
                if (cnst2->src->getValue<int>() != cnst2->src->getValue<int>()) return false;
            }
            return true;
        }

        void visit(LoadPointerMIR *mir) override {
            if (pass.analyzeUnwrittenGlobalValuePass.constantReadMap.count(mir)) {
                auto load_cnst = new LoadConstantMIR(
                        pass.analyzeUnwrittenGlobalValuePass.constantReadMap[mir]->getType(),
                        "", pass.currentId++,
                        pass.analyzeUnwrittenGlobalValuePass.constantReadMap[mir]);

                new_mir = load_cnst;
                replaceTable[mir] = load_cnst;
                pass.pointerModified = true;
                return;
            }
            auto *ptr = pass.analyzePointerPass.ptrNode[mir->src];
            if (!ptr->offsetBase.has_value()) {
                return;
            }
            if (ptr->base->isConstant()) {
                auto *cnst = dynamic_cast<Constant *>(ptr->base);
                int v = cnst->getValue<vector<int >>()[ptr->offsetBase.value() / 4];
                cnst = pass.moduleIn->declareGlobalImmediateInt(v);
                auto load_cnst = new LoadConstantMIR(cnst->getType(), "", pass.currentId++, cnst);
                new_mir = load_cnst;
                replaceTable[mir] = load_cnst;
                pass.pointerModified = true;
                return;
            }
            if (!pass.analyzeArrayAccessPass.mirReadTable.count(mir)) {
                return;
            }
            auto read = pass.analyzeArrayAccessPass.mirReadTable[mir];
            ArrayWrite *write = read->useLastWrite;
            for (int i = 0;; ++i) {
                auto copy = dynamic_cast<ArrayEntireCopy *>(write);
                if (copy != nullptr)break;
                auto type = isCoveredWrite(read->readOffset, write->writeOffset);
                if (type) break;
                auto *update = dynamic_cast<ArrayWriteUpdate *> (write);
                if (update == nullptr) {
                    break;
                }
                write = update->updateLastWrite;
            }
            auto copy = dynamic_cast<ArrayEntireCopy *>(write);
            if (copy != nullptr && copy->writeOffset.empty()) {
                auto memCopy = dynamic_cast<MemoryCopyMIR *>(pass.analyzeArrayAccessPass.writeMIRTable[copy]);
                if (memCopy != nullptr) {
                    auto valueAdd = dynamic_cast<ValueAddressingMIR *>(memCopy->src);
                    if (valueAdd != nullptr) {
                        auto *cnst = dynamic_cast<Constant *>(valueAdd->base);
                        int v = cnst->getValue<vector<int >>()[ptr->offsetBase.value() / 4];
                        cnst = pass.moduleIn->declareGlobalImmediateInt(v);
                        auto load_cnst = new LoadConstantMIR(cnst->getType(), "", pass.currentId++, cnst);
                        new_mir = load_cnst;
                        replaceTable[mir] = load_cnst;
                        pass.pointerModified = true;
                        return;
                    }
                }
            }
        }

        void visit(CallWithAssignMIR *mir) override {
            if (pass.analyzeSideEffectPass.sideEffects[mir->func]->returnConstant &&
                pass.analyzeSideEffectPass.sideEffects[mir->func]->writeGlobalVariable.empty() &&
                pass.analyzeSideEffectPass.sideEffects[mir->func]->writeByPointerArg.empty() &&
                pass.analyzeSideEffectPass.sideEffects[mir->func]->callExternalFunction.empty()) {
                Constant *cnst = pass.analyzeSideEffectPass.sideEffects[mir->func]->returnValue;
                auto load_const0 = new LoadConstantMIR(
                        cnst->getType(), "", pass.currentId++, cnst);
                replaceTable[mir] = load_const0;
                new_mir = load_const0;
            }
        }

        //这里仅作加速, 保障只有单入口的block需要进行phi折叠时无需迭代
        void visit(PhiMIR *mir) override {
            Assignment *a = nullptr;
            std::vector<BasicBlock *> toErase = {};
            for (auto &x : mir->incomingTable) {
                if (!pass.analyzeCFGPass.result[curBlock].prev.count(x.first)) {
                    toErase.push_back(x.first);
                    continue;
                }
                a = x.second;
            }
            for (auto *b :toErase)mir->incomingTable.erase(b);
            assert(a != nullptr);

            if (pass.analyzeCFGPass.result[curBlock].prev.size() == 1) {
                replaceTable[mir] = a;
                new_mir = nullptr;
            }
        }

    };

    mir->doReplacement(replaceTable);
    MyVisitor visitor(*this, replaceTable);
    visitor.new_mir = mir;
    visitor.curBlock = block;
    mir->accept(visitor);
    return visitor.new_mir;
}

Assignment *ConstantFoldingPass::getPhiDestination(PhiMIR *phiMir) {
    if (destination.count(phiMir)) return destination[phiMir];
    Assignment *a = nullptr;
    destination[phiMir] = nullptr;
    for (auto x : phiMir->incomingTable) {
        auto *aa = x.second;
        auto *phi = dynamic_cast<PhiMIR *>(aa);
        if (phi != nullptr) {
            if (phi == phiMir) {
                return destination[phiMir] = nullptr;
            }
            auto *d = getPhiDestination(phi);
            if (d != nullptr) aa = d;
        }
        if (a != nullptr && aa != a) {
            return destination[phiMir] = nullptr;
        }
        a = aa;
    }
    return destination[phiMir] = a;
}
