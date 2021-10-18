//
// Created by Joeye on 2021/7/31.
//

#include <queue>
#include <algorithm>
#include "ErasePseudoInstLIRPass.h"
#include "../../utils/AssemblyUtils.h"
#include "../../utils/BasicBlockUtils.h"

bool ErasePseudoInstLIRPass::run() {
    // additional args(more than 4) offset allocation
    out << "\n--- " << fn.getName() << " stack begin ---\n";
    analyzeCallGraphPass.run();
    int args = 0;
    for (auto func: analyzeCallGraphPass.callGraph[&fn]) {
        args = func->args.size() > args ? (int) func->args.size() : args;
    }
    bool callOther = !analyzeCallGraphPass.callGraph[&fn].empty();
    analyzeCallGraphPass.invalidate();
    int offset = args > 4 ? args - 4 : 0;
    out << "extra args: " << offset << "\n";
    // spill zone offset allocation
    analyzeCFGPass.run();

    // 确定溢出区是否需要store
    checkNeedStore();

    // 为 r2,r3留出空间
    offset += 2;
    int base = offset * 4;
    offset = getMemOffsetAllocation(base);
    if (hasConflict) {
        out << "memory overflow: " << (offset - base) << "\n";
    } else {
        out << "memory overflow: " << offset << "\n";
    }

    base = offset;
    analyzeCFGPass.invalidate();
    analyzeCFGPass.run();
    // array base offset allocation
    auto constantTable = fn.getLocalConstantVecDictOrder();
    auto variableTable = fn.getLocalVariableVecDictOrder();
//    for (auto constant: constantTable) {
//        if (constant->getType()->getId() == Type::ID::ARRAY) {
//            valueOffset[constant] = offset;
//            offset += (int) constant->getValue<vector<int>>().size() * 4;
//        }
//    }
    if (!constantTable.empty()) {
        std::cerr << "[ErasePseudoInstLIRPASS]!!!!!!!!!!!!!!!!!!!!!!constantTable is not empty!!!!!!!!!!!!!!!!!!!!!!\n";
    }
    for (auto variable: variableTable) {
        if (variable->isArgument()) continue;
        valueOffset[variable] = offset;
        if (variable->getType()->getId() == Type::ID::ARRAY && !variable->isReference()
            && dynamic_cast<ArrayType *>(variable->getType().get())->getElementNum().has_value()) {
            offset += (int) dynamic_cast<ArrayType *>(variable->getType().get())->getSizeOfType();
        } else {
            offset += 4;
        }
    }
    out << "value: " << (offset - base) << "\n";
    out << "--- " << fn.getName() << " stack end ---\n";

    frameSize = offset;
    if (callOther) {
        regList = {14};
    }
    if (fn.getName() != "main") {
        for (auto &bb: analyzeCFGPass.dfsSequence) {
            for (auto &lir: bb->lirTable) {
                auto data = dynamic_cast<DataRegAssign *>(lir.get());
                if (data != nullptr) {
                    // r12 临时寄存器不需要保存
                    if (data->pReg.has_value() && data->pReg.value() >= 4 && data->pReg.value() != 12) {
                        regList.insert(data->pReg.value());
                    }
                }
            }
        }
    }

    // 删除lir中的伪指令
    analyzeCFGPass.invalidate();
    analyzeDomTreePass.invalidate();
    analyzeDomTreePass.run();
//    std::map<CoreRegAssign *, CoreRegAssign *> replaceTable;
//    for (auto &bb: analyzeCFGPass.dfsSequence) {
//        eliminatePseudo(*bb, replaceTable);
//    }
    eliminatePseudo(analyzeDomTreePass.root, {});
    out << fn.getName() << " memory overflow instr: " << memInstrCnt << "\n\n";
    // 如果没有调用其他函数，无需压入lr，用到哪些寄存器，就需要保存哪些寄存器，main函数不用保存
    vector<unique_ptr<LIR>> lirTable;
    if (!regList.empty()) lirTable.emplace_back(new PushInst(regList));
    if (frameSize != 0) {
        auto sp = new CoreRegAssign(13);
        sp->pReg = 13;
        if (checkImmediate(frameSize)) {
            auto sub = new BinaryImmInst(ArmInst::BinaryOperator::SUB, 18373808, sp, frameSize);
            sub->pReg = 13;
            lirTable.emplace_back(sub);
        } else {
            auto low = new MovwInst(18373808, frameSize & 0xffff);
            auto high = new MovtInst(18373808, frameSize >> 16, low);
            low->pReg = 2;
            high->pReg = 2;
            auto sub = new BinaryRegInst(ArmInst::BinaryOperator::SUB, 18373808, sp, high);
            sub->pReg = 13;
            lirTable.emplace_back(low);
            lirTable.emplace_back(high);
            lirTable.emplace_back(sub);
        }
    }
    for (auto &lir : fn.entryBlock->lirTable) {
        lirTable.emplace_back(std::move(lir));
    }
    fn.entryBlock->lirTable.swap(lirTable);
    return !toErase.empty();
}


void
ErasePseudoInstLIRPass::eliminatePseudo(DomTreeNode *node, std::map<CoreRegAssign *, CoreRegAssign *> replaceTable) {
    vector<unique_ptr<LIR>> lirTable;
    for (auto &lir : node->block->lirTable) {
        auto news = revisePseudo(lir.get(), replaceTable);
        lirTable.emplace_back(std::move(lir));
        for (auto &new_lir: news) {
            if (new_lir != nullptr) {
                lirTable.emplace_back(new_lir);
            }
        }
    }
    node->block->lirTable.swap(lirTable);
    auto rearVec = analyzeCFGPass.result[node->block].rear;
    for (auto rear : rearVec) {
        replacePhiIncoming(rear, replaceTable, {}, {});
    }
    for (auto &child: getDomChildrenBlockNameOrder(node)) {
        eliminatePseudo(child, replaceTable);
    }
}

vector<LIR *>
ErasePseudoInstLIRPass::revisePseudo(LIR *lir, std::map<CoreRegAssign *, CoreRegAssign *> &replaceTable) {
    struct MyVisitor : LIR::Visitor {
        ErasePseudoInstLIRPass &pass;
        std::map<CoreRegAssign *, CoreRegAssign *> &replaceTable;
        vector<LIR *> news;

        explicit MyVisitor(ErasePseudoInstLIRPass &pass, std::map<CoreRegAssign *, CoreRegAssign *> &replaceTable)
                : pass(pass), replaceTable(replaceTable) {}

        static CoreRegAssign *copyReg(CoreRegAssign *src, CoreRegAssign *dst) {
            dst->pReg = src->pReg;
            return dst;
        }

        static CoreRegAssign *createReg(size_t index) {
            auto res = new CoreRegAssign(18373808);
            res->pReg = index;
            return res;
        }

        void stackLoad(CoreRegAssign *lir, int offset) {
            auto sp = createReg(13);
            if (offset <= 4095) {
                auto load = new LoadImmInst(lir->vReg, sp, offset);
                news.emplace_back(load);
                replaceTable[lir] = copyReg(lir, load);
            } else {
                auto low = new MovwInst(18373808, offset & 0xffff);
                auto high = new MovtInst(18373808, offset >> 16, low);
                copyReg(lir, low);
                auto load = new LoadRegInst(lir->vReg, sp, copyReg(lir, high));
                news.emplace_back(low);
                news.emplace_back(high);
                news.emplace_back(load);
                replaceTable[lir] = copyReg(lir, load);
            }
        }

        void stackStore(CoreRegAssign *src, int offset) {
            auto sp = createReg(13);
            if (offset <= 4095) {
                auto store = new StoreImmInst(src, sp, offset);
                news.emplace_back(store);
            } else {
                auto low = new MovwInst(18373808, offset & 0xffff);
                auto high = new MovtInst(18373808, offset >> 16, low);
                low->pReg = 12;
                high->pReg = 12;
                auto store = new StoreRegInst(src, sp, high);
                news.emplace_back(store);
            }
        }

        void visit(LoadImm32LIR *lir) override {
            auto low = new MovwInst(lir->vReg, lir->imm32 & 0xffff);
            auto high = new MovtInst(lir->vReg, lir->imm32 >> 16, low);
            news.emplace_back(low);
            news.emplace_back(high);
            copyReg(lir, low);
            replaceTable[lir] = copyReg(lir, high);
        }

        void visit(ValueAddressingLIR *lir) override {
            auto iter = pass.valueOffset.find(lir->base);
            if (iter == pass.valueOffset.end()) {
                auto low = new MovwInst(lir->vReg, lir->base->getName());
                auto high = new MovtInst(lir->vReg, lir->base->getName(), low);
                news.emplace_back(low);
                news.emplace_back(high);
                copyReg(lir, low);
                replaceTable[lir] = copyReg(lir, high);
            } else {
                auto sp = createReg(13);
                if (checkImmediate(iter->second)) {
                    auto add = new BinaryImmInst(ArmInst::BinaryOperator::ADD, lir->vReg, sp, iter->second);
                    news.emplace_back(add);
                    replaceTable[lir] = copyReg(lir, add);
                } else {
                    auto low = new MovwInst(18373808, iter->second & 0xffff);
                    auto high = new MovtInst(18373808, iter->second >> 16, low);
                    copyReg(lir, low);
                    auto add = new BinaryRegInst(ArmInst::BinaryOperator::ADD, lir->vReg, sp, copyReg(lir, high));
                    news.emplace_back(low);
                    news.emplace_back(high);
                    news.emplace_back(add);
                    replaceTable[lir] = copyReg(lir, add);
                }
            }
        }

        void visit(GetArgumentLIR *lir) override {
            if (lir->index < 4) {
                if (lir->pReg != lir->index) {
                    auto mov = new UnaryRegInst(ArmInst::UnaryOperator::MOV, lir->vReg, createReg(lir->index));
                    news.emplace_back(mov);
                    replaceTable[lir] = copyReg(lir, mov);
                }
            } else {
                int offset = pass.frameSize + (int) pass.regList.size() * 4 + (int) (lir->index - 4) * 4;
                auto sp = createReg(13);
                if (checkImmediate(offset)) {
                    auto load = new LoadImmInst(18373808, sp, offset);
                    news.emplace_back(load);
                    replaceTable[lir] = copyReg(lir, load);
                } else {
                    auto low = new MovwInst(lir->vReg, offset & 0xffff);
                    auto high = new MovtInst(lir->vReg, offset >> 16, low);
                    news.emplace_back(low);
                    news.emplace_back(high);
                    copyReg(lir, low);
                    auto load = new LoadRegInst(18373808, sp, copyReg(lir, high));
                    news.emplace_back(load);
                    replaceTable[lir] = copyReg(lir, load);
                }

            }
        }

        void visit(SetArgumentLIR *lir) override {
            if (lir->index < 4) {
                if (lir->index != lir->src->pReg) {
                    auto mov = new UnaryRegInst(ArmInst::UnaryOperator::MOV, 18373808, lir->src);
                    mov->pReg = lir->index;
                    news.emplace_back(mov);
                }
            } else {
                stackStore(lir->src, (int) (lir->index - 4) * 4);
            }
        }

        void visit(GetReturnValueLIR *lir) override {
            if (lir->pReg != 0) {
                auto mov = new UnaryRegInst(ArmInst::UnaryOperator::MOV, lir->vReg, createReg(0));
                news.emplace_back(mov);
                replaceTable[lir] = copyReg(lir, mov);
            }
        }

        void visit(ReturnLIR *lir) override {
            if (lir->val != nullptr && lir->val->pReg != 0) {
                auto mov = new UnaryRegInst(ArmInst::UnaryOperator::MOV, 18373808, lir->val);
                mov->pReg = 0;
                news.emplace_back(mov);
            }
            // add sp, sp, #frameSize
            if (pass.frameSize != 0) {
                auto sp = new CoreRegAssign(13);
                sp->pReg = 13;
                if (checkImmediate(pass.frameSize)) {
                    auto add = new BinaryImmInst(ArmInst::BinaryOperator::ADD, 18373808, sp, pass.frameSize);
                    add->pReg = 13;
                    news.emplace_back(add);
                } else {
                    auto low = new MovwInst(18373808, pass.frameSize & 0xffff);
                    auto high = new MovtInst(18373808, pass.frameSize >> 16, low);
                    low->pReg = 12;
                    high->pReg = 12;
                    auto add = new BinaryRegInst(ArmInst::BinaryOperator::ADD, 18373808, sp, high);
                    add->pReg = 13;
                    news.emplace_back(low);
                    news.emplace_back(high);
                    news.emplace_back(add);
                }
            }
            if (!pass.regList.empty()) news.emplace_back(new PopInst(pass.regList));
            news.emplace_back(new BranchAndExchangeInst(createReg(14)));
        }

        void visit(LoadScalarLIR *lir) override {
            if (lir->src->rematerialization != nullptr) {
                auto re = (*lir->src->rematerialization)(lir->src);
                news.emplace_back(re->castToLIR());
                replaceTable[lir] = copyReg(lir, re);
            } else {
                pass.memInstrCnt++;
                if (lir->src->offset.has_value()) {
                    stackLoad(lir, lir->src->offset.value());
                }
            }
        }

        void visit(StoreScalarLIR *lir) override {
            if (lir->needStore) {
                pass.memInstrCnt++;
                if (lir->offset.has_value()) {
                    stackStore(lir->src, lir->offset.value());
                }
            }
        }

        void visit(CoreMemPhiLIR *lir) override {
            pass.memInstrCnt++;
        }
    };
    lir->doReplacement(replaceTable, {}, {});
    MyVisitor visitor(*this, replaceTable);
    visitor.news = {};
    lir->accept(visitor);
    return visitor.news;
}

int ErasePseudoInstLIRPass::getMemOffsetAllocation(int base) {
    // 溢出区伪指令的活跃变量分析
    analyzeDataFlowPass.invalidate();
    analyzeActivityPass.invalidate();
    analyzeDataFlowPass.run();
    // 根据分析出的活跃链（活跃区间）构建冲突图，在活跃处定义即为冲突
    for (auto &target: analyzeActivityPass.assignActivityMapCoreMem) {
        for (auto &cmp: analyzeActivityPass.assignActivityMapCoreMem) {
            bool is_tar_phi = dynamic_cast<CoreMemPhiLIR *>(target.first) != nullptr;
            bool is_cmp_phi = dynamic_cast<CoreMemPhiLIR *>(cmp.first) != nullptr;
            auto tar_def_bb = target.second.def.first;
            auto cmp_def_bb = cmp.second.def.first;
            auto tar_bb_iter = analyzeDataFlowPass.dataFlowMapCoreMem.find(tar_def_bb);
            if (tar_bb_iter == analyzeDataFlowPass.dataFlowMapCoreMem.end()) continue;
            bool out_has_cmp = tar_bb_iter->second.out.find(cmp.first) != tar_bb_iter->second.out.end();
            bool conflict = false;
            if (out_has_cmp) {
                if (tar_def_bb == cmp_def_bb) {
                    if (target.second.def.second > cmp.second.def.second) {
                        conflict = true;
                    }
                } else {
                    conflict = true;
                }
            } else {
                bool in_has_cmp = tar_bb_iter->second.in.find(cmp.first) != tar_bb_iter->second.in.end();
                if (in_has_cmp) {
                    int use_idx = -1;
                    for (auto &use:cmp.second.usePos) {
                        if (use.first == tar_def_bb) {
                            use_idx = use.second;
                            break;
                        }
                    }
                    if (use_idx == -1) continue;
                    if (target.second.def.second < use_idx) {
                        conflict = true;
                    } else {
                        if (tar_def_bb == cmp_def_bb && is_cmp_phi && is_tar_phi &&
                            target.second.def.second == use_idx) {
                            conflict = true;
                        }
                    }
                } else {
                    //in,out均没有, 在同一个块内：cmp-tar-cmp 冲突
                    if (tar_def_bb == cmp_def_bb && target.second.def.second > cmp.second.def.second) {
                        for (auto &use_iter:cmp.second.usePos) {
                            if (use_iter.first == tar_def_bb && use_iter.second > target.second.def.second) {
                                conflict = true;
                            }
                        }
                    }
                }
            }
            if (conflict) {
                addConflictEdge(target.first, cmp.first);
                addConflictEdge(cmp.first, target.first);
            }
        }
    }

    // 根据phi的incoming table和loadMemAssign的src前驱关系构建后继关系
    for (auto &bb: analyzeCFGPass.dfsSequence) {
        for (auto &lir: bb->lirTable) {
            auto load = dynamic_cast<LoadScalarLIR *>(lir.get());
            if (load != nullptr) {
                addSuccessor(load->src, lir.get());
                continue;
            }
            auto phi = dynamic_cast<CoreMemPhiLIR *>(lir.get());
            if (phi != nullptr) {
                for (auto &from :phi->incomingTable) {
                    addSuccessor(from.second, phi);
                }
                phiTable.insert(phi);
                continue;
            }
            auto store = dynamic_cast<StoreScalarLIR *>(lir.get());
            if (store != nullptr) {
                starts.emplace_back(store);
            }
        }
    }

    // 构建以store为起点的连通网
    for (auto &store: starts) {
        auto connect = std::set<CoreMemAssign *>();
        connect.insert(store);
        std::queue<CoreMemAssign *> queue;
        queue.push(store);
        while (!queue.empty()) {
            auto first = queue.front();
            queue.pop();
            auto successors = successorTable.find(first);
            if (successors != successorTable.end()) {
                for (auto &successor:successors->second) {
                    auto memAssign = dynamic_cast<CoreMemAssign *>(successor);
                    if (memAssign != nullptr && std::find(connect.begin(), connect.end(), memAssign) == connect.end()) {
                        queue.push(memAssign);
                        connect.insert(memAssign);
                    }
                }
            }
        }
        connects.emplace_back(connect);
    }

    auto tarConnects = connects;
    connects.clear();
    while (!tarConnects.empty()) {
        vector<std::set<CoreMemAssign *>> tmpConnects;
        for (auto &connect: tarConnects) {
            std::map<CoreMemAssign *, int> connectMap;
            std::set<CoreMemAssign *> conflictConnect;
            std::set<CoreMemAssign *> normalConnect;
            std::set<CoreMemAssign *> oriConnect;
            for (auto assign: connect) {
                connectMap[assign] = 0;
            }
            for (auto &target:connectMap) {
                if (target.second) continue;
                auto conflictIter = conflictGraph.find(target.first);
                if (conflictIter == conflictGraph.end()) continue;
                int i = conflictIter->second;
//                if(!conflictGraphkeys.count(target.first))continue;
                for (auto &cmp:connectMap) {
                    if (cmp.second) continue;
//                    if(!conflictGraph.count({target.first, cmp.first}))continue;
                    auto conflict = conflictEdges[i].next.find(cmp.first);
                    if (conflict == conflictEdges[i].next.end()) continue;
                    cmp.second = 1;
                    target.second = 2;
                }
            }
            for (auto target:connectMap) {
                if (!target.second) {
                    normalConnect.insert(target.first);
                } else if (target.second == 1) {
                    conflictConnect.insert(target.first);
                } else {
                    oriConnect.insert(target.first);
                }
            }
            connects.emplace_back(normalConnect);
            if (!conflictConnect.empty()) {
                tmpConnects.emplace_back(conflictConnect);
                tmpConnects.emplace_back(oriConnect);
            }
        }
        tarConnects = tmpConnects;
    }
    // 对于不含冲突关系的连通网，赋予相同的偏移量offset
    int res = base;
    for (auto &connect: connects) {
        for (auto &assign: connect) {
            if (!assign->offset.has_value()) {
                assign->offset = res;
            }
        }
        res += 4;
    }

    hasConflict = false;
    for (auto &phi:phiTable) {
        for (auto &iter:phi->incomingTable) {
            if (iter.second->offset != phi->offset) {
                hasConflict = true;
                break;
            }
        }
    }
    if (!hasConflict) {
        for (auto &connect: connects) {
            for (auto &assign: connect) {
                assign->offset = assign->offset.value() - 8;
            }
        }
        analyzeActivityPass.invalidate();
        return res - 8;
    }

    // 对于offset前后不一致的phi，添加load, store
    for (auto &phi:phiTable) {
        for (auto &iter:phi->incomingTable) {
            if (iter.second->offset == phi->offset) continue;
            // 冲突时需要添加load store
            auto phi_iter = analyzeActivityPass.assignActivityMapCoreMem.find(phi);
            if (phi_iter == analyzeActivityPass.assignActivityMapCoreMem.end()) continue;
            auto now_bb = phi_iter->second.def.first;
            auto prev_bb = iter.first;
            vector<unique_ptr<LIR>> lirTable;
            auto bb = fn.getBasicBlockByName(prev_bb->getName() + string("_2_") + now_bb->getName());
            if (bb == nullptr) {
                bb = fn.createBasicBlock(prev_bb->getName() + string("_2_") + now_bb->getName());
            } else {
                for (auto &lir:bb->lirTable) {
                    auto load = dynamic_cast<LoadImmInst *>(lir.get());
                    if (load != nullptr && load->imm12 != base - 4 && load->imm12 != base - 8) {
                        lirTable.emplace_back(std::move(lir));
                    } else {
                        auto store = dynamic_cast<StoreImmInst *>(lir.get());
                        if (store != nullptr && store->imm12 != base - 4 && store->imm12 != base - 8) {
                            lirTable.emplace_back(std::move(lir));
                        }
                    }
                }
            }
            for (auto &lir:prev_bb->lirTable) {
                auto branch_lir = dynamic_cast<BranchInst *>(lir.get());
                if (branch_lir != nullptr && branch_lir->block == now_bb) {
                    branch_lir->block = bb;
                }
            }
            int oriOffset = -1;
            for (int i = 0; i < lirTable.size(); i++) {
                auto store = dynamic_cast<StoreImmInst *>(lirTable[i].get());
                if (store != nullptr) {
                    if (store->imm12 == iter.second->offset.value()) {
                        for (int j = 0; j < i; j++) {
                            auto assign = dynamic_cast<CoreRegAssign *>(lirTable[j].get());
                            if (assign != nullptr && assign == store->rt) {
                                auto load = dynamic_cast<LoadImmInst *>(assign);
                                oriOffset = (int) load->imm12;
                                break;
                            }
                        }
                    }
                }
            }
            auto sp = new CoreRegAssign(13);
            sp->pReg = 13;
            if (oriOffset != -1) {
                vector<unique_ptr<LIR>> conflictTable;
                auto r2 = new CoreRegAssign(2);
                auto r3 = new CoreRegAssign(3);
                r2->pReg = 2;
                r3->pReg = 3;
                conflictTable.emplace_back(new StoreImmInst(r2, sp, base - 8));
                conflictTable.emplace_back(new StoreImmInst(r3, sp, base - 4));
                auto xLoad = new LoadImmInst(18373808, sp, oriOffset);
                auto yLoad = new LoadImmInst(18373808, sp, iter.second->offset.value());
                xLoad->pReg = 2;
                yLoad->pReg = 3;
                conflictTable.emplace_back(xLoad);
                conflictTable.emplace_back(yLoad);
                auto xStore = new StoreImmInst(xLoad, sp, iter.second->offset.value());
                auto yStore = new StoreImmInst(yLoad, sp, phi->offset.value());
                conflictTable.emplace_back(xStore);
                conflictTable.emplace_back(yStore);
                for (auto &lir:lirTable) {
                    auto load = dynamic_cast<LoadImmInst *>(lir.get());
                    if (load != nullptr) {
                        if (load->imm12 != oriOffset) {
                            conflictTable.emplace_back(std::move(lir));
                        }
                    } else {
                        auto store = dynamic_cast<StoreImmInst *>(lir.get());
                        if (store != nullptr) {
                            if (store->imm12 != iter.second->offset.value()) {
                                conflictTable.emplace_back(std::move(lir));
                            }
                        }
                    }
                }
                auto loadR2 = new LoadImmInst(18373808, sp, base - 8);
                loadR2->pReg = 2;
                auto loadR3 = new LoadImmInst(18373808, sp, base - 4);
                loadR3->pReg = 3;
                conflictTable.emplace_back(loadR2);
                conflictTable.emplace_back(loadR3);
                conflictTable.emplace_back(new BranchInst(now_bb));
                bb->lirTable.swap(conflictTable);
            } else {
                auto xLoad = new LoadImmInst(18373808, sp, iter.second->offset.value());
                xLoad->pReg = 12;
                lirTable.emplace_back(xLoad);
                auto yStore = new StoreImmInst(xLoad, sp, phi->offset.value());
                lirTable.emplace_back(yStore);
                lirTable.emplace_back(new BranchInst(now_bb));
                bb->lirTable.swap(lirTable);
            }
        }
    }
    analyzeActivityPass.invalidate();
    return res;
}

void ErasePseudoInstLIRPass::addConflictEdge(CoreMemAssign *x, CoreMemAssign *y) {
    auto iter = conflictGraph.find(x);
    if (iter == conflictGraph.end()) {
        conflictEdges.push_back({{y}});
        conflictGraph[x] = conflictEdges.size() - 1;
    } else {
        conflictEdges[iter->second].next.insert(y);
    }
}

void ErasePseudoInstLIRPass::addSuccessor(CoreMemAssign *from, LIR *to) {
    auto iter = successorTable.find(from);
    if (iter == successorTable.end()) {
        successorTable[from] = vector<LIR *>{to};
    } else {
        iter->second.emplace_back(to);
    }
}

void ErasePseudoInstLIRPass::checkNeedStore() {
    std::set<CoreMemAssign *> allMemAssign;
    for (auto bb : analyzeCFGPass.dfsSequence) {
        for (auto &lir : bb->lirTable) {
            auto mem_assign_lir = dynamic_cast<CoreMemAssign *>(lir.get());
            if (mem_assign_lir != nullptr) {
                allMemAssign.insert(mem_assign_lir);
            }
        }
    }
    for (auto ssa : allMemAssign) {
        dfsCheckRematerialization(ssa);
    }
    while (!allMemAssign.empty()) {
        dfsCheckNeedStore(allMemAssign, *allMemAssign.begin(), false);
    }
}

Rematerialization *ErasePseudoInstLIRPass::dfsCheckRematerialization(CoreMemAssign *ssa) {
    auto phi_lir = dynamic_cast<CoreMemPhiLIR *>(ssa);
    if (phi_lir != nullptr) {
        if (phi_lir->incomingTable.size() == 1) {
            if (ssa->rematerialization != nullptr) {
                return ssa->rematerialization.get();
            }
            return dfsCheckRematerialization(phi_lir->incomingTable.begin()->second);
        }
        return nullptr;
    }
    return ssa->rematerialization.get();
}

bool ErasePseudoInstLIRPass::dfsCheckNeedStore(std::set<CoreMemAssign *> &allMemAssign,
                                               CoreMemAssign *ssa, bool userNeedStore) {
    allMemAssign.erase(ssa);
    if (userNeedStore || ssa->rematerialization == nullptr) {
        if (!ssa->needStore) {
            ssa->needStore = true;
            auto phi_lir = dynamic_cast<CoreMemPhiLIR *>(ssa);
            if (phi_lir != nullptr) {
                for (auto &incoming : phi_lir->incomingTable) {
                    dfsCheckNeedStore(allMemAssign, incoming.second, true);
                }
            }
        }
    } else if (!ssa->needStore) {
        auto phi_lir = dynamic_cast<CoreMemPhiLIR *>(ssa);
        if (phi_lir != nullptr) {
            for (auto &incoming : phi_lir->incomingTable) {
                if (dfsCheckNeedStore(allMemAssign, incoming.second, false)) {
                    return dfsCheckNeedStore(allMemAssign, ssa, true);
                }
            }
        }
    }
    return ssa->needStore;
}
