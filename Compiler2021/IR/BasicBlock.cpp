//
// Created by 陈思言 on 2021/5/21.
//

#include <sstream>
#include <stdexcept>
#include <iostream>
#include "Function.h"

using std::logic_error;
using std::stringstream;
using std::make_unique;
using std::make_shared;

string BasicBlock::toString(LevelOfIR level) const {
    stringstream ss;
//    std::cout << name << ":\n";
    ss << name << ":\n";
    switch (level) {
        case LevelOfIR::HIGH:
            for (const auto &hir : hirTable) {
                ss << '\t' << hir->toString() << '\n';
            }
            break;
        case LevelOfIR::MEDIUM:
            for (const auto &mir : mirTable) {
//                std::cout << '\t' << mir->toString() << '\n';
                ss << '\t' << mir->toString() << '\n';
            }
            break;
        case LevelOfIR::LOW:
            for (const auto &lir : lirTable) {
//                std::cout << '\t' << lir->toString(RegAssign::Format::ALL) << '\n';
                auto tmp = lir.get();
                ss << '\t' << tmp->toString(RegAssign::Format::ALL) << '\n';
            }
            break;
    }
    return ss.str();
}

UnaryHIR *BasicBlock::createUnaryHIR(UnaryHIR::Operator op, Variable *dst, Value *src) {
    auto hir = new UnaryHIR(op, dst, src);
    hirTable.emplace_back(hir);
    return hir;
}

UnaryHIR *BasicBlock::createAssignHIR(Variable *dst, Value *src) {
    return createUnaryHIR(UnaryHIR::Operator::ASSIGN, dst, src);
}

UnaryHIR *BasicBlock::createNegHIR(Variable *dst, Value *src) {
    return createUnaryHIR(UnaryHIR::Operator::NEG, dst, src);
}

UnaryHIR *BasicBlock::createNotHIR(Variable *dst, Value *src) {
    return createUnaryHIR(UnaryHIR::Operator::NOT, dst, src);
}

UnaryHIR *BasicBlock::createRevHIR(Variable *dst, Value *src) {
    return createUnaryHIR(UnaryHIR::Operator::REV, dst, src);
}

BinaryHIR *BasicBlock::createBinaryHIR(BinaryHIR::Operator op, Variable *dst, Value *src1, Value *src2) {
    auto hir = new BinaryHIR(op, dst, src1, src2);
    hirTable.emplace_back(hir);
    return hir;
}

BinaryHIR *BasicBlock::createAddHIR(Variable *dst, Value *src1, Value *src2) {
    return createBinaryHIR(BinaryHIR::Operator::ADD, dst, src1, src2);
}

BinaryHIR *BasicBlock::createSubHIR(Variable *dst, Value *src1, Value *src2) {
    return createBinaryHIR(BinaryHIR::Operator::SUB, dst, src1, src2);
}

BinaryHIR *BasicBlock::createMulHIR(Variable *dst, Value *src1, Value *src2) {
    return createBinaryHIR(BinaryHIR::Operator::MUL, dst, src1, src2);
}

BinaryHIR *BasicBlock::createDivHIR(Variable *dst, Value *src1, Value *src2) {
    return createBinaryHIR(BinaryHIR::Operator::DIV, dst, src1, src2);
}

BinaryHIR *BasicBlock::createModHIR(Variable *dst, Value *src1, Value *src2) {
    return createBinaryHIR(BinaryHIR::Operator::MOD, dst, src1, src2);
}

BinaryHIR *BasicBlock::createAndHIR(Variable *dst, Value *src1, Value *src2) {
    return createBinaryHIR(BinaryHIR::Operator::AND, dst, src1, src2);
}

BinaryHIR *BasicBlock::createOrHIR(Variable *dst, Value *src1, Value *src2) {
    return createBinaryHIR(BinaryHIR::Operator::OR, dst, src1, src2);
}

BinaryHIR *BasicBlock::createXorHIR(Variable *dst, Value *src1, Value *src2) {
    return createBinaryHIR(BinaryHIR::Operator::XOR, dst, src1, src2);
}

BinaryHIR *BasicBlock::createLslHIR(Variable *dst, Value *src1, Value *src2) {
    return createBinaryHIR(BinaryHIR::Operator::LSL, dst, src1, src2);
}

BinaryHIR *BasicBlock::createLsrHIR(Variable *dst, Value *src1, Value *src2) {
    return createBinaryHIR(BinaryHIR::Operator::LSR, dst, src1, src2);
}

BinaryHIR *BasicBlock::createAsrHIR(Variable *dst, Value *src1, Value *src2) {
    return createBinaryHIR(BinaryHIR::Operator::ASR, dst, src1, src2);
}

BinaryHIR *BasicBlock::createCmpEqHIR(Variable *dst, Value *src1, Value *src2) {
    return createBinaryHIR(BinaryHIR::Operator::CMP_EQ, dst, src1, src2);
}

BinaryHIR *BasicBlock::createCmpNeHIR(Variable *dst, Value *src1, Value *src2) {
    return createBinaryHIR(BinaryHIR::Operator::CMP_NE, dst, src1, src2);
}

BinaryHIR *BasicBlock::createCmpGtHIR(Variable *dst, Value *src1, Value *src2) {
    return createBinaryHIR(BinaryHIR::Operator::CMP_GT, dst, src1, src2);
}

BinaryHIR *BasicBlock::createCmpLtHIR(Variable *dst, Value *src1, Value *src2) {
    return createBinaryHIR(BinaryHIR::Operator::CMP_LT, dst, src1, src2);
}

BinaryHIR *BasicBlock::createCmpGeHIR(Variable *dst, Value *src1, Value *src2) {
    return createBinaryHIR(BinaryHIR::Operator::CMP_GE, dst, src1, src2);
}

BinaryHIR *BasicBlock::createCmpLeHIR(Variable *dst, Value *src1, Value *src2) {
    return createBinaryHIR(BinaryHIR::Operator::CMP_LE, dst, src1, src2);
}

BinaryHIR *BasicBlock::createAddressingHIR(Variable *dst, Value *src1, Value *src2) {
    return createBinaryHIR(BinaryHIR::Operator::ADDRESSING, dst, src1, src2);
}

JumpHIR *BasicBlock::createJumpHIR(BasicBlock *block) {
    auto hir = new JumpHIR(block);
    hirTable.emplace_back(hir);
    return hir;
}

BranchHIR *BasicBlock::createBranchHIR(Value *cond, BasicBlock *block1, BasicBlock *block2) {
    auto hir = new BranchHIR(cond, block1, block2);
    hirTable.emplace_back(hir);
    return hir;
}

CallHIR *BasicBlock::createCallHIR(string_view func_name, vector<Value *> args) {
    auto hir = new CallHIR(func_name, std::move(args));
    hirTable.emplace_back(hir);
    return hir;
}

CallHIR *BasicBlock::createCallHIR(Variable *ret, string_view func_name, vector<Value *> args) {
    auto hir = new CallHIR(ret, func_name, std::move(args));
    hirTable.emplace_back(hir);
    return hir;
}

ReturnHIR *BasicBlock::createReturnHIR(Value *val) {
    auto hir = new ReturnHIR(val);
    hirTable.emplace_back(hir);
    return hir;
}

PutfHIR *BasicBlock::createPutfHIR(string_view format, vector<Value *> args) {
    auto hir = new PutfHIR(format, std::move(args));
    hirTable.emplace_back(hir);
    return hir;
}

UninitializedMIR *BasicBlock::createUninitializedMIR(shared_ptr<Type> type, string_view mir_name, size_t id) {
    auto mir = new UninitializedMIR(type, mir_name, id);
    mirTable.emplace_back(mir);
    return mir;
}

LoadConstantMIR *BasicBlock::createLoadConstantMIR(string_view mir_name, size_t id, Constant *src) {
    auto mir = new LoadConstantMIR(src->getType(), mir_name, id, src);
    mirTable.emplace_back(mir);
    return mir;
}

LoadVariableMIR *BasicBlock::createLoadVariableMIR(string_view mir_name, size_t id, Variable *src) {
    auto mir = new LoadVariableMIR(src->getType(), mir_name, id, src);
    mirTable.emplace_back(mir);
    return mir;
}

LoadPointerMIR *BasicBlock::createLoadPointerMIR(string_view mir_name, size_t id, Assignment *src) {
    auto src_type = dynamic_cast<PointerType *>(src->getType().get());
    auto mir = new LoadPointerMIR(src_type->getElementType(), mir_name, id, src);
    mirTable.emplace_back(mir);
    return mir;
}

StoreVariableMIR *BasicBlock::createStoreVariableMIR(Variable *dst, Assignment *src) {
    auto mir = new StoreVariableMIR(dst, src);
    mirTable.emplace_back(mir);
    return mir;
}

StorePointerMIR *BasicBlock::createStorePointerMIR(Assignment *dst, Assignment *src) {
    auto mir = new StorePointerMIR(dst, src);
    mirTable.emplace_back(mir);
    return mir;
}

MemoryCopyMIR *BasicBlock::createMemoryCopyMIR(Assignment *dst, Assignment *src) {
    auto mir = new MemoryCopyMIR(dst, src);
    mirTable.emplace_back(mir);
    return mir;
}

UnaryMIR *BasicBlock::createUnaryMIR(UnaryMIR::Operator op, shared_ptr<Type> type,
                                     string_view mir_name, size_t id, Assignment *src) {
    auto mir = new UnaryMIR(op, std::move(type), mir_name, id, src);
    mirTable.emplace_back(mir);
    return mir;
}

UnaryMIR *BasicBlock::createNegMIR(string_view mir_name, size_t id, Assignment *src) {
    return createUnaryMIR(UnaryMIR::Operator::NEG, IntegerType::object, mir_name, id, src);
}

UnaryMIR *BasicBlock::createNotMIR(string_view mir_name, size_t id, Assignment *src) {
    return createUnaryMIR(UnaryMIR::Operator::NOT, BooleanType::object, mir_name, id, src);
}

UnaryMIR *BasicBlock::createRevMIR(string_view mir_name, size_t id, Assignment *src) {
    return createUnaryMIR(UnaryMIR::Operator::REV, IntegerType::object, mir_name, id, src);
}

BinaryMIR *BasicBlock::createBinaryMIR(BinaryMIR::Operator op, shared_ptr<Type> type,
                                       string_view mir_name, size_t id, Assignment *src1, Assignment *src2) {
    auto mir = new BinaryMIR(op, std::move(type), mir_name, id, src1, src2);
    mirTable.emplace_back(mir);
    return mir;
}

BinaryMIR *BasicBlock::createAddMIR(string_view mir_name, size_t id, Assignment *src1, Assignment *src2) {
    return createBinaryMIR(BinaryMIR::Operator::ADD, IntegerType::object, mir_name, id, src1, src2);
}

BinaryMIR *BasicBlock::createSubMIR(string_view mir_name, size_t id, Assignment *src1, Assignment *src2) {
    return createBinaryMIR(BinaryMIR::Operator::SUB, IntegerType::object, mir_name, id, src1, src2);
}

BinaryMIR *BasicBlock::createMulMIR(string_view mir_name, size_t id, Assignment *src1, Assignment *src2) {
    return createBinaryMIR(BinaryMIR::Operator::MUL, IntegerType::object, mir_name, id, src1, src2);
}

BinaryMIR *BasicBlock::createDivMIR(string_view mir_name, size_t id, Assignment *src1, Assignment *src2) {
    return createBinaryMIR(BinaryMIR::Operator::DIV, IntegerType::object, mir_name, id, src1, src2);
}

BinaryMIR *BasicBlock::createModMIR(string_view mir_name, size_t id, Assignment *src1, Assignment *src2) {
    return createBinaryMIR(BinaryMIR::Operator::MOD, IntegerType::object, mir_name, id, src1, src2);
}

BinaryMIR *BasicBlock::createAndMIR(string_view mir_name, size_t id, Assignment *src1, Assignment *src2) {
    return createBinaryMIR(BinaryMIR::Operator::AND, IntegerType::object, mir_name, id, src1, src2);
}

BinaryMIR *BasicBlock::createOrMIR(string_view mir_name, size_t id, Assignment *src1, Assignment *src2) {
    return createBinaryMIR(BinaryMIR::Operator::OR, IntegerType::object, mir_name, id, src1, src2);
}

BinaryMIR *BasicBlock::createXorMIR(string_view mir_name, size_t id, Assignment *src1, Assignment *src2) {
    return createBinaryMIR(BinaryMIR::Operator::XOR, IntegerType::object, mir_name, id, src1, src2);
}

BinaryMIR *BasicBlock::createLslMIR(string_view mir_name, size_t id, Assignment *src1, Assignment *src2) {
    return createBinaryMIR(BinaryMIR::Operator::LSL, IntegerType::object, mir_name, id, src1, src2);
}

BinaryMIR *BasicBlock::createLsrMIR(string_view mir_name, size_t id, Assignment *src1, Assignment *src2) {
    return createBinaryMIR(BinaryMIR::Operator::LSR, IntegerType::object, mir_name, id, src1, src2);
}

BinaryMIR *BasicBlock::createAsrMIR(string_view mir_name, size_t id, Assignment *src1, Assignment *src2) {
    return createBinaryMIR(BinaryMIR::Operator::ASR, IntegerType::object, mir_name, id, src1, src2);
}

BinaryMIR *BasicBlock::createCmpEqMIR(string_view mir_name, size_t id, Assignment *src1, Assignment *src2) {
    return createBinaryMIR(BinaryMIR::Operator::CMP_EQ, BooleanType::object, mir_name, id, src1, src2);
}

BinaryMIR *BasicBlock::createCmpNeMIR(string_view mir_name, size_t id, Assignment *src1, Assignment *src2) {
    return createBinaryMIR(BinaryMIR::Operator::CMP_NE, BooleanType::object, mir_name, id, src1, src2);
}

BinaryMIR *BasicBlock::createCmpGtMIR(string_view mir_name, size_t id, Assignment *src1, Assignment *src2) {
    return createBinaryMIR(BinaryMIR::Operator::CMP_GT, BooleanType::object, mir_name, id, src1, src2);
}

BinaryMIR *BasicBlock::createCmpLtMIR(string_view mir_name, size_t id, Assignment *src1, Assignment *src2) {
    return createBinaryMIR(BinaryMIR::Operator::CMP_LT, BooleanType::object, mir_name, id, src1, src2);
}

BinaryMIR *BasicBlock::createCmpGeMIR(string_view mir_name, size_t id, Assignment *src1, Assignment *src2) {
    return createBinaryMIR(BinaryMIR::Operator::CMP_GE, BooleanType::object, mir_name, id, src1, src2);
}

BinaryMIR *BasicBlock::createCmpLeMIR(string_view mir_name, size_t id, Assignment *src1, Assignment *src2) {
    return createBinaryMIR(BinaryMIR::Operator::CMP_LE, BooleanType::object, mir_name, id, src1, src2);
}

ValueAddressingMIR *BasicBlock::createValueAddressingMIR(string_view mir_name, size_t id, Value *base) {
    auto ptr_type = make_shared<PointerType>(base->getType());
    auto mir = new ValueAddressingMIR(ptr_type, mir_name, id, base);
    mirTable.emplace_back(mir);
    return mir;
}

ArrayAddressingMIR *BasicBlock::createArrayAddressingMIR(string_view mir_name, size_t id,
                                                         Value *base, Assignment *offset) {
    auto array_type = dynamic_cast<ArrayType *>(base->getType().get());
    if (array_type == nullptr) {
        // Pointer to array
        auto ptr_type = dynamic_cast<PointerType *>(base->getType().get());
        if (ptr_type == nullptr) {
            throw logic_error("Invalid type");
        }
        array_type = dynamic_cast<ArrayType *>(ptr_type->getElementType().get());
    }
    auto element_ptr_type = make_shared<PointerType>(array_type->getElementType());
    auto mir = new ArrayAddressingMIR(element_ptr_type, mir_name, id, base, offset);
    mirTable.emplace_back(mir);
    return mir;
}

JumpMIR *BasicBlock::createJumpMIR(BasicBlock *block) {
    auto mir = new JumpMIR(block);
    mirTable.emplace_back(mir);
    return mir;
}

BranchMIR *BasicBlock::createBranchMIR(Assignment *cond, BasicBlock *block1, BasicBlock *block2) {
    auto mir = new BranchMIR(cond, block1, block2);
    mirTable.emplace_back(mir);
    return mir;
}

CallMIR *BasicBlock::createCallMIR(Function *func, vector<Assignment *> args) {
    auto mir = new CallMIR(func, std::move(args));
    mirTable.emplace_back(mir);
    return mir;
}

CallWithAssignMIR *BasicBlock::createCallMIR(string_view mir_name, size_t id,
                                             Function *func, vector<Assignment *> args) {
    auto mir = new CallWithAssignMIR(func->getReturnType(), mir_name, id, func, std::move(args));
    mirTable.emplace_back(mir);
    return mir;
}

ReturnMIR *BasicBlock::createReturnMIR(Assignment *val) {
    auto mir = new ReturnMIR(val);
    mirTable.emplace_back(mir);
    return mir;
}

PhiMIR *BasicBlock::createPhiMIR(shared_ptr<Type> type, string_view mir_name, size_t id) {
    auto mir = new PhiMIR(type, mir_name, id);
    mirTable.emplace_back(mir);
    return mir;
}

SelectMIR *
BasicBlock::createSelectMIR(string_view name, size_t id, Assignment *cond, Assignment *src1,
                            Assignment *src2) {
    auto mir = new SelectMIR(IntegerType::object, name, id, cond, src1, src2);
    mirTable.emplace_back(mir);
    return mir;
}
