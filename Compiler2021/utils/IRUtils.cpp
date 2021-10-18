//
// Created by 陈思言 on 2021/8/1.
//

#include <stack>
#include "IRUtils.h"
#include "../IR/Function.h"

MIR *copyMIR(MIR *ir, int &currentId) {

    struct Visitor : MIR::Visitor {
        MIR *newIR = nullptr;
        int &newId;

        explicit Visitor(int &newId) : newId(newId) {};


        void visit(UnaryMIR *mir) override {
            newIR = new UnaryMIR(mir->op, mir->getType(), mir->getName(), newId++, mir->src);
        }

        void visit(BinaryMIR *mir) override {
            newIR = new BinaryMIR(mir->op, mir->getType(), mir->getName(), newId++, mir->src1, mir->src2);
        }

        void visit(ArrayAddressingMIR *mir) override {
            newIR = new ArrayAddressingMIR(mir->getType(), mir->getName(), newId++, mir->base, mir->offset);
        }

        void visit(LoadConstantMIR *mir) override {
            newIR = new LoadConstantMIR(mir->getType(), mir->getName(), newId++, mir->src);
        }

        void visit(ValueAddressingMIR *mir) override {
            newIR = new ValueAddressingMIR(mir->getType(), mir->getName(), newId++, mir->base);
        }


        void visit(SelectMIR *mir) override {
            newIR = new SelectMIR(mir->getType(), mir->getName(), newId++, mir->cond, mir->src1, mir->src2);
        }

        void visit(UninitializedMIR *mir) override {
            newIR = new UninitializedMIR(mir->getType(), mir->getName(), newId++);
        }

        void visit(LoadVariableMIR *mir) override {
            newIR = new LoadVariableMIR(mir->getType(), mir->getName(), newId++, mir->src);
        }

        void visit(LoadPointerMIR *mir) override {
            newIR = new LoadPointerMIR(mir->getType(), mir->getName(), newId++, mir->src);
        }

        void visit(PhiMIR *mir) override {
            auto *phi = new PhiMIR(mir->getType(), mir->getName(), newId++);
            for (auto x : mir->incomingTable) {
                phi->addIncoming(x.first, x.second);
            }
            newIR = phi;
        }

        void visit(StoreVariableMIR *mir) override {
            newIR = new StoreVariableMIR(*mir);
        }

        void visit(StorePointerMIR *mir) override {
            newIR = new StorePointerMIR(*mir);
        }

        void visit(MemoryCopyMIR *mir) override {
            newIR = new MemoryCopyMIR(*mir);
        }

        void visit(MemoryFillMIR *mir) override {
            newIR = new MemoryFillMIR(*mir);
        }

        void visit(JumpMIR *mir) override {
            newIR = new JumpMIR(*mir);
        }

        void visit(BranchMIR *mir) override {
            newIR = new BranchMIR(*mir);
        }

        void visit(CallMIR *mir) override {
            newIR = new CallMIR(*mir);
        }

        void visit(CallWithAssignMIR *mir) override {
            newIR = new CallWithAssignMIR(mir->getType(), mir->getName(), newId++, mir->func, mir->args);
        }

        void visit(MultiCallMIR *mir) override {
            newIR = new MultiCallMIR(*mir);
        }

        void visit(ReturnMIR *mir) override {
            newIR = new ReturnMIR(*mir);
        }

        void visit(AtomicLoopCondMIR *mir) override {
            newIR = new AtomicLoopCondMIR(mir->getName(), newId++, mir->atomic_var, mir->update_op, mir->step,
                                          mir->compare_op, mir->border, mir->body, mir->exit);
        }
    };
    Visitor v{currentId};
    ir->accept(v);
    return v.newIR;
}


MIR *
cloneMIR(MIR *mir, std::map<Assignment *, Assignment *> &replaceTable, std::map<BasicBlock *, BasicBlock *> blockTable,
         std::map<Constant *, Constant *> &constantTable,
         std::map<Variable *, Variable *> &variableTable) {
    struct MyVisitor : MIR::Visitor {
        std::map<Assignment *, Assignment *> &replaceTable;
        std::map<Constant *, Constant *> &constantTable;
        std::map<Variable *, Variable *> &variableTable;
        std::map<BasicBlock *, BasicBlock *> &blockTable;
        MIR *new_mir = nullptr;

        explicit MyVisitor(std::map<Assignment *, Assignment *> &replaceTable,
                           std::map<BasicBlock *, BasicBlock *> &blockTable,
                           std::map<Constant *, Constant *> &constantTable,
                           std::map<Variable *, Variable *> &variableTable)
                : replaceTable(replaceTable), blockTable(blockTable),
                  constantTable(constantTable), variableTable(variableTable) {}

        void visit(UninitializedMIR *mir) override {
            auto newMIR = new UninitializedMIR(*mir);
            new_mir = newMIR;
            replaceTable[mir] = newMIR;
        }

        void visit(LoadConstantMIR *mir) override {
            auto newConst = mir->src;
            if (constantTable.count(newConst) != 0) newConst = constantTable[newConst];
            auto newMIR = new LoadConstantMIR(mir->getType(), mir->getName(), mir->id, newConst);
            new_mir = newMIR;
            replaceTable[mir] = newMIR;
        }

        void visit(LoadVariableMIR *mir) override {
            auto newVar = mir->src;
            if (variableTable.count(newVar) != 0) newVar = variableTable[newVar];
            auto newMIR = new LoadVariableMIR(mir->getType(), mir->getName(), mir->id, newVar);
            new_mir = newMIR;
            replaceTable[mir] = newMIR;
        }

        void visit(LoadPointerMIR *mir) override {
            auto newMIR = new LoadPointerMIR(*mir);
            new_mir = newMIR;
            replaceTable[mir] = newMIR;
        }

        void visit(StoreVariableMIR *mir) override {
            auto newVar = mir->dst;
            if (variableTable.count(newVar) != 0) newVar = variableTable[newVar];
            new_mir = new StoreVariableMIR(newVar, mir->src);
        }

        void visit(StorePointerMIR *mir) override {
            new_mir = new StorePointerMIR(*mir);
        }

        void visit(MemoryCopyMIR *mir) override {
            new_mir = new MemoryCopyMIR(*mir);
        }

        void visit(MemoryFillMIR *mir) override {
            new_mir = new MemoryFillMIR(*mir);
        }

        void visit(UnaryMIR *mir) override {
            auto newMIR = new UnaryMIR(*mir);
            new_mir = newMIR;
            replaceTable[mir] = newMIR;
        }

        void visit(BinaryMIR *mir) override {
            auto newMIR = new BinaryMIR(*mir);
            new_mir = newMIR;
            replaceTable[mir] = newMIR;
        }

        void visit(ValueAddressingMIR *mir) override {
            auto base = mir->base;
            if (base->isConstant()) {
                auto newConst = dynamic_cast<Constant *>(base);
                if (constantTable.count(newConst) != 0) newConst = constantTable[newConst];
                auto newMIR = new ValueAddressingMIR(mir->getType(), mir->getName(), mir->id, newConst);
                new_mir = newMIR;
                replaceTable[mir] = newMIR;
            } else {
                auto newVar = dynamic_cast<Variable *>(base);
                if (variableTable.count(newVar) != 0) newVar = variableTable[newVar];
                auto newMIR = new ValueAddressingMIR(mir->getType(), mir->getName(), mir->id, newVar);
                new_mir = newMIR;
                replaceTable[mir] = newMIR;
            }
        }

        void visit(ArrayAddressingMIR *mir) override {
            auto base_ptr = dynamic_cast<Assignment *>(mir->base);
            if (base_ptr != nullptr) {
                auto newMIR = new ArrayAddressingMIR(*mir);
                new_mir = newMIR;
                replaceTable[mir] = newMIR;
            } else if (mir->base->isConstant()) {
                auto newConst = dynamic_cast<Constant *>(mir->base);
                if (constantTable.count(newConst) != 0) newConst = constantTable[newConst];
                auto newMIR = new ArrayAddressingMIR(mir->getType(), mir->getName(), mir->id, newConst, mir->offset);
                new_mir = newMIR;
                replaceTable[mir] = newMIR;
            } else {
                auto newVar = dynamic_cast<Variable *>(mir->base);
                if (variableTable.count(newVar) != 0) newVar = variableTable[newVar];
                auto newMIR = new ArrayAddressingMIR(mir->getType(), mir->getName(), mir->id, newVar, mir->offset);
                new_mir = newMIR;
                replaceTable[mir] = newMIR;
            }
        }

        void visit(SelectMIR *mir) override {
            auto newMIR = new SelectMIR(*mir);
            new_mir = newMIR;
            replaceTable[mir] = newMIR;
        }

        void visit(JumpMIR *mir) override {
            new_mir = new JumpMIR(blockTable[mir->block]);
        }

        void visit(BranchMIR *mir) override {
            new_mir = new BranchMIR(mir->cond, blockTable[mir->block1], blockTable[mir->block2]);
        }

        void visit(CallMIR *mir) override {
            new_mir = new CallMIR(*mir);
        }

        void visit(CallWithAssignMIR *mir) override {
            auto newMIR = new CallWithAssignMIR(*mir);
            replaceTable[mir] = newMIR;
            new_mir = newMIR;
        }

        void visit(MultiCallMIR *mir) override {
            auto newMIR = new MultiCallMIR(*mir);
            auto newVar = mir->atomic_var;
            if (variableTable.count(newVar) != 0) newVar = variableTable[newVar];
            newMIR->atomic_var = newVar;
            new_mir = newMIR;
        }

        void visit(ReturnMIR *mir) override {
            auto newMIR = new ReturnMIR(*mir);
            new_mir = newMIR;
        }

        void visit(PhiMIR *mir) override {
            auto newMIR = new PhiMIR(mir->getType(), mir->getName(), mir->id);
            for (auto &incoming : mir->incomingTable) {
                newMIR->addIncoming(blockTable[incoming.first], incoming.second);
            }
            new_mir = newMIR;
            replaceTable[mir] = newMIR;
        }

        void visit(AtomicLoopCondMIR *mir) override {
            auto newMIR = new AtomicLoopCondMIR(*mir);
            auto newVar = mir->atomic_var;
            if (variableTable.count(newVar) != 0) newVar = variableTable[newVar];
            newMIR->atomic_var = newVar;
            replaceTable[mir] = newMIR;
            new_mir = newMIR;
        }
    };

    MyVisitor visitor(replaceTable, blockTable, constantTable, variableTable);
    visitor.new_mir = mir;
    mir->accept(visitor);
    return visitor.new_mir;
}

MIR *
cloneMIR(MIR *mir, std::map<Assignment *, Assignment *> &replaceTable, std::map<BasicBlock *, BasicBlock *> blockTable,
         std::map<Constant *, Constant *> &constantTable,
         std::map<Variable *, Variable *> &variableTable, CallMIR *callMir) {

    struct MyVisitor : MIR::Visitor {
        std::map<Assignment *, Assignment *> &replaceTable;
        std::map<Constant *, Constant *> &constantTable;
        std::map<Variable *, Variable *> &variableTable;
        std::map<BasicBlock *, BasicBlock *> &blockTable;
        MIR *new_mir;
        CallMIR *callMir;

        explicit MyVisitor(std::map<Assignment *, Assignment *> &replaceTable,
                           std::map<BasicBlock *, BasicBlock *> &blockTable,
                           std::map<Constant *, Constant *> &constantTable,
                           std::map<Variable *, Variable *> &variableTable, CallMIR *callMir)
                : replaceTable(replaceTable), blockTable(blockTable),
                  constantTable(constantTable), variableTable(variableTable),
                  callMir(callMir) {}

        void visit(UninitializedMIR *mir) override {
            auto newMIR = new UninitializedMIR(*mir);
            new_mir = newMIR;
            replaceTable[mir] = newMIR;
        }

        void visit(LoadConstantMIR *mir) override {
            auto newConst = mir->src;
            if (constantTable.count(newConst) != 0) newConst = constantTable[newConst];
            auto newMIR = new LoadConstantMIR(mir->getType(), mir->getName(), mir->id, newConst);
            new_mir = newMIR;
            replaceTable[mir] = newMIR;
        }

        void visit(LoadVariableMIR *mir) override {
            if (mir->src->isArgument()) {
                size_t i = 0;
                auto func = callMir->func;
                for (; i < func->args.size(); i++) {
                    if (func->args[i]->getName() == mir->src->getName()) {
                        break;
                    }
                }
                replaceTable[mir] = callMir->args[i];
                new_mir = nullptr;
            } else {
                auto newVar = mir->src;
                if (variableTable.count(newVar) != 0) newVar = variableTable[newVar];
                auto newMIR = new LoadVariableMIR(mir->getType(), mir->getName(), mir->id, newVar);
                new_mir = newMIR;
                replaceTable[mir] = newMIR;
            }
        }

        void visit(LoadPointerMIR *mir) override {
            auto newMIR = new LoadPointerMIR(*mir);
            new_mir = newMIR;
            replaceTable[mir] = newMIR;
        }

        void visit(StoreVariableMIR *mir) override {
            auto newVar = mir->dst;
            if (variableTable.count(newVar) != 0) newVar = variableTable[newVar];
            auto newMIR = new StoreVariableMIR(newVar, mir->src);
            new_mir = newMIR;
        }

        void visit(StorePointerMIR *mir) override {
            auto newMIR = new StorePointerMIR(*mir);
            new_mir = newMIR;
        }

        void visit(MemoryCopyMIR *mir) override {
            auto newMIR = new MemoryCopyMIR(*mir);
            new_mir = newMIR;
        }

        void visit(MemoryFillMIR *mir) override {
            auto newMIR = new MemoryFillMIR(*mir);
            new_mir = newMIR;
        }

        void visit(UnaryMIR *mir) override {
            auto newMIR = new UnaryMIR(*mir);
            new_mir = newMIR;
            replaceTable[mir] = newMIR;
        }

        void visit(BinaryMIR *mir) override {
            auto newMIR = new BinaryMIR(*mir);
            new_mir = newMIR;
            replaceTable[mir] = newMIR;
        }

        void visit(ValueAddressingMIR *mir) override {
            auto base = mir->base;
            if (base->isConstant()) {
                auto newConst = dynamic_cast<Constant *>(base);
                if (constantTable.count(newConst) != 0) newConst = constantTable[newConst];
                auto newMIR = new ValueAddressingMIR(mir->getType(), mir->getName(), mir->id, newConst);
                new_mir = newMIR;
                replaceTable[mir] = newMIR;
            } else {
                auto newVar = dynamic_cast<Variable *>(base);
                if (variableTable.count(newVar) != 0) newVar = variableTable[newVar];
                auto newMIR = new ValueAddressingMIR(mir->getType(), mir->getName(), mir->id, newVar);
                new_mir = newMIR;
                replaceTable[mir] = newMIR;
            }
        }

        void visit(ArrayAddressingMIR *mir) override {
            auto base_ptr = dynamic_cast<Assignment *>(mir->base);
            if (base_ptr != nullptr) {
                auto newMIR = new ArrayAddressingMIR(*mir);
                new_mir = newMIR;
                replaceTable[mir] = newMIR;
            } else if (mir->base->isConstant()) {
                auto newConst = dynamic_cast<Constant *>(mir->base);
                if (constantTable.count(newConst) != 0) newConst = constantTable[newConst];
                auto newMIR = new ArrayAddressingMIR(mir->getType(), mir->getName(), mir->id, newConst, mir->offset);
                new_mir = newMIR;
                replaceTable[mir] = newMIR;
            } else {
                auto newVar = dynamic_cast<Variable *>(mir->base);
                if (variableTable.count(newVar) != 0) newVar = variableTable[newVar];
                auto newMIR = new ArrayAddressingMIR(mir->getType(), mir->getName(), mir->id, newVar, mir->offset);
                new_mir = newMIR;
                replaceTable[mir] = newMIR;
            }
        }

        void visit(SelectMIR *mir) override {
            auto newMIR = new SelectMIR(*mir);
            new_mir = newMIR;
            replaceTable[mir] = newMIR;
        }

        void visit(JumpMIR *mir) override {
            auto newMIR = new JumpMIR(blockTable[mir->block]);
            new_mir = newMIR;
        }

        void visit(BranchMIR *mir) override {
            auto newMIR = new BranchMIR(mir->cond, blockTable[mir->block1], blockTable[mir->block2]);
            new_mir = newMIR;
        }

        void visit(CallMIR *mir) override {
            new_mir = new CallMIR(*mir);
        }

        void visit(CallWithAssignMIR *mir) override {
            vector<Assignment *> newArgs(mir->args);
            auto newMIR = new CallWithAssignMIR(*mir);
            replaceTable[mir] = newMIR;
            new_mir = newMIR;
        }

        void visit(MultiCallMIR *mir) override {
            auto newMIR = new MultiCallMIR(*mir);
            auto newVar = mir->atomic_var;
            if (variableTable.count(newVar) != 0) newVar = variableTable[newVar];
            newMIR->atomic_var = newVar;
            new_mir = newMIR;
        }

        void visit(ReturnMIR *mir) override {
            auto newMIR = new ReturnMIR(mir->val);
            new_mir = newMIR;
        }

        void visit(PhiMIR *mir) override {
            auto newMIR = new PhiMIR(mir->getType(), mir->getName(), mir->id);
            for (auto &incoming : mir->incomingTable) {
                newMIR->addIncoming(blockTable[incoming.first], incoming.second);
            }
            new_mir = newMIR;
            replaceTable[mir] = newMIR;
        }

        void visit(AtomicLoopCondMIR *mir) override {
            auto newMIR = new AtomicLoopCondMIR(*mir);
            auto newVar = mir->atomic_var;
            if (variableTable.count(newVar) != 0) newVar = variableTable[newVar];
            newMIR->atomic_var = newVar;
            replaceTable[mir] = newMIR;
            new_mir = newMIR;
        }
    };

    MyVisitor visitor(replaceTable, blockTable, constantTable, variableTable, callMir);
    visitor.new_mir = mir;
    mir->accept(visitor);
    return visitor.new_mir;
}

#pragma clang diagnostic push
#pragma ide diagnostic ignored "hicpp-signed-bitwise"

optional<int> calculate(Assignment *val, std::map<Assignment *, optional<int>> &assignValues,
                        std::map<Assignment *, optional<int>> &visitedValues) {
    if (assignValues.count(val))
        return assignValues[val];
    if (visitedValues.count(val))
        return assignValues[val];
    auto cnst = dynamic_cast<LoadConstantMIR *>(val);
    if (cnst != nullptr) {
        return visitedValues[val] = cnst->src->getValue<int>();
    }
    auto binary = dynamic_cast<BinaryMIR *>(val);
    if (binary != nullptr) {
        auto a = calculate(binary->src1, assignValues, visitedValues);
        auto b = calculate(binary->src2, assignValues, visitedValues);
        if (!a.has_value() || !b.has_value()) return visitedValues[val] = optional<int>();
        int x = a.value(), y = b.value();
        switch (binary->op) {
            case BinaryMIR::Operator::ADD:
                return visitedValues[val] = x + y;
            case BinaryMIR::Operator::SUB:
                return visitedValues[val] = x - y;
            case BinaryMIR::Operator::MUL:
                return visitedValues[val] = x * y;
            case BinaryMIR::Operator::DIV:
                return visitedValues[val] = y == 0 ? 0 : x / y;
            case BinaryMIR::Operator::MOD:
                return visitedValues[val] = y == 0 ? 0 : x % y;
            case BinaryMIR::Operator::AND:
                return visitedValues[val] = x & y;
            case BinaryMIR::Operator::OR:
                return visitedValues[val] = x | y;
            case BinaryMIR::Operator::XOR:
                return visitedValues[val] = x ^ y;
            case BinaryMIR::Operator::LSL:
                return visitedValues[val] = x << y;
            case BinaryMIR::Operator::LSR:
                return visitedValues[val] = (unsigned int) x >> y;
            case BinaryMIR::Operator::ASR:
                return visitedValues[val] = x >> y;
            case BinaryMIR::Operator::CMP_EQ:
                return visitedValues[val] = x == y;
            case BinaryMIR::Operator::CMP_NE:
                return visitedValues[val] = x != y;
            case BinaryMIR::Operator::CMP_GT:
                return visitedValues[val] = x > y;
            case BinaryMIR::Operator::CMP_LT:
                return visitedValues[val] = x < y;
            case BinaryMIR::Operator::CMP_GE:
                return visitedValues[val] = x >= y;
            case BinaryMIR::Operator::CMP_LE:
                return visitedValues[val] = x <= y;
        }
    }
    auto unary = dynamic_cast<UnaryMIR *>(val);
    if (unary != nullptr) {
        auto a = calculate(unary->src, assignValues, visitedValues);
        if (!a.has_value()) return visitedValues[val] = optional<int>();
        int v = a.value();
        switch (unary->op) {
            case UnaryMIR::Operator::NEG:
                return visitedValues[val] = -v;
            case UnaryMIR::Operator::NOT:
                return visitedValues[val] = !v;
            case UnaryMIR::Operator::REV:
                return visitedValues[val] = ~v;
        }
    }

    auto select = dynamic_cast<SelectMIR *>(val);
    if (select != nullptr) {
        auto cond = calculate(select->cond, assignValues, visitedValues);
        if (!cond.has_value())
            return visitedValues[val] = false;
        if (cond.value())return visitedValues[val] = calculate(binary->src1, assignValues, visitedValues);
        else return visitedValues[val] = calculate(binary->src2, assignValues, visitedValues);
    }
    return visitedValues[val] = optional<int>();
}

#pragma clang diagnostic pop