//
// Created by 陈思言 on 2021/7/20.
//

#include "MIR2LIRPass.h"
#include "../../utils/AssemblyUtils.h"
#include "../parallel/AnalyzeParallelFunctionPass.h"
#include <stdexcept>

using std::logic_error;
using std::make_pair;
using std::make_shared;

bool MIR2LIRPass::run() {
    auto functionTable = md.getFunctionVecDictOrder();
    AnalyzeModulePasses analyzeModulePasses = buildModulePass(&md);
    for (auto fn : functionTable) {
        if (fn->isExternal) {
            continue;
        }
        AnalyzeFunctionPasses analyzeFunctionPasses = buildFunctionPass(fn, &md, &analyzeModulePasses);
        AnalyzeParallelFunctionPass analyzeParallelFunctionPass(*fn, analyzeFunctionPasses, out);
        analyzeParallelFunctionPass.run();
        vReg = 0;
        mapCoreRegAssign.clear();
        mapNeonRegAssign.clear();
        callSyncTimeVecMap.clear();
        callSyncTimeVecMap.swap(analyzeParallelFunctionPass.callSyncTimeVecMap);
        dfnMap = analyzeFunctionPasses.analyzeCFGPass->getDfnMap();
        analyzeFunctionPasses.analyzeLoopPass->run();
        loopIn = analyzeFunctionPasses.analyzeLoopPass->loopIn;
        loops = analyzeFunctionPasses.analyzeLoopPass->loops;
        if (fn->hasAtomicVar) {
            fn->atomic_var_ptr = fn->entryBlock->addLIR(new GetArgumentLIR(15271139, fn->args.size() - 1));
        }
        runOnNode(*fn, analyzeFunctionPasses.analyzeDomTreePass->root, {}, {}, {});
        for (auto bb : analyzeFunctionPasses.analyzeCFGPass->dfsSequence) {
            addPhiIncoming(bb);
        }
        for (auto bb : analyzeFunctionPasses.analyzeCFGPass->dfsSequence) {
            bb->mirTable.clear();
        }
    }
    if (md.useThreadPool) {
        auto main_fn = md.getFunctionByName("main");
        for (auto bb : main_fn->getBasicBlockVecDictOrder()) {
            if (bb == main_fn->entryBlock) {
                bb->lirTable.emplace(
                        bb->lirTable.begin(),
                        new BranchAndLinkInst(md.getFunctionByName("initThreadPool")));
            }
            if (dynamic_cast<ReturnLIR *>(bb->lirTable.back().get()) != nullptr) {
                bb->lirTable.emplace(
                        bb->lirTable.end() - 1,
                        new BranchAndLinkInst(md.getFunctionByName("deleteThreadPool")));
            }
        }
    }
    return true;
}

void MIR2LIRPass::runOnNode(Function &fn, DomTreeNode *node, std::set<DivideInst *> divideCache,
                            std::map<std::pair<CoreRegAssign *, int>, CoreRegAssign *> fastDivideCache,
                            std::set<ValueAddressingLIR *> valueAddressingCache) {
    struct MyVisitor : MIR::Visitor {
        MIR2LIRPass &pass;
        Function &fn;
        BasicBlock &bb;
        CondInst::Cond cpsr_cond = CondInst::Cond::AL;
        std::pair<Assignment *, StatusRegAssign *> lastStatusRegAssign = make_pair(nullptr, nullptr);
        std::map<Assignment *, CoreRegAssign *> &mapCoreRegAssign;
        std::map<Assignment *, NeonRegAssign *> &mapNeonRegAssign;
        std::set<DivideInst *> &divideCache;
        std::map<std::pair<CoreRegAssign *, int>, CoreRegAssign *> &fastDivideCache;
        std::set<ValueAddressingLIR *> &valueAddressingCache;
        vector<std::pair<size_t, size_t>> &callSyncTimeVec;
        size_t index = 0;

        MyVisitor(MIR2LIRPass &pass, Function &fn, BasicBlock &bb, std::set<DivideInst *> &divideCache,
                  std::map<std::pair<CoreRegAssign *, int>, CoreRegAssign *> &fastDivideCache,
                  std::set<ValueAddressingLIR *> &valueAddressingCache,
                  vector<std::pair<size_t, size_t>> &callSyncTimeVec)
                : pass(pass), fn(fn), bb(bb), divideCache(divideCache), fastDivideCache(fastDivideCache),
                  mapCoreRegAssign(pass.mapCoreRegAssign), mapNeonRegAssign(pass.mapNeonRegAssign),
                  valueAddressingCache(valueAddressingCache), callSyncTimeVec(callSyncTimeVec) {}

        CoreRegAssign *movImm(int imm) {
            return bb.addLIR(new UnaryImmInst(ArmInst::UnaryOperator::MOV, pass.vReg++, imm));
        }

        CoreRegAssign *movInvalidImm(int imm) {
            // invalid immediate in data processing instruction
            if (imm < 0 || imm > 0x0000ffff) return bb.addLIR(new LoadImm32LIR(pass.vReg++, imm));
            return bb.addLIR(new MovwInst(pass.vReg++, imm & 0xffff));
        }

        CoreRegAssign *loadImm(int imm) {
            if (checkImmediate(imm)) {
                return movImm(imm);
            }
            if (imm < 0 && checkImmediate(~imm)) {
                return bb.addLIR(new UnaryImmInst(ArmInst::UnaryOperator::MVN, pass.vReg++, ~imm));
            }
            return movInvalidImm(imm);
        }

        void compareMIR2LIR(BinaryMIR *mir, CondInst::Cond cond) {
            // 此处既设置了cpsr又将bool值赋给了通用寄存器，可根据实际需求使用，未使用的赋值会被优化掉
            auto op_lir = optimizeImmCompareOperation(mir);
            auto flag_lir = bb.addLIR(new SetFlagInst(pass.vReg++, op_lir));
            cpsr_cond = cond;
            lastStatusRegAssign = make_pair(mir, flag_lir);
            auto bool_lir = bb.addLIR(new UnaryImmInst(ArmInst::UnaryOperator::MOV,
                                                       pass.vReg++, 0));
            mapCoreRegAssign[mir] = bb.addLIR(
                    new UnaryImmInst(ArmInst::UnaryOperator::MOV,
                                     pass.vReg++, 1,
                                     cpsr_cond, flag_lir, bool_lir));
        }

        ArmInst *optimizeImmCompareOperation(BinaryMIR *mir) {
            auto src1 = dynamic_cast<LoadConstantMIR *>(mir->src1);
            auto src2 = dynamic_cast<LoadConstantMIR *>(mir->src2);
            if (src1 != nullptr && src2 == nullptr && checkImmediate(src1->src->getValue<int>())) {
                return bb.addLIR(new CompareImmInst(ArmInst::CompareOperator::CMP,
                                                    mapCoreRegAssign[mir->src2], src1->src->getValue<int>()));
            } else if (src1 == nullptr && src2 != nullptr && checkImmediate(src2->src->getValue<int>())) {
                return bb.addLIR(new CompareImmInst(ArmInst::CompareOperator::CMP,
                                                    mapCoreRegAssign[mir->src1], src2->src->getValue<int>()));
            } else {
                return bb.addLIR(new CompareRegInst(ArmInst::CompareOperator::CMP,
                                                    mapCoreRegAssign[mir->src1], mapCoreRegAssign[mir->src2]));
            }
        }

        CoreRegAssign *optimizeImmBinaryOperation(BinaryMIR *mir, ArmInst::BinaryOperator op) {
            auto src1 = dynamic_cast<LoadConstantMIR *>(mir->src1);
            auto src2 = dynamic_cast<LoadConstantMIR *>(mir->src2);
            if (src1 != nullptr && src2 != nullptr) {
                int x = src1->src->getValue<int>();
                int y = src2->src->getValue<int>();
                int imm = x + y;
                if (op == ArmInst::BinaryOperator::SUB) imm = x - y;
                else if (op == ArmInst::BinaryOperator::AND) imm = x & y;
                else if (op == ArmInst::BinaryOperator::ORR) imm = x | y;
                else if (op == ArmInst::BinaryOperator::EOR) imm = x ^ y;
//                if (checkImmediate(imm)) return movImm(imm);
//                else return movInvalidImm(imm);
                return loadImm(imm);
            } else if (src1 != nullptr) {
                int imm = src1->src->getValue<int>();
                if (!checkImmediate(imm))
                    return bb.addLIR(new BinaryRegInst(op, pass.vReg++,
                                                       mapCoreRegAssign[mir->src1], mapCoreRegAssign[mir->src2]));
                if (op == ArmInst::BinaryOperator::SUB) {
                    return bb.addLIR(new BinaryImmInst(ArmInst::BinaryOperator::RSB, pass.vReg++,
                                                       mapCoreRegAssign[mir->src2], imm));
                }
                return bb.addLIR(new BinaryImmInst(op, pass.vReg++, mapCoreRegAssign[mir->src2], imm));
            } else if (src2 != nullptr) {
                int imm = src2->src->getValue<int>();
                if (!checkImmediate(imm))
                    return bb.addLIR(new BinaryRegInst(op, pass.vReg++,
                                                       mapCoreRegAssign[mir->src1], mapCoreRegAssign[mir->src2]));
                return bb.addLIR(new BinaryImmInst(op, pass.vReg++, mapCoreRegAssign[mir->src1], imm));
            } else {
                return bb.addLIR(new BinaryRegInst(op, pass.vReg++,
                                                   mapCoreRegAssign[mir->src1], mapCoreRegAssign[mir->src2]));
            }
        }

        CoreRegAssign *optimizeImmShiftOperation(BinaryMIR *mir, ArmInst::ShiftOperator op) {
            auto src1 = dynamic_cast<LoadConstantMIR *>(mir->src1);
            auto src2 = dynamic_cast<LoadConstantMIR *>(mir->src2);
            if (src1 != nullptr && src2 != nullptr) {
                int x = src1->src->getValue<int>();
                int y = src2->src->getValue<int>();
                int imm = x << y;
                if (op == ArmInst::ShiftOperator::LSR) imm = (int) ((unsigned) x >> y);
                else if (op == ArmInst::ShiftOperator::ASR) imm = x >> y;
//                if (checkImmediate(imm)) return movImm(imm);
//                else return movInvalidImm(imm);
                return loadImm(imm);
            } else if (src2 != nullptr) {
                int imm = src2->src->getValue<int>();
                if (!checkImmediate(imm))
                    return bb.addLIR(new UnaryShiftInst(ArmInst::UnaryOperator::MOV, pass.vReg++,
                                                        mapCoreRegAssign[mir->src1], op, mapCoreRegAssign[mir->src2]));
                return bb.addLIR(new UnaryRegInst(ArmInst::UnaryOperator::MOV, pass.vReg++, mapCoreRegAssign[mir->src1],
                                                  op, imm));
            } else {
                return bb.addLIR(new UnaryShiftInst(ArmInst::UnaryOperator::MOV, pass.vReg++,
                                                    mapCoreRegAssign[mir->src1], op, mapCoreRegAssign[mir->src2]));
            }
        }

        CoreRegAssign *multiplyConstant(int x, CoreRegAssign *y) {
            // useless immediate load will be deleted
            uint32_t absX = abs(x);
            if ((x >> 31) != 0) {
                // x is negative
                if (x == -1) return bb.addLIR(new BinaryImmInst(ArmInst::BinaryOperator::RSB, pass.vReg++, y, 0));
                if (isPowerOfTwo(absX)) {
                    auto shift = bb.addLIR(new UnaryRegInst(ArmInst::UnaryOperator::MOV, pass.vReg++, y,
                                                            ArmInst::ShiftOperator::LSL, floorLog(absX)));
                    return bb.addLIR(new BinaryImmInst(ArmInst::BinaryOperator::RSB, pass.vReg++, shift, 0));
                } else if (isPowerOfTwo(abs(x + 1))) {
                    // x = - (abs(x+1) + 1)
                    auto big = bb.addLIR(new BinaryRegInst(ArmInst::BinaryOperator::ADD, pass.vReg++, y, y,
                                                           ArmInst::ShiftOperator::LSL, floorLog(abs(x + 1))));
                    return bb.addLIR(new BinaryImmInst(ArmInst::BinaryOperator::RSB, pass.vReg++, big, 0));
                } else if (isPowerOfTwo(abs(x - 1))) {
                    // x = - (abs(x-1) - 1)
                    auto small = bb.addLIR(new BinaryRegInst(ArmInst::BinaryOperator::RSB, pass.vReg++, y, y,
                                                             ArmInst::ShiftOperator::LSL, floorLog(abs(x - 1))));
                    return bb.addLIR(new BinaryImmInst(ArmInst::BinaryOperator::RSB, pass.vReg++, small, 0));
                } else {
                    return bb.addLIR(new MultiplyInst(pass.vReg++, loadImm(x), y));
                }
            }
            // x is positive
            if (x % 2 == 0) {
                if (x == 0) return movImm(0);
                if (isPowerOfTwo(absX))
                    return bb.addLIR(new UnaryRegInst(ArmInst::UnaryOperator::MOV, pass.vReg++, y,
                                                      ArmInst::ShiftOperator::LSL, floorLog(absX)));
                int ceilPower = findCeilPowerOfTwo(x);
                int right = ceilPower - x;
                int left = x - ceilPower / 2;
                if (isPowerOfTwo(right)) {
                    // x = ceil - right
                    auto big = bb.addLIR(new UnaryRegInst(ArmInst::UnaryOperator::MOV, pass.vReg++, y,
                                                          ArmInst::ShiftOperator::LSL, floorLog(ceilPower)));
                    return bb.addLIR(new BinaryRegInst(ArmInst::BinaryOperator::SUB, pass.vReg++, big, y,
                                                       ArmInst::ShiftOperator::LSL, floorLog(right)));
                } else if (isPowerOfTwo(left)) {
                    // x = ceil/2 + left
                    auto small = bb.addLIR(new UnaryRegInst(ArmInst::UnaryOperator::MOV, pass.vReg++, y,
                                                            ArmInst::ShiftOperator::LSL, floorLog(ceilPower / 2)));
                    return bb.addLIR(new BinaryRegInst(ArmInst::BinaryOperator::ADD, pass.vReg++, small, y,
                                                       ArmInst::ShiftOperator::LSL, floorLog(left)));
                } else {
                    return bb.addLIR(new MultiplyInst(pass.vReg++, loadImm(x), y));
                }
            } else {
                if (x == 1) return bb.addLIR(new UnaryRegInst(ArmInst::UnaryOperator::MOV, pass.vReg++, y));
                if (isPowerOfTwo(x - 1)) {
                    // x = 2^n + 1
                    return bb.addLIR(new BinaryRegInst(ArmInst::BinaryOperator::ADD, pass.vReg++, y, y,
                                                       ArmInst::ShiftOperator::LSL, floorLog(x - 1)));
                } else if (isPowerOfTwo(x + 1)) {
                    // x = 2^n - 1
                    return bb.addLIR(new BinaryRegInst(ArmInst::BinaryOperator::RSB, pass.vReg++, y, y,
                                                       ArmInst::ShiftOperator::LSL, floorLog(x + 1)));
                } else {
                    return bb.addLIR(new MultiplyInst(pass.vReg++, loadImm(x), y));
                }
            }
        }

        CoreRegAssign *fastSignedDivide(CoreRegAssign *x, int y) {
            if (y == 1) return bb.addLIR(new UnaryRegInst(ArmInst::UnaryOperator::MOV, pass.vReg++, x));
            if (y == -1) return bb.addLIR(new BinaryImmInst(ArmInst::BinaryOperator::RSB, pass.vReg++, x, 0));
            uint32_t absY = abs(y);
            int neg = y >> 31;
            uint8_t log = floorLog(absY);
            if (isPowerOfTwo(absY)) {
                auto op_lir = bb.addLIR(new CompareImmInst(ArmInst::CompareOperator::CMP, x, 0));
                auto flag_lir = bb.addLIR(new SetFlagInst(pass.vReg++, op_lir));
                lastStatusRegAssign = make_pair(nullptr, nullptr);
                CoreRegAssign *add;
                if (checkImmediate((int) absY - 1)) {
                    add = bb.addLIR(new BinaryImmInst(ArmInst::BinaryOperator::ADD, pass.vReg++, x,
                                                      absY - 1, CondInst::Cond::LT, flag_lir, x));
                } else {
                    add = bb.addLIR(new BinaryRegInst(ArmInst::BinaryOperator::ADD, pass.vReg++, x,
                                                      loadImm((int) absY - 1), ArmInst::ShiftOperator::LSL, 0,
                                                      CondInst::Cond::LT, flag_lir, x));
                }
                auto res = bb.addLIR(new UnaryRegInst(ArmInst::UnaryOperator::MOV, pass.vReg++, add,
                                                      ArmInst::ShiftOperator::ASR, log));
                if (neg == 0) {
                    return res;
                }
                return bb.addLIR(new BinaryImmInst(ArmInst::BinaryOperator::RSB, pass.vReg++, res, 0));
            }
            if ((1 << log) < absY) log++;
            if (absY == 1) log = 1;
            uint8_t shift = log - 1;
            uint32_t m_down = (((uint64_t) 1) << (32 + shift)) / absY;
            uint32_t m_up = m_down + 1; // magic number
            auto imm = loadImm((int) m_up);
            auto hi = bb.addLIR(new Multiply64GetHighInst(pass.vReg++, x, imm));
            auto unsigned_hi = bb.addLIR(new BinaryRegInst(ArmInst::BinaryOperator::ADD, pass.vReg++, x, hi));
            auto round_down_quotient = bb.addLIR(new UnaryRegInst(ArmInst::UnaryOperator::MOV, pass.vReg++, unsigned_hi,
                                                                  ArmInst::ShiftOperator::ASR, shift));
            auto round_to_zero_quotient = bb.addLIR(new BinaryRegInst(ArmInst::BinaryOperator::SUB, pass.vReg++,
                                                                      round_down_quotient, x,
                                                                      ArmInst::ShiftOperator::ASR, 31));
            if (neg == 0) {
                return round_to_zero_quotient;
            }
            return bb.addLIR(new BinaryImmInst(ArmInst::BinaryOperator::RSB, pass.vReg++, round_to_zero_quotient, 0));
        }

        CoreRegAssign *modConstant(CoreRegAssign *x, int y) {
            // x % y = x % |y|
            uint32_t absY = abs(y);
            if (y == 1) return movImm(0);
            if (y == 2) return bb.addLIR(new BinaryImmInst(ArmInst::BinaryOperator::AND, pass.vReg++, x, 1));
            if (isPowerOfTwo(absY)) {
                auto op_lir = bb.addLIR(new CompareImmInst(ArmInst::CompareOperator::CMP, x, 0));
                auto flag_lir = bb.addLIR(new SetFlagInst(pass.vReg++, op_lir));
                lastStatusRegAssign = make_pair(nullptr, nullptr);
                auto rsb = bb.addLIR(new BinaryImmInst(ArmInst::BinaryOperator::RSB, pass.vReg++, x, 0,
                                                       CondInst::Cond::LT, flag_lir, x));
                auto tmp = bb.addLIR(new BinaryImmInst(ArmInst::BinaryOperator::AND, pass.vReg++, rsb, absY - 1));
                return bb.addLIR(new BinaryImmInst(ArmInst::BinaryOperator::RSB, pass.vReg++, tmp, 0,
                                                   CondInst::Cond::LT, flag_lir, tmp));
            } else {
                CoreRegAssign *res = nullptr;
                for (auto &iter: fastDivideCache) {
                    if (iter.first.first == x && iter.first.second == (int) absY) {
                        res = iter.second;
                    }
                }
                if (res == nullptr) {
                    res = fastSignedDivide(x, (int) absY);
                    fastDivideCache[std::make_pair(x, (int) absY)] = res;
                }
                auto mul = multiplyConstant((int) absY, res);
                //auto mul = multiplyConstant((int) absY, fastSignedDivide(x, (int) absY));
                return bb.addLIR(new BinaryRegInst(ArmInst::BinaryOperator::SUB, pass.vReg++, x, mul));
            }
        }

        void visit(UninitializedMIR *mir) override {
            mapCoreRegAssign[mir] = loadImm(18373808);
        }

        void visit(LoadConstantMIR *mir) override {
            mapCoreRegAssign[mir] = loadImm(mir->src->getValue<int>());
        }

        void visit(LoadVariableMIR *mir) override {
            if (mir->src->isArgument()) {
                for (size_t i = 0; i < fn.args.size(); i++) {
                    if (fn.args[i] == mir->src) {
                        mapCoreRegAssign[mir] = bb.addLIR(new GetArgumentLIR(pass.vReg++, i));
                        return;
                    }
                }
            } else {
                ValueAddressingLIR *addressing_lir = nullptr;
                for (auto &iter:valueAddressingCache) {
                    if (iter->base == mir->src) {
                        addressing_lir = iter;
                    }
                }
                if (addressing_lir == nullptr) {
                    addressing_lir = bb.addLIR(new ValueAddressingLIR(pass.vReg++, mir->src));
                    valueAddressingCache.insert(addressing_lir);
                }
                //auto addressing_lir = bb.addLIR(new ValueAddressingLIR(pass.vReg++, mir->src));
                mapCoreRegAssign[mir] = bb.addLIR(new LoadImmInst(pass.vReg++, addressing_lir, 0));
            }
        }

        void visit(LoadPointerMIR *mir) override {
            auto ptr_lir = mapCoreRegAssign[mir->src];
            auto cond_lir = dynamic_cast<CondInst *>(ptr_lir);
            if (cond_lir != nullptr && cond_lir->cond == CondInst::Cond::AL) {
                auto addr1_ptr_lir = dynamic_cast<BinaryImmInst *>(ptr_lir);
                if (addr1_ptr_lir != nullptr && addr1_ptr_lir->op == ArmInst::BinaryOperator::ADD) {
                    mapCoreRegAssign[mir] = bb.addLIR(
                            new LoadImmInst(pass.vReg++, addr1_ptr_lir->rn, addr1_ptr_lir->imm12));
                    return;
                }
                auto addr2_ptr_lir = dynamic_cast<BinaryRegInst *>(ptr_lir);
                if (addr2_ptr_lir != nullptr && addr2_ptr_lir->op == ArmInst::BinaryOperator::ADD) {
                    if (addr2_ptr_lir->shiftOp == ArmInst::ShiftOperator::LSL &&
                        (addr2_ptr_lir->shiftImm == 0 || addr2_ptr_lir->shiftImm == 2)) {
                        mapCoreRegAssign[mir] = bb.addLIR(
                                new LoadRegInst(pass.vReg++, addr2_ptr_lir->rn, addr2_ptr_lir->rm,
                                                addr2_ptr_lir->shiftOp, addr2_ptr_lir->shiftImm));
                        return;
                    }
                }
            }
            mapCoreRegAssign[mir] = bb.addLIR(new LoadImmInst(pass.vReg++, ptr_lir, 0));
        }

        void visit(StoreVariableMIR *mir) override {
            ValueAddressingLIR *addressing_lir = nullptr;
            for (auto &iter:valueAddressingCache) {
                if (iter->base == mir->dst) {
                    addressing_lir = iter;
                }
            }
            if (addressing_lir == nullptr) {
                addressing_lir = bb.addLIR(new ValueAddressingLIR(pass.vReg++, mir->dst));
                valueAddressingCache.insert(addressing_lir);
            }
            //auto addressing_lir = bb.addLIR(new ValueAddressingLIR(pass.vReg++, mir->dst));
            auto src_lir = mapCoreRegAssign[mir->src];
            bb.addLIR(new StoreImmInst(src_lir, addressing_lir, 0));
        }

        void visit(StorePointerMIR *mir) override {
            auto ptr_lir = mapCoreRegAssign[mir->dst];
            auto src_lir = mapCoreRegAssign[mir->src];
            auto cond_lir = dynamic_cast<CondInst *>(ptr_lir);
            if (cond_lir != nullptr && cond_lir->cond == CondInst::Cond::AL) {
                auto addr1_ptr_lir = dynamic_cast<BinaryImmInst *>(ptr_lir);
                if (addr1_ptr_lir != nullptr && addr1_ptr_lir->op == ArmInst::BinaryOperator::ADD) {
                    bb.addLIR(new StoreImmInst(src_lir, addr1_ptr_lir->rn, addr1_ptr_lir->imm12));
                    return;
                }
                auto addr2_ptr_lir = dynamic_cast<BinaryRegInst *>(ptr_lir);
                if (addr2_ptr_lir != nullptr && addr2_ptr_lir->op == ArmInst::BinaryOperator::ADD) {
                    if (addr2_ptr_lir->shiftOp == ArmInst::ShiftOperator::LSL &&
                        (addr2_ptr_lir->shiftImm == 0 || addr2_ptr_lir->shiftImm == 2)) {
                        bb.addLIR(new StoreRegInst(src_lir, addr2_ptr_lir->rn, addr2_ptr_lir->rm,
                                                   addr2_ptr_lir->shiftOp, addr2_ptr_lir->shiftImm));
                        return;
                    }
                }
            }
            bb.addLIR(new StoreImmInst(src_lir, ptr_lir, 0));
        }

        void visit(MemoryFillMIR *mir) override {
            auto dst_ptr_lir = mapCoreRegAssign[mir->dst];
            auto r0 = bb.addLIR(new UnaryRegInst(ArmInst::UnaryOperator::MOV, pass.vReg++, dst_ptr_lir));
            auto r1 = mapCoreRegAssign[mir->src];
            auto r2 = loadImm((int) mir->dst->getType()->getSizeOfType());
            auto type = std::make_shared<FunctionType>(VoidType::object);
            auto bl = new BranchAndLinkInst(new Function(type, "_mymemset"));
            bb.addLIR(new SetArgumentLIR(bl, r0, 0));
            bb.addLIR(new SetArgumentLIR(bl, r1, 1));
            bb.addLIR(new SetArgumentLIR(bl, r2, 2));
            bb.addLIR(bl);
        }

        void visit(MemoryCopyMIR *mir) override {
            // todo memset and memcpy vectorize
            lastStatusRegAssign = make_pair(nullptr, nullptr);
            auto src_ptr_lir = mapCoreRegAssign[mir->src];
            auto dst_ptr_lir = mapCoreRegAssign[mir->dst];
            auto arr = dynamic_cast<ArrayType *>(dynamic_cast<PointerType *>(mir->src->getType().get())->getElementType().get());
            int len = 1;
            unsigned size = 4;
            while (arr != nullptr) {
                if (!arr->getElementNum().has_value()) {
                    break;
                }
                len *= (int) arr->getElementNum().value();
                size = arr->getElementType()->getSizeOfType();
                arr = dynamic_cast<ArrayType *>(arr->getElementType().get());
            }
            auto src = dynamic_cast<ValueAddressingMIR *>(mir->src);
            if (src != nullptr) {
                auto base = dynamic_cast<Constant *>(src->base);
                if (base != nullptr) {
                    auto values = base->getValue<vector<int>>();
                    bool same = true;
                    int start = values[0];
                    for (auto value: values) {
                        if (value != start) {
                            same = false;
                            break;
                        }
                    }
                    if (same) {
                        auto r0 = bb.addLIR(new UnaryRegInst(ArmInst::UnaryOperator::MOV, pass.vReg++, dst_ptr_lir));
                        auto r1 = loadImm(start);
                        auto r2 = loadImm(4 * (int) values.size());
                        auto type = std::make_shared<FunctionType>(VoidType::object);
                        auto bl = new BranchAndLinkInst(new Function(type, "_mymemset"));
                        bb.addLIR(new SetArgumentLIR(bl, r0, 0));
                        bb.addLIR(new SetArgumentLIR(bl, r1, 1));
                        bb.addLIR(new SetArgumentLIR(bl, r2, 2));
                        bb.addLIR(bl);
                        return;
                    } else {
                        if (values.size() < 4) {
                            auto load0 = loadImm(values[0]);
                            bb.addLIR(new StoreImmInst(load0, dst_ptr_lir, 0));
                            if (values.size() > 1) {
                                auto load1 = loadImm(values[1]);
                                bb.addLIR(new StoreImmInst(load1, dst_ptr_lir, 4));
                            }
                            if (values.size() > 2) {
                                auto load2 = loadImm(values[2]);
                                bb.addLIR(new StoreImmInst(load2, dst_ptr_lir, 8));
                            }
                        } else {
                            auto r0 = bb.addLIR(
                                    new UnaryRegInst(ArmInst::UnaryOperator::MOV, pass.vReg++, dst_ptr_lir));
                            ValueAddressingLIR *addressing_lir = nullptr;
                            for (auto &iter:valueAddressingCache) {
                                if (iter->base == base) {
                                    addressing_lir = iter;
                                }
                            }
                            if (addressing_lir == nullptr) {
                                addressing_lir = bb.addLIR(new ValueAddressingLIR(pass.vReg++, base));
                                valueAddressingCache.insert(addressing_lir);
                            }
                            auto r1 = addressing_lir;
                            //auto r1 = bb.addLIR(new ValueAddressingLIR(pass.vReg++, base));
                            auto r2 = loadImm(4 * (int) values.size());
                            auto type = std::make_shared<FunctionType>(VoidType::object);
                            auto bl = new BranchAndLinkInst(new Function(type, "_mymemcpy"));
                            bb.addLIR(new SetArgumentLIR(bl, r0, 0));
                            bb.addLIR(new SetArgumentLIR(bl, r1, 1));
                            bb.addLIR(new SetArgumentLIR(bl, r2, 2));
                            bb.addLIR(bl);
                        }
                        return;
                    }
                }
            }
            if (checkImmediate((int) size)) {
                auto new_src_ptr_lir = src_ptr_lir;
                auto new_dst_ptr_lir = dst_ptr_lir;
                for (int i = 0; i < len; i++) {
                    auto load = bb.addLIR(new LoadImmInst(pass.vReg++, new_src_ptr_lir, 0));
                    bb.addLIR(new StoreImmInst(load, new_dst_ptr_lir, 0));
                    if (i != len - 1) {
                        new_src_ptr_lir = bb.addLIR(
                                new BinaryImmInst(ArmInst::BinaryOperator::ADD, pass.vReg++, new_src_ptr_lir, size));
                        new_dst_ptr_lir = bb.addLIR(
                                new BinaryImmInst(ArmInst::BinaryOperator::ADD, pass.vReg++, new_dst_ptr_lir, size));
                    }
                }
            } else {
                auto size_lir = loadImm((int) size);
                auto new_src_ptr_lir = src_ptr_lir;
                auto new_dst_ptr_lir = dst_ptr_lir;
                for (int i = 0; i < len; i++) {
                    auto new_src_lir = bb.addLIR(new LoadImmInst(pass.vReg++, new_src_ptr_lir, 0));
                    bb.addLIR(new StoreImmInst(new_src_lir, new_dst_ptr_lir, 0));
                    if (i != len - 1) {
                        new_src_ptr_lir = bb.addLIR(new BinaryRegInst(ArmInst::BinaryOperator::ADD, pass.vReg++,
                                                                      new_src_ptr_lir, size_lir));
                        new_dst_ptr_lir = bb.addLIR(new BinaryRegInst(ArmInst::BinaryOperator::ADD, pass.vReg++,
                                                                      new_dst_ptr_lir, size_lir));
                    }
                }
            }

        }

        void visit(UnaryMIR *mir) override {
            switch (mir->op) {
                case UnaryMIR::Operator::NEG: {
                    mapCoreRegAssign[mir] = bb.addLIR(new BinaryImmInst(ArmInst::BinaryOperator::RSB, pass.vReg++,
                                                                        mapCoreRegAssign[mir->src], 0));
                    break;
                }
                case UnaryMIR::Operator::NOT: {
                    // dst = src == 0 ? 1 : 0
                    // 此处既设置了cpsr又将bool值赋给了通用寄存器，可根据实际需求使用，未使用的赋值会被优化掉
                    auto op_lir = bb.addLIR(new CompareImmInst(ArmInst::CompareOperator::CMP,
                                                               mapCoreRegAssign[mir->src], 0));
                    auto flag_lir = bb.addLIR(new SetFlagInst(pass.vReg++, op_lir));
                    cpsr_cond = CondInst::Cond::EQ;
                    lastStatusRegAssign = make_pair(mir, flag_lir);
                    auto bool_lir = bb.addLIR(new UnaryImmInst(ArmInst::UnaryOperator::MOV,
                                                               pass.vReg++, 0));
                    mapCoreRegAssign[mir] = bb.addLIR(new UnaryImmInst(ArmInst::UnaryOperator::MOV, pass.vReg++,
                                                                       1, cpsr_cond, flag_lir, bool_lir));

                    break;
                }
                case UnaryMIR::Operator::REV: {
                    // int dst = ~src
                    mapCoreRegAssign[mir] = bb.addLIR(
                            new UnaryRegInst(ArmInst::UnaryOperator::MVN, pass.vReg++, mapCoreRegAssign[mir->src]));
                    break;
                }
            }
        }

        void visit(BinaryMIR *mir) override {
            switch (mir->op) {
                case BinaryMIR::Operator::ADD:
                    mapCoreRegAssign[mir] = optimizeImmBinaryOperation(mir, ArmInst::BinaryOperator::ADD);
                    break;
                case BinaryMIR::Operator::SUB:
                    mapCoreRegAssign[mir] = optimizeImmBinaryOperation(mir, ArmInst::BinaryOperator::SUB);
                    break;
                case BinaryMIR::Operator::MUL: {
                    // mul = 3 instr time
                    auto src1 = dynamic_cast<LoadConstantMIR *>(mir->src1);
                    auto src2 = dynamic_cast<LoadConstantMIR *>(mir->src2);
                    if (src1 != nullptr && src2 != nullptr) {
                        int res = src1->src->getValue<int>() * src2->src->getValue<int>();
                        mapCoreRegAssign[mir] = loadImm(res);
                    } else if (src1 != nullptr) {
                        mapCoreRegAssign[mir] = multiplyConstant(src1->src->getValue<int>(),
                                                                 mapCoreRegAssign[mir->src2]);
                    } else if (src2 != nullptr) {
                        mapCoreRegAssign[mir] = multiplyConstant(src2->src->getValue<int>(),
                                                                 mapCoreRegAssign[mir->src1]);
                    } else {
                        mapCoreRegAssign[mir] = bb.addLIR(new MultiplyInst(pass.vReg++, mapCoreRegAssign[mir->src1],
                                                                           mapCoreRegAssign[mir->src2]));
                    }
                    break;
                }

                case BinaryMIR::Operator::DIV: {
                    auto src = dynamic_cast<LoadConstantMIR *>(mir->src2);
                    if (src != nullptr) {
                        int cnt = src->src->getValue<int>();
                        CoreRegAssign *res = nullptr;
                        for (auto &iter: fastDivideCache) {
                            if (iter.first.first == mapCoreRegAssign[mir->src1] && iter.first.second == cnt) {
                                res = iter.second;
                            }
                        }
                        if (res == nullptr) {
                            res = fastSignedDivide(mapCoreRegAssign[mir->src1], cnt);
                            fastDivideCache[make_pair(mapCoreRegAssign[mir->src1], cnt)] = res;
                        }
                        mapCoreRegAssign[mir] = res;
                    } else {
                        DivideInst *divide = nullptr;
                        for (auto &iter:divideCache) {
                            if (iter->rn == mapCoreRegAssign[mir->src1] && iter->rm == mapCoreRegAssign[mir->src2]) {
                                divide = iter;
                                break;
                            }
                        }
                        if (divide != nullptr) {
                            mapCoreRegAssign[mir] = divide;
                        } else {
                            divide = bb.addLIR(new DivideInst(pass.vReg++, mapCoreRegAssign[mir->src1],
                                                              mapCoreRegAssign[mir->src2]));
                            mapCoreRegAssign[mir] = divide;
                            divideCache.insert(divide);
                        }
                    }
                    break;
                }
                case BinaryMIR::Operator::MOD: {
                    auto src = dynamic_cast<LoadConstantMIR *>(mir->src2);
                    CoreRegAssign *quotient;
                    CoreRegAssign *mul;
                    if (src != nullptr) {
                        mapCoreRegAssign[mir] = modConstant(mapCoreRegAssign[mir->src1], src->src->getValue<int>());
                    } else {
                        quotient = nullptr;
                        for (auto &iter:divideCache) {
                            if (iter->rn == mapCoreRegAssign[mir->src1] && iter->rm == mapCoreRegAssign[mir->src2]) {
                                quotient = iter;
                                break;
                            }
                        }
                        if (quotient == nullptr) {
                            quotient = bb.addLIR(new DivideInst(pass.vReg++, mapCoreRegAssign[mir->src1],
                                                                mapCoreRegAssign[mir->src2]));
                        }
                        mul = bb.addLIR(new MultiplyInst(pass.vReg++, quotient, mapCoreRegAssign[mir->src2]));
                        mapCoreRegAssign[mir] = bb.addLIR(new BinaryRegInst(ArmInst::BinaryOperator::SUB,
                                                                            pass.vReg++, mapCoreRegAssign[mir->src1],
                                                                            mul));
                    }

                    break;
                }
                case BinaryMIR::Operator::AND:
                    mapCoreRegAssign[mir] = optimizeImmBinaryOperation(mir, ArmInst::BinaryOperator::AND);
                    break;
                case BinaryMIR::Operator::OR:
                    mapCoreRegAssign[mir] = optimizeImmBinaryOperation(mir, ArmInst::BinaryOperator::ORR);
                    break;
                case BinaryMIR::Operator::XOR:
                    mapCoreRegAssign[mir] = optimizeImmBinaryOperation(mir, ArmInst::BinaryOperator::EOR);
                    break;
                case BinaryMIR::Operator::LSL:
                    mapCoreRegAssign[mir] = optimizeImmShiftOperation(mir, ArmInst::ShiftOperator::LSL);
                    break;
                case BinaryMIR::Operator::LSR:
                    mapCoreRegAssign[mir] = optimizeImmShiftOperation(mir, ArmInst::ShiftOperator::LSR);
                    break;
                case BinaryMIR::Operator::ASR:
                    mapCoreRegAssign[mir] = optimizeImmShiftOperation(mir, ArmInst::ShiftOperator::ASR);
                    break;
                case BinaryMIR::Operator::CMP_EQ:
                    compareMIR2LIR(mir, CondInst::Cond::EQ);
                    break;
                case BinaryMIR::Operator::CMP_NE:
                    compareMIR2LIR(mir, CondInst::Cond::NE);
                    break;
                case BinaryMIR::Operator::CMP_GT:
                    compareMIR2LIR(mir, CondInst::Cond::GT);
                    break;
                case BinaryMIR::Operator::CMP_LT:
                    compareMIR2LIR(mir, CondInst::Cond::LT);
                    break;
                case BinaryMIR::Operator::CMP_GE:
                    compareMIR2LIR(mir, CondInst::Cond::GE);
                    break;
                case BinaryMIR::Operator::CMP_LE:
                    compareMIR2LIR(mir, CondInst::Cond::LE);
                    break;
            }
        }

        void visit(ValueAddressingMIR *mir) override {
            ValueAddressingLIR *addressing_lir = nullptr;
            for (auto &iter:valueAddressingCache) {
                if (iter->base == mir->base) {
                    addressing_lir = iter;
                }
            }
            if (addressing_lir == nullptr) {
                addressing_lir = bb.addLIR(new ValueAddressingLIR(pass.vReg++, mir->base));
                valueAddressingCache.insert(addressing_lir);
            }
            mapCoreRegAssign[mir] = addressing_lir;
            //mapCoreRegAssign[mir] = bb.addLIR(new ValueAddressingLIR(pass.vReg++, mir->base));
        }

        void visit(ArrayAddressingMIR *mir) override {
            auto base_ptr = dynamic_cast<Assignment *>(mir->base);
            ArrayType *base_type;
            CoreRegAssign *base_lir = nullptr;
            if (base_ptr == nullptr) {
                // 数组作为基址
                base_type = dynamic_cast<ArrayType *>(mir->base->getType().get());
                for (auto &iter:valueAddressingCache) {
                    if (iter->base == mir->base) {
                        base_lir = iter;
                    }
                }
                if (base_lir == nullptr) {
                    base_lir = bb.addLIR(new ValueAddressingLIR(pass.vReg++, mir->base));
                    valueAddressingCache.insert(dynamic_cast<ValueAddressingLIR *>(base_lir));
                }
                // base_lir = bb.addLIR(new ValueAddressingLIR(pass.vReg++, mir->base));
            } else {
                // 指针作为基址
                base_type = dynamic_cast<ArrayType *>(dynamic_cast<PointerType *>(mir->base->getType().get())->getElementType().get());
                base_lir = mapCoreRegAssign[base_ptr];
            }
            auto const_offset = dynamic_cast<LoadConstantMIR *>(mir->offset);
            int sizeOfType = base_type->getElementType()->getSizeOfType();
            if (const_offset != nullptr) {
                int offset_val = const_offset->src->getValue<int>() * sizeOfType;
                if (offset_val == 0) {
                    mapCoreRegAssign[mir] = base_lir;
                }
                if (checkImmediate(offset_val)) {
                    mapCoreRegAssign[mir] = bb.addLIR(
                            new BinaryImmInst(ArmInst::BinaryOperator::ADD, pass.vReg++, base_lir, offset_val));
                } else {
                    auto load_imm = movInvalidImm(offset_val);
                    mapCoreRegAssign[mir] = bb.addLIR(
                            new BinaryRegInst(ArmInst::BinaryOperator::ADD, pass.vReg++, base_lir, load_imm));
                }
            } else if (sizeOfType == 4) {
                mapCoreRegAssign[mir] = bb.addLIR(
                        new BinaryRegInst(ArmInst::BinaryOperator::ADD, pass.vReg++, base_lir,
                                          mapCoreRegAssign[mir->offset],
                                          ArmInst::ShiftOperator::LSL, 2));
            } else {
                auto mul = multiplyConstant(sizeOfType, mapCoreRegAssign[mir->offset]);
                mapCoreRegAssign[mir] = bb.addLIR(
                        new BinaryRegInst(ArmInst::BinaryOperator::ADD, pass.vReg++, base_lir, mul));
            }
        }

        void visit(SelectMIR *mir) override {
            auto src1_lir = mapCoreRegAssign[mir->src1];
            auto src2_lir = mapCoreRegAssign[mir->src2];
            if (lastStatusRegAssign.first != mir->cond) {
                auto op_lir = bb.addLIR(new CompareImmInst(ArmInst::CompareOperator::CMP,
                                                           mapCoreRegAssign[mir->cond], 0));
                auto flag_lir = bb.addLIR(new SetFlagInst(pass.vReg++, op_lir));
                cpsr_cond = CondInst::Cond::NE;
                lastStatusRegAssign = make_pair(mir, flag_lir);
            }
            auto tmp_lir = bb.addLIR(new UnaryRegInst(ArmInst::UnaryOperator::MOV, pass.vReg++, src2_lir));
            mapCoreRegAssign[mir] = bb.addLIR(
                    new UnaryRegInst(ArmInst::UnaryOperator::MOV, pass.vReg++, src1_lir,
                                     ArmInst::ShiftOperator::LSL, 0, cpsr_cond,
                                     lastStatusRegAssign.second, tmp_lir));

        }

        void visit(JumpMIR *mir) override {
            bb.addLIR(new BranchInst(mir->block));
        }

        void visit(BranchMIR *mir) override {
            bool b1 = pass.dfnMap[mir->block1] < pass.dfnMap[&bb];
            auto &myIn = pass.loopIn[&bb];
            for (auto i : myIn) {
                if (pass.loops[i].exits.count(mir->block1) > 0) {
                    b1 = true;
                    break;
                }
                if (pass.loops[i].exits.count(mir->block2) > 0) {
                    b1 = false;
                    break;
                }
            }
            if (lastStatusRegAssign.first == mir->cond) {
                if (b1) {
                    bb.addLIR(new BranchInst(mir->block1, cpsr_cond, lastStatusRegAssign.second));
                } else {
                    bb.addLIR(new BranchInst(mir->block2, CondInst::getContrary(cpsr_cond), lastStatusRegAssign.second));
                }
            } else {
                auto op_lir = bb.addLIR(new CompareImmInst(ArmInst::CompareOperator::CMP,
                                                           mapCoreRegAssign[mir->cond], 0));
                auto flag_lir = bb.addLIR(new SetFlagInst(pass.vReg++, op_lir));
                if (b1) {
                    bb.addLIR(new BranchInst(mir->block1, CondInst::Cond::NE, flag_lir));
                } else {
                    bb.addLIR(new BranchInst(mir->block2, CondInst::Cond::EQ, flag_lir));
                }
            }
            bb.addLIR(new BranchInst(b1 ? mir->block2 : mir->block1));
        }

        void visit(CallMIR *mir) override {
//            if (mir->func->isExternal) {
            auto bl_lir = new BranchAndLinkInst(mir->func);
            for (size_t i = 0; i < mir->func->args.size(); i++) {
                bb.addLIR(new SetArgumentLIR(bl_lir, mapCoreRegAssign[mir->args[i]], i));
            }
            bb.addLIR(bl_lir);
//            } else {
//                pass.syncLastCall(bb, pass.createAsyncCall(fn, bb, mir));
//            }
            lastStatusRegAssign = make_pair(nullptr, nullptr);
//            index++;
        }

        void visit(CallWithAssignMIR *mir) override {
            auto bl_lir = new BranchAndLinkInst(mir->func);
            for (size_t i = 0; i < mir->func->args.size(); i++) {
                bb.addLIR(new SetArgumentLIR(bl_lir, mapCoreRegAssign[mir->args[i]], i));
            }
            bb.addLIR(bl_lir);
            mapCoreRegAssign[mir] = bb.addLIR(new GetReturnValueLIR(pass.vReg++, bl_lir));
            lastStatusRegAssign = make_pair(nullptr, nullptr);
        }

        void visit(MultiCallMIR *mir) override {
            ValueAddressingLIR *addressing_lir = nullptr;
            if (mir->atomic_var != nullptr) {
                for (auto &iter:valueAddressingCache) {
                    if (iter->base == mir->atomic_var) {
                        addressing_lir = iter;
                    }
                }
                if (addressing_lir == nullptr) {
                    addressing_lir = bb.addLIR(new ValueAddressingLIR(pass.vReg++, mir->atomic_var));
                    valueAddressingCache.insert(addressing_lir);
                }
            }
            auto task = pass.createMultiCall(fn, bb, mir, addressing_lir);
            auto bl_lir = new BranchAndLinkInst(mir->func);
            for (size_t i = 0; i < mir->func->args.size(); i++) {
                if (i == mir->func->args.size() - 1 && mir->atomic_var != nullptr) {
                    bb.addLIR(new SetArgumentLIR(bl_lir, addressing_lir, i));
                    break;
                }
                bb.addLIR(new SetArgumentLIR(bl_lir, mapCoreRegAssign[mir->args[i]], i));
            }
            bb.addLIR(bl_lir);
            pass.syncLastCall(bb, task);
            lastStatusRegAssign = make_pair(nullptr, nullptr);
        }

        void visit(ReturnMIR *mir) override {
            if (mir->val == nullptr) {
                bb.addLIR(new ReturnLIR);
            } else {
                bb.addLIR(new ReturnLIR(mapCoreRegAssign[mir->val]));
            }
        }

        void visit(PhiMIR *mir) override {
            mapCoreRegAssign[mir] = bb.addLIR(new CoreRegPhiLIR(pass.vReg++));
        }

        void visit(AtomicLoopCondMIR *mir) override {
            CoreRegAssign *atomic_lir;
            if (mir->atomic_var->isReference()) {
                atomic_lir = fn.atomic_var_ptr;
            } else {
                ValueAddressingLIR *addressing_lir = nullptr;
                for (auto &iter:valueAddressingCache) {
                    if (iter->base == mir->atomic_var) {
                        addressing_lir = iter;
                    }
                }
                if (addressing_lir == nullptr) {
                    addressing_lir = bb.addLIR(new ValueAddressingLIR(pass.vReg++, mir->atomic_var));
                    valueAddressingCache.insert(addressing_lir);
                }
                atomic_lir = addressing_lir;
            }
            ArmInst::BinaryOperator update_op;
            switch (mir->update_op) {
                case BinaryMIR::Operator::ADD:
                    update_op = ArmInst::BinaryOperator::ADD;
                    break;
                case BinaryMIR::Operator::SUB:
                    update_op = ArmInst::BinaryOperator::SUB;
                    break;
                default:
                    throw std::logic_error("invalid update_op");
            }
            CondInst::Cond cond;
            switch (mir->compare_op) {
                case BinaryMIR::Operator::CMP_EQ:
                    cond = CondInst::Cond::EQ;
                    break;
                case BinaryMIR::Operator::CMP_NE:
                    cond = CondInst::Cond::NE;
                    break;
                case BinaryMIR::Operator::CMP_GT:
                    cond = CondInst::Cond::GT;
                    break;
                case BinaryMIR::Operator::CMP_LT:
                    cond = CondInst::Cond::LT;
                    break;
                case BinaryMIR::Operator::CMP_GE:
                    cond = CondInst::Cond::GE;
                    break;
                case BinaryMIR::Operator::CMP_LE:
                    cond = CondInst::Cond::LE;
                    break;
                default:
                    throw std::logic_error("invalid compare_op");
            }
            auto tmp = bb.addLIR(
                    new UnaryRegInst(ArmInst::UnaryOperator::MOV, pass.vReg++, mapCoreRegAssign[mir->border]));
            mapCoreRegAssign[mir] = bb.addLIR(
                    new AtomicLoopCondLIR(pass.vReg++, atomic_lir,
                                          update_op, mapCoreRegAssign[mir->step],
                                          cond, mapCoreRegAssign[mir->border], tmp,
                                          mir->body, mir->exit));
        }
    };

    auto &bb = *node->block;
    MyVisitor visitor(*this, fn, bb, divideCache, fastDivideCache, valueAddressingCache, callSyncTimeVecMap[&bb]);
    for (auto &mir : bb.mirTable) {
        mir->accept(visitor);
    }
    for (auto &child : node->children) {
        runOnNode(fn, child.get(), divideCache, fastDivideCache, valueAddressingCache);
    }
}

void MIR2LIRPass::addPhiIncoming(BasicBlock *block) {
    auto iter1 = block->mirTable.begin();
    auto iter2 = block->lirTable.begin();
    while (iter1 != block->mirTable.end()) {
        auto mir = iter1->get();
        auto mir_phi = dynamic_cast<PhiMIR *>(mir);
        if (mir_phi == nullptr) {
            break;
        }
        auto lir = iter2->get();
        auto core_lir_phi = dynamic_cast<CoreRegPhiLIR *>(lir);
        if (core_lir_phi != nullptr) {
            for (auto item : mir_phi->incomingTable) {
                core_lir_phi->addIncoming(item.first, mapCoreRegAssign[item.second]);
            }
        } else {
            throw logic_error("Neon???");
        }
        iter1++;
        iter2++;
    }
}

uint8_t MIR2LIRPass::floorLog(uint32_t x) {
    uint8_t res = 0;
    if (x & 0xffff0000) {
        x >>= 16;
        res += 16;
    }
    if (x & 0x0000ff00) {
        x >>= 8;
        res += 8;
    }
    if (x & 0x000000f0) {
        x >>= 4;
        res += 4;
    }
    if (x & 0x0000000c) {
        x >>= 2;
        res += 2;
    }
    if (x & 0x00000002) {
        res += 1;
    }
    return res;
}

uint32_t MIR2LIRPass::abs(int x) {
    return x < 0 ? (-x) : x;
}

int MIR2LIRPass::findCeilPowerOfTwo(int x) {
    int res = x - 1;
    res |= res >> 1;
    res |= res >> 2;
    res |= res >> 4;
    res |= res >> 8;
    res |= res >> 16;
    return (res < 0) ? 1 : res + 1;
}

void MIR2LIRPass::prepareMultiThread() {
    md.useThreadPool = true;
    auto getTaskType = make_shared<FunctionType>(
            VoidType::object, vector<shared_ptr<Type >>{
                    make_shared<ArrayType>(IntegerType::object), IntegerType::object});
    md.declareFunction(getTaskType, "addTask")->isExternal = true;
    auto waitTaskType = make_shared<FunctionType>(
            VoidType::object, vector<shared_ptr<Type >>{
                    make_shared<ArrayType>(IntegerType::object)});
    md.declareFunction(getTaskType, "waitTask")->isExternal = true;
    md.declareFunction(make_shared<FunctionType>(VoidType::object),
                       "initThreadPool")->isExternal = true;
    md.declareFunction(make_shared<FunctionType>(VoidType::object),
                       "deleteThreadPool")->isExternal = true;
}

ValueAddressingLIR *MIR2LIRPass::createAsyncCall(Function &fn, BasicBlock &bb, CallMIR *call_mir) {
    if (!md.useThreadPool) {
        prepareMultiThread();
    }

    // 创建任务描述结构
    static size_t buf_id = 0;
    auto buf_name = "task_buf_" + std::to_string(buf_id++);
    auto buf_int_num = call_mir->args.size() + 3;
    auto buf_type = make_shared<ArrayType>(IntegerType::object, buf_int_num);
    auto buf = fn.declareLocalVariable(buf_type, buf_name, false);
    auto buf_ptr = bb.addLIR(new ValueAddressingLIR(vReg++, buf));

    // 传入函数指针
    auto wrapper = createFunctionWrapper(call_mir->func, call_mir->args.size());
    auto callee_ptr_lower = bb.addLIR(new MovwInst(vReg++, wrapper->getName()));
    auto callee_ptr_upper = bb.addLIR(new MovtInst(vReg++, wrapper->getName(), callee_ptr_lower));
    bb.addLIR(new StoreImmInst(callee_ptr_upper, buf_ptr, 0));

    // 传入参数
    for (size_t i = 0; i < call_mir->args.size(); i++) {
        bb.addLIR(new StoreImmInst(mapCoreRegAssign[call_mir->args[i]], buf_ptr, i * 4 + 12));
    }

    // 添加任务
    auto add_task_call = new BranchAndLinkInst(md.getFunctionByName("addTask"));
    bb.addLIR(new SetArgumentLIR(add_task_call, buf_ptr, 0));
    auto task_num = bb.addLIR(new MovwInst(vReg++, 1));
    bb.addLIR(new SetArgumentLIR(add_task_call, task_num, 1));
    bb.addLIR(add_task_call);

    // 返回任务描述结构
    return buf_ptr;
}

Function *MIR2LIRPass::createFunctionWrapper(Function *callee, size_t num_args) {
    // 声明包装函数
    auto wrapperType = make_shared<FunctionType>(
            VoidType::object, vector<shared_ptr<Type >>{
                    make_shared<ArrayType>(IntegerType::object)});
    auto wrapper = md.getFunctionByName(callee->getName() + "wrapper");
    if (wrapper == nullptr) {
        wrapper = md.declareFunction(wrapperType, callee->getName() + "wrapper");
        wrapper->entryBlock = wrapper->createBasicBlock(wrapper->getName() + "_entry");
        auto &bb = *wrapper->entryBlock;

        // 调用真正函数
        auto buf_ptr = bb.addLIR(new GetArgumentLIR(vReg++, 0));
        auto real_call = new BranchAndLinkInst(callee);
        size_t vReg = 0;
        for (size_t i = 0; i < num_args; i++) {
            auto arg = bb.addLIR(new LoadImmInst(vReg++, buf_ptr, i * 4 + 12));
            bb.addLIR(new SetArgumentLIR(real_call, arg, i));
        }
        bb.addLIR(real_call);

        // 包装函数返回
        bb.addLIR(new ReturnLIR);
    }
    return wrapper;
}

ValueAddressingLIR *
MIR2LIRPass::createMultiCall(Function &fn, BasicBlock &bb, MultiCallMIR *call_mir, CoreRegAssign *atomic_var_ptr) {
    if (!md.useThreadPool) {
        prepareMultiThread();
    }

    // 创建任务描述结构
    static size_t buf_id = 0;
    auto buf_name = "task_buf_" + std::to_string(buf_id++);
    auto buf_int_num = call_mir->args.size() + 4;
    auto buf_type = make_shared<ArrayType>(IntegerType::object, buf_int_num);
    auto buf = fn.declareLocalVariable(buf_type, buf_name, false);
    auto buf_ptr = bb.addLIR(new ValueAddressingLIR(vReg++, buf));

    // 传入原子变量
    bb.addLIR(new StoreImmInst(atomic_var_ptr, buf_ptr, 12));

    // 传入函数指针
    auto wrapper = createMultiFunctionWrapper(call_mir->func, call_mir->args.size());
    auto callee_ptr_lower = bb.addLIR(new MovwInst(vReg++, wrapper->getName()));
    auto callee_ptr_upper = bb.addLIR(new MovtInst(vReg++, wrapper->getName(), callee_ptr_lower));
    bb.addLIR(new StoreImmInst(callee_ptr_upper, buf_ptr, 0));

    // 传入参数
    for (size_t i = 0; i < call_mir->args.size(); i++) {
        bb.addLIR(new StoreImmInst(mapCoreRegAssign[call_mir->args[i]], buf_ptr, i * 4 + 16));
    }

    // 添加任务
    auto add_task_call = new BranchAndLinkInst(md.getFunctionByName("addTask"));
    bb.addLIR(new SetArgumentLIR(add_task_call, buf_ptr, 0));
    auto task_num = bb.addLIR(new MovwInst(vReg++, call_mir->thread_num));
    bb.addLIR(new SetArgumentLIR(add_task_call, task_num, 1));
    bb.addLIR(add_task_call);

    // 返回任务描述结构
    return buf_ptr;
}

Function *MIR2LIRPass::createMultiFunctionWrapper(Function *callee, size_t num_args) {
    // 声明包装函数
    auto wrapperType = make_shared<FunctionType>(
            VoidType::object, vector<shared_ptr<Type >>{
                    make_shared<ArrayType>(IntegerType::object)});
    auto wrapper = md.getFunctionByName(callee->getName() + "wrapper");
    if (wrapper == nullptr) {
        wrapper = md.declareFunction(wrapperType, callee->getName() + "wrapper");
        wrapper->entryBlock = wrapper->createBasicBlock(wrapper->getName() + "_entry");
        auto &bb = *wrapper->entryBlock;

        // 调用真正函数
        auto buf_ptr = bb.addLIR(new GetArgumentLIR(vReg++, 0));
        auto real_call = new BranchAndLinkInst(callee);
        size_t vReg = 0;
        for (size_t i = 0; i < num_args; i++) {
            auto arg = bb.addLIR(new LoadImmInst(vReg++, buf_ptr, i * 4 + 16));
            bb.addLIR(new SetArgumentLIR(real_call, arg, i));
        }
        auto atomic_var_ptr = bb.addLIR(new LoadImmInst(vReg++, buf_ptr, 12));
        bb.addLIR(new SetArgumentLIR(real_call, atomic_var_ptr, num_args));
        bb.addLIR(real_call);

        // 包装函数返回
        bb.addLIR(new ReturnLIR);
    }
    return wrapper;
}

void MIR2LIRPass::syncLastCall(BasicBlock &bb, ValueAddressingLIR *buf_ptr) {
    auto wait_task_call = new BranchAndLinkInst(md.getFunctionByName("waitTask"));
    bb.addLIR(new SetArgumentLIR(wait_task_call, buf_ptr, 0));
    bb.addLIR(wait_task_call);
}
