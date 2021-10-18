//
// Created by 陈思言 on 2021/8/9.
//

#include "AnalyzeArrayAccessPass.h"
#include <queue>
#include <stack>

bool AnalyzeArrayAccessPass::analyze() {
    analyzeDomTreePass.run();
    analyzePointerPass.run();
    auto variableVec = fn.getLocalVariableVecDictOrder();
    auto globalVar = md.getGlobalVariableVecDictOrder();
    variableVec.insert(variableVec.end(), globalVar.begin(), globalVar.end());
    std::queue<BasicBlock *> workList;
    std::map<BasicBlock *, Variable *> inserted;
    std::map<BasicBlock *, Variable *> inWorkList;
    std::map<BasicBlock *, std::set<BasicBlock *>> DF;
    analyzeDomTreePass.collectDF(DF, analyzeDomTreePass.root);
    for (auto block : analyzeCFGPass.dfsSequence) {
        inserted[block] = nullptr;
        inWorkList[block] = nullptr;
    }
    for (auto x : variableVec) {
        if (x->getType()->getId() != Type::ID::ARRAY &&
            (x->getType()->getId() != Type::ID::POINTER ||
             dynamic_cast<PointerType *>(x->getType().get())->getElementType()->getId() != Type::ID::ARRAY)) {
            continue;
        }
        auto assignSet = getWriteSet(x);
        for (auto block : assignSet) {
            inWorkList[block] = x;
            workList.push(block);
        }
        while (!workList.empty()) {
            auto n = workList.front();
            workList.pop();
            for (auto m : DF[n]) {
                if (inserted[m] != x) {
                    auto phi = new ArrayAccessPhi;
                    phi->writeBase = x;
                    phiTable[m] = phi;
                    allArrayAccessPhi.emplace(phi, m);
                    inserted[m] = x;
                    if (inWorkList[m] != x) {
                        inWorkList[m] = x;
                        workList.push(m);
                    }
                }
            }
        }
        switch (x->getLocation()) {
            case Value::Location::ARGUMENT: {
                auto ssa = new InputArrayArgument;
                ssa->writeBase = x;
                allInputArrayArgument.emplace(ssa);
                renameVariable(fn.entryBlock, nullptr, x, ssa);
                break;
            }
            case Value::Location::STACK: {
                auto ssa = new UninitializedArray;
                ssa->writeBase = x;
                allUninitializedArray.emplace(ssa);
                renameVariable(fn.entryBlock, nullptr, x, ssa);
                break;
            }
            case Value::Location::STATIC: {
                auto ssa = new InputArrayGlobal;
                ssa->writeBase = x;
                allInputArrayGlobal.emplace(ssa);
                renameVariable(fn.entryBlock, nullptr, x, ssa);
                break;
            }
            default:
                throw std::logic_error("invalid location");
        }
        phiTable.clear();
        visited.clear();
    }

    return true;
}

void AnalyzeArrayAccessPass::print() const {
    for (auto &item : mirReadTable) {
        out << item.first->toString() << '\n';
        out << "\tread = " << item.second << ", useLastWrite = " << item.second->useLastWrite << '\n';
        out << "\toffset = ";
        if (item.second->readOffset.empty()) {
            out << "{}\n";
        } else {
            out << "{ " << item.second->readOffset.front();
            for (size_t i = 1; i < item.second->readOffset.size(); i++) {
                out << ", " << item.second->readOffset[i];
            }
            out << " }\n";
        }
    }
    for (auto &item : mirWriteTable) {
        out << item.first->toString() << '\n';
        out << "\twrite = " << item.second << ", updateLastWrite = ";
        auto write_update = dynamic_cast<ArrayWriteUpdate *>(item.second);
        out << (write_update == nullptr ? nullptr : write_update->updateLastWrite) << '\n';
    }
}

std::set<BasicBlock *> AnalyzeArrayAccessPass::getWriteSet(Variable *variable) {
    struct MyVisitor : MIR::Visitor {
        AnalyzeArrayAccessPass &pass;
        Variable *variable = nullptr;
        bool hasWrite = false;

        MyVisitor(AnalyzeArrayAccessPass &pass, Variable *variable)
                : pass(pass), variable(variable) {}

        void visit(StorePointerMIR *mir) override {
            if (pass.analyzePointerPass.ptrNode[mir->dst]->base == variable) {
                hasWrite = true;
            }
        }

        void visit(MemoryFillMIR *mir) override {
            if (pass.analyzePointerPass.ptrNode[mir->dst]->base == variable) {
                hasWrite = true;
            }
        }

        void visit(MemoryCopyMIR *mir) override {
            if (pass.analyzePointerPass.ptrNode[mir->dst]->base == variable) {
                hasWrite = true;
            }
        }

        void visit(CallMIR *mir) override {
            for (size_t i : pass.analyzeSideEffectPass.sideEffects[mir->func]->writeByPointerArg) {
                if (pass.analyzePointerPass.ptrNode[mir->args[i]]->base == variable) {
                    hasWrite = true;
                    return;
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
            // todo
        }
    };

    MyVisitor visitor(*this, variable);
    std::set<BasicBlock *> result;
    for (auto block : analyzeCFGPass.dfsSequence) {
        visitor.hasWrite = false;
        for (auto &mir : block->mirTable) {
            mir->accept(visitor);
            if (visitor.hasWrite) {
                result.insert(block);
                break;
            }
        }
    }
    return std::move(result);
}

void AnalyzeArrayAccessPass::renameVariable(BasicBlock *block, BasicBlock *prev, Variable *variable, ArrayWrite *ssa) {
    struct MyVisitor : MIR::Visitor {
        AnalyzeArrayAccessPass &pass;
        Variable *variable = nullptr;
        BasicBlock *block = nullptr;
        ArrayWrite *ssa = nullptr;

        MyVisitor(AnalyzeArrayAccessPass &pass, Variable *variable, BasicBlock *block)
                : pass(pass), variable(variable), block(block) {}

        static vector<Assignment *> getOffset(PointerNode *p) {
            vector<Assignment *> offset;
            std::stack<PointerNode *> pointers;
            while (p != nullptr) {
                pointers.push(p);
                p = p->father;
            }
            while (!pointers.empty()) {
                if (pointers.top()->offsetAssignment != nullptr) {
                    offset.push_back(pointers.top()->offsetAssignment);
                }
                pointers.pop();
            }
            return offset;
        }

        void visit(LoadPointerMIR *mir) override {
            auto src_node = pass.analyzePointerPass.ptrNode[mir->src];
            if (src_node->base != variable) {
                return;
            }
            auto elem_read = new ArrayElementRead;
            elem_read->readBase = ssa->writeBase;
            elem_read->readOffset = getOffset(src_node);
            elem_read->useLastWrite = ssa;
            elem_read->mir = mir;
            pass.mirReadTable.emplace(mir, elem_read);
            pass.allArrayElementRead.emplace(elem_read, block);
        }

        void visit(StorePointerMIR *mir) override {
            auto dst_node = pass.analyzePointerPass.ptrNode[mir->dst];
            if (dst_node->base != variable) {
                return;
            }
            auto elem_write = new ArrayElementWrite;
            elem_write->writeBase = ssa->writeBase;
            elem_write->writeOffset = getOffset(dst_node);
            elem_write->updateLastWrite = ssa;
            elem_write->mir = mir;
            elem_write->isConst = ssa->isConst && dynamic_cast<LoadConstantMIR *>(mir->src) != nullptr;
            pass.mirWriteTable.emplace(mir, elem_write);
            pass.allArrayElementWrite.emplace(elem_write, block);
            ssa = elem_write;
        }

        void visit(MemoryFillMIR *mir) override {
            auto dst_node = pass.analyzePointerPass.ptrNode[mir->dst];
            if (dst_node->base != variable) {
                return;
            }
            auto dst_offset = getOffset(dst_node);
            if (dst_offset.empty()) {
                auto entire_fill = new ArrayEntireFill;
                entire_fill->writeBase = ssa->writeBase;
                entire_fill->writeOffset = dst_offset;
                entire_fill->mir = mir;
                entire_fill->isConst = dynamic_cast<LoadConstantMIR *>(mir->src) != nullptr;
                pass.mirWriteTable.emplace(mir, entire_fill);
                pass.allArrayEntireFill.emplace(entire_fill, block);
                ssa = entire_fill;
            } else {
                auto slice_fill = new ArraySliceFill;
                slice_fill->writeBase = ssa->writeBase;
                slice_fill->writeOffset = dst_offset;
                slice_fill->updateLastWrite = ssa;
                slice_fill->mir = mir;
                slice_fill->isConst = ssa->isConst && dynamic_cast<LoadConstantMIR *>(mir->src) != nullptr;
                pass.mirWriteTable.emplace(mir, slice_fill);
                pass.allArraySliceFill.emplace(slice_fill, block);
                ssa = slice_fill;
            }
        }

        void visit(MemoryCopyMIR *mir) override {
            auto ptr_node = pass.analyzePointerPass.ptrNode[mir->dst];
            if (ptr_node->base != variable) {
                return;
            }
            auto dst_offset = getOffset(ptr_node);
            if (dst_offset.empty()) {
                auto entire_copy = new ArrayEntireCopy;
                entire_copy->readBase = ssa->writeBase;
                entire_copy->readOffset = getOffset(pass.analyzePointerPass.ptrNode[mir->src]);
                entire_copy->useLastWrite = ssa;
                entire_copy->writeBase = ssa->writeBase;
                entire_copy->writeOffset = dst_offset;
                entire_copy->mir = mir;
                entire_copy->isConst = pass.analyzePointerPass.ptrNode[mir->src]->base->isConstant();
                pass.mirReadTable.emplace(mir, entire_copy);
                pass.mirWriteTable.emplace(mir, entire_copy);
                pass.allArrayEntireCopy.emplace(entire_copy, block);
                ssa = entire_copy;
            } else {
                auto slice_copy = new ArraySliceCopy;
                slice_copy->readBase = ssa->writeBase;
                slice_copy->readOffset = dst_offset;
                slice_copy->useLastWrite = ssa;
                slice_copy->writeBase = ssa->writeBase;
                slice_copy->writeOffset = dst_offset;
                slice_copy->updateLastWrite = ssa;
                slice_copy->mir = mir;
                slice_copy->isConst = ssa->isConst && pass.analyzePointerPass.ptrNode[mir->src]->base->isConstant();
                pass.mirReadTable.emplace(mir, slice_copy);
                pass.mirWriteTable.emplace(mir, slice_copy);
                pass.allArraySliceCopy.emplace(slice_copy, block);
                ssa = slice_copy;
            }
        }

        void visit(CallMIR *mir) override {
            for (size_t i : pass.analyzeSideEffectPass.sideEffects[mir->func]->readByPointerArg) {
                if (pass.analyzePointerPass.ptrNode[mir->args[i]]->base == variable) {
                    auto read_fn = new ArrayReadByFunction;
                    read_fn->argIndex = i;
                    read_fn->readBase = ssa->writeBase;
                    read_fn->readOffset = getOffset(pass.analyzePointerPass.ptrNode[mir->args[i]]);
                    read_fn->useLastWrite = ssa;
                    read_fn->mir = mir;
                    pass.functionReadTable[mir].insert(read_fn);
                    pass.allArrayReadByFunction.emplace(read_fn, block);
                }
            }
            for (size_t i : pass.analyzeSideEffectPass.sideEffects[mir->func]->writeByPointerArg) {
                if (pass.analyzePointerPass.ptrNode[mir->args[i]]->base == variable) {
                    // todo: 精细化写入类型
                    auto slice_write_fn = new ArraySliceWriteByFunction;
                    slice_write_fn->argIndex = i;
                    slice_write_fn->writeBase = ssa->writeBase;
                    slice_write_fn->writeOffset = getOffset(pass.analyzePointerPass.ptrNode[mir->args[i]]);
                    slice_write_fn->updateLastWrite = ssa;
                    slice_write_fn->mir = mir;
                    pass.functionWriteTable[mir].insert(slice_write_fn);
                    pass.allArraySliceWriteByFunction.emplace(slice_write_fn, block);
                    ssa = slice_write_fn;
                }
            }
            for (auto v : pass.analyzeSideEffectPass.sideEffects[mir->func]->readGlobalVariable) {
                if (v == variable) {
                    auto read_fn = new ArrayReadByFunction;
                    read_fn->readBase = ssa->writeBase;
                    read_fn->readOffset = getOffset(pass.analyzePointerPass.ptrNode[v]);
                    read_fn->useLastWrite = ssa;
                    read_fn->mir = mir;
                    pass.functionReadTable[mir].insert(read_fn);
                    pass.allArrayReadByFunction.emplace(read_fn, block);
                }
            }
            for (auto v : pass.analyzeSideEffectPass.sideEffects[mir->func]->writeGlobalVariable) {
                if (v == variable) {
                    // todo: 精细化写入类型
                    auto slice_write_fn = new ArraySliceWriteByFunction;
                    slice_write_fn->writeBase = ssa->writeBase;
                    slice_write_fn->writeOffset = getOffset(pass.analyzePointerPass.ptrNode[v]);
                    slice_write_fn->updateLastWrite = ssa;
                    slice_write_fn->mir = mir;
                    pass.functionWriteTable[mir].insert(slice_write_fn);
                    pass.allArraySliceWriteByFunction.emplace(slice_write_fn, block);
                    ssa = slice_write_fn;
                }
            }
        }

        void visit(CallWithAssignMIR *mir) override {
            visit(static_cast<CallMIR *>(mir));
        }

        void visit(MultiCallMIR *mir) override {
            visit(static_cast<CallMIR *>(mir));
        }
    };

    auto phi = phiTable.find(block);
    if (visited.find(block) != visited.end()) {
        if (phi != phiTable.end()) {
            phi->second->addIncoming(prev, ssa);
        }
        return;
    }
    if (phi != phiTable.end()) {
        phi->second->addIncoming(prev, ssa);
        ssa = phi->second;
    }
    MyVisitor visitor(*this, variable, block);
    for (auto &mir : block->mirTable) {
        visitor.ssa = ssa;
        mir->accept(visitor);
        ssa = visitor.ssa;
    }
    visited.insert(block);
    const auto &edgeSet = analyzeCFGPass.result[block];
    for (auto rear : edgeSet.rear) {
        renameVariable(rear, block, variable, ssa);
    }
    for (auto x : mirWriteTable) {
        writeMIRTable[x.second] = x.first;
    }
}

void AnalyzeArrayAccessPass::invalidate() {
    AnalyzePass::invalidate();
    writeMIRTable.clear();
    mirReadTable.clear();
    mirWriteTable.clear();
    functionReadTable.clear();
    functionWriteTable.clear();
    allUninitializedArray.clear();
    allInputArrayArgument.clear();
    allArrayElementRead.clear();
    allArrayElementWrite.clear();
    allArraySliceFill.clear();
    allArraySliceCopy.clear();
    allArrayEntireFill.clear();
    allArrayEntireCopy.clear();
    allArrayReadByFunction.clear();
    allArrayElementWriteByFunction.clear();
    allArraySliceWriteByFunction.clear();
    allArrayEntireWriteByFunction.clear();
    allArrayAccessPhi.clear();
}
