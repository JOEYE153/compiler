//
// Created by 陈思言 on 2021/6/17.
//

#include "EraseUselessValuePass.h"
#include "../analyze/AnalyzeSideEffectPass.h"
#include "../../utils/FunctionUtils.h"

void EraseUselessGlobalValuePass::eraseUselessValue(std::set<Value *> &uselessValueSet) {
    for (auto value : uselessValueSet) {
        if (dynamic_cast<Constant *>(value) != nullptr) {
            md.eraseGlobalConstantByName(value->getName());
        } else if (dynamic_cast<Variable *>(value) != nullptr) {
            md.eraseGlobalVariableByName(value->getName());
        }
    }
}

void EraseUselessLocalValuePass::eraseUselessValue(std::set<Value *> &uselessValueSet) {
    for (auto value : uselessValueSet) {
        if (dynamic_cast<Constant *>(value) != nullptr) {
            fn.eraseLocalConstantByName(value->getName());
        } else if (dynamic_cast<Variable *>(value) != nullptr) {
            fn.eraseLocalVariableByName(value->getName());
        }
    }
}

struct UselessValueVisitorHIR : HIR::Visitor {
    std::set<Value *> uselessValueSet;

    void visit(UnaryHIR *hir) override {
        uselessValueSet.erase(hir->dst);
        uselessValueSet.erase(hir->src);
    }

    void visit(BinaryHIR *hir) override {
        uselessValueSet.erase(hir->dst);
        uselessValueSet.erase(hir->src1);
        uselessValueSet.erase(hir->src2);
    }

    void visit(BranchHIR *hir) override {
        uselessValueSet.erase(hir->cond);
    }

    void visit(CallHIR *hir) override {
        if (hir->ret != nullptr) {
            uselessValueSet.erase(hir->ret);
        }
        for (auto arg : hir->args) {
            uselessValueSet.erase(arg);
        }
    }

    void visit(ReturnHIR *hir) override {
        if (hir->val != nullptr) {
            uselessValueSet.erase(hir->val);
        }
    }

    void visit(PutfHIR *hir) override {
        for (auto arg : hir->args) {
            uselessValueSet.erase(arg);
        }
    }
};

bool EraseUselessGlobalValuePass_HIR::run() {
    UselessValueVisitorHIR visitor;
    auto variableSet = md.getGlobalVariableSetPtrOrder();
    visitor.uselessValueSet.insert(variableSet.begin(), variableSet.end());
    auto constantSet = md.getGlobalConstantSetPtrOrder();
    visitor.uselessValueSet.insert(constantSet.begin(), constantSet.end());
    auto functionSet = md.getFunctionSetPtrOrder();
    for (auto fn : functionSet) {
        auto blockSet = fn->getBasicBlockSetPtrOrder();
        for (auto bb : blockSet) {
            for (auto &hir : bb->hirTable) {
                hir->accept(visitor);
                if (visitor.uselessValueSet.empty()) {
                    return false;
                }
            }
        }
    }
    eraseUselessValue(visitor.uselessValueSet);
    return true;
}

bool EraseUselessLocalValuePass_HIR::run() {
    UselessValueVisitorHIR visitor;
    auto variableSet = fn.getLocalVariableSetPtrOrder();
    visitor.uselessValueSet.insert(variableSet.begin(), variableSet.end());
    for (auto arg : fn.args) {
        visitor.uselessValueSet.erase(arg);
    }
    auto constantSet = fn.getLocalConstantSetPtrOrder();
    visitor.uselessValueSet.insert(constantSet.begin(), constantSet.end());
    auto blockSet = fn.getBasicBlockSetPtrOrder();
    for (auto bb : blockSet) {
        for (auto &hir : bb->hirTable) {
            hir->accept(visitor);
            if (visitor.uselessValueSet.empty()) {
                return false;
            }
        }
    }
    eraseUselessValue(visitor.uselessValueSet);
    return true;
}

struct UselessValueVisitorMIR : MIR::Visitor {
    std::set<Value *> uselessValueSet;

    void visit(LoadConstantMIR *mir) override {
        uselessValueSet.erase(mir->src);
    }

    void visit(LoadVariableMIR *mir) override {
        uselessValueSet.erase(mir->src);
    }

    void visit(StoreVariableMIR *mir) override {
        uselessValueSet.erase(mir->dst);
    }

    void visit(ValueAddressingMIR *mir) override {
        uselessValueSet.erase(mir->base);
    }

    void visit(ArrayAddressingMIR *mir) override {
        auto base_assignment = dynamic_cast<Assignment *>(mir->base);
        if (base_assignment == nullptr) {
            uselessValueSet.erase(mir->base);
        }
    }

    void visit(AtomicLoopCondMIR *mir) override {
        uselessValueSet.erase(mir->atomic_var);
    }
};

bool EraseUselessGlobalValuePass_MIR::run() {
    UselessValueVisitorMIR visitor;
    auto variableSet = md.getGlobalVariableSetPtrOrder();
    visitor.uselessValueSet.insert(variableSet.begin(), variableSet.end());
    auto constantSet = md.getGlobalConstantSetPtrOrder();
    visitor.uselessValueSet.insert(constantSet.begin(), constantSet.end());
    auto functionSet = md.getFunctionSetPtrOrder();
    for (auto fn : functionSet) {
        auto blockSet = fn->getBasicBlockSetPtrOrder();
        for (auto bb : blockSet) {
            for (auto &mir : bb->mirTable) {
                mir->accept(visitor);
                if (visitor.uselessValueSet.empty()) {
                    return false;
                }
            }
        }
    }
    eraseUselessValue(visitor.uselessValueSet);
    return true;
}

bool EraseUselessLocalValuePass_MIR::run() {
    blockSet = fn.getBasicBlockSetPtrOrder();
    initUselessAssignmentSet();
    updateUselessAssignmentSet();
    vector<unique_ptr<MIR>> toErase;
    for (auto bb : blockSet) {
        vector<unique_ptr<MIR>> mirTable;
        for (auto &mir : bb->mirTable) {
            auto mir_assignment = dynamic_cast<Assignment *>(mir.get());
            if (uselessAssignmentSet.find(mir_assignment) != uselessAssignmentSet.end()) {
                toErase.emplace_back(std::move(mir));
            } else {
                mirTable.emplace_back(std::move(mir));
            }
        }
        mirTable.swap(bb->mirTable);
    }

    UselessValueVisitorMIR visitor;
    auto variableSet = fn.getLocalVariableSetPtrOrder();
    visitor.uselessValueSet.insert(variableSet.begin(), variableSet.end());
    for (auto arg : fn.args) {
        visitor.uselessValueSet.erase(arg);
    }
    auto constantSet = fn.getLocalConstantSetPtrOrder();
    visitor.uselessValueSet.insert(constantSet.begin(), constantSet.end());
    for (auto bb : blockSet) {
        for (auto &mir : bb->mirTable) {
            mir->accept(visitor);
            if (visitor.uselessValueSet.empty()) {
                return !toErase.empty();
            }
        }
    }
    eraseUselessValue(visitor.uselessValueSet);
    return true;
}

struct VisitorInitUselessAssignmentSet : MIR::Visitor {
    std::set<Assignment *> uselessAssignmentSet;
    std::queue<Assignment *> usefulAssignmentQueue;

    void visit(UninitializedMIR *mir) override {
        uselessAssignmentSet.insert(mir);
    }

    void visit(LoadConstantMIR *mir) override {
        uselessAssignmentSet.insert(mir);
    }

    void visit(LoadVariableMIR *mir) override {
        uselessAssignmentSet.insert(mir);
    }

    void visit(LoadPointerMIR *mir) override {
        uselessAssignmentSet.insert(mir);
    }

    void visit(StoreVariableMIR *mir) override {
        usefulAssignmentQueue.push(mir->src);
    }

    void visit(StorePointerMIR *mir) override {
        usefulAssignmentQueue.push(mir->dst);
        usefulAssignmentQueue.push(mir->src);
    }

    void visit(MemoryCopyMIR *mir) override {
        usefulAssignmentQueue.push(mir->dst);
        usefulAssignmentQueue.push(mir->src);
    }

    void visit(UnaryMIR *mir) override {
        uselessAssignmentSet.insert(mir);
    }

    void visit(BinaryMIR *mir) override {
        uselessAssignmentSet.insert(mir);
    }

    void visit(ValueAddressingMIR *mir) override {
        uselessAssignmentSet.insert(mir);
    }

    void visit(ArrayAddressingMIR *mir) override {
        uselessAssignmentSet.insert(mir);
    }

    void visit(SelectMIR *mir) override {
        uselessAssignmentSet.insert(mir);
    }

    void visit(BranchMIR *mir) override {
        usefulAssignmentQueue.push(mir->cond);
    }

    void visit(CallMIR *mir) override {
        for (auto arg : mir->args) {
            usefulAssignmentQueue.push(arg);
        }
    }

    void visit(CallWithAssignMIR *mir) override {
        visit(static_cast<CallMIR *>(mir));
        usefulAssignmentQueue.push(mir);
    }

    void visit(MultiCallMIR *mir) override {
        visit(static_cast<CallMIR *>(mir));
    }

    void visit(ReturnMIR *mir) override {
        if (mir->val != nullptr) {
            usefulAssignmentQueue.push(mir->val);
        }
    }

    void visit(PhiMIR *mir) override {
        uselessAssignmentSet.insert(mir);
    }

    void visit(AtomicLoopCondMIR *mir) override {
        usefulAssignmentQueue.push(mir);
        usefulAssignmentQueue.push(mir->step);
        usefulAssignmentQueue.push(mir->border);
    }
};

void EraseUselessLocalValuePass_MIR::initUselessAssignmentSet() {
    VisitorInitUselessAssignmentSet visitor;
    for (auto bb : blockSet) {
        for (auto &mir : bb->mirTable) {
            mir->accept(visitor);
        }
    }
    uselessAssignmentSet.swap(visitor.uselessAssignmentSet);
    usefulAssignmentQueue.swap(visitor.usefulAssignmentQueue);
}

void EraseUselessLocalValuePass_MIR::updateUselessAssignmentSet() {
    struct MyVisitor : MIR::Visitor {
        std::set<Assignment *> uselessAssignmentSet;
        std::queue<Assignment *> usefulAssignmentQueue;

        void visit(UninitializedMIR *mir) override {
            uselessAssignmentSet.erase(mir);
        }

        void visit(LoadConstantMIR *mir) override {
            uselessAssignmentSet.erase(mir);
        }

        void visit(LoadVariableMIR *mir) override {
            uselessAssignmentSet.erase(mir);
        }

        void visit(LoadPointerMIR *mir) override {
            if (uselessAssignmentSet.erase(mir) > 0) {
                usefulAssignmentQueue.push(mir->src);
            }
        }

        void visit(UnaryMIR *mir) override {
            if (uselessAssignmentSet.erase(mir) > 0) {
                usefulAssignmentQueue.push(mir->src);
            }
        }

        void visit(BinaryMIR *mir) override {
            if (uselessAssignmentSet.erase(mir) > 0) {
                usefulAssignmentQueue.push(mir->src1);
                usefulAssignmentQueue.push(mir->src2);
            }
        }

        void visit(ValueAddressingMIR *mir) override {
            uselessAssignmentSet.erase(mir);
        }

        void visit(ArrayAddressingMIR *mir) override {
            if (uselessAssignmentSet.erase(mir) > 0) {
                usefulAssignmentQueue.push(mir->offset);
                auto base_assignment = dynamic_cast<Assignment *>(mir->base);
                if (base_assignment != nullptr) {
                    usefulAssignmentQueue.push(base_assignment);
                }
            }
        }

        void visit(SelectMIR *mir) override {
            if (uselessAssignmentSet.erase(mir) > 0) {
                usefulAssignmentQueue.push(mir->cond);
                usefulAssignmentQueue.push(mir->src1);
                usefulAssignmentQueue.push(mir->src2);
            }
        }

        void visit(PhiMIR *mir) override {
            if (uselessAssignmentSet.erase(mir) > 0) {
                for (auto &item : mir->incomingTable) {
                    usefulAssignmentQueue.push(item.second);
                }
            }
        }

        void visit(CallWithAssignMIR *mir) override {
            if (uselessAssignmentSet.erase(mir) > 0) {
                for (auto *arg : mir->args) {
                    usefulAssignmentQueue.push(arg);
                }
            }
        }
    };

    MyVisitor visitor;
    uselessAssignmentSet.swap(visitor.uselessAssignmentSet);
    usefulAssignmentQueue.swap(visitor.usefulAssignmentQueue);
    while (!visitor.usefulAssignmentQueue.empty()) {
        visitor.usefulAssignmentQueue.front()->castToMIR()->accept(visitor);
        visitor.usefulAssignmentQueue.pop();
    }
    uselessAssignmentSet.swap(visitor.uselessAssignmentSet);
}

void SimplifyFunctionCallPass::initUselessAssignmentSet() {
    struct MyVisitor : VisitorInitUselessAssignmentSet {
        AnalyzeSideEffectPass *sideEffectPass = nullptr;
        std::set<CallMIR *> uselessCallVoid;

        void visit(CallMIR *mir) override {
            if (sideEffectPass->sideEffects[mir->func]->writeByPointerArg.empty() &&
                sideEffectPass->sideEffects[mir->func]->writeGlobalVariable.empty() &&
                sideEffectPass->sideEffects[mir->func]->callExternalFunction.empty()) {
                uselessCallVoid.insert(mir);
                return;
            }
            VisitorInitUselessAssignmentSet::visit(mir);
        }

        void visit(CallWithAssignMIR *mir) override {
            visit(static_cast<CallMIR *>(mir));
            uselessCallVoid.erase(mir);
            uselessAssignmentSet.insert(mir);
        }
    };

    MyVisitor visitor;
    visitor.sideEffectPass = &analyzeSideEffectPass;
    for (auto bb : blockSet) {
        for (auto &mir : bb->mirTable) {
            mir->accept(visitor);
        }
    }
    uselessAssignmentSet.swap(visitor.uselessAssignmentSet);
    usefulAssignmentQueue.swap(visitor.usefulAssignmentQueue);
    uselessCallVoid.swap(visitor.uselessCallVoid);
}

bool SimplifyFunctionCallPass::run() {
    analyzeSideEffectPass.run();
    blockSet = fn.getBasicBlockSetPtrOrder();
    initUselessAssignmentSet();
    updateUselessAssignmentSet();
    vector<unique_ptr<MIR>> toErase;
    std::set<Function *> cloned;
    std::vector<CallMIR *> toClone;
    bool functionInserted = false;
    for (auto bb : blockSet) {
        vector<unique_ptr<MIR>> mirTable;
        for (auto &mir : bb->mirTable) {
            if (uselessCallVoid.count(dynamic_cast<CallMIR *>(mir.get()))) {
                toErase.emplace_back(std::move(mir));
                continue;
            }
            auto mir_assignment = dynamic_cast<Assignment *>(mir.get());
            auto mir_call = dynamic_cast<CallWithAssignMIR *>(mir.get());
            if (uselessAssignmentSet.find(mir_assignment) != uselessAssignmentSet.end()) {
                if (mir_call != nullptr &&
                    !(analyzeSideEffectPass.sideEffects[mir_call->func]->writeByPointerArg.empty() &&
                      analyzeSideEffectPass.sideEffects[mir_call->func]->writeGlobalVariable.empty() &&
                      analyzeSideEffectPass.sideEffects[mir_call->func]->callExternalFunction.empty())) {
                    Function *call = mir_call->func;
                    if (!call->isExternal) {
                        auto *newMir = new CallMIR(call, mir_call->args);
                        mirTable.emplace_back(newMir);
                        toClone.push_back(newMir);
                        toErase.emplace_back(std::move(mir));
                    } else mirTable.emplace_back(std::move(mir));
                } else toErase.emplace_back(std::move(mir));
            } else {
                mirTable.emplace_back(std::move(mir));
            }
        }
        mirTable.swap(bb->mirTable);
    }
    for (auto *mir_call:toClone) {
        Function *call = mir_call->func;
        mir_call->func = cloneFunctionReturn0(moduleIn, call);
        if (call != mir_call->func && !cloned.count(mir_call->func)) {
            functionInserted = true;
            cloned.insert(mir_call->func);
            toInsert.push(mir_call->func);
        }
    }

    UselessValueVisitorMIR visitor;
    auto variableSet = fn.getLocalVariableSetPtrOrder();
    visitor.uselessValueSet.insert(variableSet.begin(), variableSet.end());
    for (auto arg : fn.args) {
        visitor.uselessValueSet.erase(arg);
    }
    auto constantSet = fn.getLocalConstantSetPtrOrder();
    visitor.uselessValueSet.insert(constantSet.begin(), constantSet.end());
    for (auto bb : blockSet) {
        for (auto &mir : bb->mirTable) {
            mir->accept(visitor);
            if (visitor.uselessValueSet.empty()) {
                return !toErase.empty() || functionInserted;
            }
        }
    }
    eraseUselessValue(visitor.uselessValueSet);
    return true;
}

struct UselessValueVisitorLIR : LIR::Visitor {
    std::set<Value *> uselessValueSet;

    void visit(ValueAddressingLIR *lir) override {
        uselessValueSet.erase(lir->base);
    }
};

bool EraseUselessGlobalValuePass_LIR::run() {
    UselessValueVisitorLIR visitor;
    auto variableSet = md.getGlobalVariableSetPtrOrder();
    visitor.uselessValueSet.insert(variableSet.begin(), variableSet.end());
    auto constantSet = md.getGlobalConstantSetPtrOrder();
    visitor.uselessValueSet.insert(constantSet.begin(), constantSet.end());
    auto functionSet = md.getFunctionSetPtrOrder();
    for (auto fn : functionSet) {
        auto blockSet = fn->getBasicBlockSetPtrOrder();
        for (auto bb : blockSet) {
            for (auto &lir : bb->lirTable) {
                lir->accept(visitor);
                if (visitor.uselessValueSet.empty()) {
                    return false;
                }
            }
        }
    }
    eraseUselessValue(visitor.uselessValueSet);
    return true;
}

bool EraseUselessLocalValuePass_LIR::run() {
    blockSet = fn.getBasicBlockSetPtrOrder();
    initUselessAssignmentSet();
    updateUselessAssignmentSet();
    vector<unique_ptr<LIR>> toErase;
    for (auto bb : blockSet) {
        vector<unique_ptr<LIR>> lirTable;
        for (auto &lir : bb->lirTable) {
            auto lir_assignment = dynamic_cast<RegAssign *>(lir.get());
            if (uselessAssignmentSet.find(lir_assignment) != uselessAssignmentSet.end()) {
                toErase.emplace_back(std::move(lir));
            } else {
                lirTable.emplace_back(std::move(lir));
            }
        }
        lirTable.swap(bb->lirTable);
    }

    UselessValueVisitorLIR visitor;
    auto variableSet = fn.getLocalVariableSetPtrOrder();
    visitor.uselessValueSet.insert(variableSet.begin(), variableSet.end());
    for (auto arg : fn.args) {
        visitor.uselessValueSet.erase(arg);
    }
    auto constantSet = fn.getLocalConstantSetPtrOrder();
    visitor.uselessValueSet.insert(constantSet.begin(), constantSet.end());
    for (auto bb : blockSet) {
        for (auto &lir : bb->lirTable) {
            lir->accept(visitor);
            if (visitor.uselessValueSet.empty()) {
                return !toErase.empty();
            }
        }
    }
    eraseUselessValue(visitor.uselessValueSet);
    return true;
}

void EraseUselessLocalValuePass_LIR::initUselessAssignmentSet() {
    struct MyVisitor : LIR::Visitor {
        std::set<RegAssign *> uselessAssignmentSet;
        std::queue<RegAssign *> usefulAssignmentQueue;

        void visitCondInst(CondInst *lir) {
            if (lir->cpsr != nullptr) {
                usefulAssignmentQueue.push(lir->cpsr);
            }
            if (lir->rd != nullptr) {
                usefulAssignmentQueue.push(lir->rd);
            }
        }

        void visit(UnaryRegInst *lir) override {
            uselessAssignmentSet.insert(lir);
        }

        void visit(BinaryRegInst *lir) override {
            uselessAssignmentSet.insert(lir);
        }

        void visit(UnaryShiftInst *lir) override {
            uselessAssignmentSet.insert(lir);
        }

        void visit(BinaryShiftInst *lir) override {
            uselessAssignmentSet.insert(lir);
        }

        void visit(UnaryImmInst *lir) override {
            uselessAssignmentSet.insert(lir);
        }

        void visit(BinaryImmInst *lir) override {
            uselessAssignmentSet.insert(lir);
        }

        void visit(SetFlagInst *lir) override {
            uselessAssignmentSet.insert(lir);
        }

        void visit(LoadRegInst *lir) override {
            uselessAssignmentSet.insert(lir);
        }

        void visit(LoadImmInst *lir) override {
            uselessAssignmentSet.insert(lir);
        }

        void visit(StoreRegInst *lir) override {
            usefulAssignmentQueue.push(lir->rm);
            usefulAssignmentQueue.push(lir->rn);
            usefulAssignmentQueue.push(lir->rt);
            visitCondInst(lir);
        }

        void visit(StoreImmInst *lir) override {
            usefulAssignmentQueue.push(lir->rn);
            usefulAssignmentQueue.push(lir->rt);
            visitCondInst(lir);
        }

        void visit(WriteBackAddressInst *lir) override {
            uselessAssignmentSet.insert(lir);
        }

        void visit(MultiplyInst *lir) override {
            uselessAssignmentSet.insert(lir);
        }

        void visit(Multiply64GetHighInst *lir) override {
            uselessAssignmentSet.insert(lir);
        }

        void visit(DivideInst *lir) override {
            uselessAssignmentSet.insert(lir);
        }

        void visit(MovwInst *lir) override {
            uselessAssignmentSet.insert(lir);
        }

        void visit(MovtInst *lir) override {
            uselessAssignmentSet.insert(lir);
        }

        void visit(BranchInst *lir) override {
            if (lir->cpsr != nullptr) {
                usefulAssignmentQueue.push(lir->cpsr);
            }
        }

        void visit(BranchAndLinkInst *lir) override {
            if (lir->cpsr != nullptr) {
                usefulAssignmentQueue.push(lir->cpsr);
            }
        }

        void visit(LoadImm32LIR *lir) override {
            uselessAssignmentSet.insert(lir);
        }

        void visit(ValueAddressingLIR *lir) override {
            uselessAssignmentSet.insert(lir);
        }

        void visit(GetArgumentLIR *lir) override {
            uselessAssignmentSet.insert(lir);
        }

        void visit(SetArgumentLIR *lir) override {
            usefulAssignmentQueue.push(lir->src);
        }

        void visit(GetReturnValueLIR *lir) override {
            uselessAssignmentSet.insert(lir);
        }

        void visit(ReturnLIR *lir) override {
            if (lir->val != nullptr) {
                usefulAssignmentQueue.push(lir->val);
            }
        }

        void visit(LoadScalarLIR *lir) override {
            uselessAssignmentSet.insert(lir);
        }

        void visit(StoreScalarLIR *lir) override {
            usefulAssignmentQueue.push(lir->src);
        }

//        void visit(LoadVectorLIR *lir) override {
//            uselessAssignmentSet.insert(lir);
//        }
//
//        void visit(StoreVectorLIR *lir) override {
//            usefulAssignmentQueue.push(lir->src);
//        }

        void visit(UninitializedCoreReg *lir) override {
            uselessAssignmentSet.insert(lir);
        }

//        void visit(UninitializedNeonReg *lir) override {
//            uselessAssignmentSet.insert(lir);
//        }

        void visit(CoreRegPhiLIR *lir) override {
            uselessAssignmentSet.insert(lir);
        }

//        void visit(NeonRegPhiLIR *lir) override {
//            uselessAssignmentSet.insert(lir);
//        }


        void visit(AtomicLoopCondLIR *mir) override {
            usefulAssignmentQueue.push(mir);
            usefulAssignmentQueue.push(mir->atomic_var_ptr);
            usefulAssignmentQueue.push(mir->step);
            usefulAssignmentQueue.push(mir->border);
            usefulAssignmentQueue.push(mir->tmp);
        }
    };


    MyVisitor visitor;
    for (auto bb : blockSet) {
        for (auto &lir : bb->lirTable) {
            lir->accept(visitor);
        }
    }
    uselessAssignmentSet.swap(visitor.uselessAssignmentSet);
    usefulAssignmentQueue.swap(visitor.usefulAssignmentQueue);
}

void EraseUselessLocalValuePass_LIR::updateUselessAssignmentSet() {
    struct MyVisitor : LIR::Visitor {
        std::set<RegAssign *> uselessAssignmentSet;
        std::queue<RegAssign *> usefulAssignmentQueue;

        void visitCondInst(CondInst *lir) {
            if (lir->cpsr != nullptr) {
                usefulAssignmentQueue.push(lir->cpsr);
            }
            if (lir->rd != nullptr) {
                usefulAssignmentQueue.push(lir->rd);
            }
        }

        void visit(UnaryRegInst *lir) override {
            if (uselessAssignmentSet.erase(lir) > 0) {
                usefulAssignmentQueue.push(lir->rm);
                visitCondInst(lir);
            }
        }

        void visit(BinaryRegInst *lir) override {
            if (uselessAssignmentSet.erase(lir) > 0) {
                usefulAssignmentQueue.push(lir->rn);
                usefulAssignmentQueue.push(lir->rm);
                visitCondInst(lir);
            }
        }

        void visit(CompareRegInst *lir) override {
            usefulAssignmentQueue.push(lir->rn);
            usefulAssignmentQueue.push(lir->rm);
            visitCondInst(lir);
        }

        void visit(UnaryShiftInst *lir) override {
            if (uselessAssignmentSet.erase(lir) > 0) {
                usefulAssignmentQueue.push(lir->rm);
                usefulAssignmentQueue.push(lir->rs);
                visitCondInst(lir);
            }
        }

        void visit(BinaryShiftInst *lir) override {
            if (uselessAssignmentSet.erase(lir) > 0) {
                usefulAssignmentQueue.push(lir->rn);
                usefulAssignmentQueue.push(lir->rm);
                usefulAssignmentQueue.push(lir->rs);
                visitCondInst(lir);
            }
        }

        void visit(CompareShiftInst *lir) override {
            usefulAssignmentQueue.push(lir->rn);
            usefulAssignmentQueue.push(lir->rm);
            usefulAssignmentQueue.push(lir->rs);
            visitCondInst(lir);
        }

        void visit(UnaryImmInst *lir) override {
            if (uselessAssignmentSet.erase(lir) > 0) {
                visitCondInst(lir);
            }
        }

        void visit(BinaryImmInst *lir) override {
            if (uselessAssignmentSet.erase(lir) > 0) {
                usefulAssignmentQueue.push(lir->rn);
                visitCondInst(lir);
            }
        }

        void visit(CompareImmInst *lir) override {
            usefulAssignmentQueue.push(lir->rn);
            visitCondInst(lir);
        }

        void visit(SetFlagInst *lir) override {
            if (uselessAssignmentSet.erase(lir) > 0) {
                lir->lastInst->accept(*this);
            }
        }

        void visit(LoadRegInst *lir) override {
            if (uselessAssignmentSet.erase(lir) > 0) {
                usefulAssignmentQueue.push(lir->rn);
                usefulAssignmentQueue.push(lir->rm);
                visitCondInst(lir);
            }
        }

        void visit(LoadImmInst *lir) override {
            if (uselessAssignmentSet.erase(lir) > 0) {
                usefulAssignmentQueue.push(lir->rn);
                visitCondInst(lir);
            }
        }

        void visit(WriteBackAddressInst *lir) override {
            if (uselessAssignmentSet.erase(lir) > 0) {
                lir->lastInst->accept(*this);
            }
        }

        void visit(MultiplyInst *lir) override {
            if (uselessAssignmentSet.erase(lir) > 0) {
                usefulAssignmentQueue.push(lir->rn);
                usefulAssignmentQueue.push(lir->rm);
                if (lir->ra != nullptr) {
                    usefulAssignmentQueue.push(lir->ra);
                }
                visitCondInst(lir);
            }
        }

        void visit(Multiply64GetHighInst *lir) override {
            if (uselessAssignmentSet.erase(lir) > 0) {
                usefulAssignmentQueue.push(lir->rn);
                usefulAssignmentQueue.push(lir->rm);
                visitCondInst(lir);
            }
        }

        void visit(DivideInst *lir) override {
            if (uselessAssignmentSet.erase(lir) > 0) {
                usefulAssignmentQueue.push(lir->rn);
                usefulAssignmentQueue.push(lir->rm);
                visitCondInst(lir);
            }
        }

        void visit(MovwInst *lir) override {
            if (uselessAssignmentSet.erase(lir) > 0) {
                visitCondInst(lir);
            }
        }

        void visit(MovtInst *lir) override {
            if (uselessAssignmentSet.erase(lir) > 0) {
                visitCondInst(lir);
            }
        }

        void visit(LoadImm32LIR *lir) override {
            uselessAssignmentSet.erase(lir);
        }

        void visit(ValueAddressingLIR *lir) override {
            uselessAssignmentSet.erase(lir);
        }

        void visit(GetArgumentLIR *lir) override {
            uselessAssignmentSet.erase(lir);
        }

        void visit(GetReturnValueLIR *lir) override {
            uselessAssignmentSet.erase(lir);
        }

        void visit(LoadScalarLIR *lir) override {
            uselessAssignmentSet.erase(lir);
        }

//        void visit(LoadVectorLIR *lir) override {
//            uselessAssignmentSet.erase(lir);
//        }

        void visit(UninitializedCoreReg *lir) override {
            uselessAssignmentSet.erase(lir);
        }

//        void visit(UninitializedNeonReg *lir) override {
//            uselessAssignmentSet.erase(lir);
//        }

        void visit(CoreRegPhiLIR *lir) override {
            if (uselessAssignmentSet.erase(lir) > 0) {
                for (auto &item : lir->incomingTable) {
                    usefulAssignmentQueue.push(item.second);
                }
            }
        }

//        void visit(NeonRegPhiLIR *lir) override {
//            if (uselessAssignmentSet.erase(lir) > 0) {
//                for (auto &item : lir->incomingTable) {
//                    usefulAssignmentQueue.push(item.second);
//                }
//            }
//        }
    };


    MyVisitor visitor;
    uselessAssignmentSet.swap(visitor.uselessAssignmentSet);
    usefulAssignmentQueue.swap(visitor.usefulAssignmentQueue);
    while (!visitor.usefulAssignmentQueue.empty()) {
        visitor.usefulAssignmentQueue.front()->castToLIR()->accept(visitor);
        visitor.usefulAssignmentQueue.pop();
    }
    uselessAssignmentSet.swap(visitor.uselessAssignmentSet);
}

