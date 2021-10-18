//
// Created by 陈思言 on 2021/7/27.
//

#include "RegisterColoringPass.h"
#include <array>
#include <algorithm>

RegisterColoringPass::RegisterColoringPass(BasicBlock &bb, AnalyzeBasicBlockPasses &dependency, ostream &out)
        : BasicBlockPass(bb, out),
          analyzeDAGPass(*dependency.analyzeDAGPass),
          analyzeCFGPass(*dependency.analyzeFunctionPasses->analyzeCFGPass),
          inputCoreRegAssign(dynamic_cast<AnalyzeDataFlowPass_LIR *>(
                                     dependency.analyzeFunctionPasses->analyzeDataFlowPass)->dataFlowMapCoreReg[&bb].in),
          inputNeonRegAssign(dynamic_cast<AnalyzeDataFlowPass_LIR *>(
                                     dependency.analyzeFunctionPasses->analyzeDataFlowPass)->dataFlowMapNeonReg[&bb].in),
          outputCoreRegAssign(dynamic_cast<AnalyzeDataFlowPass_LIR *>(
                                      dependency.analyzeFunctionPasses->analyzeDataFlowPass)->dataFlowMapCoreReg[&bb].out),
          outputNeonRegAssign(dynamic_cast<AnalyzeDataFlowPass_LIR *>(
                                      dependency.analyzeFunctionPasses->analyzeDataFlowPass)->dataFlowMapNeonReg[&bb].out) {
    std::set_intersection(inputCoreRegAssign.begin(), inputCoreRegAssign.end(),
                          outputCoreRegAssign.begin(), outputCoreRegAssign.end(),
                          std::inserter(unusedTraverseCoreReg, unusedTraverseCoreReg.end()));
}

bool RegisterColoringPass::run() {
    // 获取DAG，用于调度
    analyzeDAGPass.run();
    currentEdgeMat = analyzeDAGPass.edgeMat;

    // 找出需要着色的IR
    for (size_t i = 0; i < bb.lirTable.size(); i++) {
        auto lir = bb.lirTable[i].get();
        auto phi_lir = dynamic_cast<CoreRegPhiLIR *>(lir);
        if (phi_lir == nullptr) {
            if (dynamic_cast<BranchInst *>(lir) != nullptr || dynamic_cast<ReturnLIR *>(lir) != nullptr) {
                break;
            }
            toColor.push_back(i);
        } else {
            setColored(i); // 认为phi结点已经着色
            if (outputCoreRegAssign.find(phi_lir) != outputCoreRegAssign.end()) {
                unusedOutputCoreRegPhi.insert(phi_lir);
            }
        }
    }
    toColorEnd = toColor.size() + analyzeDAGPass.phiNum;

    // 根据DAG初始化赋值使用集合
    for (size_t i = 0; i < toColorEnd; i++) {
        auto def_lir = dynamic_cast<RegAssign *>(bb.lirTable[i].get());
        if (def_lir != nullptr) {
            for (size_t j = i + 1; j < bb.lirTable.size(); j++) {
                if (analyzeDAGPass.getTypeAt(i, j) & AnalyzeDAGPass::DependencyType::DEF_USE) {
                    auto use_lir = bb.lirTable[j].get();
                    regUseSetMap[def_lir].insert(use_lir);
                }
            }
        }
    }

    // 预调度phi结点
    newSequence.reserve(bb.lirTable.size());
    for (size_t i = 0; i < analyzeDAGPass.phiNum; i++) {
        newSequence.push_back(i);
    }

    // 进行调度和着色
    insertTable.resize(bb.lirTable.size());
    doColoring();

    // 插入溢出指令
    vector<unique_ptr<LIR>> lirTable;
    for (size_t i = 0; i < newSequence.size(); i++) {
        for (auto &new_lir : insertTable[i]) {
            lirTable.emplace_back(std::move(new_lir));
        }
        lirTable.emplace_back(std::move(bb.lirTable[newSequence[i]]));
    }
    lirTable.swap(bb.lirTable);
    return true;
}

void RegisterColoringPass::setColored(size_t i) {
    for (size_t j = i + 1; j < analyzeDAGPass.exprNum; j++) {
        setCurrentEdgeAt(i, j, 0);
    }
}

bool RegisterColoringPass::isReady(size_t j) const {
    for (size_t i = analyzeDAGPass.phiNum; i < j; i++) {
        if (getCurrentEdgeAt(i, j) > 0) {
            return false;
        }
    }
    return true;
}

list<size_t>::iterator RegisterColoringPass::preColorSchedule() {
    return toColor.begin();
}

void RegisterColoringPass::doColoring() {
    struct MyVisitor : LIR::Visitor {
        RegisterColoringPass &pass;
        size_t tick = 1;
        // 当前时刻寄存器是否空闲
        std::array<bool, CORE_REG_NUM> colorCoreRegValidTable = {};
        // 当前时刻寄存器是否冻结
        std::array<bool, CORE_REG_NUM> colorCoreRegFreezeTable = {};
        // 溢出寄存器的代价
        std::array<float, CORE_REG_NUM> spillCostCoreRegTable = {};
        // 保存被溢出的虚拟寄存器和对应的溢出指令
        std::map<size_t, CoreMemAssign *> spilledCoreReg;
        // 用于替换的表
        std::map<CoreRegAssign *, CoreRegAssign *> replaceTableCoreReg;

        explicit MyVisitor(RegisterColoringPass &pass)
                : pass(pass) {
            for (uint8_t i = 0; i < CORE_REG_NUM; i++) {
                if (!pass.colorCoreRegTable[i].empty()) {
                    // 有占位符
                    colorCoreRegValidTable[i] = true;
                }
            }
        }

        // 通过遍历所有寄存器，找到已经保存所需赋值的或代价最小的
        uint8_t findColor(CoreRegAssign *ssa) {
            float min_cost = 1e20f;
            uint8_t best_color = 0xff;
            for (uint8_t i = 0; i < pass.colorCoreRegTable.size(); i++) {
                if (pass.colorCoreRegTable[i].empty()) {
                    // 该操作数可以使用一个空闲寄存器
                    if (dynamic_cast<PhiLIR *>(ssa) != nullptr) {
                        // phi结点优先使用空寄存器
                        return i;
                    }
                    if (pass.inputCoreRegAssign.find(ssa) != pass.inputCoreRegAssign.end()) {
                        // 输入结点优先使用空寄存器
                        return i;
                    }
                    if (min_cost > 0) {
                        min_cost = 0;
                        best_color = i;
                    }
                } else if (!colorCoreRegValidTable[i]) {
                    // 该操作数可以使用被释放的寄存器
                    if (min_cost > 0) {
                        min_cost = 0;
                        best_color = i;
                    }
                } else if (pass.colorCoreRegTable[i].back().ssa == nullptr) {
                    // 此处放置了占位符
                    continue;
                } else {
                    if (pass.colorCoreRegTable[i].back().ssa->vReg == ssa->vReg) {
                        // 该操作数已经在寄存器里了
                        return i;
                    }
                    if (!colorCoreRegFreezeTable[i] && spillCostCoreRegTable[i] < min_cost) {
                        // 该操作数可以抢占溢出代价更小的寄存器
                        min_cost = spillCostCoreRegTable[i];
                        best_color = i;
                    }
                }
            }
            if (min_cost == 1e20f) {
                throw std::logic_error("Coloring failed!");
            }
            return best_color;
        }

        // 为新的赋值申请新的寄存器
        void allocateCoreReg(CoreRegAssign *ssa, uint8_t color) {
//            printf("allocate %zu to %u\n", ssa->vReg, unsigned(color));
            ssa->color = color;
            pass.colorCoreRegTable[color].emplace_back(ssa, tick);
            colorCoreRegValidTable[color] = true;
            if (spilledCoreReg[ssa->vReg] != nullptr) {
                // 之前已经溢出过了
                if (spilledCoreReg[ssa->vReg]->rematerialization != nullptr) {
                    spillCostCoreRegTable[color] = 2.0f;
                } else {
                    spillCostCoreRegTable[color] = 5.0f;
                }
            } else {
                spillCostCoreRegTable[color] = 7.0f;
            }
        }

        // 加载一个已有的赋值
        void loadCoreReg(CoreRegAssign *ssa, CoreRegAssign *load, uint8_t color, size_t inc_tick) {
//            printf("load %zu to %u\n", ssa->vReg, unsigned(color));
            pass.insertTable[pass.newSequence.size()].emplace_back(load->castToLIR());
            replaceTableCoreReg[ssa] = load;
            allocateCoreReg(load, color);
            tick += inc_tick;
        }

        // 获取一个已有的赋值
        void getCoreReg(CoreRegAssign *ssa, uint8_t color) {
//            printf("get %zu to %u\n", ssa->vReg, unsigned(color));
            auto iter = spilledCoreReg.find(ssa->vReg);
            if (iter != spilledCoreReg.end()) {
                // 该赋值是在当前块溢出的
                loadCoreReg(ssa, new LoadScalarLIR(ssa->vReg, iter->second), color, 1);
            } else {
                // 该赋值是来自phi结点或者支配树祖先结点的
                if (pass.colorCoreRegTable[color].empty()) {
                    // 该赋值可以从寄存器输入
                    auto reg_phi_lir = dynamic_cast<CoreRegPhiLIR *>(ssa);
                    if (reg_phi_lir != nullptr) {
                        for (size_t i = 0; i < pass.analyzeDAGPass.phiNum; i++) {
                            if (pass.bb.lirTable[i].get() == reg_phi_lir) {
                                // 是来自phi结点的赋值
                                auto &prevSet = pass.analyzeCFGPass.result[&pass.bb].prev;
                                for (auto prev : prevSet) {
                                    auto vReg = reg_phi_lir->incomingTable[prev]->vReg;
                                    pass.inputCoreReg[vReg].insert(reg_phi_lir);
                                }
                                allocateCoreReg(ssa, color);
                                pass.colorCoreRegTable[color].back().start = 0;
                                pass.unusedOutputCoreRegPhi.erase(reg_phi_lir);
                                return;
                            }
                        }
                    }
                    // 是来自前驱基本块的赋值
                    reg_phi_lir = new CoreRegPhiLIR(ssa->vReg);
                    auto &prevSet = pass.analyzeCFGPass.result[&pass.bb].prev;
                    for (auto prev : prevSet) {
                        reg_phi_lir->addIncoming(prev, reg_phi_lir);
                    }
                    pass.inputCoreReg[ssa->vReg].insert(reg_phi_lir);
                    loadCoreReg(ssa, reg_phi_lir, color, 0);
                    pass.colorCoreRegTable[color].back().start = 0;
                    pass.regUseSetMap[ssa].insert(nullptr); // FAKE
                } else {
                    // 将该赋值标记为从溢出区读取
                    auto reg_phi_lir = dynamic_cast<CoreRegPhiLIR *>(ssa);
                    if (reg_phi_lir != nullptr) {
                        for (size_t i = 0; i < pass.analyzeDAGPass.phiNum; i++) {
                            if (pass.bb.lirTable[i].get() == reg_phi_lir) {
                                // 是来自phi结点的赋值
                                auto &prevSet = pass.analyzeCFGPass.result[&pass.bb].prev;
                                pass.toErase.emplace_back(std::move(pass.bb.lirTable[i]));
                                auto mem_phi_lir = new CoreMemPhiLIR(reg_phi_lir->vReg);
                                for (auto prev : prevSet) {
                                    size_t vReg = reg_phi_lir->incomingTable[prev]->vReg;
                                    pass.inputCoreMem[vReg].insert(mem_phi_lir);
                                    mem_phi_lir->addIncoming(prev, new CoreMemAssign(vReg));
                                }
                                pass.outputCoreMem.emplace(reg_phi_lir->vReg, mem_phi_lir);
                                pass.bb.lirTable[i].reset(mem_phi_lir);
                                auto load_lir = new LoadScalarLIR(ssa->vReg, mem_phi_lir);
                                loadCoreReg(ssa, load_lir, color, 1);
                                spilledCoreReg[ssa->vReg] = mem_phi_lir;
                                return;
                            }
                        }
                    }
                    auto mem_phi_lir = new CoreMemPhiLIR(ssa->vReg);
                    auto &prevSet = pass.analyzeCFGPass.result[&pass.bb].prev;
                    for (auto prev : prevSet) {
                        mem_phi_lir->addIncoming(prev, mem_phi_lir);
                    }
                    pass.inputCoreMem[ssa->vReg].insert(mem_phi_lir);
                    pass.insertTable[pass.newSequence.size()].emplace_back(mem_phi_lir);
                    auto load_lir = new LoadScalarLIR(ssa->vReg, mem_phi_lir);
                    loadCoreReg(ssa, load_lir, color, 1);
                    spilledCoreReg[ssa->vReg] = mem_phi_lir;
                }
            }
        }

        // 释放一个寄存器
        void releaseCoreReg(CoreRegAssign *ssa, uint8_t color) {
//            printf("release %zu from %u\n", ssa->vReg, unsigned(color));
            pass.colorCoreRegTable[color].back().stop = tick;
            colorCoreRegValidTable[color] = false;
        }

        // 尝试释放一个寄存器
        void tryReleaseCoreReg(LIR *lir, CoreRegAssign *ssa, uint8_t color) {
            colorCoreRegFreezeTable[color] = false;
            auto &regUseSet = pass.regUseSetMap[ssa];
            regUseSet.erase(lir);
            if (regUseSet.empty() && pass.outputCoreRegAssign.find(ssa) == pass.outputCoreRegAssign.end()) {
                releaseCoreReg(ssa, color);
            }
        }

        // 溢出一个赋值到内存
        void spillCoreReg(CoreRegAssign *ssa, uint8_t color) {
//            printf("spill %zu from %u\n", ssa->vReg, unsigned(color));
            if (spilledCoreReg[ssa->vReg] != nullptr) {
                // 之前已经溢出过了
                pass.colorCoreRegTable[color].back().stop = tick;
                return;
            }
            Rematerialization *rematerialization = nullptr;
            auto cond_lir = dynamic_cast<CondInst *>(ssa);
            if (cond_lir != nullptr) {
                if (cond_lir->cpsr == nullptr) {
                    auto unary_imm_lir = dynamic_cast<UnaryImmInst *>(ssa);
                    if (unary_imm_lir != nullptr) {
                        rematerialization = new UnaryImmRematerialization(unary_imm_lir->op, unary_imm_lir->imm12);
                    }
                    auto movw_lir = dynamic_cast<MovwInst *>(ssa);
                    if (movw_lir != nullptr) {
                        rematerialization = new MovwRematerialization(movw_lir->imm16);
                    }
                }
            }
            auto spill_lir = new StoreScalarLIR(ssa->vReg, ssa, rematerialization);
            spilledCoreReg[ssa->vReg] = spill_lir;
            pass.insertTable[pass.newSequence.size()].emplace_back(spill_lir);
            pass.colorCoreRegTable[color].back().stop = tick++;
        }

        // 确保一个赋值在寄存器中
        uint8_t prepareCoreReg(CoreRegAssign *ssa) {
            pass.unusedTraverseCoreReg.erase(ssa);
            auto color = findColor(ssa);
            if (!colorCoreRegValidTable[color] ||
                pass.colorCoreRegTable[color].back().ssa == nullptr ||
                pass.colorCoreRegTable[color].back().ssa->vReg != ssa->vReg) {
                if (colorCoreRegValidTable[color]) {
                    // 找到的最合适的寄存器仍然被其他赋值占据，需要溢出
                    spillCoreReg(pass.colorCoreRegTable[color].back().ssa, color);
                }
                getCoreReg(ssa, color);
            }
            colorCoreRegFreezeTable[color] = true;
            return color;
        }

        // 给占位符分配颜色
        uint8_t findColorForPlaceholder(uint8_t arg) {
            float min_cost = 1e20f;
            uint8_t best_color = 0xff;
            for (uint8_t i = 0; i < pass.colorCoreRegTable.size(); i++) {
                if (pass.colorCoreRegTable[i].empty()) {
                    // 该占位符可以使用一个空闲寄存器
                    if (min_cost > 0) {
                        min_cost = 0;
                        best_color = i;
                    }
                } else if (!colorCoreRegValidTable[i]) {
                    // 该占位符可以使用被释放的寄存器
                    if (min_cost > 0) {
                        min_cost = 0;
                        best_color = i;
                    }
                } else if (pass.colorCoreRegTable[i].back().ssa == nullptr) {
                    // 此处放置了占位符
                    if (pass.colorCoreRegTable[i].back().requirePReg == arg) {
                        // 已经有了相同的占位符
                        return i;
                    }
                    // 不能抢占其他占位符
                    continue;
                } else {
                    if (!colorCoreRegFreezeTable[i] && spillCostCoreRegTable[i] < min_cost) {
                        // 该占位符可以抢占溢出代价更小的寄存器
                        min_cost = spillCostCoreRegTable[i];
                        best_color = i;
                    }
                }
            }
            if (min_cost == 1e20f) {
                throw std::logic_error("Coloring failed!");
            }
            return best_color;
        }

        // 准备一个占位符
        uint8_t preparePlaceHolder(uint8_t arg) {
            auto color = findColorForPlaceholder(arg);
            if (!colorCoreRegValidTable[color] || pass.colorCoreRegTable[color].back().requirePReg != arg) {
                if (colorCoreRegValidTable[color]) {
                    // 找到的最合适的寄存器仍然被其他赋值占据，需要溢出
                    spillCoreReg(pass.colorCoreRegTable[color].back().ssa, color);
                }
                pass.colorCoreRegTable[color].emplace_back(nullptr, tick, ~(1 << arg), arg, arg);
                colorCoreRegValidTable[color] = true;
            }
            return color;
        }

        // 释放一个占位符
        void releasePlaceHolder(uint8_t arg) {
            for (uint8_t i = 0; i < CORE_REG_NUM; i++) {
                if (!pass.colorCoreRegTable[i].empty() && pass.colorCoreRegTable[i].back().requirePReg == arg) {
                    pass.colorCoreRegTable[i].back().stop = tick;
                    colorCoreRegValidTable[i] = false;
                    break;
                }
            }
        }

        uint8_t visitCoreRegAssign(CoreRegAssign *lir) {
            auto color = findColor(lir);
            if (colorCoreRegValidTable[color]) {
                spillCoreReg(pass.colorCoreRegTable[color].back().ssa, color);
            }
            allocateCoreReg(lir, color);
            return color;
        }

        void visitCondInst(CondInst *lir) {
            // todo: check
            if (lir->rd != nullptr) {
                auto rd_color = prepareCoreReg(lir->rd);
                tryReleaseCoreReg(lir, lir->rd, rd_color);
            }
        }

        void visit(UnaryRegInst *lir) override {
            auto rm_color = prepareCoreReg(lir->rm);
            visitCondInst(lir);
            tryReleaseCoreReg(lir, lir->rm, rm_color);
            visitCoreRegAssign(lir);
        }

        void visit(BinaryRegInst *lir) override {
            auto rm_color = prepareCoreReg(lir->rm);
            auto rn_color = prepareCoreReg(lir->rn);
            visitCondInst(lir);
            tryReleaseCoreReg(lir, lir->rm, rm_color);
            tryReleaseCoreReg(lir, lir->rn, rn_color);
            visitCoreRegAssign(lir);
        }

        void visit(CompareRegInst *lir) override {
            auto rm_color = prepareCoreReg(lir->rm);
            auto rn_color = prepareCoreReg(lir->rn);
            visitCondInst(lir);
            tryReleaseCoreReg(lir, lir->rm, rm_color);
            tryReleaseCoreReg(lir, lir->rn, rn_color);
        }

        void visit(UnaryShiftInst *lir) override {
            auto rm_color = prepareCoreReg(lir->rm);
            auto rs_color = prepareCoreReg(lir->rs);
            visitCondInst(lir);
            tryReleaseCoreReg(lir, lir->rm, rm_color);
            tryReleaseCoreReg(lir, lir->rs, rs_color);
            visitCoreRegAssign(lir);
        }

        void visit(BinaryShiftInst *lir) override {
            auto rm_color = prepareCoreReg(lir->rm);
            auto rn_color = prepareCoreReg(lir->rn);
            auto rs_color = prepareCoreReg(lir->rs);
            visitCondInst(lir);
            tryReleaseCoreReg(lir, lir->rm, rm_color);
            tryReleaseCoreReg(lir, lir->rn, rn_color);
            tryReleaseCoreReg(lir, lir->rs, rs_color);
            visitCoreRegAssign(lir);
        }

        void visit(CompareShiftInst *lir) override {
            auto rm_color = prepareCoreReg(lir->rm);
            auto rn_color = prepareCoreReg(lir->rn);
            auto rs_color = prepareCoreReg(lir->rs);
            visitCondInst(lir);
            tryReleaseCoreReg(lir, lir->rm, rm_color);
            tryReleaseCoreReg(lir, lir->rn, rn_color);
            tryReleaseCoreReg(lir, lir->rs, rs_color);
        }

        void visit(UnaryImmInst *lir) override {
            visitCondInst(lir);
            visitCoreRegAssign(lir);
        }

        void visit(BinaryImmInst *lir) override {
            auto rn_color = prepareCoreReg(lir->rn);
            visitCondInst(lir);
            tryReleaseCoreReg(lir, lir->rn, rn_color);
            visitCoreRegAssign(lir);
        }

        void visit(CompareImmInst *lir) override {
            auto rn_color = prepareCoreReg(lir->rn);
            visitCondInst(lir);
            tryReleaseCoreReg(lir, lir->rn, rn_color);
        }

        void visit(SetFlagInst *lir) override {
            // todo: check
        }

        void visit(LoadRegInst *lir) override {
            auto rm_color = prepareCoreReg(lir->rm);
            auto rn_color = prepareCoreReg(lir->rn);
            visitCondInst(lir);
            tryReleaseCoreReg(lir, lir->rm, rm_color);
            tryReleaseCoreReg(lir, lir->rn, rn_color);
            visitCoreRegAssign(lir);
        }

        void visit(LoadImmInst *lir) override {
            auto rn_color = prepareCoreReg(lir->rn);
            visitCondInst(lir);
            tryReleaseCoreReg(lir, lir->rn, rn_color);
            visitCoreRegAssign(lir);
        }

        void visit(StoreRegInst *lir) override {
            auto rt_color = prepareCoreReg(lir->rt);
            auto rm_color = prepareCoreReg(lir->rm);
            auto rn_color = prepareCoreReg(lir->rn);
            visitCondInst(lir);
            tryReleaseCoreReg(lir, lir->rt, rt_color);
            tryReleaseCoreReg(lir, lir->rm, rm_color);
            tryReleaseCoreReg(lir, lir->rn, rn_color);
        }

        void visit(StoreImmInst *lir) override {
            auto rt_color = prepareCoreReg(lir->rt);
            auto rn_color = prepareCoreReg(lir->rn);
            visitCondInst(lir);
            tryReleaseCoreReg(lir, lir->rt, rt_color);
            tryReleaseCoreReg(lir, lir->rn, rn_color);
        }

        void visit(WriteBackAddressInst *lir) override {
            visitCoreRegAssign(lir);
        }

        void visit(MultiplyInst *lir) override {
            auto rm_color = prepareCoreReg(lir->rm);
            auto rn_color = prepareCoreReg(lir->rn);
            uint8_t ra_color;
            if (lir->ra != nullptr) {
                ra_color = prepareCoreReg(lir->ra);
            }
            visitCondInst(lir);
            tryReleaseCoreReg(lir, lir->rm, rm_color);
            tryReleaseCoreReg(lir, lir->rn, rn_color);
            if (lir->ra != nullptr) {
                tryReleaseCoreReg(lir, lir->ra, ra_color);
            }
            visitCoreRegAssign(lir);
        }

        void visit(Multiply64GetHighInst *lir) override {
            auto rm_color = prepareCoreReg(lir->rm);
            auto rn_color = prepareCoreReg(lir->rn);
            visitCondInst(lir);
            tryReleaseCoreReg(lir, lir->rm, rm_color);
            tryReleaseCoreReg(lir, lir->rn, rn_color);
            visitCoreRegAssign(lir);
        }

        void visit(DivideInst *lir) override {
            auto rm_color = prepareCoreReg(lir->rm);
            auto rn_color = prepareCoreReg(lir->rn);
            visitCondInst(lir);
            tryReleaseCoreReg(lir, lir->rm, rm_color);
            tryReleaseCoreReg(lir, lir->rn, rn_color);
            visitCoreRegAssign(lir);
        }

        void visit(MovwInst *lir) override {
            visitCondInst(lir);
            visitCoreRegAssign(lir);
        }

        void visit(MovtInst *lir) override {
            visitCondInst(lir);
            visitCoreRegAssign(lir);
        }

        void visit(BranchInst *lir) override {
            visitCondInst(lir);
        }

        void visit(BranchAndLinkInst *lir) override {
            for (uint8_t i : {0, 1, 2, 3, 14}) {
                auto color = preparePlaceHolder(i);
                if (pass.colorCoreRegTable[color].back().placeHolderOrigin == nullptr) {
                    pass.colorCoreRegTable[color].back().placeHolderOrigin = lir;
                }
            }
            visitCondInst(lir);
            for (uint8_t i = 0; i < pass.colorCoreRegTable.size(); i++) {
                if (colorCoreRegValidTable[i]) {
                    pass.colorCoreRegTable[i].back().preferGlobal = true;
                }
            }
            tick++;
            for (uint8_t i : {0, 1, 2, 3, 14}) {
                releasePlaceHolder(i);
            }
        }

        void visit(LoadImm32LIR *lir) override {
            visitCoreRegAssign(lir);
        }

        void visit(ValueAddressingLIR *lir) override {
            visitCoreRegAssign(lir);
        }

        void visit(GetArgumentLIR *lir) override {
            uint8_t my_color = visitCoreRegAssign(lir);
            if (lir->index < 4) {
                pass.colorCoreRegTable[my_color].back().preferPReg |= 1 << lir->index;
                releasePlaceHolder(lir->index);
            }
        }

        void visit(SetArgumentLIR *lir) override {
            auto color_src = prepareCoreReg(lir->src);
            if (lir->index < 4) {
                auto color = preparePlaceHolder(lir->index);
                pass.colorCoreRegTable[color].back().placeHolderOrigin = lir;
                pass.colorCoreRegTable[color_src].back().preferPReg |= 1 << lir->index;
            }
            tryReleaseCoreReg(lir, lir->src, color_src);
            tick++;
        }

        void visit(GetReturnValueLIR *lir) override {
            uint8_t my_color = visitCoreRegAssign(lir);
            pass.colorCoreRegTable[my_color].back().preferPReg |= 1;
        }

        void visit(ReturnLIR *lir) override {
            if (lir->val != nullptr) {
                auto color_val = prepareCoreReg(lir->val);
                tryReleaseCoreReg(lir, lir->val, color_val);
                pass.colorCoreRegTable[color_val].back().preferPReg |= 1;
            }
        }

        void visit(AtomicLoopCondLIR *lir) override {
            auto atomic_var_ptr_color = prepareCoreReg(lir->atomic_var_ptr);
            auto step_color = prepareCoreReg(lir->step);
            auto border_color = prepareCoreReg(lir->border);
            auto tmp_color = prepareCoreReg(lir->border);
            visitCoreRegAssign(lir);
            tryReleaseCoreReg(lir, lir->atomic_var_ptr, atomic_var_ptr_color);
            tryReleaseCoreReg(lir, lir->step, step_color);
            tryReleaseCoreReg(lir, lir->border, border_color);
            tryReleaseCoreReg(lir, lir->border, tmp_color);
        }
    };

    // 每次调度一个合适的IR进行着色
    MyVisitor visitor(*this);
    while (!toColor.empty()) {
        auto iter = preColorSchedule();
        size_t i = *iter;
//        printf("coloring %zu num %zu\n", i, newSequence.size());
        bb.lirTable[i]->accept(visitor);
        visitor.tick++;
        bb.lirTable[i]->doReplacement(visitor.replaceTableCoreReg, {}, {});
//        puts(bb.lirTable[i]->toString(RegAssign::Format::VREG).c_str());
        setColored(i);
        toColor.erase(iter);
        newSequence.push_back(i);
    }

    // 调度跳转或返回指令
    for (size_t i = toColorEnd; i < bb.lirTable.size(); i++) {
//        printf("coloring %zu num %zu\n", i, newSequence.size());
        bb.lirTable[i]->accept(visitor);
        visitor.tick++;
        bb.lirTable[i]->doReplacement(visitor.replaceTableCoreReg, {}, {});
//        puts(bb.lirTable[i]->toString(RegAssign::Format::VREG).c_str());
        setColored(i);
        newSequence.push_back(i);
    }

    // 确定哪些寄存器可以输出
    for (uint8_t i = 0; i < colorCoreRegTable.size(); i++) {
        if (visitor.colorCoreRegValidTable[i]) {
            auto ssa = colorCoreRegTable[i].back().ssa;
            if (ssa == nullptr) {
                continue;
            }
            for (auto out_ssa : outputCoreRegAssign) {
                if (ssa->vReg == out_ssa->vReg) {
                    outputCoreReg.emplace(ssa->vReg, ssa);
                    break;
                }
            }
        }
    }

    // 确定哪些内存赋值可以输出
    for (auto &item : visitor.spilledCoreReg) {
        for (auto out_ssa : outputCoreRegAssign) {
            if (item.first == out_ssa->vReg) {
                outputCoreMem.emplace(item.first, item.second);
                break;
            }
        }
    }

    // 查看一共使用了多少种颜色
    for (uint8_t i = 0; i < CORE_REG_NUM; i++) {
        if (colorCoreRegTable[i].empty()) {
            colorUsed = i;
            break;
        }
    }
//    printf("%s %u\n\n", bb.getName().c_str(), unsigned(colorUsed));
}

void RegisterColoringPass::spillUnusedAssign(const std::map<size_t, std::set<size_t>> &coreRegSpill) {
    // 当前基本块未使用但需要输出的phi结点
    for (auto &lir : bb.lirTable) {
        auto reg_phi_lir = dynamic_cast<CoreRegPhiLIR *>(lir.get());
        if (reg_phi_lir != nullptr && unusedOutputCoreRegPhi.find(reg_phi_lir) != unusedOutputCoreRegPhi.end()) {
            auto &prevSet = analyzeCFGPass.result[&bb].prev;
            if (coreRegSpill.find(reg_phi_lir->vReg) == coreRegSpill.end()) {
                reg_phi_lir->color = colorUsed;
                colorCoreRegTable[colorUsed++].emplace_back(reg_phi_lir, 0);
                for (auto prev : prevSet) {
                    size_t vReg = reg_phi_lir->incomingTable[prev]->vReg;
                    inputCoreReg[vReg].insert(reg_phi_lir);
                }
                outputCoreReg.emplace(reg_phi_lir->vReg, reg_phi_lir);
            } else {
                toErase.emplace_back(std::move(lir));
                auto mem_phi_lir = new CoreMemPhiLIR(reg_phi_lir->vReg);
                for (auto prev : prevSet) {
                    size_t vReg = reg_phi_lir->incomingTable[prev]->vReg;
                    inputCoreMem[vReg].insert(mem_phi_lir);
                    mem_phi_lir->addIncoming(prev, new CoreMemAssign(vReg));
                }
                outputCoreMem.emplace(reg_phi_lir->vReg, mem_phi_lir);
                lir.reset(mem_phi_lir);
            }
        }
    }

    // 当前基本块未使用但需要输出的来自支配树的赋值
    for (auto ssa : unusedTraverseCoreReg) {
        if (unusedOutputCoreRegPhi.find(dynamic_cast<CoreRegPhiLIR *>(ssa)) != unusedOutputCoreRegPhi.end()) {
            continue;
        }
        auto &prevSet = analyzeCFGPass.result[&bb].prev;
        if (coreRegSpill.find(ssa->vReg) == coreRegSpill.end()) {
            auto reg_phi_lir = new CoreRegPhiLIR(ssa->vReg);
            reg_phi_lir->color = colorUsed;
            colorCoreRegTable[colorUsed++].emplace_back(reg_phi_lir, 0);
            for (auto prev : prevSet) {
                reg_phi_lir->addIncoming(prev, reg_phi_lir);
            }
            inputCoreReg[ssa->vReg].insert(reg_phi_lir);
            outputCoreReg.emplace(ssa->vReg, reg_phi_lir);
            bb.lirTable.emplace_back(reg_phi_lir);
        } else {
            auto mem_phi_lir = new CoreMemPhiLIR(ssa->vReg);
            for (auto prev : prevSet) {
                mem_phi_lir->addIncoming(prev, mem_phi_lir);
            }
            inputCoreMem[ssa->vReg].insert(mem_phi_lir);
            outputCoreMem.emplace(ssa->vReg, mem_phi_lir);
            bb.lirTable.emplace_back(mem_phi_lir);
        }
    }
}
