//
// Created by 陈思言 on 2021/6/4.
//

#ifndef COMPILER2021_MIR_H
#define COMPILER2021_MIR_H

#include <utility>
#include <map>
#include <set>

#include "Value.h"

class BasicBlock;

class Function;

class MIR {
public:
    class Visitor;

    virtual ~MIR() = default;

    [[nodiscard]] virtual string toString() const = 0;

    virtual void accept(Visitor &visitor) = 0;

    virtual void doReplacement(const std::map<Assignment *, Assignment *> &replaceTable) {}

protected:
    MIR() = default;
};

// 表示可以使用任意值，用于未初始化的局部变量
class UninitializedMIR final : public MIR, public Assignment {
public:
    UninitializedMIR(shared_ptr<Type> type, string_view name, size_t id)
            : Assignment(std::move(type), name, id) {}

    [[nodiscard]] string toString() const override;

    void accept(Visitor &visitor) override;

    MIR *castToMIR() override { return this; }
};

class LoadConstantMIR final : public MIR, public Assignment {
public:
    LoadConstantMIR(shared_ptr<Type> type, string_view name, size_t id, Constant *src)
            : Assignment(std::move(type), name, id), src(src) {}

    [[nodiscard]] string toString() const override;

    void accept(Visitor &visitor) override;

    MIR *castToMIR() override { return this; }

public:
    Constant *src;
};

class LoadVariableMIR final : public MIR, public Assignment {
public:
    LoadVariableMIR(shared_ptr<Type> type, string_view name, size_t id, Variable *src)
            : Assignment(std::move(type), name, id), src(src) {}

    [[nodiscard]] string toString() const override;

    void accept(Visitor &visitor) override;

    MIR *castToMIR() override { return this; }

public:
    Variable *src;
};

// 根据指针值读取内存
class LoadPointerMIR final : public MIR, public Assignment {
public:
    LoadPointerMIR(shared_ptr<Type> type, string_view name, size_t id, Assignment *src)
            : Assignment(std::move(type), name, id), src(src) {}

    [[nodiscard]] string toString() const override;

    void accept(Visitor &visitor) override;

    MIR *castToMIR() override { return this; }

    void doReplacement(const std::map<Assignment *, Assignment *> &replaceTable) override;

public:
    Assignment *src;
};

class StoreVariableMIR final : public MIR {
public:
    StoreVariableMIR(Variable *dst, Assignment *src)
            : dst(dst), src(src) {}

    [[nodiscard]] string toString() const override;

    void accept(Visitor &visitor) override;

    void doReplacement(const std::map<Assignment *, Assignment *> &replaceTable) override;

public:
    Variable *dst;
    Assignment *src;
};

// 根据指针值写入内存
class StorePointerMIR final : public MIR {
public:
    StorePointerMIR(Assignment *dst, Assignment *src)
            : dst(dst), src(src) {}

    [[nodiscard]] string toString() const override;

    void accept(Visitor &visitor) override;

    void doReplacement(const std::map<Assignment *, Assignment *> &replaceTable) override;

public:
    Assignment *dst;
    Assignment *src;
};

// 参数dst是指向数组的指针，src是被广播的值，表示数组填充
class MemoryFillMIR final : public MIR {
public:
    MemoryFillMIR(Assignment *dst, Assignment *src)
            : dst(dst), src(src) {}

    [[nodiscard]] string toString() const override;

    void accept(Visitor &visitor) override;

    void doReplacement(const std::map<Assignment *, Assignment *> &replaceTable) override;

public:
    Assignment *dst;
    Assignment *src;
};

// 参数dst和src是指向数组的指针，表示数组拷贝
class MemoryCopyMIR final : public MIR {
public:
    MemoryCopyMIR(Assignment *dst, Assignment *src)
            : dst(dst), src(src) {}

    [[nodiscard]] string toString() const override;

    void accept(Visitor &visitor) override;

    void doReplacement(const std::map<Assignment *, Assignment *> &replaceTable) override;

public:
    Assignment *dst;
    Assignment *src;
};

class UnaryMIR final : public MIR, public Assignment {
public:
    enum class Operator {
        NEG, NOT, REV
    };

    UnaryMIR(Operator op, shared_ptr<Type> type, string_view name, size_t id, Assignment *src)
            : op(op), Assignment(std::move(type), name, id), src(src) {}

    [[nodiscard]] string toString() const override;

    void accept(Visitor &visitor) override;

    MIR *castToMIR() override { return this; }

    void doReplacement(const std::map<Assignment *, Assignment *> &replaceTable) override;

public:
    Operator op;
    Assignment *src;
};

class BinaryMIR final : public MIR, public Assignment {
public:
    enum class Operator {
        ADD, SUB, MUL, DIV, MOD, AND, OR, XOR, LSL, LSR, ASR,
        CMP_EQ, CMP_NE, CMP_GT, CMP_LT, CMP_GE, CMP_LE,
    };

    BinaryMIR(Operator op, shared_ptr<Type> type, string_view name, size_t id, Assignment *src1, Assignment *src2)
            : op(op), Assignment(std::move(type), name, id), src1(src1), src2(src2) {}

    [[nodiscard]] string toString() const override;

    void accept(Visitor &visitor) override;

    MIR *castToMIR() override { return this; }

    void doReplacement(const std::map<Assignment *, Assignment *> &replaceTable) override;

public:
    Operator op;
    Assignment *src1;
    Assignment *src2;
};

// 取某个变量的基地址，可以是数组
class ValueAddressingMIR final : public MIR, public Assignment {
public:
    ValueAddressingMIR(shared_ptr<Type> type, string_view name, size_t id, Value *base)
            : Assignment(std::move(type), name, id), base(base) {}

    [[nodiscard]] string toString() const override;

    void accept(Visitor &visitor) override;

    MIR *castToMIR() override { return this; }

public:
    Value *base;
};

class ArrayAddressingMIR final : public MIR, public Assignment {
public:
    ArrayAddressingMIR(shared_ptr<Type> type, string_view name, size_t id, Value *base, Assignment *offset)
            : Assignment(std::move(type), name, id), base(base), offset(offset) {}

    [[nodiscard]] string toString() const override;

    void accept(Visitor &visitor) override;

    MIR *castToMIR() override { return this; }

    void doReplacement(const std::map<Assignment *, Assignment *> &replaceTable) override;

public:
    Value *base; // 根据type字段，或者使用 dynamic_cast<Assignment *> 来判断是否为指向数组的指针
    Assignment *offset;
};

class SelectMIR final : public MIR, public Assignment {
public:
    SelectMIR(shared_ptr<Type> type, string_view name, size_t id, Assignment *cond, Assignment *src1, Assignment *src2)
            : Assignment(std::move(type), name, id), cond(cond), src1(src1), src2(src2) {}

    [[nodiscard]] string toString() const override;

    void accept(Visitor &visitor) override;

    MIR *castToMIR() override { return this; }

    void doReplacement(const std::map<Assignment *, Assignment *> &replaceTable) override;

public:
    Assignment *cond;
    Assignment *src1;
    Assignment *src2;
};

class JumpMIR final : public MIR {
public:
    explicit JumpMIR(BasicBlock *block) : block(block) {}

    [[nodiscard]] string toString() const override;

    void accept(Visitor &visitor) override;

public:
    BasicBlock *block;
};

class BranchMIR final : public MIR {
public:
    BranchMIR(Assignment *cond, BasicBlock *block1, BasicBlock *block2)
            : cond(cond), block1(block1), block2(block2) {}

    [[nodiscard]] string toString() const override;

    void accept(Visitor &visitor) override;

    void doReplacement(const std::map<Assignment *, Assignment *> &replaceTable) override;

public:
    Assignment *cond;
    BasicBlock *block1;
    BasicBlock *block2;
};

class CallMIR : public MIR {
public:
    explicit CallMIR(Function *func, vector<Assignment *> args = {})
            : func(func), args(std::move(args)) {}

    [[nodiscard]] string toString() const override;

    void accept(Visitor &visitor) override;

    void doReplacement(const std::map<Assignment *, Assignment *> &replaceTable) override;

public:
    Function *func;
    vector<Assignment *> args;
};

class CallWithAssignMIR final : public CallMIR, public Assignment {
public:
    explicit CallWithAssignMIR(shared_ptr<Type> type, string_view name, size_t id,
                               Function *func, vector<Assignment *> args = {})
            : CallMIR(func, std::move(args)), Assignment(std::move(type), name, id) {}

    [[nodiscard]] string toString() const override;

    MIR *castToMIR() override { return this; }

    void accept(Visitor &visitor) override;
};

class MultiCallMIR : public CallMIR {
public:
    explicit MultiCallMIR(Function *func, vector<Assignment *> args, Variable *atomic_var = nullptr, int thread_num = 3)
            : CallMIR(func, std::move(args)), atomic_var(atomic_var), thread_num(thread_num) {}

    [[nodiscard]] string toString() const override;

    void accept(Visitor &visitor) override;

public:
    Variable *atomic_var;
    int thread_num;
};

class ReturnMIR final : public MIR {
public:
    explicit ReturnMIR(Assignment *val = nullptr) : val(val) {}

    [[nodiscard]] string toString() const override;

    void accept(Visitor &visitor) override;

    void doReplacement(const std::map<Assignment *, Assignment *> &replaceTable) override;

public:
    Assignment *val;
};

class PhiMIR final : public MIR, public Assignment {
public:
    PhiMIR(shared_ptr<Type> type, string_view name, size_t id)
            : Assignment(std::move(type), name, id) {}

    [[nodiscard]] string toString() const override;

    void accept(Visitor &visitor) override;

    MIR *castToMIR() override { return this; }

    void doReplacement(const std::map<Assignment *, Assignment *> &replaceTable) override;

    void addIncoming(BasicBlock *block, Assignment *ssa) {
        incomingTable.insert(std::make_pair(block, ssa));
    }

public:
    std::map<BasicBlock *, Assignment *> incomingTable;
};

//  if (!compare_op(atomic_var, border))
//      goto exit;
//  induction = atomic_var;
//  atomic_var = update_op(atomic_var, step);
//  goto body;
class AtomicLoopCondMIR : public MIR, public Assignment {
public:
    AtomicLoopCondMIR(string_view name, size_t id, Variable *atomic_var,
                      BinaryMIR::Operator update_op, Assignment *step,
                      BinaryMIR::Operator compare_op, Assignment *border,
                      BasicBlock *body, BasicBlock *exit)
            : Assignment(AtomicIntegerType::object, name, id),
              atomic_var(atomic_var), update_op(update_op), step(step),
              compare_op(compare_op), border(border), body(body), exit(exit) {}

    [[nodiscard]] string toString() const override;

    void accept(Visitor &visitor) override;

    MIR *castToMIR() override { return this; }

    void doReplacement(const std::map<Assignment *, Assignment *> &replaceTable) override;

public:
    Variable *atomic_var;
    BinaryMIR::Operator update_op; // ADD, SUB
    Assignment *step;
    BinaryMIR::Operator compare_op;
    Assignment *border;
    BasicBlock *body;
    BasicBlock *exit;
};

struct MIR::Visitor {
    virtual void visit(UninitializedMIR *mir) {}

    virtual void visit(LoadConstantMIR *mir) {}

    virtual void visit(LoadVariableMIR *mir) {}

    virtual void visit(LoadPointerMIR *mir) {}

    virtual void visit(StoreVariableMIR *mir) {}

    virtual void visit(StorePointerMIR *mir) {}

    virtual void visit(MemoryFillMIR *mir) {}

    virtual void visit(MemoryCopyMIR *mir) {}

    virtual void visit(UnaryMIR *mir) {}

    virtual void visit(BinaryMIR *mir) {}

    virtual void visit(ValueAddressingMIR *mir) {}

    virtual void visit(ArrayAddressingMIR *mir) {}

    virtual void visit(SelectMIR *mir) {}

    virtual void visit(JumpMIR *mir) {}

    virtual void visit(BranchMIR *mir) {}

    virtual void visit(CallMIR *mir) {}

    virtual void visit(CallWithAssignMIR *mir) {}

    virtual void visit(MultiCallMIR *mir) {}

    virtual void visit(ReturnMIR *mir) {}

    virtual void visit(PhiMIR *mir) {}

    virtual void visit(AtomicLoopCondMIR *mir) {}
};

#endif //COMPILER2021_MIR_H
