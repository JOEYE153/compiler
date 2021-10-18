//
// Created by 陈思言 on 2021/5/21.
//

#ifndef COMPILER2021_BASICBLOCK_H
#define COMPILER2021_BASICBLOCK_H

#include "HIR.h"
#include "MIR.h"
#include "LIR.h"

using std::unique_ptr;

class BasicBlock final {
public:
    explicit BasicBlock(string_view name) : name(name) {}

    [[nodiscard]] string getName() const {
        return name;
    }

    enum class LevelOfIR {
        HIGH, MEDIUM, LOW
    };

    [[nodiscard]] string toString(LevelOfIR level) const;

    // HIR Builder Functions

    UnaryHIR *createUnaryHIR(UnaryHIR::Operator op, Variable *dst, Value *src);

    UnaryHIR *createAssignHIR(Variable *dst, Value *src);

    UnaryHIR *createNegHIR(Variable *dst, Value *src);

    UnaryHIR *createNotHIR(Variable *dst, Value *src);

    UnaryHIR *createRevHIR(Variable *dst, Value *src);

    BinaryHIR *createBinaryHIR(BinaryHIR::Operator op, Variable *dst, Value *src1, Value *src2);

    BinaryHIR *createAddHIR(Variable *dst, Value *src1, Value *src2);

    BinaryHIR *createSubHIR(Variable *dst, Value *src1, Value *src2);

    BinaryHIR *createMulHIR(Variable *dst, Value *src1, Value *src2);

    BinaryHIR *createDivHIR(Variable *dst, Value *src1, Value *src2);

    BinaryHIR *createModHIR(Variable *dst, Value *src1, Value *src2);

    BinaryHIR *createAndHIR(Variable *dst, Value *src1, Value *src2);

    BinaryHIR *createOrHIR(Variable *dst, Value *src1, Value *src2);

    BinaryHIR *createXorHIR(Variable *dst, Value *src1, Value *src2);

    BinaryHIR *createLslHIR(Variable *dst, Value *src1, Value *src2);

    BinaryHIR *createLsrHIR(Variable *dst, Value *src1, Value *src2);

    BinaryHIR *createAsrHIR(Variable *dst, Value *src1, Value *src2);

    BinaryHIR *createCmpEqHIR(Variable *dst, Value *src1, Value *src2);

    BinaryHIR *createCmpNeHIR(Variable *dst, Value *src1, Value *src2);

    BinaryHIR *createCmpGtHIR(Variable *dst, Value *src1, Value *src2);

    BinaryHIR *createCmpLtHIR(Variable *dst, Value *src1, Value *src2);

    BinaryHIR *createCmpGeHIR(Variable *dst, Value *src1, Value *src2);

    BinaryHIR *createCmpLeHIR(Variable *dst, Value *src1, Value *src2);

    BinaryHIR *createAddressingHIR(Variable *dst, Value *src1, Value *src2);

    JumpHIR *createJumpHIR(BasicBlock *block);

    BranchHIR *createBranchHIR(Value *cond, BasicBlock *block1, BasicBlock *block2);

    CallHIR *createCallHIR(string_view func_name, vector<Value *> args = {});

    CallHIR *createCallHIR(Variable *ret, string_view func_name, vector<Value *> args = {});

    ReturnHIR *createReturnHIR(Value *val = nullptr);

    PutfHIR *createPutfHIR(string_view format, vector<Value *> args = {});

    // MIR Builder Functions

    UninitializedMIR *createUninitializedMIR(shared_ptr<Type> type, string_view mir_name, size_t id);

    LoadConstantMIR *createLoadConstantMIR(string_view mir_name, size_t id, Constant *src);

    LoadVariableMIR *createLoadVariableMIR(string_view mir_name, size_t id, Variable *src);

    LoadPointerMIR *createLoadPointerMIR(string_view mir_name, size_t id, Assignment *src);

    StoreVariableMIR *createStoreVariableMIR(Variable *dst, Assignment *src);

    StorePointerMIR *createStorePointerMIR(Assignment *dst, Assignment *src);

    MemoryCopyMIR *createMemoryCopyMIR(Assignment *dst, Assignment *src);

    UnaryMIR *createUnaryMIR(UnaryMIR::Operator op, shared_ptr<Type> type,
                             string_view mir_name, size_t id, Assignment *src);

    UnaryMIR *createNegMIR(string_view mir_name, size_t id, Assignment *src);

    UnaryMIR *createNotMIR(string_view mir_name, size_t id, Assignment *src);

    UnaryMIR *createRevMIR(string_view mir_name, size_t id, Assignment *src);

    BinaryMIR *createBinaryMIR(BinaryMIR::Operator op, shared_ptr<Type> type,
                               string_view mir_name, size_t id, Assignment *src1, Assignment *src2);

    BinaryMIR *createAddMIR(string_view mir_name, size_t id, Assignment *src1, Assignment *src2);

    BinaryMIR *createSubMIR(string_view mir_name, size_t id, Assignment *src1, Assignment *src2);

    BinaryMIR *createMulMIR(string_view mir_name, size_t id, Assignment *src1, Assignment *src2);

    BinaryMIR *createDivMIR(string_view mir_name, size_t id, Assignment *src1, Assignment *src2);

    BinaryMIR *createModMIR(string_view mir_name, size_t id, Assignment *src1, Assignment *src2);

    BinaryMIR *createAndMIR(string_view mir_name, size_t id, Assignment *src1, Assignment *src2);

    BinaryMIR *createOrMIR(string_view mir_name, size_t id, Assignment *src1, Assignment *src2);

    BinaryMIR *createXorMIR(string_view mir_name, size_t id, Assignment *src1, Assignment *src2);

    BinaryMIR *createLslMIR(string_view mir_name, size_t id, Assignment *src1, Assignment *src2);

    BinaryMIR *createLsrMIR(string_view mir_name, size_t id, Assignment *src1, Assignment *src2);

    BinaryMIR *createAsrMIR(string_view mir_name, size_t id, Assignment *src1, Assignment *src2);

    BinaryMIR *createCmpEqMIR(string_view mir_name, size_t id, Assignment *src1, Assignment *src2);

    BinaryMIR *createCmpNeMIR(string_view mir_name, size_t id, Assignment *src1, Assignment *src2);

    BinaryMIR *createCmpGtMIR(string_view mir_name, size_t id, Assignment *src1, Assignment *src2);

    BinaryMIR *createCmpLtMIR(string_view mir_name, size_t id, Assignment *src1, Assignment *src2);

    BinaryMIR *createCmpGeMIR(string_view mir_name, size_t id, Assignment *src1, Assignment *src2);

    BinaryMIR *createCmpLeMIR(string_view mir_name, size_t id, Assignment *src1, Assignment *src2);

    ValueAddressingMIR *createValueAddressingMIR(string_view mir_name, size_t id, Value *base);

    ArrayAddressingMIR *createArrayAddressingMIR(string_view mir_name, size_t id, Value *base, Assignment *offset);

    JumpMIR *createJumpMIR(BasicBlock *block);

    BranchMIR *createBranchMIR(Assignment *cond, BasicBlock *block1, BasicBlock *block2);

    CallMIR *createCallMIR(Function *func, vector<Assignment *> args = {});

    CallWithAssignMIR *createCallMIR(string_view mir_name, size_t id,
                                     Function *func, vector<Assignment *> args = {});

    ReturnMIR *createReturnMIR(Assignment *val = nullptr);

    PhiMIR *createPhiMIR(shared_ptr<Type> type, string_view mir_name, size_t id);

    SelectMIR *createSelectMIR(string_view name, size_t id, Assignment *cond, Assignment *src1, Assignment *src2);

    // 没有 LIR Builder Function, 用new构造再用这个添加进去

    template<class T>
    T *addLIR(T *lir) {
        lirTable.emplace_back(lir);
        return lir;
    }

private:
    string name;

public:
    vector<unique_ptr<HIR>> hirTable;
    vector<unique_ptr<MIR>> mirTable;
    vector<unique_ptr<LIR>> lirTable;
};


#endif //COMPILER2021_BASICBLOCK_H
