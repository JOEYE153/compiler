//
// Created by 陈思言 on 2021/6/18.
//

#ifndef COMPILER2021_LIR_H
#define COMPILER2021_LIR_H

#include <utility>
#include <map>
#include <set>
#include <array>
#include <cinttypes>
#include <optional>
#include "Value.h"

using std::optional;

class BasicBlock;

class Function;

// 表示对一个虚拟寄存器赋值，类似MIR里的Assignment类
class RegAssign {
public:
    enum class Format {
        VREG, COLOR, PREG, ALL
    };

public:
    virtual ~RegAssign() = default;

    [[nodiscard]] virtual string getRegString(Format format) const = 0;

    virtual class LIR *castToLIR() { return nullptr; }

protected:
    explicit RegAssign(size_t vReg) : vReg(vReg) {}

public:
    size_t vReg; // 虚拟寄存器编号
};

// 状态寄存器，只有一个物理寄存器，而且不方便保存，因此该类的对象生命周期不可重叠
class StatusRegAssign : public RegAssign {
public:
    explicit StatusRegAssign(size_t vReg) : RegAssign(vReg) {}

    [[nodiscard]] string getRegString(Format format) const override;
};

// 数据寄存器，分为两组，是寄存器分配算法的考虑对象
struct DataRegAssign : public RegAssign {
protected:
    explicit DataRegAssign(size_t vReg) : RegAssign(vReg) {}

public:
    optional<uint8_t> color = std::nullopt; // 寄存器染色算法使用
    optional<uint8_t> pReg = std::nullopt; // 分配到的物理寄存器
};

// 除去pc和sp以外的14个通用寄存器
class CoreRegAssign : public DataRegAssign {
public:
    explicit CoreRegAssign(size_t vReg) : DataRegAssign(vReg) {}

    [[nodiscard]] string getRegString(Format format) const override;
};

enum class NeonForm {
    D, Q
};

// 向量寄存器
class NeonRegAssign : public DataRegAssign {
public:
    explicit NeonRegAssign(size_t vReg, NeonForm form) : DataRegAssign(vReg), form(form) {}

    [[nodiscard]] string getRegString(Format format) const override;

public:
    NeonForm form;
};

// 表示对一个溢出区内存赋值
class MemAssign {
public:
    virtual ~MemAssign() = default;

    [[nodiscard]] virtual string getMemString() const = 0;

    virtual class LIR *castToLIR() { return nullptr; }

protected:
    explicit MemAssign(size_t vMem) : vMem(vMem) {}

public:
    size_t vMem; // 虚拟寄存器编号
    optional<int> offset;
};

class Rematerialization;

class CoreMemAssign : public MemAssign {
public:
    explicit CoreMemAssign(size_t vMem, Rematerialization *rematerialization = nullptr)
            : MemAssign(vMem), rematerialization(rematerialization) {}

    [[nodiscard]] string getMemString() const override;

public:
    std::unique_ptr<Rematerialization> rematerialization;
    bool needStore = false;
};

// 溢出区再物质化函数
class Rematerialization {
public:
    virtual ~Rematerialization() = default;

    virtual CoreRegAssign *operator()(CoreMemAssign *coreMemAssign) const = 0;

protected:
    Rematerialization() = default;
};

class NeonMemAssign : public MemAssign {
public:
    explicit NeonMemAssign(size_t vMem, NeonForm form) : MemAssign(vMem), form(form) {}

    [[nodiscard]] string getMemString() const override;

public:
    NeonForm form;
};

class LIR {
public:
    class Visitor;

    virtual ~LIR() = default;

    [[nodiscard]] virtual string toString(RegAssign::Format format) const = 0;

    virtual void accept(Visitor &visitor) = 0;

    virtual void doReplacement(const std::map<CoreRegAssign *, CoreRegAssign *> &replaceTableCoreReg,
                               const std::map<NeonRegAssign *, NeonRegAssign *> &replaceTableNeonReg,
                               const std::map<StatusRegAssign *, StatusRegAssign *> &replaceTableStatusReg) {}

protected:
    LIR() = default;
};

// 普通ARM指令
class ArmInst : virtual public LIR {
public:
    enum class ShiftOperator {
        LSL, LSR, ASR
    };

    enum class UnaryOperator {
        MOV, MVN
    };

    enum class BinaryOperator {
        AND, EOR, SUB, RSB, ADD, ORR, BIC
    };

    enum class CompareOperator {
        TST, TEQ, CMP, CMN
    };

    static const char *getString(ShiftOperator op);

    static const char *getString(UnaryOperator op);

    static const char *getString(BinaryOperator op);

    static const char *getString(CompareOperator op);

protected:
    ArmInst() = default;
};

// 向量指令
class NeonInst : virtual public LIR {
protected:
    NeonInst() = default;
};

// 伪指令
class PseudoInst : virtual public LIR {
protected:
    PseudoInst() = default;
};

// 可以条件执行的指令
class CondInst : virtual public LIR {
public:
    enum class Cond {
        EQ, NE, CS, CC, MI, PL, VS, VC, HI, LS, GE, LT, GT, LE, AL
    };

    static const char *getCondString(Cond cond);

    static Cond getContrary(Cond cond);

protected:
    CondInst(Cond cond, StatusRegAssign *cpsr, CoreRegAssign *rd)
            : cond(cond), cpsr(cpsr), rd(rd) {}

    void doReplacement(const std::map<CoreRegAssign *, CoreRegAssign *> &replaceTableCoreReg,
                       const std::map<NeonRegAssign *, NeonRegAssign *> &replaceTableNeonReg,
                       const std::map<StatusRegAssign *, StatusRegAssign *> &replaceTableStatusReg) override;

    void addComment(std::stringstream &ss) const;

public:
    Cond cond;
    StatusRegAssign *cpsr; // 该指令使用的虚拟状态寄存器
    CoreRegAssign *rd; // 当该指令不被执行时，rd使用此虚拟数据寄存器赋值，用于保持SSA的特性
};

/*----------------------------------下面是ARM指令----------------------------------*/

// 单源寄存器操作数，可选立即数移位
class UnaryRegInst final : public ArmInst, public CondInst, public CoreRegAssign {
public:
    UnaryRegInst(UnaryOperator op, size_t vReg, CoreRegAssign *rm,
                 ShiftOperator shiftOp = ShiftOperator::LSL, uint8_t shiftImm = 0, Cond cond = Cond::AL,
                 StatusRegAssign *cpsr = nullptr, CoreRegAssign *rd = nullptr)
            : CondInst(cond, cpsr, rd), CoreRegAssign(vReg),
              op(op), shiftOp(shiftOp), rm(rm), shiftImm(shiftImm) {}

    [[nodiscard]] string toString(RegAssign::Format format) const override;

    void accept(Visitor &visitor) override;

    LIR *castToLIR() override { return this; }

    void doReplacement(const std::map<CoreRegAssign *, CoreRegAssign *> &replaceTableCoreReg,
                       const std::map<NeonRegAssign *, NeonRegAssign *> &replaceTableNeonReg,
                       const std::map<StatusRegAssign *, StatusRegAssign *> &replaceTableStatusReg) override;

public:
    UnaryOperator op;
    ShiftOperator shiftOp;
    CoreRegAssign *rm;
    uint8_t shiftImm;
};

// 双源寄存器操作数，可选立即数移位
class BinaryRegInst final : public ArmInst, public CondInst, public CoreRegAssign {
public:
    BinaryRegInst(BinaryOperator op, size_t vReg, CoreRegAssign *rn, CoreRegAssign *rm,
                  ShiftOperator shiftOp = ShiftOperator::LSL, uint8_t shiftImm = 0, Cond cond = Cond::AL,
                  StatusRegAssign *cpsr = nullptr, CoreRegAssign *rd = nullptr)
            : CondInst(cond, cpsr, rd), CoreRegAssign(vReg),
              op(op), shiftOp(shiftOp), rn(rn), rm(rm), shiftImm(shiftImm) {}

    [[nodiscard]] string toString(RegAssign::Format format) const override;

    void accept(Visitor &visitor) override;

    LIR *castToLIR() override { return this; }

    void doReplacement(const std::map<CoreRegAssign *, CoreRegAssign *> &replaceTableCoreReg,
                       const std::map<NeonRegAssign *, NeonRegAssign *> &replaceTableNeonReg,
                       const std::map<StatusRegAssign *, StatusRegAssign *> &replaceTableStatusReg) override;

public:
    BinaryOperator op;
    ShiftOperator shiftOp;
    CoreRegAssign *rn;
    CoreRegAssign *rm;
    uint8_t shiftImm;
};

// 比较指令寄存器操作数，可选立即数移位
class CompareRegInst final : public ArmInst, public CondInst {
public:
    CompareRegInst(CompareOperator op, CoreRegAssign *rn, CoreRegAssign *rm,
                   ShiftOperator shiftOp = ShiftOperator::LSL, uint8_t shiftImm = 0, Cond cond = Cond::AL,
                   StatusRegAssign *cpsr = nullptr)
            : CondInst(cond, cpsr, nullptr),
              op(op), shiftOp(shiftOp), rn(rn), rm(rm), shiftImm(shiftImm) {}

    [[nodiscard]] string toString(RegAssign::Format format) const override;

    void accept(Visitor &visitor) override;

    void doReplacement(const std::map<CoreRegAssign *, CoreRegAssign *> &replaceTableCoreReg,
                       const std::map<NeonRegAssign *, NeonRegAssign *> &replaceTableNeonReg,
                       const std::map<StatusRegAssign *, StatusRegAssign *> &replaceTableStatusReg) override;

public:
    CompareOperator op;
    ShiftOperator shiftOp;
    CoreRegAssign *rn;
    CoreRegAssign *rm;
    uint8_t shiftImm;
};

// 单源寄存器操作数，寄存器移位
class UnaryShiftInst final : public ArmInst, public CondInst, public CoreRegAssign {
public:
    UnaryShiftInst(UnaryOperator op, size_t vReg, CoreRegAssign *rm,
                   ShiftOperator shiftOp, CoreRegAssign *rs, Cond cond = Cond::AL,
                   StatusRegAssign *cpsr = nullptr, CoreRegAssign *rd = nullptr)
            : CondInst(cond, cpsr, rd), CoreRegAssign(vReg),
              op(op), shiftOp(shiftOp), rm(rm), rs(rs) {}

    [[nodiscard]] string toString(RegAssign::Format format) const override;

    void accept(Visitor &visitor) override;

    LIR *castToLIR() override { return this; }

    void doReplacement(const std::map<CoreRegAssign *, CoreRegAssign *> &replaceTableCoreReg,
                       const std::map<NeonRegAssign *, NeonRegAssign *> &replaceTableNeonReg,
                       const std::map<StatusRegAssign *, StatusRegAssign *> &replaceTableStatusReg) override;

public:
    UnaryOperator op;
    ShiftOperator shiftOp;
    CoreRegAssign *rm;
    CoreRegAssign *rs;
};

// 双源寄存器操作数，寄存器移位
class BinaryShiftInst final : public ArmInst, public CondInst, public CoreRegAssign {
public:
    BinaryShiftInst(BinaryOperator op, size_t vReg, CoreRegAssign *rn, CoreRegAssign *rm,
                    ShiftOperator shiftOp, CoreRegAssign *rs, Cond cond = Cond::AL,
                    StatusRegAssign *cpsr = nullptr, CoreRegAssign *rd = nullptr)
            : CondInst(cond, cpsr, rd), CoreRegAssign(vReg),
              op(op), shiftOp(shiftOp), rn(rn), rm(rm), rs(rs) {}

    [[nodiscard]] string toString(RegAssign::Format format) const override;

    void accept(Visitor &visitor) override;

    LIR *castToLIR() override { return this; }

    void doReplacement(const std::map<CoreRegAssign *, CoreRegAssign *> &replaceTableCoreReg,
                       const std::map<NeonRegAssign *, NeonRegAssign *> &replaceTableNeonReg,
                       const std::map<StatusRegAssign *, StatusRegAssign *> &replaceTableStatusReg) override;

public:
    BinaryOperator op;
    ShiftOperator shiftOp;
    CoreRegAssign *rn;
    CoreRegAssign *rm;
    CoreRegAssign *rs;
};

// 比较指令寄存器操作数，寄存器移位
class CompareShiftInst final : public ArmInst, public CondInst {
public:
    CompareShiftInst(CompareOperator op, CoreRegAssign *rn, CoreRegAssign *rm,
                     ShiftOperator shiftOp, CoreRegAssign *rs, Cond cond = Cond::AL,
                     StatusRegAssign *cpsr = nullptr)
            : CondInst(cond, cpsr, nullptr),
              op(op), shiftOp(shiftOp), rn(rn), rm(rm), rs(rs) {}

    [[nodiscard]] string toString(RegAssign::Format format) const override;

    void accept(Visitor &visitor) override;

    void doReplacement(const std::map<CoreRegAssign *, CoreRegAssign *> &replaceTableCoreReg,
                       const std::map<NeonRegAssign *, NeonRegAssign *> &replaceTableNeonReg,
                       const std::map<StatusRegAssign *, StatusRegAssign *> &replaceTableStatusReg) override;

public:
    CompareOperator op;
    ShiftOperator shiftOp;
    CoreRegAssign *rn;
    CoreRegAssign *rm;
    CoreRegAssign *rs;
};

// 单源12位立即数操作数
class UnaryImmInst final : public ArmInst, public CondInst, public CoreRegAssign {
public:
    UnaryImmInst(UnaryOperator op, size_t vReg,
                 uint32_t imm12, Cond cond = Cond::AL,
                 StatusRegAssign *cpsr = nullptr, CoreRegAssign *rd = nullptr)
            : CondInst(cond, cpsr, rd), CoreRegAssign(vReg),
              op(op), imm12(imm12) {}

    [[nodiscard]] string toString(RegAssign::Format format) const override;

    void accept(Visitor &visitor) override;

    LIR *castToLIR() override { return this; }

    void doReplacement(const std::map<CoreRegAssign *, CoreRegAssign *> &replaceTableCoreReg,
                       const std::map<NeonRegAssign *, NeonRegAssign *> &replaceTableNeonReg,
                       const std::map<StatusRegAssign *, StatusRegAssign *> &replaceTableStatusReg) override;

public:
    UnaryOperator op;
    uint32_t imm12;
};

// 双源12位立即数操作数
class BinaryImmInst final : public ArmInst, public CondInst, public CoreRegAssign {
public:
    BinaryImmInst(BinaryOperator op, size_t vReg, CoreRegAssign *rn,
                  uint32_t imm12, Cond cond = Cond::AL,
                  StatusRegAssign *cpsr = nullptr, CoreRegAssign *rd = nullptr)
            : CondInst(cond, cpsr, rd), CoreRegAssign(vReg),
              op(op), rn(rn), imm12(imm12) {}

    [[nodiscard]] string toString(RegAssign::Format format) const override;

    void accept(Visitor &visitor) override;

    LIR *castToLIR() override { return this; }

    void doReplacement(const std::map<CoreRegAssign *, CoreRegAssign *> &replaceTableCoreReg,
                       const std::map<NeonRegAssign *, NeonRegAssign *> &replaceTableNeonReg,
                       const std::map<StatusRegAssign *, StatusRegAssign *> &replaceTableStatusReg) override;

public:
    BinaryOperator op;
    CoreRegAssign *rn;
    uint32_t imm12;
};

// 比较指令12位立即数操作数
class CompareImmInst final : public ArmInst, public CondInst {
public:
    CompareImmInst(CompareOperator op, CoreRegAssign *rn,
                   uint32_t imm12, Cond cond = Cond::AL,
                   StatusRegAssign *cpsr = nullptr)
            : CondInst(cond, cpsr, nullptr),
              op(op), rn(rn), imm12(imm12) {}

    [[nodiscard]] string toString(RegAssign::Format format) const override;

    void accept(Visitor &visitor) override;

    void doReplacement(const std::map<CoreRegAssign *, CoreRegAssign *> &replaceTableCoreReg,
                       const std::map<NeonRegAssign *, NeonRegAssign *> &replaceTableNeonReg,
                       const std::map<StatusRegAssign *, StatusRegAssign *> &replaceTableStatusReg) override;

public:
    CompareOperator op;
    CoreRegAssign *rn;
    uint32_t imm12;
};

// 设置状态寄存器
class SetFlagInst final : public ArmInst, public StatusRegAssign {
public:
    SetFlagInst(size_t vReg, ArmInst *lastInst)
            : StatusRegAssign(vReg), lastInst(lastInst) {}

    [[nodiscard]] string toString(RegAssign::Format format) const override;

    void accept(Visitor &visitor) override;

    LIR *castToLIR() override { return this; }

public:
    ArmInst *lastInst; // 执行这条指令的同时设置状态寄存器
};

// 寄存器变址读内存
class LoadRegInst final : public ArmInst, public CondInst, public CoreRegAssign {
public:
    LoadRegInst(size_t vReg, CoreRegAssign *rn, CoreRegAssign *rm,
                ShiftOperator shiftOp = ShiftOperator::LSL, uint8_t shiftImm = 0,
                bool subIndexing = false, bool postIndexed = false, Cond cond = Cond::AL,
                StatusRegAssign *cpsr = nullptr, CoreRegAssign *rd = nullptr)
            : CondInst(cond, cpsr, rd), CoreRegAssign(vReg),
              shiftOp(shiftOp), rn(rn), rm(rm), shiftImm(shiftImm),
              subIndexing(subIndexing), postIndexed(postIndexed) {}

    [[nodiscard]] string toString(RegAssign::Format format) const override;

    void accept(Visitor &visitor) override;

    LIR *castToLIR() override { return this; }

    void doReplacement(const std::map<CoreRegAssign *, CoreRegAssign *> &replaceTableCoreReg,
                       const std::map<NeonRegAssign *, NeonRegAssign *> &replaceTableNeonReg,
                       const std::map<StatusRegAssign *, StatusRegAssign *> &replaceTableStatusReg) override;

public:
    ShiftOperator shiftOp;
    CoreRegAssign *rn;
    CoreRegAssign *rm;
    uint8_t shiftImm;
    bool subIndexing; // 是否减法寻址
    bool postIndexed; // 为true时表示后自增，读内存的地址不加变址
};

// 立即数变址读内存
class LoadImmInst final : public ArmInst, public CondInst, public CoreRegAssign {
public:
    LoadImmInst(size_t vReg, CoreRegAssign *rn, uint32_t imm12,
                bool subIndexing = false, bool postIndexed = false, Cond cond = Cond::AL,
                StatusRegAssign *cpsr = nullptr, CoreRegAssign *rd = nullptr)
            : CondInst(cond, cpsr, rd), CoreRegAssign(vReg),
              rn(rn), imm12(imm12), subIndexing(subIndexing), postIndexed(postIndexed) {}

    [[nodiscard]] string toString(RegAssign::Format format) const override;

    void accept(Visitor &visitor) override;

    LIR *castToLIR() override { return this; }

    void doReplacement(const std::map<CoreRegAssign *, CoreRegAssign *> &replaceTableCoreReg,
                       const std::map<NeonRegAssign *, NeonRegAssign *> &replaceTableNeonReg,
                       const std::map<StatusRegAssign *, StatusRegAssign *> &replaceTableStatusReg) override;

public:
    CoreRegAssign *rn;
    uint32_t imm12;
    bool subIndexing; // 是否减法寻址
    bool postIndexed; // 为true时表示后自增，读内存的地址不加变址
};

// 寄存器变址写内存
class StoreRegInst final : public ArmInst, public CondInst {
public:
    StoreRegInst(CoreRegAssign *rt, CoreRegAssign *rn, CoreRegAssign *rm,
                 ShiftOperator shiftOp = ShiftOperator::LSL, uint8_t shiftImm = 0,
                 bool subIndexing = false, bool postIndexed = false, Cond cond = Cond::AL,
                 StatusRegAssign *cpsr = nullptr)
            : CondInst(cond, cpsr, nullptr), rt(rt),
              shiftOp(shiftOp), rn(rn), rm(rm), shiftImm(shiftImm),
              subIndexing(subIndexing), postIndexed(postIndexed) {}

    [[nodiscard]] string toString(RegAssign::Format format) const override;

    void accept(Visitor &visitor) override;

    void doReplacement(const std::map<CoreRegAssign *, CoreRegAssign *> &replaceTableCoreReg,
                       const std::map<NeonRegAssign *, NeonRegAssign *> &replaceTableNeonReg,
                       const std::map<StatusRegAssign *, StatusRegAssign *> &replaceTableStatusReg) override;

public:
    ShiftOperator shiftOp;
    CoreRegAssign *rt;
    CoreRegAssign *rn;
    CoreRegAssign *rm;
    uint8_t shiftImm;
    bool subIndexing; // 是否减法寻址
    bool postIndexed; // 为true时表示后自增，读内存的地址不加变址，需要配合WriteBackAddressInst使用
};

// 立即数变址写内存
class StoreImmInst final : public ArmInst, public CondInst {
public:
    StoreImmInst(CoreRegAssign *rt, CoreRegAssign *rn, uint32_t imm12,
                 bool subIndexing = false, bool postIndexed = false, Cond cond = Cond::AL,
                 StatusRegAssign *cpsr = nullptr)
            : CondInst(cond, cpsr, nullptr), rt(rt),
              rn(rn), imm12(imm12), subIndexing(subIndexing), postIndexed(postIndexed) {}

    [[nodiscard]] string toString(RegAssign::Format format) const override;

    void accept(Visitor &visitor) override;

    void doReplacement(const std::map<CoreRegAssign *, CoreRegAssign *> &replaceTableCoreReg,
                       const std::map<NeonRegAssign *, NeonRegAssign *> &replaceTableNeonReg,
                       const std::map<StatusRegAssign *, StatusRegAssign *> &replaceTableStatusReg) override;

public:
    CoreRegAssign *rt;
    CoreRegAssign *rn;
    uint32_t imm12;
    bool subIndexing; // 是否减法寻址
    bool postIndexed; // 为true时表示后自增，读内存的地址不加变址，需要配合WriteBackAddressInst使用
};

// 寻址完成后写回基址寄存器
class WriteBackAddressInst final : public ArmInst, public CoreRegAssign {
public:
    WriteBackAddressInst(size_t vReg, ArmInst *lastInst)
            : CoreRegAssign(vReg), lastInst(lastInst) {}

    [[nodiscard]] string toString(RegAssign::Format format) const override;

    void accept(Visitor &visitor) override;

    LIR *castToLIR() override { return this; }

public:
    ArmInst *lastInst;
};

// 乘法，可选融合加减
class MultiplyInst final : public ArmInst, public CondInst, public CoreRegAssign {
public:
    enum class Operator {
        MUL, MLA, MLS
    };

public:
    MultiplyInst(size_t vReg, CoreRegAssign *rn, CoreRegAssign *rm, Operator op = Operator::MUL,
                 CoreRegAssign *ra = nullptr, Cond cond = Cond::AL,
                 StatusRegAssign *cpsr = nullptr, CoreRegAssign *rd = nullptr)
            : CondInst(cond, cpsr, rd), CoreRegAssign(vReg),
              op(op), rn(rn), rm(rm), ra(ra) {}

    [[nodiscard]] string toString(RegAssign::Format format) const override;

    void accept(Visitor &visitor) override;

    LIR *castToLIR() override { return this; }

    void doReplacement(const std::map<CoreRegAssign *, CoreRegAssign *> &replaceTableCoreReg,
                       const std::map<NeonRegAssign *, NeonRegAssign *> &replaceTableNeonReg,
                       const std::map<StatusRegAssign *, StatusRegAssign *> &replaceTableStatusReg) override;

public:
    Operator op;
    CoreRegAssign *rn;
    CoreRegAssign *rm;
    CoreRegAssign *ra;
};

// 带符号64位乘法取高32位
class Multiply64GetHighInst final : public ArmInst, public CondInst, public CoreRegAssign {
public:
    Multiply64GetHighInst(size_t vReg, CoreRegAssign *rn, CoreRegAssign *rm, Cond cond = Cond::AL,
                          StatusRegAssign *cpsr = nullptr, CoreRegAssign *rd = nullptr)
            : CondInst(cond, cpsr, rd), CoreRegAssign(vReg),
              rn(rn), rm(rm) {}

    [[nodiscard]] string toString(RegAssign::Format format) const override;

    void accept(Visitor &visitor) override;

    LIR *castToLIR() override { return this; }

    void doReplacement(const std::map<CoreRegAssign *, CoreRegAssign *> &replaceTableCoreReg,
                       const std::map<NeonRegAssign *, NeonRegAssign *> &replaceTableNeonReg,
                       const std::map<StatusRegAssign *, StatusRegAssign *> &replaceTableStatusReg) override;

public:
    CoreRegAssign *rn;
    CoreRegAssign *rm;
};

// 带符号除法
class DivideInst final : public ArmInst, public CondInst, public CoreRegAssign {
public:
    DivideInst(size_t vReg, CoreRegAssign *rn, CoreRegAssign *rm, Cond cond = Cond::AL,
               StatusRegAssign *cpsr = nullptr, CoreRegAssign *rd = nullptr)
            : CondInst(cond, cpsr, rd), CoreRegAssign(vReg),
              rn(rn), rm(rm) {}

    [[nodiscard]] string toString(RegAssign::Format format) const override;

    void accept(Visitor &visitor) override;

    LIR *castToLIR() override { return this; }

    void doReplacement(const std::map<CoreRegAssign *, CoreRegAssign *> &replaceTableCoreReg,
                       const std::map<NeonRegAssign *, NeonRegAssign *> &replaceTableNeonReg,
                       const std::map<StatusRegAssign *, StatusRegAssign *> &replaceTableStatusReg) override;

public:
    CoreRegAssign *rn;
    CoreRegAssign *rm;
};

// movw指令，低16位零扩展到32位
class MovwInst final : public ArmInst, public CondInst, public CoreRegAssign {
public:
    MovwInst(size_t vReg, string_view label, Cond cond = Cond::AL,
             StatusRegAssign *cpsr = nullptr, CoreRegAssign *rd = nullptr)
            : CondInst(cond, cpsr, rd), CoreRegAssign(vReg), label(label), imm16(0) {}

    MovwInst(size_t vReg, uint16_t imm16, Cond cond = Cond::AL,
             StatusRegAssign *cpsr = nullptr, CoreRegAssign *rd = nullptr)
            : CondInst(cond, cpsr, rd), CoreRegAssign(vReg), imm16(imm16) {}

    [[nodiscard]] string toString(RegAssign::Format format) const override;

    void accept(Visitor &visitor) override;

    LIR *castToLIR() override { return this; }

    void doReplacement(const std::map<CoreRegAssign *, CoreRegAssign *> &replaceTableCoreReg,
                       const std::map<NeonRegAssign *, NeonRegAssign *> &replaceTableNeonReg,
                       const std::map<StatusRegAssign *, StatusRegAssign *> &replaceTableStatusReg) override;

public:
    string label; // 此项非空表示使用标签低16位，否则使用下面的立即数
    uint16_t imm16;
};

// movt指令，高16位与原先的低16位拼接
class MovtInst final : public ArmInst, public CondInst, public CoreRegAssign {
public:
    MovtInst(size_t vReg, string_view label, CoreRegAssign *rd,
             Cond cond = Cond::AL, StatusRegAssign *cpsr = nullptr)
            : CondInst(cond, cpsr, rd), CoreRegAssign(vReg), label(label), imm16(0) {}

    MovtInst(size_t vReg, uint16_t imm16, CoreRegAssign *rd,
             Cond cond = Cond::AL, StatusRegAssign *cpsr = nullptr)
            : CondInst(cond, cpsr, rd), CoreRegAssign(vReg), imm16(imm16) {}

    [[nodiscard]] string toString(RegAssign::Format format) const override;

    void accept(Visitor &visitor) override;

    LIR *castToLIR() override { return this; }

    void doReplacement(const std::map<CoreRegAssign *, CoreRegAssign *> &replaceTableCoreReg,
                       const std::map<NeonRegAssign *, NeonRegAssign *> &replaceTableNeonReg,
                       const std::map<StatusRegAssign *, StatusRegAssign *> &replaceTableStatusReg) override;

public:
    string label; // 此项非空表示使用标签高16位，否则使用下面的立即数
    uint16_t imm16;
};

// 跳转指令
class BranchInst final : public ArmInst, public CondInst {
public:
    explicit BranchInst(BasicBlock *block, Cond cond = Cond::AL, StatusRegAssign *cpsr = nullptr)
            : CondInst(cond, cpsr, nullptr), block(block) {}

    [[nodiscard]] string toString(RegAssign::Format format) const override;

    void accept(Visitor &visitor) override;

    void doReplacement(const std::map<CoreRegAssign *, CoreRegAssign *> &replaceTableCoreReg,
                       const std::map<NeonRegAssign *, NeonRegAssign *> &replaceTableNeonReg,
                       const std::map<StatusRegAssign *, StatusRegAssign *> &replaceTableStatusReg) override;

public:
    BasicBlock *block;
};

// 带链接跳转指令（函数调用），返回值用伪指令获取
class BranchAndLinkInst : public ArmInst, public CondInst {
public:
    explicit BranchAndLinkInst(Function *func, Cond cond = Cond::AL, StatusRegAssign *cpsr = nullptr)
            : CondInst(cond, cpsr, nullptr), func(func) {}

    [[nodiscard]] string toString(RegAssign::Format format) const override;

    void accept(Visitor &visitor) override;

    void doReplacement(const std::map<CoreRegAssign *, CoreRegAssign *> &replaceTableCoreReg,
                       const std::map<NeonRegAssign *, NeonRegAssign *> &replaceTableNeonReg,
                       const std::map<StatusRegAssign *, StatusRegAssign *> &replaceTableStatusReg) override;

public:
    Function *func;
};

// 用于函数返回（bx lr），或者查表跳转
class BranchAndExchangeInst : public ArmInst, public CondInst {
public:
    explicit BranchAndExchangeInst(CoreRegAssign *rm, Cond cond = Cond::AL, StatusRegAssign *cpsr = nullptr)
            : CondInst(cond, cpsr, nullptr), rm(rm) {}

    [[nodiscard]] string toString(RegAssign::Format format) const override;

    void accept(Visitor &visitor) override;

    void doReplacement(const std::map<CoreRegAssign *, CoreRegAssign *> &replaceTableCoreReg,
                       const std::map<NeonRegAssign *, NeonRegAssign *> &replaceTableNeonReg,
                       const std::map<StatusRegAssign *, StatusRegAssign *> &replaceTableStatusReg) override;

public:
    CoreRegAssign *rm;
};

// 入栈
class PushInst final : public ArmInst {
public:
    explicit PushInst(std::set<int> regList) : regList(std::move(regList)) {}

    [[nodiscard]] string toString(RegAssign::Format format) const override;

    void accept(Visitor &visitor) override;

public:
    std::set<int> regList;
};

// 出栈
class PopInst final : public ArmInst {
public:
    explicit PopInst(std::set<int> regList) : regList(std::move(regList)) {}

    [[nodiscard]] string toString(RegAssign::Format format) const override;

    void accept(Visitor &visitor) override;

public:
    std::set<int> regList;
};

/*----------------------------------下面是伪指令----------------------------------*/

// 加载32位立即数
class LoadImm32LIR final : public PseudoInst, public CoreRegAssign {
public:
    LoadImm32LIR(size_t vReg, uint32_t imm32)
            : CoreRegAssign(vReg), imm32(imm32) {}

    [[nodiscard]] string toString(RegAssign::Format format) const override;

    void accept(Visitor &visitor) override;

    LIR *castToLIR() override { return this; }

public:
    uint32_t imm32;
};

// 取某个变量的基地址，可以是数组，类似MIR
class ValueAddressingLIR final : public PseudoInst, public CoreRegAssign {
public:
    ValueAddressingLIR(size_t vReg, Value *base)
            : CoreRegAssign(vReg), base(base) {}

    [[nodiscard]] string toString(RegAssign::Format format) const override;

    void accept(Visitor &visitor) override;

    LIR *castToLIR() override { return this; }

public:
    Value *base;
};

// 获取形参
class GetArgumentLIR final : public PseudoInst, public CoreRegAssign {
public:
    GetArgumentLIR(size_t vReg, size_t index)
            : CoreRegAssign(vReg), index(index) {}

    [[nodiscard]] string toString(RegAssign::Format format) const override;

    void accept(Visitor &visitor) override;

    LIR *castToLIR() override { return this; }

public:
    size_t index;
};

// 传递实参
class SetArgumentLIR final : public PseudoInst {
public:
    SetArgumentLIR(BranchAndLinkInst *call, CoreRegAssign *src, size_t index)
            : call(call), src(src), index(index) {}

    [[nodiscard]] string toString(RegAssign::Format format) const override;

    void accept(Visitor &visitor) override;

    void doReplacement(const std::map<CoreRegAssign *, CoreRegAssign *> &replaceTableCoreReg,
                       const std::map<NeonRegAssign *, NeonRegAssign *> &replaceTableNeonReg,
                       const std::map<StatusRegAssign *, StatusRegAssign *> &replaceTableStatusReg) override;

public:
    BranchAndLinkInst *call;
    CoreRegAssign *src;
    size_t index;
};

// 获取上一个调用的函数的返回值
class GetReturnValueLIR final : public PseudoInst, public CoreRegAssign {
public:
    GetReturnValueLIR(size_t vReg, BranchAndLinkInst *call)
            : CoreRegAssign(vReg), call(call) {}

    [[nodiscard]] string toString(RegAssign::Format format) const override;

    void accept(Visitor &visitor) override;

    LIR *castToLIR() override { return this; }

public:
    BranchAndLinkInst *call;
};

// 当前函数返回
class ReturnLIR final : public PseudoInst {
public:
    explicit ReturnLIR(CoreRegAssign *val = nullptr) : val(val) {}

    [[nodiscard]] string toString(RegAssign::Format format) const override;

    void accept(Visitor &visitor) override;

    void doReplacement(const std::map<CoreRegAssign *, CoreRegAssign *> &replaceTableCoreReg,
                       const std::map<NeonRegAssign *, NeonRegAssign *> &replaceTableNeonReg,
                       const std::map<StatusRegAssign *, StatusRegAssign *> &replaceTableStatusReg) override;

public:
    CoreRegAssign *val;
};

// 从溢出区读取一个标量
class LoadScalarLIR final : public PseudoInst, public CoreRegAssign {
public:
    LoadScalarLIR(size_t vReg, CoreMemAssign *src)
            : CoreRegAssign(vReg), src(src) {}

    [[nodiscard]] string toString(RegAssign::Format format) const override;

    void accept(Visitor &visitor) override;

    LIR *castToLIR() override { return this; }

public:
    CoreMemAssign *src;
};

// 向溢出区写入一个标量
class StoreScalarLIR final : public PseudoInst, public CoreMemAssign {
public:
    StoreScalarLIR(size_t vMem, CoreRegAssign *src, Rematerialization *rematerialization = nullptr)
            : CoreMemAssign(vMem, rematerialization), src(src) {}

    [[nodiscard]] string toString(RegAssign::Format format) const override;

    void accept(Visitor &visitor) override;

    LIR *castToLIR() override { return this; }

    void doReplacement(const std::map<CoreRegAssign *, CoreRegAssign *> &replaceTableCoreReg,
                       const std::map<NeonRegAssign *, NeonRegAssign *> &replaceTableNeonReg,
                       const std::map<StatusRegAssign *, StatusRegAssign *> &replaceTableStatusReg) override;

public:
    CoreRegAssign *src;
};

// 从溢出区读取一个向量
//class LoadVectorLIR final : public PseudoInst, public NeonRegAssign {
//public:
//    LoadVectorLIR(size_t vReg, NeonMemAssign *src)
//            : NeonRegAssign(vReg, src->form), src(src) {}
//
//    [[nodiscard]] string toString(RegAssign::Format format) const override;
//
//    void accept(Visitor &visitor) override;
//
//    LIR *castToLIR() override { return this; }
//
//public:
//    NeonMemAssign *src;
//};
//
// 向溢出区写入一个向量
//class StoreVectorLIR final : public PseudoInst, public NeonMemAssign {
//public:
//    StoreVectorLIR(size_t vMem, NeonRegAssign *src)
//            : NeonMemAssign(vMem, src->form), src(src) {}
//
//    [[nodiscard]] string toString(RegAssign::Format format) const override;
//
//    void accept(Visitor &visitor) override;
//
//    LIR *castToLIR() override { return this; }
//
//    void doReplacement(const std::map<CoreRegAssign *, CoreRegAssign *> &replaceTableCoreReg,
//                       const std::map<NeonRegAssign *, NeonRegAssign *> &replaceTableNeonReg,
//                       const std::map<StatusRegAssign *, StatusRegAssign *> &replaceTableStatusReg) override;
//
//public:
//    NeonRegAssign *src;
//};

class AtomicLoopCondLIR : public PseudoInst, public CoreRegAssign {
public:
    AtomicLoopCondLIR(size_t vReg, CoreRegAssign *atomic_var_ptr,
                      ArmInst::BinaryOperator update_op, CoreRegAssign *step,
                      CondInst::Cond cond, CoreRegAssign *border, CoreRegAssign *tmp,
                      BasicBlock *body, BasicBlock *exit)
            : CoreRegAssign(vReg),
              atomic_var_ptr(atomic_var_ptr), update_op(update_op), step(step),
              cond(cond), border(border), tmp(tmp), body(body), exit(exit) {}

    [[nodiscard]] string toString(RegAssign::Format format) const override;

    void accept(Visitor &visitor) override;

    LIR *castToLIR() override { return this; }

    void doReplacement(const std::map<CoreRegAssign *, CoreRegAssign *> &replaceTableCoreReg,
                       const std::map<NeonRegAssign *, NeonRegAssign *> &replaceTableNeonReg,
                       const std::map<StatusRegAssign *, StatusRegAssign *> &replaceTableStatusReg) override;

public:
    CoreRegAssign *atomic_var_ptr;
    ArmInst::BinaryOperator update_op; // ADD, SUB
    CoreRegAssign *step;
    CondInst::Cond cond; // 继续循环的条件
    CoreRegAssign *border;
    CoreRegAssign *tmp;
    BasicBlock *body;
    BasicBlock *exit;
};

/*----------------------------------不能直接翻译成指令的IR----------------------------------*/

class UninitializedLIR : public LIR {
};

class UninitializedCoreReg final : public UninitializedLIR, public CoreRegAssign {
public:
    UninitializedCoreReg(size_t vReg) : CoreRegAssign(vReg) {}

    [[nodiscard]] string toString(RegAssign::Format format) const override;

    void accept(Visitor &visitor) override;

    LIR *castToLIR() override { return this; }
};

//class UninitializedNeonReg final : public UninitializedLIR, public NeonRegAssign {
//public:
//    UninitializedNeonReg(size_t vReg, NeonForm form) : NeonRegAssign(vReg, form) {}
//
//    [[nodiscard]] string toString(RegAssign::Format format) const override;
//
//    void accept(Visitor &visitor) override;
//
//    LIR *castToLIR() override { return this; }
//};

class PhiLIR : public LIR {
public:
    [[nodiscard]] virtual size_t getId() const = 0;

    virtual void replaceBlock(BasicBlock *dst, BasicBlock *src) = 0;
};

class CoreRegPhiLIR final : public PhiLIR, public CoreRegAssign {
public:
    CoreRegPhiLIR(size_t vReg) : CoreRegAssign(vReg) {}

    void addIncoming(BasicBlock *block, CoreRegAssign *ssa) {
        incomingTable.insert(std::make_pair(block, ssa));
    }

    [[nodiscard]] size_t getId() const override { return vReg; }

    [[nodiscard]] string toString(RegAssign::Format format) const override;

    void accept(Visitor &visitor) override;

    LIR *castToLIR() override { return this; }

    void doReplacement(const std::map<CoreRegAssign *, CoreRegAssign *> &replaceTableCoreReg,
                       const std::map<NeonRegAssign *, NeonRegAssign *> &replaceTableNeonReg,
                       const std::map<StatusRegAssign *, StatusRegAssign *> &replaceTableStatusReg) override;

    void replaceBlock(BasicBlock *dst, BasicBlock *src) override;

public:
    std::map<BasicBlock *, CoreRegAssign *> incomingTable;
};

//class NeonRegPhiLIR final : public PhiLIR, public NeonRegAssign {
//public:
//    NeonRegPhiLIR(size_t vReg, NeonForm form) : NeonRegAssign(vReg, form) {}
//
//    void addIncoming(BasicBlock *block, NeonRegAssign *ssa) {
//        incomingTable.insert(std::make_pair(block, ssa));
//    }
//
//    [[nodiscard]] size_t getId() const override { return vReg; }
//
//    [[nodiscard]] string toString(RegAssign::Format format) const override;
//
//    void accept(Visitor &visitor) override;
//
//    LIR *castToLIR() override { return this; }
//
//    void doReplacement(const std::map<CoreRegAssign *, CoreRegAssign *> &replaceTableCoreReg,
//                       const std::map<NeonRegAssign *, NeonRegAssign *> &replaceTableNeonReg,
//                       const std::map<StatusRegAssign *, StatusRegAssign *> &replaceTableStatusReg) override;
//
//    void replaceBlock(BasicBlock *dst, BasicBlock *src) override;
//
//public:
//    std::map<BasicBlock *, NeonRegAssign *> incomingTable;
//};

class CoreMemPhiLIR final : public PhiLIR, public CoreMemAssign {
public:
    CoreMemPhiLIR(size_t vReg) : CoreMemAssign(vReg, nullptr) {}

    void addIncoming(BasicBlock *block, CoreMemAssign *ssa) {
        incomingTable.insert(std::make_pair(block, ssa));
    }

    [[nodiscard]] size_t getId() const override { return vMem; }

    [[nodiscard]] string toString(RegAssign::Format format) const override;

    void accept(Visitor &visitor) override;

    LIR *castToLIR() override { return this; }

    void replaceBlock(BasicBlock *dst, BasicBlock *src) override;

public:
    std::map<BasicBlock *, CoreMemAssign *> incomingTable;
};

//class NeonMemPhiLIR final : public PhiLIR, public NeonMemAssign {
//public:
//    NeonMemPhiLIR(size_t vReg, NeonForm form) : NeonMemAssign(vReg, form) {}
//
//    void addIncoming(BasicBlock *block, NeonMemAssign *ssa) {
//        incomingTable.insert(std::make_pair(block, ssa));
//    }
//
//    [[nodiscard]] size_t getId() const override { return vMem; }
//
//    [[nodiscard]] string toString(RegAssign::Format format) const override;
//
//    void accept(Visitor &visitor) override;
//
//    LIR *castToLIR() override { return this; }
//
//    void replaceBlock(BasicBlock *dst, BasicBlock *src) override;
//
//public:
//    std::map<BasicBlock *, NeonMemAssign *> incomingTable;
//};

/*----------------------------------访问者基类----------------------------------*/

struct LIR::Visitor {
    virtual void visit(UnaryRegInst *lir) {}

    virtual void visit(BinaryRegInst *lir) {}

    virtual void visit(CompareRegInst *lir) {}

    virtual void visit(UnaryShiftInst *lir) {}

    virtual void visit(BinaryShiftInst *lir) {}

    virtual void visit(CompareShiftInst *lir) {}

    virtual void visit(UnaryImmInst *lir) {}

    virtual void visit(BinaryImmInst *lir) {}

    virtual void visit(CompareImmInst *lir) {}

    virtual void visit(SetFlagInst *lir) {}

    virtual void visit(LoadRegInst *lir) {}

    virtual void visit(LoadImmInst *lir) {}

    virtual void visit(StoreRegInst *lir) {}

    virtual void visit(StoreImmInst *lir) {}

    virtual void visit(WriteBackAddressInst *lir) {}

    virtual void visit(MultiplyInst *lir) {}

    virtual void visit(Multiply64GetHighInst *lir) {}

    virtual void visit(DivideInst *lir) {}

    virtual void visit(MovwInst *lir) {}

    virtual void visit(MovtInst *lir) {}

    virtual void visit(BranchInst *lir) {}

    virtual void visit(BranchAndLinkInst *lir) {}

    virtual void visit(BranchAndExchangeInst *lir) {}

    virtual void visit(PushInst *lir) {}

    virtual void visit(PopInst *lir) {}

    virtual void visit(LoadImm32LIR *lir) {}

    virtual void visit(ValueAddressingLIR *lir) {}

    virtual void visit(GetArgumentLIR *lir) {}

    virtual void visit(SetArgumentLIR *lir) {}

    virtual void visit(GetReturnValueLIR *lir) {}

    virtual void visit(ReturnLIR *lir) {}

    virtual void visit(LoadScalarLIR *lir) {}

    virtual void visit(StoreScalarLIR *lir) {}

//    virtual void visit(LoadVectorLIR *lir) {}

//    virtual void visit(StoreVectorLIR *lir) {}

    virtual void visit(AtomicLoopCondLIR *lir) {}

    virtual void visit(UninitializedCoreReg *lir) {}

//    virtual void visit(UninitializedNeonReg *lir) {}

    virtual void visit(CoreRegPhiLIR *lir) {}

//    virtual void visit(NeonRegPhiLIR *lir) {}

    virtual void visit(CoreMemPhiLIR *lir) {}

//    virtual void visit(NeonMemPhiLIR *lir) {}
};

#endif //COMPILER2021_LIR_H
