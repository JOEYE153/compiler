//
// Created by 陈思言 on 2021/8/1.
//

#ifndef COMPILER2021_IRUTILS_H
#define COMPILER2021_IRUTILS_H

#include "../IR/MIR.h"

MIR *copyMIR(MIR *IR, int &currentId);

optional<int> calculate(Assignment *, std::map<Assignment *, optional<int>> &assignValues,
                        std::map<Assignment *, optional<int>> &visitedValues);

MIR *
cloneMIR(MIR *mir, std::map<Assignment *, Assignment *> &replaceTable, std::map<BasicBlock *, BasicBlock *> blockTable,
         std::map<Constant *, Constant *> &constantTable,
         std::map<Variable *, Variable *> &variableTable);

MIR *
cloneMIR(MIR *mir, std::map<Assignment *, Assignment *> &replaceTable, std::map<BasicBlock *, BasicBlock *> blockTable,
         std::map<Constant *, Constant *> &constantTable,
         std::map<Variable *, Variable *> &variableTable, CallMIR *callMir);
#endif //COMPILER2021_IRUTILS_H
