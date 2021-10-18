//
// Created by 陈思言 on 2021/7/7.
//

#include "DataSectionGenerator.h"

using std::make_pair;

DataSectionGenerator::DataSectionGenerator() {
    dataSection << "\t.section .data\n";
    rodataSection << "\t.section .rodata\n";
}

void DataSectionGenerator::addVariable(const vector<Variable *> &variableTable,
                                       const std::map<Variable *, std::any> &initializationTable) {
    for (auto variable : variableTable) {
        auto type = variable->getType();
        auto iter = initializationTable.find(variable);
        if (iter == initializationTable.end()) {
            dataSection << /*"g.v." <<*/ variable->getName() << ":\n";
            dataSection << "\t.space " << type->getSizeOfType() << '\n';
        } else {
            dataSection << /*"g.v." <<*/ variable->getName() << ":\n";
            if (type->getId() == Type::ID::ARRAY) {
                auto value = any_cast<vector<int>>(iter->second);
                for (auto v : value) {
                    dataSection << "\t.word " << v << '\n';
                }
            } else {
                dataSection << "\t.word " << any_cast<int>(iter->second) << '\n';
            }
        }
    }
}

void DataSectionGenerator::addConstant(const vector<Constant *> &constantTable) {
    for (auto constant : constantTable) {
        auto type = constant->getType();
        if (type->getId() == Type::ID::ARRAY) {
            rodataSection << /*"g.c." <<*/ constant->getName() << ":\n";
            auto value = constant->getValue<vector<int>>();
            for (auto v : value) {
                rodataSection << "\t.word " << v << '\n';
            }
        }
    }
}

size_t DataSectionGenerator::addStringToPool(string_view src) {
    auto iter = stringPool.find(string(src));
    if (iter == stringPool.end()) {
        auto id = currentId++;
        stringPool.emplace(make_pair(src, id));
        return id;
    }
    return iter->second;
}

void DataSectionGenerator::generateStringPool() {
    std::map<size_t, string> revMap;
    for (auto &iter : stringPool) {
        revMap.emplace(make_pair(iter.second, iter.first));
    }
    for (auto &iter : revMap) {
        rodataSection << ".LC" << iter.first << ":\n";
        rodataSection << "\t.asciz \"" << iter.second << "\"\n";
    }
}

void DataSectionGenerator::createThreadPool() {
    dataSection <<
                "taskQueue:\n"
                "\t.space 4096\n"
                "queueInfo:\n"
                "\t.space 16\n"
                "workerThreads:\n"
                "\t.space 24\n"
                "readyThreads:\n"
                "\t.space 4\n"
                "main_pid:\n"
                "\t.space 4\n";
}
