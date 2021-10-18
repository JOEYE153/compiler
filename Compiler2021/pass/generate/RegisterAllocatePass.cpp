//
// Created by 陈思言 on 2021/7/27.
//

#include "RegisterAllocatePass.h"
#include <queue>
#include <algorithm>
#include <stdexcept>

using std::make_unique;
using std::make_move_iterator;
using std::queue;
using std::set_intersection;
using std::logic_error;

bool RegisterAllocatePass::run() {
    // 先对每个基本块进行局部着色，确定每个块需要的寄存器数量，确定块内溢出的虚拟寄存器
    analyzeDataFlowPass.run();
//    analyzeActivityPass.printAssignActivityMap();
//    analyzeActivityPass.printBlockActivityMap();
//    analyzeDataFlowPass.printDataFlowMap();
    for (auto bb : analyzeCFGPass.dfsSequence) {
        AnalyzeFunctionPasses analyzeFunctionPasses;
        analyzeFunctionPasses.analyzeCFGPass = &analyzeCFGPass;
        analyzeFunctionPasses.analyzeActivityPass = &analyzeActivityPass;
        analyzeFunctionPasses.analyzeDataFlowPass = &analyzeDataFlowPass;
        AnalyzeBasicBlockPasses analyzePasses;
        analyzePasses.analyzeFunctionPasses = &analyzeFunctionPasses;
        AnalyzeDAGPass_LIR analyzeDAGPass(*bb, false);
        analyzePasses.analyzeDAGPass = &analyzeDAGPass;
        auto registerColoringPass = new RegisterColoringPass(*bb, analyzePasses);
        if (bb == fn.entryBlock) {
            for (size_t i = 0; i < std::min(fn.args.size(), size_t(4)); i++) {
                registerColoringPass->colorCoreRegTable[i].emplace_back(nullptr, 0, ~(1 << i), i, i);
            }
        }
        registerColoringPass->run();
        registerColoringPasses.emplace(bb, registerColoringPass);
    }

    // 决定基本块内没使用的赋值是否要溢出
    vector<std::map<size_t, std::set<size_t>>> coreRegSpill(analyzeCFGPass.dfsSequence.size());
    for (size_t i = 0; i < analyzeCFGPass.dfsSequence.size(); i++) {
        auto bb = analyzeCFGPass.dfsSequence[i];
        auto &registerColoringPass = registerColoringPasses[bb];
        for (auto ssa : registerColoringPass->unusedOutputCoreRegPhi) {
            std::set<size_t> incomingVReg;
            for (auto &incoming : ssa->incomingTable) {
                incomingVReg.insert(incoming.second->vReg);
            }
            coreRegSpill[i].emplace(ssa->vReg, incomingVReg);
        }
        for (auto ssa : registerColoringPass->unusedTraverseCoreReg) {
            std::set<size_t> incomingVReg = {ssa->vReg};
            coreRegSpill[i].emplace(ssa->vReg, incomingVReg);
        }
    }
    scheduleUnusedVReg(coreRegSpill);

    // 溢出应该溢出的那些赋值
    for (size_t i = 0; i < analyzeCFGPass.dfsSequence.size(); i++) {
        auto bb = analyzeCFGPass.dfsSequence[i];
        auto &registerColoringPass = registerColoringPasses[bb];
        registerColoringPass->spillUnusedAssign(coreRegSpill[i]);
    }

    // phi指令提前
    for (auto bb : analyzeCFGPass.dfsSequence) {
        vector<unique_ptr<LIR>> lirTable;
        vector<unique_ptr<LIR>> phiTable;
        for (auto &lir : bb->lirTable) {
            if (dynamic_cast<PhiLIR *>(lir.get()) != nullptr) {
                phiTable.emplace_back(std::move(lir));
            } else {
                lirTable.emplace_back(std::move(lir));
            }
        }
        bb->lirTable.clear();
        for (auto &lir : phiTable) {
            bb->lirTable.emplace_back(std::move(lir));
        }
        for (auto &lir : lirTable) {
            bb->lirTable.emplace_back(std::move(lir));
        }
    }

    // 进行物理寄存器分配
    for (auto bb : analyzeCFGPass.dfsSequence) {
        allocatePReg(bb);
    }

    // 处理CFG的边
    for (auto bb : analyzeCFGPass.dfsSequence) {
        auto &rearVec = analyzeCFGPass.result[bb].rear;
        for (auto rear : rearVec) {
            dealWithEdge(bb, rear);
        }
    }

    analyzeDataFlowPass.invalidate();
    analyzeDomTreePass.invalidate();
    analyzeCFGPass.invalidate();
    return true;
}

void RegisterAllocatePass::scheduleUnusedVReg(vector<std::map<size_t, std::set<size_t>>> &coreRegSpill) {
    // 收集输入输出信息
    vector<std::set<size_t>> coreRegIn(analyzeCFGPass.dfsSequence.size());
    vector<std::set<size_t>> coreRegOut(analyzeCFGPass.dfsSequence.size());
    vector<uint8_t> num(analyzeCFGPass.dfsSequence.size());
    for (size_t i = 0; i < analyzeCFGPass.dfsSequence.size(); i++) {
        auto bb = analyzeCFGPass.dfsSequence[i];
        auto &registerColoringPass = registerColoringPasses[bb];
        for (auto &item : registerColoringPass->inputCoreReg) {
            coreRegIn[i].insert(item.first);
        }
        for (auto &item : registerColoringPass->outputCoreReg) {
            coreRegOut[i].insert(item.first);
        }
        num[i] = CORE_REG_NUM - registerColoringPass->colorUsed;
        if (!use_complex_alg) {
            for (auto j = 0; j < num[i]; j++) {
                if (coreRegSpill[i].empty()) {
                    break;
                }
                coreRegSpill[i].erase(coreRegSpill[i].begin());
            }
        }
    }
    if (!use_complex_alg) {
        return;
    }

    // 开始迭代式探测哪些变量应该放寄存器里
    auto dfnMap = analyzeCFGPass.getDfnMap();
    vector<vector<vector<std::pair<size_t, size_t>>>> coreRegOutIndirect(analyzeCFGPass.dfsSequence.size());
    RESTART:
    for (auto &indirect : coreRegOutIndirect) {
        indirect.clear();
    }
    // 一次间接
    for (size_t i = 0; i < analyzeCFGPass.dfsSequence.size(); i++) {
        if (coreRegSpill[i].empty() || num[i] == 0) {
            continue;
        }
        auto bb = analyzeCFGPass.dfsSequence[i];
        auto &prevSet = analyzeCFGPass.result[bb].prev;
        auto &rearVec = analyzeCFGPass.result[bb].rear;
        for (auto prev : prevSet) {
            size_t j = dfnMap[prev];
            for (auto vReg : coreRegOut[j]) {
                for (auto &item : coreRegSpill[i]) {
                    if (item.second.find(vReg) != item.second.end()) {
                        coreRegOutIndirect[i] = {{{item.first, i}}};
                    }
                }
            }
        }
        for (auto rear : rearVec) {
            size_t j = dfnMap[rear];
            for (auto &indirect_i : coreRegOutIndirect[i]) {
                auto vReg = indirect_i.back().first;
                if (coreRegIn[j].find(vReg) == coreRegIn[j].end()) {
                    continue;
                }
                for (auto incoming : coreRegSpill[i][vReg]) {
                    coreRegIn[i].insert(incoming);
                }
                coreRegOut[i].insert(vReg);
                coreRegSpill[i].erase(vReg);
                num[i]--;
                goto RESTART;
            }
        }
    }
    // 多次间接
    for (size_t n = 2; n < analyzeCFGPass.dfsSequence.size(); n++) {
        vector<vector<vector<std::pair<size_t, size_t>>>> coreRegOutIndirect2(coreRegOutIndirect.size());
        for (size_t i = 0; i < analyzeCFGPass.dfsSequence.size(); i++) {
            if (coreRegSpill[i].empty() || num[i] == 0) {
                continue;
            }
            auto bb = analyzeCFGPass.dfsSequence[i];
            auto &prevSet = analyzeCFGPass.result[bb].prev;
            auto &rearVec = analyzeCFGPass.result[bb].rear;
            for (auto prev : prevSet) {
                size_t j = dfnMap[prev];
                for (auto &indirect_j : coreRegOutIndirect[j]) {
                    auto vReg = indirect_j.back().first;
                    for (auto &x : indirect_j) {
                        if (x.second == i) {
                            goto SKIP; // 已经成环了
                        }
                    }
                    for (auto &item : coreRegSpill[i]) {
                        if (item.second.find(vReg) != item.second.end()) {
                            coreRegOutIndirect2[i].push_back(indirect_j);
                            coreRegOutIndirect2[i].back().emplace_back(item.first, i);
                        }
                    }
                    SKIP:;
                }
            }
            for (auto rear : rearVec) {
                size_t j = dfnMap[rear];
                for (auto &indirect_i : coreRegOutIndirect2[i]) {
                    auto vReg = indirect_i.back().first;
                    if (coreRegIn[j].find(vReg) == coreRegIn[j].end()) {
                        continue;
                    }
                    for (auto &x : indirect_i) {
                        for (auto incoming : coreRegSpill[x.second][x.first]) {
                            coreRegIn[x.second].insert(incoming);
                        }
                        coreRegOut[x.second].insert(x.first);
                        coreRegSpill[x.second].erase(x.first);
                        num[x.second]--;
                    }
                    goto RESTART;
                }
            }
        }
        coreRegOutIndirect.swap(coreRegOutIndirect2);
        coreRegOutIndirect2.clear();
    }
}

void RegisterAllocatePass::allocatePReg(BasicBlock *bb) {
    auto &registerColoringPass = registerColoringPasses[bb];
    auto &prevSet = analyzeCFGPass.result[bb].prev;
    auto &rearVec = analyzeCFGPass.result[bb].rear;

    // 收集并排序着色信息
    vector<CoreRegColorInfo> colorCoreRegTable;
    for (uint8_t i = 0; i < CORE_REG_NUM; i++) {
        colorCoreRegTable.insert(colorCoreRegTable.end(),
                                 registerColoringPass->colorCoreRegTable[i].begin(),
                                 registerColoringPass->colorCoreRegTable[i].end());
    }
    std::sort(colorCoreRegTable.begin(), colorCoreRegTable.end(),
              [](const CoreRegColorInfo &a, const CoreRegColorInfo &b) {
                  return a.start < b.start;
              });

//    puts("");
//    puts(bb->getName().c_str());
//    for (size_t i = 0; i < colorCoreRegTable.size(); i++) {
//        if (colorCoreRegTable[i].ssa == nullptr) {
//            printf("placeholder %u\n", (unsigned) colorCoreRegTable[i].requirePReg.value());
//        } else {
//            puts(colorCoreRegTable[i].ssa->castToLIR()->toString(RegAssign::Format::ALL).c_str());
//        }
//        printf("%zu %zu %04x\n", colorCoreRegTable[i].start, colorCoreRegTable[i].stop,
//               colorCoreRegTable[i].preferPReg);
//    }

    vector<unique_ptr<LIR>> lirTable;
    size_t vRegIndex = 0;
    size_t colorIndex = 0;

    // 收集需要分配寄存器的phi结点
    std::map<CoreRegPhiLIR *, size_t> coreRegPhiToAllocate;
    while (dynamic_cast<PhiLIR *>(bb->lirTable[vRegIndex].get()) != nullptr) {
        for (size_t i = 0; i < colorCoreRegTable.size(); i++) {
            if (colorCoreRegTable[i].ssa != nullptr &&
                bb->lirTable[vRegIndex].get() == colorCoreRegTable[i].ssa->castToLIR()) {
                coreRegPhiToAllocate.emplace(dynamic_cast<CoreRegPhiLIR *>(bb->lirTable[vRegIndex].get()), i);
                break;
            }
        }
        lirTable.emplace_back(std::move(bb->lirTable[vRegIndex++]));
    }

    // 优先给phi分配和incoming相同的物理寄存器，或者和后继输入相同的寄存器
    std::array<vector<CoreRegColorInfo>, 16> physicalCoreRegTable;
    for (auto &item : coreRegPhiToAllocate) {
        auto core_reg_phi = item.first;
        auto &info = colorCoreRegTable[item.second];
        for (auto prev : prevSet) {
            auto iter = pRegOutputMap[prev].find(core_reg_phi->vReg);
            if (iter != pRegOutputMap[prev].end()) {
                uint8_t pReg = iter->second;
                if (physicalCoreRegTable[pReg].empty()) {
                    info.ssa->pReg = pReg;
                    physicalCoreRegTable[pReg].push_back(info);
                    goto NEXT;
                }
            }
        }
        if (info.stop == 0xffffffffffffffff) {
            for (auto rear : rearVec) {
                auto iter = pRegInputMap[rear].find(info.ssa->vReg);
                if (iter != pRegInputMap[rear].end()) {
                    uint8_t pReg = iter->second;
                    if (physicalCoreRegTable[pReg].empty()) {
                        info.ssa->pReg = pReg;
                        physicalCoreRegTable[pReg].push_back(info);
                        break;
                    }
                }
            }
        }
        NEXT:;
    }

    // 如果incoming都未分配寄存器，则从可用寄存器中选择一个
    static const uint8_t preferGlobalSeq[CORE_REG_NUM] = {4, 5, 6, 7, 8, 9, 10, 11, 3, 2, 1, 0, 14};
    static const uint8_t preferTempSeq[CORE_REG_NUM] = {3, 2, 1, 0, 4, 5, 6, 7, 8, 9, 10, 11, 14};
    for (auto &item : coreRegPhiToAllocate) {
        uint8_t best_pReg = 0xff;
        int cost = 0x7fffffff;
        auto phi_lir = item.first;
        if (phi_lir->pReg.has_value()) {
            continue;
        }
        auto &info = colorCoreRegTable[item.second];
        const uint8_t *seq = info.preferGlobal ? preferGlobalSeq : preferTempSeq;
        for (uint8_t i = 0; i < CORE_REG_NUM; i++) {
            uint8_t pReg = seq[i];
            int new_cost = i - ((info.preferPReg & 1 << pReg) ? 100 : 0);
            if (physicalCoreRegTable[pReg].empty() && new_cost < cost) {
                best_pReg = pReg;
                cost = new_cost;
            }
        }
        info.ssa->pReg = best_pReg;
        physicalCoreRegTable[best_pReg].push_back(info);
    }

    // 给基本块内部分配寄存器
    auto findPReg = [&](const CoreRegColorInfo &info) {
        if (info.stop == 0xffffffffffffffff) {
            // 输出的赋值优先与后继的输入分配相同的寄存器
            for (auto rear : rearVec) {
                auto iter = pRegInputMap[rear].find(info.ssa->vReg);
                if (iter != pRegInputMap[rear].end()) {
                    uint8_t pReg = iter->second;
                    if (physicalCoreRegTable[pReg].empty() || physicalCoreRegTable[pReg].back().stop <= info.start) {
                        info.ssa->pReg = iter->second;
                        physicalCoreRegTable[pReg].push_back(info);
                        return iter->second;
                    }
                }
            }
        }
        uint8_t best_pReg = 0xff;
        int cost = 0x7fffffff;
        const uint8_t *seq = info.preferGlobal ? preferGlobalSeq : preferTempSeq;
        for (uint8_t i = 0; i < CORE_REG_NUM; i++) {
            uint8_t pReg = seq[i];
            int new_cost = i - ((info.preferPReg & 1 << pReg) ? 100 : 0);
            new_cost -= info.start;
            if (!physicalCoreRegTable[pReg].empty()) {
                new_cost += physicalCoreRegTable[pReg].back().stop;
            }
            if ((physicalCoreRegTable[pReg].empty() || physicalCoreRegTable[pReg].back().stop <= info.start) && new_cost < cost) {
                best_pReg = pReg;
                cost = new_cost;
            }
        }
        if (best_pReg == 0xff) {
            throw std::runtime_error("No pReg!!!");
        }
        info.ssa->pReg = best_pReg;
        physicalCoreRegTable[best_pReg].push_back(info);
        return best_pReg;
    };

    auto findPRegAgain = [&](const CoreRegColorInfo &info, size_t start) {
        if (info.stop == 0xffffffffffffffff) {
            // 输出的赋值优先与后继的输入分配相同的寄存器
            for (auto rear : rearVec) {
                auto iter = pRegInputMap[rear].find(info.ssa->vReg);
                if (iter != pRegInputMap[rear].end()) {
                    uint8_t pReg = iter->second;
                    if (pReg >= 4) {
                        if (physicalCoreRegTable[pReg].empty() || physicalCoreRegTable[pReg].back().stop < start) {
                            return iter->second;
                        }
                    }
                }
            }
        }
        for (unsigned char pReg : preferGlobalSeq) {
            if (physicalCoreRegTable[pReg].empty() || physicalCoreRegTable[pReg].back().stop < start) {
                return pReg;
            }
        }
        throw std::runtime_error("No pReg!!!");
    };

    std::map<CoreRegAssign *, CoreRegAssign *> replaceTableCoreReg;
    std::map<CoreRegAssign *, CoreRegAssign *> originTableCoreReg;
    while (vRegIndex < bb->lirTable.size() && colorIndex < colorCoreRegTable.size()) {
        auto &lir = bb->lirTable[vRegIndex];
        auto &info = colorCoreRegTable[colorIndex];
        lir->doReplacement(replaceTableCoreReg, {}, {});

        // 跳过phi，并收集输入的虚拟寄存器与物理寄存器对应关系
        if (dynamic_cast<PhiLIR *>(info.ssa) != nullptr) {
            auto core_reg_phi = dynamic_cast<CoreRegPhiLIR *>(info.ssa);
            if (core_reg_phi != nullptr) {
                for (auto incoming : core_reg_phi->incomingTable) {
                    pRegInputMap[bb].emplace(incoming.second->vReg, core_reg_phi->pReg.value());
                }
            }
            colorIndex++;
            continue;
        }

        // 处理占位符
        if (info.ssa == nullptr) {
            if (info.placeHolderOrigin != nullptr && lir.get() != info.placeHolderOrigin) {
                lirTable.emplace_back(std::move(lir));
                vRegIndex++;
                continue;
            }
            auto requirePReg = info.requirePReg.value();
            if (!physicalCoreRegTable[requirePReg].empty() &&
                physicalCoreRegTable[requirePReg].back().stop > info.start) {
                auto mov_dst = findPRegAgain(physicalCoreRegTable[requirePReg].back(), info.start);
                auto mov_lir = new UnaryRegInst(ArmInst::UnaryOperator::MOV,
                                                physicalCoreRegTable[requirePReg].back().ssa->vReg,
                                                physicalCoreRegTable[requirePReg].back().ssa);
                mov_lir->color = physicalCoreRegTable[requirePReg].back().ssa->color;
                mov_lir->pReg = mov_dst;
                lirTable.emplace_back(mov_lir);
                replaceTableCoreReg[mov_lir->rm] = mov_lir;
                auto iter = originTableCoreReg.find(mov_lir->rm);
                if (iter != originTableCoreReg.end()) {
                    replaceTableCoreReg[iter->second] = mov_lir;
                    originTableCoreReg.emplace(mov_lir, iter->second);
                } else {
                    originTableCoreReg.emplace(mov_lir, mov_lir->rm);
                }
                physicalCoreRegTable[mov_dst].emplace_back(mov_lir, info.start);
                physicalCoreRegTable[mov_dst].back().stop = physicalCoreRegTable[requirePReg].back().stop;
                physicalCoreRegTable[requirePReg].back().stop = info.start;
            }
            physicalCoreRegTable[requirePReg].push_back(info);
            colorIndex++;
            continue;
        }

        // 处理普通赋值
//        puts(bb->lirTable[vRegIndex]->toString(RegAssign::Format::ALL).c_str());
//        printf("%zu %zu\n", info.start, info.stop);
        if (lir.get() != info.ssa->castToLIR()) {
            lirTable.emplace_back(std::move(lir));
            vRegIndex++;
            continue;
        }

        // 条件赋值语句尽量和rd分配同一个寄存器
        auto cond_inst = dynamic_cast<CondInst *>(info.ssa);
        if (cond_inst != nullptr && cond_inst->rd != nullptr) {
            uint8_t pReg = cond_inst->rd->pReg.value();
            if (physicalCoreRegTable[pReg].empty() || physicalCoreRegTable[pReg].back().stop <= info.start) {
                info.ssa->pReg = pReg;
                physicalCoreRegTable[pReg].push_back(info);
//                printf("allocate %u\n", (unsigned) pReg);
            } else {
                auto best_pReg = findPReg(info);
                auto mov_lir = new UnaryRegInst(ArmInst::UnaryOperator::MOV, info.ssa->vReg, cond_inst->rd);
                mov_lir->pReg = best_pReg;
                lirTable.emplace_back(mov_lir);
                cond_inst->rd = mov_lir;
//                printf("allocate %u\n", (unsigned) best_pReg);
            }
        } else {
            auto best_pReg = findPReg(info);
//            printf("allocate %u\n", (unsigned) best_pReg);
        }
        colorIndex++;
    }
    while (vRegIndex < bb->lirTable.size()) {
        bb->lirTable[vRegIndex]->doReplacement(replaceTableCoreReg, {}, {});
        lirTable.emplace_back(std::move(bb->lirTable[vRegIndex++]));
    }
    lirTable.swap(bb->lirTable);

    // 对输出执行替换
    for (auto &item : registerColoringPass->outputCoreReg) {
        auto iter = replaceTableCoreReg.find(item.second);
        if (iter != replaceTableCoreReg.end()) {
            item.second = iter->second;
        }
    }

    // 收集输出的虚拟寄存器与物理寄存器对应关系
    for (auto &item : physicalCoreRegTable) {
        if (!item.empty() && item.back().ssa != nullptr && item.back().stop == 0xffffffffffffffff) {
            pRegOutputMap[bb].emplace(item.back().ssa->vReg, item.back().ssa->pReg.value());
        }
    }
}

//#define PRINT_DEBUG_INFO
void RegisterAllocatePass::dealWithEdge(BasicBlock *prev, BasicBlock *rear) {
    auto &prev_registerColoringPass = registerColoringPasses[prev];
    auto &rear_registerColoringPass = registerColoringPasses[rear];
#ifdef PRINT_DEBUG_INFO
    puts(prev->getName().c_str());
    puts(rear->getName().c_str());
#endif

    // 前驱的输出和后继的输入的交集，是当前边需要传递的赋值
    std::set<CoreRegAssign *> commonCoreRegAssign;
    set_intersection(prev_registerColoringPass->outputCoreRegAssign.begin(),
                     prev_registerColoringPass->outputCoreRegAssign.end(),
                     rear_registerColoringPass->inputCoreRegAssign.begin(),
                     rear_registerColoringPass->inputCoreRegAssign.end(),
                     inserter(commonCoreRegAssign, commonCoreRegAssign.end()));

    // 找到每个赋值对应的输入和输出，分情况处理
    vector<unique_ptr<LIR>> loadTable;
    vector<unique_ptr<LIR>> storeTable;
    std::set<std::pair<CoreRegAssign *, CoreRegPhiLIR *>> toMov;
    for (auto ssa : commonCoreRegAssign) {
        auto &outputCoreReg = prev_registerColoringPass->outputCoreReg;
        CoreRegAssign *out_reg = nullptr;
        for (auto iter = outputCoreReg.begin(); iter != outputCoreReg.end(); iter++) {
            if (iter->first == ssa->vReg) {
                // 该赋值通过寄存器输出
                out_reg = iter->second;
                break;
            }
        }
        auto &outputCoreMem = prev_registerColoringPass->outputCoreMem;
        CoreMemAssign *out_mem = nullptr;
        for (auto iter = outputCoreMem.begin(); iter != outputCoreMem.end(); iter++) {
            if (iter->first == ssa->vReg) {
                // 该赋值通过内存输出
                out_mem = iter->second;
                break;
            }
        }
        auto &inputCoreReg = rear_registerColoringPass->inputCoreReg;
        std::set<CoreRegPhiLIR *> in_regs;
        for (auto iter = inputCoreReg.begin(); iter != inputCoreReg.end(); iter++) {
            for (auto phi_lir : iter->second) {
                auto incoming_iter = phi_lir->incomingTable.find(prev);
                if (incoming_iter != phi_lir->incomingTable.end()) {
                    if (incoming_iter->second->vReg == ssa->vReg) {
                        // 该赋值通过寄存器输入，且经由phi结点
                        in_regs.insert(phi_lir);
                    }
                }
            }
        }
        auto &inputCoreMem = rear_registerColoringPass->inputCoreMem;
        std::set<CoreMemPhiLIR *> in_mems;
        for (auto iter = inputCoreMem.begin(); iter != inputCoreMem.end(); iter++) {
            for (auto phi_lir : iter->second) {
                auto incoming_iter = phi_lir->incomingTable.find(prev);
                if (incoming_iter != phi_lir->incomingTable.end()) {
                    if (incoming_iter->second->vMem == ssa->vReg) {
                        // 该赋值通过内存输入，且经由phi结点
                        in_mems.insert(phi_lir);
                    }
                }
            }
        }
#ifdef PRINT_DEBUG_INFO
        if (out_reg != nullptr) {
            puts("out_reg");
            puts(out_reg->castToLIR()->toString(RegAssign::Format::ALL).c_str());
        }
        if (out_mem != nullptr) {
            puts("out_mem");
            puts(out_mem->castToLIR()->toString(RegAssign::Format::ALL).c_str());
        }
        if (!in_regs.empty()) {
            puts("in_reg");
            for (auto &in_reg : in_regs) {
                puts(in_reg->castToLIR()->toString(RegAssign::Format::ALL).c_str());
            }
        }
        if (!in_mems.empty()) {
            puts("in_mem");
            for (auto &in_mem : in_mems) {
                puts(in_mem->castToLIR()->toString(RegAssign::Format::ALL).c_str());
            }
        }
        puts("");
#endif
        if (!in_regs.empty()) {
            for (auto &in_reg : in_regs) {
                if (out_reg != nullptr) {
                    // 寄存器到寄存器
                    if (out_reg->pReg.has_value() && in_reg->pReg.has_value()) {
                        if (out_reg->pReg.value() == in_reg->pReg.value()) {
                            auto phi_lir = in_reg;
                            auto incoming_iter = phi_lir->incomingTable.find(prev);
                            if (incoming_iter != phi_lir->incomingTable.end()) {
                                incoming_iter->second = out_reg;
                            }
                        } else {
                            toMov.emplace(out_reg, in_reg);
                        }
                    } else {
                        puts(out_reg->castToLIR()->toString(RegAssign::Format::ALL).c_str());
                        puts(in_reg->castToLIR()->toString(RegAssign::Format::ALL).c_str());
                        throw logic_error("No pReg!");
                    }
                } else if (out_mem != nullptr) {
                    // 内存到寄存器
                    auto load_lir = new LoadScalarLIR(out_mem->vMem, out_mem);
                    load_lir->pReg = in_reg->pReg;
                    loadTable.emplace_back(load_lir);
                    auto incoming_iter = in_reg->incomingTable.find(prev);
                    if (incoming_iter != in_reg->incomingTable.end()) {
                        incoming_iter->second = load_lir;
                    }
                }
            }
        }
        if (!in_mems.empty()) {
            for (auto &in_mem : in_mems) {
                if (out_mem != nullptr) {
                    // 内存到内存
                    auto incoming_iter = in_mem->incomingTable.find(prev);
                    if (incoming_iter != in_mem->incomingTable.end()) {
                        if (incoming_iter->second->castToLIR() == nullptr) {
                            delete incoming_iter->second; // 删掉占位符
                        }
                        incoming_iter->second = out_mem;
                    }
                } else if (out_reg != nullptr) {
                    // 寄存器到内存
                    auto store_lir = new StoreScalarLIR(out_reg->vReg, out_reg);
                    storeTable.emplace_back(store_lir);
                    auto incoming_iter = in_mem->incomingTable.find(prev);
                    if (incoming_iter != in_mem->incomingTable.end()) {
                        if (incoming_iter->second->castToLIR() == nullptr) {
                            delete incoming_iter->second; // 删掉占位符
                        }
                        incoming_iter->second = store_lir;
                    }
                }
            }
        }
    }

    // 插入辅助指令
    vector<unique_ptr<LIR>> lirTable;
    lirTable.reserve(storeTable.size() + loadTable.size() + toMov.size());
    for (auto &lir : storeTable) {
        lirTable.emplace_back(std::move(lir));
    }

    std::array<uint32_t, 16> usedPReg = {
            0, 0, 0, 0,
            0, 0, 0, 0,
            0, 0, 0, 0,
            0, 0xffffffff, 0, 0xffffffff
    };
    for (auto &item : toMov) {
        usedPReg[item.first->pReg.value()]++;
#ifdef PRINT_DEBUG_INFO
        printf("%u <- %u\n", (unsigned) item.second->pReg.value(), (unsigned) item.first->pReg.value());
#endif
    }
    UnaryRegInst *mov_r12_src = nullptr;
    UnaryRegInst *mov_dst_r12 = nullptr;
    while (!toMov.empty()) {
        for (auto &item : toMov) {
            auto src = item.first;
            auto dst = item.second;
            if (usedPReg[dst->pReg.value()] == 0) {
                usedPReg[dst->pReg.value()] = 1;
                auto mov_lir = new UnaryRegInst(ArmInst::UnaryOperator::MOV, src->vReg, src);
                mov_lir->pReg = dst->pReg;
                lirTable.emplace_back(mov_lir);
                auto incoming_iter = dst->incomingTable.find(prev);
                if (incoming_iter != dst->incomingTable.end()) {
                    incoming_iter->second = mov_lir;
                }
                usedPReg[src->pReg.value()]--;
                toMov.erase(item);
                goto NEXT_STEP;
            }
        }
        for (auto &item : toMov) {
            auto src = item.first;
            auto dst = item.second;
            if (usedPReg[dst->pReg.value()] == 1) {
                // swap
                if (usedPReg[12] == 0) {
                    usedPReg[12] = 1;
                } else {
                    lirTable.emplace_back(mov_dst_r12);
                }
                mov_r12_src = new UnaryRegInst(ArmInst::UnaryOperator::MOV, src->vReg, src);
                mov_r12_src->pReg = 12;
                mov_dst_r12 = new UnaryRegInst(ArmInst::UnaryOperator::MOV, dst->vReg, mov_r12_src);
                mov_dst_r12->pReg = dst->pReg;
                lirTable.emplace_back(mov_r12_src);
                usedPReg[src->pReg.value()]--;
                toMov.erase(item);
                goto NEXT_STEP;
            }
        }
        throw logic_error("swap failed!");
        NEXT_STEP:;
    }
    if (usedPReg[12] != 0) {
        lirTable.emplace_back(mov_dst_r12);
    }
    for (auto &lir : loadTable) {
        lirTable.emplace_back(std::move(lir));
    }

    // 如果添加了辅助指令，修改CFG
    if (!lirTable.empty()) {
        auto bb = fn.createBasicBlock(prev->getName() + string("_to_") + rear->getName());

        // 修改前驱的跳转目标
        for (auto &lir : prev->lirTable) {
            auto branch_lir = dynamic_cast<BranchInst *>(lir.get());
            if (branch_lir != nullptr && branch_lir->block == rear) {
                branch_lir->block = bb;
            }
        }

        // 修改后继phi的输入
        for (auto &lir : rear->lirTable) {
            auto phi_lir = dynamic_cast<PhiLIR *>(lir.get());
            if (phi_lir != nullptr) {
                phi_lir->replaceBlock(bb, prev);
            }
        }

        // 添加到后继的跳转指令
        lirTable.emplace_back(new BranchInst(rear));
        bb->lirTable.swap(lirTable);
    }
}
