//
// Created by 陈思言 on 2021/8/9.
//

#ifndef COMPILER2021_ANALYZEARRAYACCESSPASS_H
#define COMPILER2021_ANALYZEARRAYACCESSPASS_H

#include "AnalyzeDomTreePass.h"
#include "AnalyzePointerPass.h"

struct ArrayWrite {
    virtual ~ArrayWrite() = default;

    Variable *writeBase = nullptr; // 最原始的数组变量
    vector<Assignment *> writeOffset; // 记录多维数组的每次寻址偏移量
    bool isConst = false;
};

struct ArrayRead {
    virtual ~ArrayRead() = default;

    Value *readBase = nullptr; // 最原始的数组变量，Variable或Constant
    vector<Assignment *> readOffset; // 记录多维数组的每次寻址偏移量
    ArrayWrite *useLastWrite = nullptr; // 读取了哪次的赋值
};

struct ArrayWriteUpdate : public ArrayWrite {
    ArrayWrite *updateLastWrite = nullptr; // 更新了哪次的赋值
};

// 未初始化的数组
struct UninitializedArray : public ArrayWrite {
    UninitializedArray() { isConst = true; }
};

// 从参数输入的数组
struct InputArrayArgument : public ArrayWrite {
};

// 从全局变量输入的数组
struct InputArrayGlobal : public ArrayWrite {
};

// 单个元素读取
struct ArrayElementRead : public ArrayRead {
    LoadPointerMIR *mir = nullptr;
};

// 单个元素写入
struct ArrayElementWrite : public ArrayWriteUpdate {
    StorePointerMIR *mir = nullptr;
};

// 多维数组某一维的切片填充
struct ArraySliceFill : public ArrayWriteUpdate {
    MemoryFillMIR *mir = nullptr;
};

// 多维数组某一维的切片拷贝
struct ArraySliceCopy : public ArrayRead, public ArrayWriteUpdate {
    MemoryCopyMIR *mir = nullptr;
};

// 整个数组填充
struct ArrayEntireFill : public ArrayWrite {
    MemoryFillMIR *mir = nullptr;
};

// 整个数组拷贝
struct ArrayEntireCopy : public ArrayRead, public ArrayWrite {
    MemoryCopyMIR *mir = nullptr;
};

// 函数访问数组
struct ArrayAccessByFunction {
    CallMIR *mir = nullptr;
    size_t argIndex = 0x12345678;
};

struct ArrayReadByFunction : public ArrayAccessByFunction, public ArrayRead {
};

struct ArrayElementWriteByFunction : public ArrayAccessByFunction, public ArrayWriteUpdate {
};

struct ArraySliceWriteByFunction : public ArrayAccessByFunction, public ArrayWriteUpdate {
};

struct ArrayEntireWriteByFunction : public ArrayAccessByFunction, public ArrayWrite {
};

// 数组不同赋值间的phi函数
struct ArrayAccessPhi : public ArrayWrite {
    void addIncoming(BasicBlock *block, ArrayWrite *ssa) {
        incomingTable.insert(std::make_pair(block, ssa));
    }

    std::map<BasicBlock *, ArrayWrite *> incomingTable;
};

class AnalyzeArrayAccessPass : public AnalyzeFunctionPass {
public:
    AnalyzeArrayAccessPass(Function &fn, Module &md, AnalyzeFunctionPasses &dependency, ostream &out = null_out)
            : AnalyzeFunctionPass(fn, out), md(md),
              analyzeCFGPass(*dependency.analyzeCFGPass),
              analyzeDomTreePass(*dependency.analyzeDomTreePass),
              analyzePointerPass(*dependency.analyzeModulePasses->analyzePointerPass),
              analyzeSideEffectPass(*dependency.analyzeModulePasses->analyzeSideEffectPass) {}

    bool analyze() override;

    void print() const;

    void invalidate() override;

private:
    std::set<BasicBlock *> getWriteSet(Variable *variable);

    void renameVariable(BasicBlock *block, BasicBlock *prev, Variable *variable, ArrayWrite *ssa);

public:
    std::map<MIR *, ArrayRead *> mirReadTable;
    std::map<MIR *, ArrayWrite *> mirWriteTable;
    std::map<ArrayWrite *, MIR *> writeMIRTable;
    std::map<CallMIR *, std::set<ArrayRead *>> functionReadTable; // set为空表示没有，map查不到表示随机
    std::map<CallMIR *, std::set<ArrayWrite *>> functionWriteTable;
    std::set<unique_ptr<UninitializedArray>> allUninitializedArray;
    std::set<unique_ptr<InputArrayArgument>> allInputArrayArgument;
    std::set<unique_ptr<InputArrayGlobal>> allInputArrayGlobal;
    std::map<unique_ptr<ArrayElementRead>, BasicBlock *> allArrayElementRead;
    std::map<unique_ptr<ArrayElementWrite>, BasicBlock *> allArrayElementWrite;
    std::map<unique_ptr<ArraySliceFill>, BasicBlock *> allArraySliceFill;
    std::map<unique_ptr<ArraySliceCopy>, BasicBlock *> allArraySliceCopy;
    std::map<unique_ptr<ArrayEntireFill>, BasicBlock *> allArrayEntireFill;
    std::map<unique_ptr<ArrayEntireCopy>, BasicBlock *> allArrayEntireCopy;
    std::map<unique_ptr<ArrayReadByFunction>, BasicBlock *> allArrayReadByFunction;
    std::map<unique_ptr<ArrayElementWriteByFunction>, BasicBlock *> allArrayElementWriteByFunction;
    std::map<unique_ptr<ArraySliceWriteByFunction>, BasicBlock *> allArraySliceWriteByFunction;
    std::map<unique_ptr<ArrayEntireWriteByFunction>, BasicBlock *> allArrayEntireWriteByFunction;
    std::map<unique_ptr<ArrayAccessPhi>, BasicBlock *> allArrayAccessPhi;

private:
    AnalyzeCFGPass &analyzeCFGPass;
    AnalyzeDomTreePass &analyzeDomTreePass;
    AnalyzePointerPass &analyzePointerPass;
    AnalyzeSideEffectPass &analyzeSideEffectPass;
    Module &md;
    std::map<BasicBlock *, ArrayAccessPhi *> phiTable;
    std::set<BasicBlock *> visited;
};


#endif //COMPILER2021_ANALYZEARRAYACCESSPASS_H
