//
// Created by 陈思言 on 2021/8/1.
//

#ifndef COMPILER2021_BASICBLOCKUTILS_H
#define COMPILER2021_BASICBLOCKUTILS_H

#include "../IR/Function.h"

// 对基本块开头的phi结点进行替换操作
void replacePhiIncoming(BasicBlock *bb,
                        const std::map<Assignment *, Assignment *> &replaceTable);

void replacePhiIncoming(BasicBlock *bb,
                        const std::map<CoreRegAssign *, CoreRegAssign *> &replaceTableCoreReg,
                        const std::map<NeonRegAssign *, NeonRegAssign *> &replaceTableNeonReg,
                        const std::map<StatusRegAssign *, StatusRegAssign *> &replaceTableStatusReg);

BasicBlock *cloneBlock(BasicBlock *block,
                       std::map<Assignment *, Assignment *> &replaceTable,
                       std::map<BasicBlock *, BasicBlock *> &blockTable,
                       std::map<Constant *, Constant *> &constantTable,
                       std::map<Variable *, Variable *> &variableTable, string_view newName);

std::map<BasicBlock *, BasicBlock *>
cloneSetOfBlocks(Function *fnIn, const std::set<BasicBlock *> &bb,
                 std::map<BasicBlock *, BasicBlock *> &blockTable,
                 std::map<Assignment *, Assignment *> &replaceTable,
                 const string &suffix, bool replaceSuffix);

#endif //COMPILER2021_BASICBLOCKUTILS_H
