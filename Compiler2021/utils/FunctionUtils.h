//
// Created by 陈思言 on 2021/8/1.
//

#ifndef COMPILER2021_FUNCTIONUTILS_H
#define COMPILER2021_FUNCTIONUTILS_H

#include "../IR/Module.h"
#include "../pass/Pass.h"
#include "../pass/analyze/AnalyzeRegionPass.h"
#include "../pass/analyze/AnalyzeDataFlowPass.h"
#include "../pass/analyze/AnalyzeActivityPass.h"
#include "../pass/analyze/AnalyzeLoopPass.h"
#include "../pass/parallel/AnalyzeParallelLoopPass.h"
#include "../IR/Type.h"

using std::pair;

// 将src中的region提取为一个新函数
Function *
extractFunction(Module *md, Function *src, AnalyzeRegionPass::NodeMin *region, AnalyzeFunctionPasses &functionPasses);

// 深拷贝一个函数
Function *cloneFunction(Module *md, Function *src, string_view dst_name);

// 深拷贝一个函数，并将返回值丢弃
Function *cloneFunctionReturn0(Module *md, Function *src);

// 将指定基本块指定位置的函数调用内联
BasicBlock *inlineFunction(Function *fn, BasicBlock *bb, size_t call_index);



std::vector<BasicBlock *>
getAllEtrBlocks(AnalyzeFunctionPasses &functionPasses, BasicBlock *entrance, BasicBlock *exit);

vector<pair<Assignment *, bool>>
getParams(vector<BasicBlock *> &etrBlocks, AnalyzeFunctionPasses &functionPasses);

void splitArrScalar(vector<pair<Assignment *, bool>> &input, vector<pair<Assignment *, bool>> &scalars,
                           vector<pair<Assignment *, bool>> &arrays);

void extractCallee(Function *new_fn, vector<BasicBlock *> &etrBlocks, vector<pair<Assignment *, bool>> &scalars,
                          vector<pair<Assignment *, bool>> &arrays, BasicBlock *oldExitBlock);

BasicBlock *
createNewPrevBlock(Function *src, vector<BasicBlock *> &etrBlocks, AnalyzeFunctionPasses &functionPasses);

void storeInputScalar(Function *src, BasicBlock *prevBlock, ValueAddressingMIR *ptr_arr,
                             vector<pair<Assignment *, bool>> &scalars, int &tmpId);

void loadOutputScalar(Function *src, BasicBlock *prevBlock, ValueAddressingMIR *ptr_arr,
                      vector<pair<Assignment *, bool>> &scalars, int &tmpId, std::map<Assignment *, Assignment *> replaceTable = {});


void replaceJumpBlock(BasicBlock *b, BasicBlock *old_b, BasicBlock *new_b);

#endif //COMPILER2021_FUNCTIONUTILS_H
