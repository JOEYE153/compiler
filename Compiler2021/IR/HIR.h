//
// Created by 陈思言 on 2021/5/22.
//

#ifndef COMPILER2021_HIR_H
#define COMPILER2021_HIR_H

#include <utility>
#include <set>

#include "Value.h"

class BasicBlock;

class HIR {
public:
    class Visitor;

    virtual ~HIR() = default;

    [[nodiscard]] virtual string toString() const = 0;

    virtual void accept(Visitor &visitor) = 0;

protected:
    HIR() = default;
};

class UnaryHIR final : public HIR {
public:
    enum class Operator {
        ASSIGN, // auto dst = src;
        NEG, // int dst = -src;
        NOT, // bool dst = !src;
        REV, // int dst = ~src;
    };

    UnaryHIR(Operator op, Variable *dst, Value *src)
            : op(op), dst(dst), src(src) {}

    [[nodiscard]] string toString() const override;

    void accept(Visitor &visitor) override;

public:
    Operator op;
    Variable *dst;
    Value *src;
};

class BinaryHIR final : public HIR {
public:
    enum class Operator {
        ADD, // int dst = src1 + src2;
        SUB, // int dst = src1 - src2;
        MUL, // int dst = src1 * src2;
        DIV, // int dst = src1 / src2;
        MOD, // int dst = src1 % src2;
        AND, // int dst = src1 & src2;
        OR, // int dst = src1 | src2;
        XOR, // int dst = src1 ^ src2;
        LSL, // int dst = src1 << src2;
        LSR, // int dst = src1 >> src2;（逻辑右移）
        ASR, // int dst = src1 >>> src2;（算术右移）
        CMP_EQ, // bool dst = src1 == src2;
        CMP_NE, // bool dst = src1 != src2;
        CMP_GT, // bool dst = src1 > src2;
        CMP_LT, // bool dst = src1 < src2;
        CMP_GE, // bool dst = src1 >= src2;
        CMP_LE, // bool dst = src1 <= src2;
        ADDRESSING, // auto &dst = src1[src2];（dst是引用类型）
    };

    BinaryHIR(Operator op, Variable *dst, Value *src1, Value *src2)
            : op(op), dst(dst), src1(src1), src2(src2) {}

    [[nodiscard]] string toString() const override;

    void accept(Visitor &visitor) override;

public:
    Operator op;
    Variable *dst;
    Value *src1;
    Value *src2;
};

class JumpHIR final : public HIR {
public:
    explicit JumpHIR(BasicBlock *block) : block(block) {}

    [[nodiscard]] string toString() const override;

    void accept(Visitor &visitor) override;

public:
    BasicBlock *block;
};

class BranchHIR final : public HIR {
public:
    BranchHIR(Value *cond, BasicBlock *block1, BasicBlock *block2)
            : cond(cond), block1(block1), block2(block2) {}

    [[nodiscard]] string toString() const override;

    void accept(Visitor &visitor) override;

public:
    Value *cond;
    BasicBlock *block1;
    BasicBlock *block2;
};

class CallHIR final : public HIR {
public:
    explicit CallHIR(string_view func_name, vector<Value *> args = {})
            : ret(nullptr), func_name(func_name), args(std::move(args)) {}

    CallHIR(Variable *ret, string_view func_name, vector<Value *> args = {})
            : ret(ret), func_name(func_name), args(std::move(args)) {}

    [[nodiscard]] string toString() const override;

    void accept(Visitor &visitor) override;

public:
    Variable *ret;
    string func_name;
    vector<Value *> args;
};

class ReturnHIR final : public HIR {
public:
    explicit ReturnHIR(Value *val = nullptr) : val(val) {}

    [[nodiscard]] string toString() const override;

    void accept(Visitor &visitor) override;

public:
    Value *val;
};

class PutfHIR final : public HIR {
public:
    explicit PutfHIR(string_view format, vector<Value *> args = {})
            : format(format), args(std::move(args)) {}

    [[nodiscard]] string toString() const override;

    void accept(Visitor &visitor) override;

public:
    string format;
    vector<Value *> args;
};

struct HIR::Visitor {
    virtual void visit(UnaryHIR *hir) {}

    virtual void visit(BinaryHIR *hir) {}

    virtual void visit(JumpHIR *hir) {}

    virtual void visit(BranchHIR *hir) {}

    virtual void visit(CallHIR *hir) {}

    virtual void visit(ReturnHIR *hir) {}

    virtual void visit(PutfHIR *hir) {}
};

#endif //COMPILER2021_HIR_H
