//
// Created by 陈思言 on 2021/7/17.
//

#ifndef COMPILER2021_ANALYZESIDEEFFECTPASS_H
#define COMPILER2021_ANALYZESIDEEFFECTPASS_H

#include "AnalyzeCallGraphPass.h"

struct FunctionSideEffect {
    std::set<Variable *> readGlobalVariable;    // 读取全局变量
    std::set<Variable *> writeGlobalVariable;   // 写入全局变量
    std::set<size_t> readByPointerArg;          // 根据函数指针类型的参数读内存
    std::set<size_t> writeByPointerArg;         // 根据函数指针类型的参数写内存
    std::set<Function *> callExternalFunction;  // 调用外部函数
    Constant *returnValue = nullptr;
    bool returnConstant = true;
};

class AnalyzeSideEffectPass : public AnalyzeModulePass {
public:
    AnalyzeSideEffectPass(Module &md, AnalyzeModulePasses &dependency, ostream &out = null_out)
            : AnalyzeModulePass(md, out),
              analyzeCallGraphPass(*dependency.analyzeCallGraphPass) {}

    //时间 & 空间复杂度： O(FunctionCount * MIRCount)
    bool analyze() override;

    void invalidate() override;


public:
    std::map<Function *, FunctionSideEffect *> sideEffects;
    std::map<Function *, std::set<Function *>> returnDataFlow;  //Flow[f2] = {f1} 表示f1使用了f2的返回值
    std::vector<Function *> funcTopoSeq;

    AnalyzeCallGraphPass &analyzeCallGraphPass;
};


#endif //COMPILER2021_ANALYZESIDEEFFECTPASS_H
