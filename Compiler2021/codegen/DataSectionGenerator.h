//
// Created by 陈思言 on 2021/7/7.
//

#ifndef COMPILER2021_DATASECTIONGENERATOR_H
#define COMPILER2021_DATASECTIONGENERATOR_H

#include "../IR/Module.h"
#include <sstream>
#include <unordered_map>

using std::stringstream;
using std::unordered_map;

class DataSectionGenerator {
public:
    DataSectionGenerator();

    void addVariable(const vector<Variable *> &variableTable,
                     const std::map<Variable *, std::any> &initializationTable);

    void addConstant(const vector<Constant *> &constantTable);

    void createThreadPool();

    size_t addStringToPool(string_view src);

    void generateStringPool();

public:
    stringstream dataSection;
    stringstream rodataSection;

private:
    unordered_map<string, size_t> stringPool;
    size_t currentId = 0;
};


#endif //COMPILER2021_DATASECTIONGENERATOR_H
