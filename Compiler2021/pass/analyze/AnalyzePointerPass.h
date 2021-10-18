//
// Created by hujin on 2021/8/5.
//

#ifndef COMPILER2021_ANALYZEPOINTERPASS_H
#define COMPILER2021_ANALYZEPOINTERPASS_H

#include "AnalyzeSideEffectPass.h"

struct PointerNode {
    Value *base = nullptr;
    PointerNode *father = nullptr;
    std::set<unique_ptr<PointerNode>> children;

    bool read = false;//该指针被读取
    bool write = false;//该指针被写入
    bool writeAll = false;//该指针被函数写入寻址范围内全部地址
    bool readAll = false;//该指针寻址范围内全部地址被读取

    Assignment *offsetAssignment = nullptr;

    optional<int> offsetBase = 0;
    optional<int> rangeSize; //child ptr ∈ [ptr, ptr + rangeSize), 缺失表示无约束
};

class AnalyzePointerPass : public AnalyzeModulePass {
public:
    AnalyzePointerPass(Module &md, AnalyzeModulePasses &dependency, ostream &out)
            : AnalyzeModulePass(md, out),
              analyzeSideEffectPass(*dependency.analyzeSideEffectPass) {};

    void invalidate() override;

private:
    bool analyze() override;

public:
    std::map<Value *, PointerNode *> ptrNode;
    //ptrTree[Value] = ptrNode[Value] = ptrNode[ValueAddress(Value)] = ptrNode[LoadVariable(Pointer Argument)]
    //ptrTree[Value].rangeSize = Value.Type.Size or empty
    std::map<Value *, unique_ptr<PointerNode>> ptrTree;
    AnalyzeSideEffectPass &analyzeSideEffectPass;
};


#endif //COMPILER2021_ANALYZEPOINTERPASS_H
