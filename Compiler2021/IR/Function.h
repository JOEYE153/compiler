//
// Created by 陈思言 on 2021/5/21.
//

#ifndef COMPILER2021_FUNCTION_H
#define COMPILER2021_FUNCTION_H

#include <set>
#include <map>
#include "BasicBlock.h"

class Function final {
public:
    Function(shared_ptr<FunctionType> type, string_view name);

    [[nodiscard]] shared_ptr<FunctionType> getType() const {
        return type;
    }

    [[nodiscard]] shared_ptr<Type> getReturnType() const {
        return type->getReturnType();
    }

    [[nodiscard]] const vector<shared_ptr<Type>> &getArgTypes() const {
        return type->getArgTypes();
    }

    [[nodiscard]] string getName() const {
        return name;
    }

    BasicBlock *createBasicBlock(string_view block_name);

    Variable *declareLocalVariable(shared_ptr<Type> type, string_view variable_name, bool is_ref, bool is_arg = false);

    Constant *declareLocalConstant(shared_ptr<Type> type, string_view constant_name, std::any value);

    Constant *declareLocalImmediateInt(int value);

    Constant *declareLocalImmediateBool(bool value);

    [[nodiscard]] BasicBlock *getBasicBlockByName(string_view block_name) const;

    [[nodiscard]] Variable *getLocalVariableByName(string_view variable_name) const;

    [[nodiscard]] Constant *getLocalConstantByName(string_view constant_name) const;

    [[nodiscard]] std::vector<BasicBlock *> getBasicBlockVecDictOrder() const;

    [[nodiscard]] std::vector<Variable *> getLocalVariableVecDictOrder() const;

    [[nodiscard]] std::vector<Constant *> getLocalConstantVecDictOrder() const;

    [[nodiscard]] std::set<BasicBlock *> getBasicBlockSetPtrOrder() const;

    [[nodiscard]] std::set<Variable *> getLocalVariableSetPtrOrder() const;

    [[nodiscard]] std::set<Constant *> getLocalConstantSetPtrOrder() const;

    void eraseBasicBlockByName(string_view block_name);

    void eraseLocalVariableByName(string_view variable_name);

    void eraseLocalConstantByName(string_view constant_name);

private:
    shared_ptr<FunctionType> type;
    string name;
    std::map<string, unique_ptr<BasicBlock>> basicBlockTable;
    std::map<string, unique_ptr<Variable>> variableTable;
    std::map<string, unique_ptr<Constant>> constantTable;

public:
    vector<Variable *> args;
    BasicBlock *entryBlock = nullptr;
    bool isExternal = false;
    bool hasAtomicVar = false;
    CoreRegAssign *atomic_var_ptr = nullptr;
};


#endif //COMPILER2021_FUNCTION_H
