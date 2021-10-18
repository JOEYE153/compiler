//
// Created by 陈思言 on 2021/7/8.
//

#ifndef COMPILER2021_CODESECTIONGENERATOR_H
#define COMPILER2021_CODESECTIONGENERATOR_H

#include "../IR/Module.h"
#include <sstream>

using std::stringstream;

class CodeSectionGenerator {
public:
    CodeSectionGenerator();

    void addFunction(Function &fn, stringstream &body);

    void createThreadPool();

public:
    stringstream textSection;
};


#endif //COMPILER2021_CODESECTIONGENERATOR_H
