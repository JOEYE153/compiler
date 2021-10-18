//
// Created by 陈思言 on 2021/8/3.
//

#ifndef COMPILER2021_ANALYZEACTIVITYPASS_H
#define COMPILER2021_ANALYZEACTIVITYPASS_H

#include "AnalyzeCFGPass.h"

template<typename T>
struct AssignActivity {
    std::pair<BasicBlock *, int> def;
    std::set<std::pair<BasicBlock *, T *>> use;
    std::map<BasicBlock *, std::set<BasicBlock *>> useByPhi; // second表示该赋值经过哪些incoming后被使用
    std::set<std::pair<BasicBlock *, int>> usePos;
};

template<typename T>
struct BlockActivity {
    std::set<T *> def;
    std::set<T *> use;
    std::set<T *> useAfterDef;
    std::map<T *, std::set<BasicBlock *>> useByPhi; // second表示使用的赋值来自哪些incoming
};

class AnalyzeActivityPass : public AnalyzeFunctionPass {
protected:
    AnalyzeActivityPass(Function &fn, AnalyzeFunctionPasses &dependency, ostream &out = null_out)
            : AnalyzeFunctionPass(fn, out),
              analyzeCFGPass(*dependency.analyzeCFGPass) {}

    bool analyze() override;

    virtual void collectDefUsePos(BasicBlock *bb) = 0;

public:
    virtual void printAssignActivityMap() const = 0;

    virtual void printBlockActivityMap() const = 0;

protected:
    AnalyzeCFGPass &analyzeCFGPass;
};

class AnalyzeActivityPass_MIR : public AnalyzeActivityPass {
public:
    AnalyzeActivityPass_MIR(Function &fn, AnalyzeFunctionPasses &dependency, ostream &out = null_out)
            : AnalyzeActivityPass(fn, dependency, out) {}

public:
    void invalidate() override;

    void printAssignActivityMap() const override;

    void printBlockActivityMap() const override;

protected:
    void collectDefUsePos(BasicBlock *bb) override;

public:
    std::map<Assignment *, AssignActivity<MIR>> assignActivityMap;
    std::map<BasicBlock *, BlockActivity<Assignment>> blockActivityMap;
};

class AnalyzeActivityPass_LIR : public AnalyzeActivityPass {
public:
    AnalyzeActivityPass_LIR(Function &fn, AnalyzeFunctionPasses &dependency, ostream &out = null_out)
            : AnalyzeActivityPass(fn, dependency, out) {}

public:
    void invalidate() override;

    void printAssignActivityMap() const override;

    void printBlockActivityMap() const override;

protected:
    void collectDefUsePos(BasicBlock *bb) override;

public:
    std::map<CoreRegAssign *, AssignActivity<LIR>> assignActivityMapCoreReg;
    std::map<BasicBlock *, BlockActivity<CoreRegAssign>> blockActivityMapCoreReg;
    std::map<NeonRegAssign *, AssignActivity<LIR>> assignActivityMapNeonReg;
    std::map<BasicBlock *, BlockActivity<NeonRegAssign>> blockActivityMapNeonReg;

    std::map<StatusRegAssign *, AssignActivity<LIR>> assignActivityMapStatusReg;
    std::map<BasicBlock *, BlockActivity<StatusRegAssign>> blockActivityMapStatusReg;

    std::map<CoreMemAssign *, AssignActivity<LIR>> assignActivityMapCoreMem;
    std::map<BasicBlock *, BlockActivity<CoreMemAssign>> blockActivityMapCoreMem;
    std::map<NeonMemAssign *, AssignActivity<LIR>> assignActivityMapNeonMem;
    std::map<BasicBlock *, BlockActivity<NeonMemAssign>> blockActivityMapNeonMem;
};


#endif //COMPILER2021_ANALYZEACTIVITYPASS_H
