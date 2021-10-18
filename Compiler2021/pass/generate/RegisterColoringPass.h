//
// Created by 陈思言 on 2021/7/27.
//

#ifndef COMPILER2021_REGISTERCOLORINGPASS_H
#define COMPILER2021_REGISTERCOLORINGPASS_H

#include "../analyze/AnalyzeDAGPass.h"
#include "../analyze/AnalyzeDataFlowPass.h"

struct CoreRegColorInfo {
    CoreRegColorInfo(CoreRegAssign *ssa, size_t start, uint16_t preferPReg = 0, bool preferGlobal = false,
                     optional<uint8_t> requirePReg = std::nullopt)
            : ssa(ssa), start(start), stop(0xffffffffffffffff),
              preferPReg(preferPReg), preferGlobal(preferGlobal), requirePReg(requirePReg) {}

    CoreRegAssign *ssa;
    size_t start;
    size_t stop;
    uint16_t preferPReg;
    bool preferGlobal;
    optional<uint8_t> requirePReg;
    LIR *placeHolderOrigin = nullptr;
};

class UnaryImmRematerialization : public Rematerialization {
public:
    UnaryImmRematerialization(UnaryImmInst::UnaryOperator op, uint32_t imm12)
            : op(op), imm12(imm12) {}

    CoreRegAssign *operator()(CoreMemAssign *coreMemAssign) const override {
        return new UnaryImmInst(op, coreMemAssign->vMem, imm12);
    }

private:
    UnaryImmInst::UnaryOperator op;
    uint32_t imm12;
};

class MovwRematerialization : public Rematerialization {
public:
    MovwRematerialization(uint16_t imm16) : imm16(imm16) {}

    CoreRegAssign *operator()(CoreMemAssign *coreMemAssign) const override {
        return new MovwInst(coreMemAssign->vMem, imm16);
    }

private:
    UnaryImmInst::UnaryOperator op;
    uint16_t imm16;
};

constexpr uint8_t CORE_REG_NUM = 13;

class RegisterColoringPass : public BasicBlockPass {
public:
    RegisterColoringPass(BasicBlock &bb, AnalyzeBasicBlockPasses &dependency, ostream &out = null_out);

    bool run() override;

    void spillUnusedAssign(const std::map<size_t, std::set<size_t>> &toSpill);

private:
    void setCurrentEdgeAt(size_t i, size_t j, int val) {
        currentEdgeMat[i * analyzeDAGPass.exprNum + j] = val;
    }

    [[nodiscard]] int getCurrentEdgeAt(size_t i, size_t j) const {
        return currentEdgeMat[i * analyzeDAGPass.exprNum + j];
    }

    void setColored(size_t i);

    [[nodiscard]] bool isReady(size_t j) const;

    list<size_t>::iterator preColorSchedule();

    void doColoring();

public:
    // 从活跃变量分析中获取的信息
    const std::set<CoreRegAssign *> &inputCoreRegAssign;
    const std::set<CoreRegAssign *> &outputCoreRegAssign;
    const std::set<NeonRegAssign *> &inputNeonRegAssign;
    const std::set<NeonRegAssign *> &outputNeonRegAssign;
    // 保存每个时刻的着色信息
    std::array<vector<CoreRegColorInfo>, CORE_REG_NUM> colorCoreRegTable;
    // 输入的寄存器
    std::map<size_t, std::set<CoreRegPhiLIR *>> inputCoreReg;
    // 输出的寄存器
    std::map<size_t, CoreRegAssign *> outputCoreReg;
    // 输入的溢出区
    std::map<size_t, std::set<CoreMemPhiLIR *>> inputCoreMem;
    // 输出的溢出区
    std::map<size_t, CoreMemAssign *> outputCoreMem;
    // 记录每个位置要插入的辅助IR
    vector<vector<unique_ptr<LIR>>> insertTable;
    // 当前基本块未使用的phi
    std::set<CoreRegPhiLIR *> unusedOutputCoreRegPhi;
    // 当前基本块未使用的输入
    std::set<CoreRegAssign *> unusedTraverseCoreReg;
    // 着色共使用多少种颜色
    uint8_t colorUsed = CORE_REG_NUM;

private:
    AnalyzeDAGPass &analyzeDAGPass;
    AnalyzeCFGPass &analyzeCFGPass;
    vector<int> currentEdgeMat; // 随着色进行更新，已经着色的结点连接的边将被删除
    std::map<RegAssign *, std::set<LIR *>> regUseSetMap; // 记录每个赋值被谁使用
    list<size_t> toColor;
    size_t toColorEnd = 0;
    vector<float> rearCost; // 小于等于0，后继数量越多、越近，调度的收益越大
    vector<size_t> newSequence; // 调度后的IR序列
    vector<unique_ptr<LIR>> toErase;
};


#endif //COMPILER2021_REGISTERCOLORINGPASS_H
