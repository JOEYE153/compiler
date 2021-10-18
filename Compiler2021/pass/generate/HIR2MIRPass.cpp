//
// Created by 陈思言 on 2021/6/5.
//

#include "HIR2MIRPass.h"

#include <stdexcept>

using std::logic_error;
using std::make_unique;
using std::make_shared;

bool HIR2MIRPass::run() {
    auto functionTable = md.getFunctionVecDictOrder();
    for (auto fn : functionTable) {
        if (fn->isExternal) {
            continue;
        }
        ref2ptrMap.clear();
        auto variableTable = fn->getLocalVariableVecDictOrder();
        for (auto variable : variableTable) {
            if (variable->isReference()) {
                auto ptr_type = make_shared<PointerType>(variable->getType());
                auto ptr_name = string("&") + variable->getName();
                bool is_arg = variable->getLocation() == Value::Location::ARGUMENT;
                auto variable_ptr = fn->declareLocalVariable(ptr_type, ptr_name, false, is_arg);
                ref2ptrMap[variable] = variable_ptr;
            }
        }
        currentId = 0;
        auto blockTable = fn->getBasicBlockVecDictOrder();
        for (auto bb : blockTable) {
            runOnBasicBlock(*bb);
            bb->hirTable.clear();
        }
        for (auto &arg : fn->args) {
            if (arg != nullptr && arg->isReference()) {
                arg = ref2ptrMap[arg];
            }
        }
    }
    return true;
}

static UnaryMIR::Operator translateOperator(UnaryHIR::Operator op) {
    switch (op) {
        case UnaryHIR::Operator::NEG:
            return UnaryMIR::Operator::NEG;
        case UnaryHIR::Operator::NOT:
            return UnaryMIR::Operator::NOT;
        case UnaryHIR::Operator::REV:
            return UnaryMIR::Operator::REV;
        default:
            throw logic_error("Cannot translate");
    }
}

static BinaryMIR::Operator translateOperator(BinaryHIR::Operator op) {
    switch (op) {
        case BinaryHIR::Operator::ADD:
            return BinaryMIR::Operator::ADD;
        case BinaryHIR::Operator::SUB:
            return BinaryMIR::Operator::SUB;
        case BinaryHIR::Operator::MUL:
            return BinaryMIR::Operator::MUL;
        case BinaryHIR::Operator::DIV:
            return BinaryMIR::Operator::DIV;
        case BinaryHIR::Operator::MOD:
            return BinaryMIR::Operator::MOD;
        case BinaryHIR::Operator::AND:
            return BinaryMIR::Operator::AND;
        case BinaryHIR::Operator::OR:
            return BinaryMIR::Operator::OR;
        case BinaryHIR::Operator::XOR:
            return BinaryMIR::Operator::XOR;
        case BinaryHIR::Operator::LSL:
            return BinaryMIR::Operator::LSL;
        case BinaryHIR::Operator::LSR:
            return BinaryMIR::Operator::LSR;
        case BinaryHIR::Operator::ASR:
            return BinaryMIR::Operator::ASR;
        case BinaryHIR::Operator::CMP_EQ:
            return BinaryMIR::Operator::CMP_EQ;
        case BinaryHIR::Operator::CMP_NE:
            return BinaryMIR::Operator::CMP_NE;
        case BinaryHIR::Operator::CMP_GT:
            return BinaryMIR::Operator::CMP_GT;
        case BinaryHIR::Operator::CMP_LT:
            return BinaryMIR::Operator::CMP_LT;
        case BinaryHIR::Operator::CMP_GE:
            return BinaryMIR::Operator::CMP_GE;
        case BinaryHIR::Operator::CMP_LE:
            return BinaryMIR::Operator::CMP_LE;
        default:
            throw logic_error("Cannot translate");
    }
}

void HIR2MIRPass::runOnBasicBlock(BasicBlock &bb) {
    struct MyVisitor : HIR::Visitor {
        HIR2MIRPass &pass;
        BasicBlock &bb;

        MyVisitor(HIR2MIRPass &pass, BasicBlock &bb) : pass(pass), bb(bb) {}

        void visit(UnaryHIR *hir) override {
            switch (hir->op) {
                case UnaryHIR::Operator::ASSIGN: {
                    if (hir->dst->getType()->getId() == Type::ID::ARRAY) {
                        auto addressing_src_mir = pass.getValuePtr(bb, hir->src);
                        auto addressing_dst_mir = pass.getValuePtr(bb, hir->dst);
                        bb.createMemoryCopyMIR(addressing_dst_mir, addressing_src_mir);
                    } else {
                        auto load_mir = pass.createLoad(bb, hir->src);
                        pass.createStore(bb, hir->dst, load_mir);
                    }
                    break;
                }
                case UnaryHIR::Operator::NEG:
                case UnaryHIR::Operator::NOT:
                case UnaryHIR::Operator::REV: {
                    auto load_mir = pass.createLoad(bb, hir->src);
                    auto unary_mir = bb.createUnaryMIR(translateOperator(hir->op), hir->dst->getType(),
                                                       "", pass.currentId++, load_mir);
                    pass.createStore(bb, hir->dst, unary_mir);
                    break;
                }
            }
        }

        void visit(BinaryHIR *hir) override {
            switch (hir->op) {
                case BinaryHIR::Operator::ADD:
                case BinaryHIR::Operator::SUB:
                case BinaryHIR::Operator::MUL:
                case BinaryHIR::Operator::DIV:
                case BinaryHIR::Operator::MOD:
                case BinaryHIR::Operator::AND:
                case BinaryHIR::Operator::OR:
                case BinaryHIR::Operator::XOR:
                case BinaryHIR::Operator::LSL:
                case BinaryHIR::Operator::LSR:
                case BinaryHIR::Operator::ASR: {
                    auto load1_mir = pass.createLoad(bb, hir->src1);
                    auto load2_mir = pass.createLoad(bb, hir->src2);
                    auto binary_mir = bb.createBinaryMIR(translateOperator(hir->op), hir->dst->getType(),
                                                         "", pass.currentId++, load1_mir, load2_mir);
                    pass.createStore(bb, hir->dst, binary_mir);
                    break;
                }
                case BinaryHIR::Operator::CMP_EQ:
                case BinaryHIR::Operator::CMP_NE:
                case BinaryHIR::Operator::CMP_GT:
                case BinaryHIR::Operator::CMP_LT:
                case BinaryHIR::Operator::CMP_GE:
                case BinaryHIR::Operator::CMP_LE: {
                    auto load1_mir = pass.createLoad(bb, hir->src1);
                    auto load2_mir = pass.createLoad(bb, hir->src2);
                    auto binary_mir = bb.createBinaryMIR(
                            translateOperator(hir->op), BooleanType::object,
                            "", pass.currentId++, load1_mir, load2_mir);
                    pass.createStore(bb, hir->dst, binary_mir);
                    break;
                }
                case BinaryHIR::Operator::ADDRESSING: {
                    auto index_mir = pass.createLoad(bb, hir->src2);
                    Value *base = hir->src1;
                    auto *base_variable = dynamic_cast<Variable *>(base);
                    if (base_variable != nullptr && base_variable->isReference()) {
                        auto ptr = pass.ref2ptrMap[base_variable];
                        base = bb.createLoadVariableMIR(ptr->getName(), pass.currentId++, ptr);
                    }
                    auto addressing_mir = bb.createArrayAddressingMIR(
                            "", pass.currentId++, base, index_mir);
                    bb.createStoreVariableMIR(pass.ref2ptrMap[hir->dst], addressing_mir);
                    break;
                }
            }
        }

        void visit(JumpHIR *hir) override {
            bb.createJumpMIR(hir->block);
        }

        void visit(BranchHIR *hir) override {
            auto load_mir = pass.createLoad(bb, hir->cond);
            bb.createBranchMIR(load_mir, hir->block1, hir->block2);
        }

        void visit(CallHIR *hir) override {
            vector<Assignment *> args(hir->args.size(), nullptr);
            for (size_t i = 0; i < args.size(); i++) {
                if (hir->args[i]->getType()->getId() == Type::ID::ARRAY) {
                    args[i] = pass.getValuePtr(bb, hir->args[i]);
                } else {
                    args[i] = pass.createLoad(bb, hir->args[i]);
                }
            }
            auto func = pass.md.getFunctionByName(hir->func_name);
            if (hir->ret == nullptr) {
                bb.createCallMIR(func, args);
            } else {
                auto call_mir = bb.createCallMIR("", pass.currentId++, func, args);
                pass.createStore(bb, hir->ret, call_mir);
            }
        }

        void visit(ReturnHIR *hir) override {
            if (hir->val != nullptr) {
                auto load_mir = pass.createLoad(bb, hir->val);
                bb.createReturnMIR(load_mir);
            } else {
                bb.createReturnMIR();
            }
        }
    };

    MyVisitor visitor(*this, bb);
    for (auto &hir : bb.hirTable) {
        hir->accept(visitor);
    }
}

Assignment *HIR2MIRPass::getValuePtr(BasicBlock &bb, Value *src) {
    if (src->isConstant()) {
        return bb.createValueAddressingMIR(string("&") + src->getName(), currentId++, src);
    } else {
        auto src_variable = reinterpret_cast<Variable *>(src);
        if (src_variable->isReference()) {
            auto ptr = ref2ptrMap[src_variable];
            return bb.createLoadVariableMIR(ptr->getName(), currentId++, ptr);
        } else {
            return bb.createValueAddressingMIR( string("&") + src->getName(), currentId++, src);
        }
    }
}

Assignment *HIR2MIRPass::createLoad(BasicBlock &bb, Value *src) {
    if (src->isConstant()) {
        auto src_constant = reinterpret_cast<Constant *>(src);
        return bb.createLoadConstantMIR(src->getName(), currentId++, src_constant);
    } else {
        auto src_variable = reinterpret_cast<Variable *>(src);
        if (src_variable->isReference()) {
            auto ptr = ref2ptrMap[src_variable];
            auto load_ptr_mir = bb.createLoadVariableMIR(ptr->getName(), currentId++, ptr);
            return bb.createLoadPointerMIR(src->getName(), currentId++, load_ptr_mir);
        } else {
            return bb.createLoadVariableMIR(src->getName(), currentId++, src_variable);
        }
    }
}

void HIR2MIRPass::createStore(BasicBlock &bb, Variable *dst, Assignment *src) {
    if (dst->isReference()) {
        auto ptr = ref2ptrMap[dst];
        auto load_ptr_mir = bb.createLoadVariableMIR(ptr->getName(), currentId++, ptr);
        bb.createStorePointerMIR(load_ptr_mir, src);
    } else {
        bb.createStoreVariableMIR(dst, src);
    }
}
