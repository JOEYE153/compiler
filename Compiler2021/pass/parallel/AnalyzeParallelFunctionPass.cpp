//
// Created by 陈思言 on 2021/8/14.
//

#include "AnalyzeParallelFunctionPass.h"

bool AnalyzeParallelFunctionPass::analyze() {
    analyzeArrayAccessPass.run();
    analyzeActivityPass.run();
    for (auto bb : analyzeCFGPass.dfsSequence) {
        runOnBasicBlock(bb);
    }
    return true;
}

void AnalyzeParallelFunctionPass::invalidate() {
    AnalyzePass::invalidate();
}

void AnalyzeParallelFunctionPass::runOnBasicBlock(BasicBlock *bb) {
    // 收集所有IR和函数调用
    vector<MIR *> allMir;
    vector<size_t> callIndexVec;
    for (size_t i = 0; i < bb->mirTable.size(); i++) {
        auto &mir = bb->mirTable[i];
        allMir.push_back(mir.get());
        if (dynamic_cast<CallMIR *>(mir.get()) != nullptr &&
            dynamic_cast<CallWithAssignMIR *>(mir.get()) == nullptr &&
            dynamic_cast<MultiCallMIR *>(mir.get()) == nullptr) {
            callIndexVec.push_back(i);
        }
    }

    // 判断每个调用的最晚同步时间
    for (auto callIndex : callIndexVec) {
        callSyncTimeVecMap[bb].emplace_back(callIndex, getLatestSyncTime(bb, callIndex));
    }
    callSyncTimeVecMap[bb].emplace_back(0xffffffff, 0xffffffff);
}

size_t AnalyzeParallelFunctionPass::getLatestSyncTime(BasicBlock *bb, size_t callIndex) {
    struct MyVisitor : MIR::Visitor {
        AnalyzeParallelFunctionPass &pass;
        std::map<Variable *, std::set<vector<Assignment *>>> readRecord;
        std::map<Variable *, std::set<vector<Assignment *>>> writeRecord;
        bool needSync = false;

        explicit MyVisitor(AnalyzeParallelFunctionPass &pass) : pass(pass) {}

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
                return;
            }
            for (auto &write_record_offset : write_record_iter->second) {
                // 与每个写操作对比是否存在地址冲突
                if (checkTwoOffsetConflict(read_offset, write_record_offset)) {
                    needSync = true;
                    return;
                }
            }
        }

        void checkWrite(ArrayWrite *array_write) {
            auto write_base = array_write->writeBase;
            auto write_offset = array_write->writeOffset;
            auto write_record_iter = writeRecord.find(write_base);
            if (write_record_iter == writeRecord.end()) {
                // 和之前的写内存使用了不同的base，没有冲突
                return;
            }
            auto read_record_iter = readRecord.find(write_base);
            if (read_record_iter == readRecord.end()) {
                // 和之前的读内存使用了不同的base，没有冲突
                return;
            }
            if (write_record_iter->second.find(write_offset) != write_record_iter->second.end()) {
                // 两次写地址完全相同，分配同一个线程就不冲突
                return;
            }
            if (read_record_iter->second.find(write_offset) != read_record_iter->second.end()) {
                // 读写地址完全相同，分配同一个线程就不冲突
                return;
            }
            for (auto &write_record_offset : write_record_iter->second) {
                // 与每个写操作对比是否存在地址冲突
                if (checkTwoOffsetConflict(write_offset, write_record_offset)) {
                    needSync = true;
                    return;
                }
            }
            for (auto &read_record_offset : read_record_iter->second) {
                // 与每个读操作对比是否存在地址冲突
                if (checkTwoOffsetConflict(write_offset, read_record_offset)) {
                    needSync = true;
                    return;
                }
            }
        }

        void checkRead(MIR *mir) {
            auto &mirReadTable = pass.analyzeArrayAccessPass.mirReadTable;
            auto read_access_iter = mirReadTable.find(mir);
            if (read_access_iter == mirReadTable.end()) {
                // 随机读存，认为发生冲突
                needSync = true;
                return;
            }
            checkRead(read_access_iter->second);
        }

        void checkWrite(MIR *mir) {
            auto &mirWriteTable = pass.analyzeArrayAccessPass.mirWriteTable;
            auto write_access_iter = mirWriteTable.find(mir);
            if (write_access_iter == mirWriteTable.end()) {
                // 随机写存，认为发生冲突
                needSync = true;
                return;
            }
            checkWrite(write_access_iter->second);
        }

        void visit(LoadVariableMIR *mir) override {
            // todo
            needSync = true;
        }

        void visit(LoadPointerMIR *mir) override {
            checkRead(mir);
        }

        void visit(StoreVariableMIR *mir) override {
            // todo
            needSync = true;
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

        void visit(JumpMIR *mir) override {
            needSync = true;
        }

        void visit(BranchMIR *mir) override {
            needSync = true;
        }

        void visit(CallMIR *mir) override {
            auto &sideEffect = pass.analyzeSideEffectPass.sideEffects[mir->func];
            if (!sideEffect->callExternalFunction.empty()) {
                // 调用外部函数是不可并行的
                needSync = true;
                return;
            }
            if (!sideEffect->readGlobalVariable.empty() || !sideEffect->readByPointerArg.empty()) {
                // 存在内存读操作
                auto &functionReadTable = pass.analyzeArrayAccessPass.functionReadTable;
                auto read_access_iter = functionReadTable.find(mir);
                if (read_access_iter == functionReadTable.end() ||
                    sideEffect->readGlobalVariable.size() +
                    sideEffect->readByPointerArg.size() >
                    read_access_iter->second.size()) {
                    // 随机读存，认为发生冲突
                    needSync = true;
                    return;
                }
                // 检查每个读内存的参数
                for (auto arg : read_access_iter->second) {
                    checkRead(arg);
                    if (needSync) {
                        return;
                    }
                }
            }
            if (!sideEffect->writeGlobalVariable.empty() || !sideEffect->writeByPointerArg.empty()) {
                // 存在内存写操作
                auto &functionWriteTable = pass.analyzeArrayAccessPass.functionWriteTable;
                auto write_access_iter = functionWriteTable.find(mir);
                if (write_access_iter == functionWriteTable.end() ||
                    sideEffect->writeGlobalVariable.size() +
                    sideEffect->writeByPointerArg.size() >
                    write_access_iter->second.size()) {
                    // 随机写存，认为发生冲突
                    needSync = true;
                    return;
                }
                for (auto arg : write_access_iter->second) {
                    checkWrite(arg);
                    if (needSync) {
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

        void visit(ReturnMIR *mir) override {
            needSync = true;
        }

        void visit(AtomicLoopCondMIR *mir) override {
            needSync = true;
        }
    };

    MyVisitor visitor(*this);
    auto call_mir = dynamic_cast<CallMIR *>(bb->mirTable[callIndex].get());
    auto &sideEffect = analyzeSideEffectPass.sideEffects[call_mir->func];
    if (!sideEffect->callExternalFunction.empty()) {
        // 调用外部函数是不可并行的
        return callIndex;
    }
    if (!sideEffect->readGlobalVariable.empty() || !sideEffect->readByPointerArg.empty()) {
        // 存在内存读操作
        auto &functionReadTable = analyzeArrayAccessPass.functionReadTable;
        auto read_access_iter = functionReadTable.find(call_mir);
        if (read_access_iter == functionReadTable.end() ||
            sideEffect->readGlobalVariable.size() +
            sideEffect->readByPointerArg.size() >
            read_access_iter->second.size()) {
            // 随机读存，认为发生冲突
            return callIndex;
        }
        // 检查每个读内存的参数
        for (auto array_read : read_access_iter->second) {
            auto read_base = array_read->readBase;
            if (read_base->isConstant()) {
                // 读常量是安全的
                continue;
            }
            auto read_base_var = dynamic_cast<Variable *>(read_base);
            if (read_base_var == nullptr) {
                throw std::logic_error("???");
            }
            auto read_offset = array_read->readOffset;
            visitor.readRecord[read_base_var].insert(read_offset);
        }
    }
    if (!sideEffect->writeGlobalVariable.empty() || !sideEffect->writeByPointerArg.empty()) {
        // 存在内存写操作
        auto &functionWriteTable = analyzeArrayAccessPass.functionWriteTable;
        auto write_access_iter = functionWriteTable.find(call_mir);
        if (write_access_iter == functionWriteTable.end() ||
            sideEffect->writeGlobalVariable.size() +
            sideEffect->writeByPointerArg.size() >
            write_access_iter->second.size()) {
            // 随机写存，认为发生冲突
            return callIndex;
        }
        for (auto array_write : write_access_iter->second) {
            auto write_base = array_write->writeBase;
            auto write_offset = array_write->writeOffset;
            visitor.writeRecord[write_base].insert(write_offset);
        }
    }

    // 检查从哪条IR开始与函数调用存在依赖
    for (size_t i = callIndex + 1; i < bb->mirTable.size(); i++) {
        auto &mir = bb->mirTable[i];
        mir->accept(visitor);
        if (visitor.needSync) {
            return i;
        }
    }
    return 0;
}
