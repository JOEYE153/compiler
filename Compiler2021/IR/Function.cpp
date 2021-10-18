//
// Created by 陈思言 on 2021/5/21.
//

#include <cassert>
#include <sstream>

using std::stringstream;

#include "Function.h"

using std::make_pair;

Function::Function(shared_ptr<FunctionType> type, string_view name)
        : type(std::move(type)), name(name) {
    args.resize(getArgTypes().size(), nullptr);
}

BasicBlock *Function::createBasicBlock(string_view block_name) {
    assert(getBasicBlockByName(block_name) == nullptr);
    auto block = new BasicBlock(block_name);
    basicBlockTable.emplace(make_pair(block_name, block));
    return block;
}

Variable *Function::declareLocalVariable(shared_ptr<Type> type, string_view variable_name, bool is_ref, bool is_arg) {
    assert(getLocalVariableByName(variable_name) == nullptr);
    auto variable = new Variable(std::move(type), variable_name,
                                 is_arg ? Value::Location::ARGUMENT : Value::Location::STACK, is_ref);
    variableTable.emplace(make_pair(variable_name, variable));
    return variable;
}

Constant *Function::declareLocalConstant(shared_ptr<Type> type, string_view constant_name, std::any value) {
    assert(getLocalConstantByName(constant_name) == nullptr);
    auto constant = new Constant(std::move(type), constant_name, std::move(value));
    constantTable.emplace(make_pair(constant_name, constant));
    return constant;
}

Constant *Function::declareLocalImmediateInt(int value) {
    stringstream ss;
    ss << value;
    auto constant = getLocalConstantByName(ss.str());
    if (constant == nullptr) {
        return declareLocalConstant(IntegerType::object, ss.str(), value);
    }
    return constant;
}

Constant *Function::declareLocalImmediateBool(bool value) {
    string constantName = value ? "1b" : "0b";
    auto constant = getLocalConstantByName(constantName);
    if (constant == nullptr) {
        return declareLocalConstant(BooleanType::object, constantName, value ? 1 : 0);
    }
    return constant;
}

BasicBlock *Function::getBasicBlockByName(string_view block_name) const {
    auto iter = basicBlockTable.find(string(block_name));
    return iter == basicBlockTable.end() ? nullptr : iter->second.get();
}

Variable *Function::getLocalVariableByName(string_view variable_name) const {
    auto iter = variableTable.find(string(variable_name));
    return iter == variableTable.end() ? nullptr : iter->second.get();
}

Constant *Function::getLocalConstantByName(string_view constant_name) const {
    auto iter = constantTable.find(string(constant_name));
    return iter == constantTable.end() ? nullptr : iter->second.get();
}

std::vector<BasicBlock *> Function::getBasicBlockVecDictOrder() const {
    std::vector<BasicBlock *> result;
    result.reserve(basicBlockTable.size());
    for (auto iter = basicBlockTable.begin(); iter != basicBlockTable.end(); iter++) {
        result.push_back(iter->second.get());
    }
    return std::move(result);
}

std::vector<Variable *> Function::getLocalVariableVecDictOrder() const {
    std::vector<Variable *> result;
    result.reserve(variableTable.size());
    for (auto iter = variableTable.begin(); iter != variableTable.end(); iter++) {
        result.push_back(iter->second.get());
    }
    return std::move(result);
}

std::vector<Constant *> Function::getLocalConstantVecDictOrder() const {
    std::vector<Constant *> result;
    result.reserve(constantTable.size());
    for (auto iter = constantTable.begin(); iter != constantTable.end(); iter++) {
        result.push_back(iter->second.get());
    }
    return std::move(result);
}

std::set<BasicBlock *> Function::getBasicBlockSetPtrOrder() const {
    std::set<BasicBlock *> result;
    for (auto iter = basicBlockTable.begin(); iter != basicBlockTable.end(); iter++) {
        result.insert(iter->second.get());
    }
    return std::move(result);
}

std::set<Variable *> Function::getLocalVariableSetPtrOrder() const {
    std::set<Variable *> result;
    for (auto iter = variableTable.begin(); iter != variableTable.end(); iter++) {
        result.insert(iter->second.get());
    }
    return std::move(result);
}

std::set<Constant *> Function::getLocalConstantSetPtrOrder() const {
    std::set<Constant *> result;
    for (auto iter = constantTable.begin(); iter != constantTable.end(); iter++) {
        result.insert(iter->second.get());
    }
    return std::move(result);
}

void Function::eraseBasicBlockByName(string_view block_name) {
    basicBlockTable.erase(string(block_name));
}

void Function::eraseLocalVariableByName(string_view variable_name) {
    variableTable.erase(string(variable_name));
}

void Function::eraseLocalConstantByName(string_view constant_name) {
    constantTable.erase(string(constant_name));
}
