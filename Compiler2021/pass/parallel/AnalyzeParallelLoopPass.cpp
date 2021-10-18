//
// Created by 陈思言 on 2021/8/13.
//

#include "AnalyzeParallelLoopPass.h"
#include <algorithm>

bool AnalyzeParallelLoopPass::analyze() {
    analyzeLoopParamPass.run();
    analyzeRegionPass.run();
    analyzeArrayAccessPass.run();
    analyzeActivityPass.run();
    out << fn.getName() << std::endl;

    // 查找单入口单出口的循环
    for (auto &item : analyzeLoopParamPass.result) {
        if (item.first->entrances.size() == 1 && item.first->exits.size() == 1) {
            if (*item.first->entrances.begin() == *item.first->exits.begin()) {
                for (auto &mir : (*item.first->entrances.begin())->mirTable) {
                    if (dynamic_cast<StoreVariableMIR *>(mir.get()) != nullptr ||
                        dynamic_cast<StorePointerMIR *>(mir.get()) != nullptr) {
                        goto FAILED;
                    }
                }
                offsetAttributeCache.clear();
                checkLoop(item.first, item.second);
            }
        }
        FAILED:;
    }
    return true;
}

void AnalyzeParallelLoopPass::invalidate() {
    AnalyzePass::invalidate();
    parallelLoop.clear();
}

void AnalyzeParallelLoopPass::checkLoop(Loop *loop, const LoopParam &param) {
    // 判断是否只有一个归纳变量和一个退出条件
    if (param.varMap.size() != 1) {
        return;
    }
    auto &induction = *param.varMap.begin();
    if (induction.second.exitCond.size() != 1 || induction.second.exitCond.size() != 1) {
        return;
    }
    auto update_lir = dynamic_cast<BinaryMIR *>(induction.second.update);
    if (update_lir == nullptr ||
        update_lir->op != BinaryMIR::Operator::ADD && update_lir->op != BinaryMIR::Operator::SUB) {
        return;
    }
    auto &exitCond = *induction.second.exitCond.begin();
    out << "find loop:" << std::endl;
    out << (*loop->entrances.begin())->getName() << std::endl;
    out << (*loop->exits.begin())->getName() << std::endl;
    out << induction.second.init->castToMIR()->toString() << std::endl;
    out << induction.second.update->castToMIR()->toString() << std::endl;
    out << induction.second.phi->castToMIR()->toString() << std::endl;
    out << exitCond.first->castToMIR()->toString() << std::endl;
    out << exitCond.second->toString() << std::endl;
    offsetAttributeCache.emplace(induction.second.phi, OffsetAttribute::INDUCED);

    // 收集所有IR
    vector<MIR *> allMir;
    bool has_call = false;
    for (auto bb : loop->nodes) {
        for (auto &mir : bb->mirTable) {
            allMir.push_back(mir.get());
            if (dynamic_cast<CallMIR *>(mir.get()) != nullptr) {
                has_call = true;
            }
        }
    }

    // 判断是否存在写内存冲突
    if (!checkArrayAccess(loop, induction.second.phi, allMir)) {
        out << "cannot parallel" << std::endl;
        out << std::endl;
        return;
    }

    // 太小的循环不并行
    if (!has_call && allMir.size() < 12) {
        out << "too small to parallel" << std::endl;
        out << std::endl;
        return;
    }

    // 寻找循环对应的区间
    auto region = analyzeRegionPass.leaves[*loop->entrances.begin()]->father->father;
    parallelLoop.emplace(loop, region);
    out << "marked parallel" << std::endl;
    out << std::endl;
}

AnalyzeParallelLoopPass::OffsetAttribute AnalyzeParallelLoopPass::getOffsetAttribute(
        Loop *loop, Assignment *induction, Assignment *offset) {
    struct MyVisitor : MIR::Visitor {
        AnalyzeParallelLoopPass &pass;
        Loop *loop;
        Assignment *induction;
        OffsetAttribute attribute = OffsetAttribute::RANDOM;

        MyVisitor(AnalyzeParallelLoopPass &pass, Loop *loop, Assignment *induction)
                : pass(pass), loop(loop), induction(induction) {}

        void visit(LoadConstantMIR *mir) override {
            attribute = OffsetAttribute::STABLE;
        }

        void visit(UnaryMIR *mir) override {
            switch (pass.getOffsetAttribute(loop, induction, mir->src)) {
                case OffsetAttribute::STABLE:
                    attribute = OffsetAttribute::STABLE;
                    break;
                case OffsetAttribute::INDUCED:
                    switch (mir->op) {
                        case UnaryMIR::Operator::NEG:
                        case UnaryMIR::Operator::REV:
                            attribute = OffsetAttribute::INDUCED;
                            break;
                        default:
                            break;
                    }
                    break;
                case OffsetAttribute::RANDOM:
                    break;
            }
        }

        void visit(BinaryMIR *mir) override {
            switch (pass.getOffsetAttribute(loop, induction, mir->src1)) {
                case OffsetAttribute::STABLE:
                    switch (pass.getOffsetAttribute(loop, induction, mir->src2)) {
                        case OffsetAttribute::STABLE:
                            attribute = OffsetAttribute::STABLE;
                            break;
                        case OffsetAttribute::INDUCED:
                            switch (mir->op) {
                                case BinaryMIR::Operator::ADD:
                                case BinaryMIR::Operator::SUB:
                                case BinaryMIR::Operator::XOR:
                                    attribute = OffsetAttribute::INDUCED;
                                    break;
                                default:
                                    break;
                            }
                            break;
                        case OffsetAttribute::RANDOM:
                            break;
                    }
                    break;
                case OffsetAttribute::INDUCED:
                    switch (pass.getOffsetAttribute(loop, induction, mir->src2)) {
                        case OffsetAttribute::STABLE:
                            switch (mir->op) {
                                case BinaryMIR::Operator::ADD:
                                case BinaryMIR::Operator::SUB:
                                case BinaryMIR::Operator::XOR:
                                    attribute = OffsetAttribute::INDUCED;
                                    break;
                                default:
                                    break;
                            }
                            break;
                        default:
                            break;
                    }
                    break;
                case OffsetAttribute::RANDOM:
                    break;
            }
        }

        void visit(SelectMIR *mir) override {
            if (pass.getOffsetAttribute(loop, induction, mir->cond) == OffsetAttribute::STABLE) {
                attribute = OffsetAttribute::STABLE;
            }
        }
    };

    auto iter = offsetAttributeCache.find(offset);
    if (iter != offsetAttributeCache.end()) {
        return iter->second;
    }
    auto def_block = analyzeActivityPass.assignActivityMap[offset].def.first;
    if (loop->nodes.find(def_block) == loop->nodes.end()) {
        // 在循环外面定义
        puts(offset->castToMIR()->toString().c_str());
        offsetAttributeCache.emplace(offset, OffsetAttribute::STABLE);
        return OffsetAttribute::STABLE;
    }
    MyVisitor visitor(*this, loop, induction);
    offset->castToMIR()->accept(visitor);
    offsetAttributeCache.emplace(offset, visitor.attribute);
    return visitor.attribute;
}

bool AnalyzeParallelLoopPass::checkArrayAccess(Loop *loop, Assignment *induction, const vector<MIR *> &allMir) {
    struct MyVisitor : MIR::Visitor {
        AnalyzeParallelLoopPass &pass;
        Loop *loop;
        Assignment *induction;
        std::map<Variable *, std::set<vector<Assignment *>>> readRecord;
        std::map<Variable *, std::set<vector<Assignment *>>> writeRecord;
        bool hasConflict = false;

        MyVisitor(AnalyzeParallelLoopPass &pass, Loop *loop, Assignment *induction)
                : pass(pass), loop(loop), induction(induction) {}

        bool checkAttributeConflict(const vector<Assignment *> &offset) {
            if (offset.empty()) {
                return true;
            }
            auto induced_iter = std::find_if(
                    offset.begin(), offset.end(), [this](Assignment *offset) {
                        return pass.getOffsetAttribute(loop, induction, offset) == OffsetAttribute::INDUCED;
                    });
            auto random_iter = std::find_if(
                    offset.begin(), offset.end(), [this](Assignment *offset) {
                        return pass.getOffsetAttribute(loop, induction, offset) == OffsetAttribute::RANDOM;
                    });
            return induced_iter >= random_iter;
        }

        bool checkTwoOffsetConflict(const vector<Assignment *> &a, const vector<Assignment *> &b) {
            for (auto i = 0; i < std::min(a.size(), b.size()); i++) {
                if (a[i] == b[i]) {
                    continue;
                }
                auto const_ai = dynamic_cast<LoadConstantMIR *>(a[i]);
                auto const_bi = dynamic_cast<LoadConstantMIR *>(a[i]);
                if (const_ai != nullptr && const_bi != nullptr) {
                    if (const_ai->src->getValue<int>() == const_bi->src->getValue<int>()) {
                        continue;
                    } else {
                        return false;
                    }
                }
                return true;
            }
            return true;
        }

        void checkRead(ArrayRead *array_read) {
            auto read_base = array_read->readBase;
            if (read_base->isConstant()) {
                // 读常量是安全的
                return;
            }
            auto read_base_var = dynamic_cast<Variable *>(read_base);
            if (read_base_var == nullptr) {
                throw std::logic_error("???");
            }
            auto read_offset = array_read->readOffset;
            auto write_record_iter = writeRecord.find(read_base_var);
            if (write_record_iter == writeRecord.end()) {
                // 和之前的写内存使用了不同的base，没有冲突
                readRecord[read_base_var].insert(read_offset);
                return;
            }
            if (write_record_iter->second.find(read_offset) != write_record_iter->second.end()) {
                // 读写地址完全相同，分配同一个线程就不冲突
                readRecord[read_base_var].insert(read_offset);
                return;
            }
            for (auto &write_record_offset : write_record_iter->second) {
                // 与每个写操作对比是否存在地址冲突
                if (checkTwoOffsetConflict(read_offset, write_record_offset)) {
                    hasConflict = true;
                    return;
                }
            }
            readRecord[read_base_var].insert(read_offset);
        }

        void checkWrite(ArrayWrite *array_write) {
            auto write_base = array_write->writeBase;
            auto write_offset = array_write->writeOffset;
            if (checkAttributeConflict(write_offset)) {
                // 此偏移量本身会引起线程冲突
                hasConflict = true;
                return;
            }
            auto write_record_iter = writeRecord.find(write_base);
            if (write_record_iter == writeRecord.end()) {
                // 和之前的写内存使用了不同的base，没有冲突
                writeRecord[write_base].insert(write_offset);
                return;
            }
            auto read_record_iter = readRecord.find(write_base);
            if (read_record_iter == readRecord.end()) {
                // 和之前的读内存使用了不同的base，没有冲突
                writeRecord[write_base].insert(write_offset);
                return;
            }
            if (write_record_iter->second.find(write_offset) != write_record_iter->second.end()) {
                // 两次写地址完全相同，分配同一个线程就不冲突
                writeRecord[write_base].insert(write_offset);
                return;
            }
            if (read_record_iter->second.find(write_offset) != read_record_iter->second.end()) {
                // 读写地址完全相同，分配同一个线程就不冲突
                writeRecord[write_base].insert(write_offset);
                return;
            }
            for (auto &write_record_offset : write_record_iter->second) {
                // 与每个写操作对比是否存在地址冲突
                if (checkTwoOffsetConflict(write_offset, write_record_offset)) {
                    hasConflict = true;
                    return;
                }
            }
            for (auto &read_record_offset : read_record_iter->second) {
                // 与每个读操作对比是否存在地址冲突
                if (checkTwoOffsetConflict(write_offset, read_record_offset)) {
                    hasConflict = true;
                    return;
                }
            }
            writeRecord[write_base].insert(write_offset);
        }

        void checkRead(MIR *mir) {
            auto &mirReadTable = pass.analyzeArrayAccessPass.mirReadTable;
            auto read_access_iter = mirReadTable.find(mir);
            if (read_access_iter == mirReadTable.end()) {
                // 随机读存，认为发生冲突
                hasConflict = true;
                return;
            }
            checkRead(read_access_iter->second);
        }

        void checkWrite(MIR *mir) {
            auto &mirWriteTable = pass.analyzeArrayAccessPass.mirWriteTable;
            auto write_access_iter = mirWriteTable.find(mir);
            if (write_access_iter == mirWriteTable.end()) {
                // 随机写存，认为发生冲突
                hasConflict = true;
                return;
            }
            checkWrite(write_access_iter->second);
        }

        void visit(LoadPointerMIR *mir) override {
            checkRead(mir);
        }

        void visit(StoreVariableMIR *mir) override {
            // 写入标量一定会发生冲突
            hasConflict = true;
        }

        void visit(StorePointerMIR *mir) override {
            checkWrite(mir);
        }

        void visit(MemoryFillMIR *mir) override {
            checkWrite(mir);
        }

        void visit(MemoryCopyMIR *mir) override {
            checkRead(mir);
            checkWrite(mir);
        }

        void visit(CallMIR *mir) override {
            auto &sideEffect = pass.analyzeSideEffectPass.sideEffects[mir->func];
            if (!sideEffect->callExternalFunction.empty()) {
                // 调用外部函数是不可并行的
                hasConflict = true;
                return;
            }
            if (!sideEffect->readGlobalVariable.empty() || !sideEffect->readByPointerArg.empty()) {
                // 存在内存读操作
                auto &functionReadTable = pass.analyzeArrayAccessPass.functionReadTable;
                auto read_access_iter = functionReadTable.find(mir);
                if (read_access_iter == functionReadTable.end()) {
                    // 随机读存，认为发生冲突
                    hasConflict = true;
                    return;
                }
                std::set<Value *> allRead;
                for (auto item : read_access_iter->second) {
                    allRead.insert(item->readBase);
                }
                for (auto v : sideEffect->readGlobalVariable) {
                    if (v->getType()->getId() == Type::ID::ARRAY && allRead.find(v) == allRead.end()) {
                        // 随机读存，认为发生冲突
                        hasConflict = true;
                        return;
                    }
                }
                for (auto i : sideEffect->readByPointerArg) {
                    if (allRead.find(mir->args[i]) == allRead.end()) {
                        // 随机读存，认为发生冲突
                        hasConflict = true;
                        return;
                    }
                }
                // 检查每个读内存的参数
                for (auto arg : read_access_iter->second) {
                    checkRead(arg);
                    if (hasConflict) {
                        return;
                    }
                }
            }
            if (!sideEffect->writeGlobalVariable.empty() || !sideEffect->writeByPointerArg.empty()) {
                // 存在内存写操作
                auto &functionWriteTable = pass.analyzeArrayAccessPass.functionWriteTable;
                auto write_access_iter = functionWriteTable.find(mir);
                if (write_access_iter == functionWriteTable.end()) {
                    // 随机写存，认为发生冲突
                    hasConflict = true;
                    return;
                }
                std::set<Value *> allWrite;
                for (auto item : write_access_iter->second) {
                    allWrite.insert(item->writeBase);
                }
                for (auto v : sideEffect->readGlobalVariable) {
                    if (v->getType()->getId() != Type::ID::ARRAY || allWrite.find(v) == allWrite.end()) {
                        // 随机写存，认为发生冲突
                        hasConflict = true;
                        return;
                    }
                }
                for (auto i : sideEffect->readByPointerArg) {
                    if (allWrite.find(mir->args[i]) == allWrite.end()) {
                        // 随机写存，认为发生冲突
                        hasConflict = true;
                        return;
                    }
                }
                for (auto arg : write_access_iter->second) {
                    checkWrite(arg);
                    if (hasConflict) {
                        return;
                    }
                }
            }
        }

        void visit(CallWithAssignMIR *mir) override {
            visit(static_cast<CallMIR *>(mir));
        }

        void visit(MultiCallMIR *mir) override {
            visit(static_cast<CallMIR *>(mir));
        }

        void visit(AtomicLoopCondMIR *mir) override {
            hasConflict = true;
        }
    };

    MyVisitor visitor(*this, loop, induction);
    for (auto &mir : allMir) {
        mir->accept(visitor);
        if (visitor.hasConflict) {
            return false;
        }
    }
    return true;
}
