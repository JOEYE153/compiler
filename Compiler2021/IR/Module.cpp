//
// Created by 陈思言 on 2021/5/21.
//

#include <cassert>
#include <sstream>

#include "Module.h"

using std::make_pair;
using std::stringstream;

Variable *Module::declareGlobalVariable(shared_ptr<Type> type, string_view variable_name, std::any value) {
    assert(getGlobalVariableByName(variable_name) == nullptr);
    auto variable = new Variable(std::move(type), variable_name, Value::Location::STATIC, false);
    if (value.has_value()) {
        initializationTable.emplace(make_pair(variable, std::move(value)));
    }
    variableTable.emplace(make_pair(variable_name, variable));
    return variable;
}

Constant *Module::declareGlobalConstant(shared_ptr<Type> type, string_view constant_name, std::any value) {
    assert(getGlobalConstantByName(constant_name) == nullptr);
    auto constant = new Constant(std::move(type), constant_name, std::move(value));
    constantTable.emplace(make_pair(constant_name, constant));
    return constant;
}

Function *Module::declareFunction(shared_ptr<FunctionType> type, string_view function_name) {
    assert(getFunctionByName(function_name) == nullptr);
    auto function = new Function(std::move(type), function_name);
    functionTable.emplace(make_pair(function_name, function));
    return function;
}

Variable *Module::getGlobalVariableByName(string_view variable_name) const {
    auto iter = variableTable.find(string(variable_name));
    return iter == variableTable.end() ? nullptr : iter->second.get();
}

Constant *Module::getGlobalConstantByName(string_view constant_name) const {
    auto iter = constantTable.find(string(constant_name));
    return iter == constantTable.end() ? nullptr : iter->second.get();
}

Function *Module::getFunctionByName(string_view function_name) const {
    auto iter = functionTable.find(string(function_name));
    return iter == functionTable.end() ? nullptr : iter->second.get();
}

std::vector<Variable *> Module::getGlobalVariableVecDictOrder() const {
    std::vector<Variable *> result;
    result.reserve(variableTable.size());
    for (auto iter = variableTable.begin(); iter != variableTable.end(); iter++) {
        result.push_back(iter->second.get());
    }
    return std::move(result);
}

std::vector<Constant *> Module::getGlobalConstantVecDictOrder() const {
    std::vector<Constant *> result;
    result.reserve(constantTable.size());
    for (auto iter = constantTable.begin(); iter != constantTable.end(); iter++) {
        result.push_back(iter->second.get());
    }
    return std::move(result);
}

std::vector<Function *> Module::getFunctionVecDictOrder() const {
    std::vector<Function *> result;
    result.reserve(functionTable.size());
    for (auto iter = functionTable.begin(); iter != functionTable.end(); iter++) {
        result.push_back(iter->second.get());
    }
    return std::move(result);
}

std::set<Variable *> Module::getGlobalVariableSetPtrOrder() const {
    std::set<Variable *> result;
    for (auto iter = variableTable.begin(); iter != variableTable.end(); iter++) {
        result.insert(iter->second.get());
    }
    return std::move(result);
}

std::set<Constant *> Module::getGlobalConstantSetPtrOrder() const {
    std::set<Constant *> result;
    for (auto iter = constantTable.begin(); iter != constantTable.end(); iter++) {
        result.insert(iter->second.get());
    }
    return std::move(result);
}

std::set<Function *> Module::getFunctionSetPtrOrder() const {
    std::set<Function *> result;
    for (auto iter = functionTable.begin(); iter != functionTable.end(); iter++) {
        result.insert(iter->second.get());
    }
    return std::move(result);
}

void Module::eraseFunctionByName(string_view func_name) {
    functionTable.erase(string(func_name));
}

void Module::eraseGlobalVariableByName(string_view variable_name) {
    auto iter = variableTable.find(string(variable_name));
    initializationTable.erase(iter->second.get());
    variableTable.erase(iter);
}

void Module::eraseGlobalConstantByName(string_view constant_name) {
    constantArrayCache.erase(getGlobalConstantByName(constant_name));
    constantTable.erase(string(constant_name));
}

Constant *Module::declareGlobalImmediateInt(int value) {
    stringstream ss;
    ss << value;
    auto constant = getGlobalConstantByName(ss.str());
    if (constant == nullptr) {
        return declareGlobalConstant(IntegerType::object, ss.str(), value);
    }
    return constant;
}
