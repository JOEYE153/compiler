//
// Created by 陈思言 on 2021/5/21.
//

#ifndef COMPILER2021_MODULE_H
#define COMPILER2021_MODULE_H

#include <utility>

#include "Function.h"

class ConstantArrayToMIR;

class Module final {
public:
    explicit Module(string name) : name(std::move(name)) {}

    [[nodiscard]] string getName() const {
        return name;
    }

    Variable *declareGlobalVariable(shared_ptr<Type> type, string_view variable_name, std::any value = {});

    Constant *declareGlobalConstant(shared_ptr<Type> type, string_view constant_name, std::any value);

    Constant *declareGlobalImmediateInt(int value);

    Function *declareFunction(shared_ptr<FunctionType> type, string_view function_name);

    [[nodiscard]] Variable *getGlobalVariableByName(string_view variable_name) const;

    [[nodiscard]] Constant *getGlobalConstantByName(string_view constant_name) const;

    [[nodiscard]] Function *getFunctionByName(string_view function_name) const;

    [[nodiscard]] std::vector<Variable *> getGlobalVariableVecDictOrder() const;

    [[nodiscard]] std::vector<Constant *> getGlobalConstantVecDictOrder() const;

    [[nodiscard]] std::vector<Function *> getFunctionVecDictOrder() const;

    [[nodiscard]] std::set<Variable *> getGlobalVariableSetPtrOrder() const;

    [[nodiscard]] std::set<Constant *> getGlobalConstantSetPtrOrder() const;

    [[nodiscard]] std::set<Function *> getFunctionSetPtrOrder() const;

    void eraseFunctionByName(string_view func_name);

    void eraseGlobalVariableByName(string_view variable_name);

    void eraseGlobalConstantByName(string_view constant_name);

private:
    string name;
    std::map<string, unique_ptr<Variable>> variableTable;
    std::map<string, unique_ptr<Constant>> constantTable;
    std::map<string, unique_ptr<Function>> functionTable;

public:
    std::map<Variable *, std::any> initializationTable;
    std::map<Constant *, ConstantArrayToMIR *> constantArrayCache;
    std::set<Function *> clonedNoRetFunctions;
    bool useThreadPool = false;
};


#endif //COMPILER2021_MODULE_H
