//
// Created by 陈思言 on 2021/5/23.
//

#ifndef COMPILER2021_PASS_H
#define COMPILER2021_PASS_H

#include "../IR/Module.h"
#include <iostream>
#include <chrono>

using std::istream;
using std::ostream;
using std::iostream;
using namespace std::chrono;

class AnalyzeCallGraphPass;

class AnalyzeSideEffectPass;

class AnalyzeCFGPass;

class AnalyzeDomTreePass;

class AnalyzeActivityPass;

class AnalyzeDataFlowPass;

class AnalyzeDAGPass;

class AnalyzeLoopPass;

class AnalyzeRegionPass;

class AnalyzePointerPass;

class AnalyzeUnwrittenGlobalValuePass;

class AnalyzeArrayAccessPass;

class AnalyzeLoopParamPass;

struct AnalyzeModulePasses {
    Module *md;
    AnalyzeCallGraphPass *analyzeCallGraphPass = nullptr;
    AnalyzeSideEffectPass *analyzeSideEffectPass = nullptr;
    AnalyzePointerPass *analyzePointerPass = nullptr;
    AnalyzeUnwrittenGlobalValuePass *analyzeUnwrittenGlobalValuePass = nullptr;
};

struct AnalyzeFunctionPasses {
    Function *fn;
    AnalyzeModulePasses *analyzeModulePasses = nullptr;
    AnalyzeCFGPass *analyzeCFGPass = nullptr;
    AnalyzeDomTreePass *analyzeDomTreePass = nullptr;
    AnalyzeActivityPass *analyzeActivityPass = nullptr;
    AnalyzeDataFlowPass *analyzeDataFlowPass = nullptr;
    AnalyzeLoopPass *analyzeLoopPass = nullptr;
    AnalyzeRegionPass *analyzeRegionPass = nullptr;
    AnalyzeArrayAccessPass *analyzeArrayAccessPass = nullptr;
    AnalyzeLoopParamPass *analyzeLoopParamPass = nullptr;
};

struct AnalyzeBasicBlockPasses {
    AnalyzeFunctionPasses *analyzeFunctionPasses = nullptr;
    AnalyzeDAGPass *analyzeDAGPass = nullptr;
};

class null_ostream : public ostream {
public:
    null_ostream() : ostream(nullptr) {}
};

extern null_ostream null_out;

class Pass {
public:
    explicit Pass(ostream &out = null_out) : out(out) {}

    virtual ~Pass() = default;

    virtual bool run(); // 返回true表示修改了原数据，返回false表示什么也没变或者只生成新数据

protected:
    ostream &out;
};

class AnalyzePass : public Pass {
public:
    explicit AnalyzePass(ostream &out = null_out) : Pass(out) {}

    bool run() override;

    virtual void invalidate() {
        isValid = false;
    }

protected:
    virtual bool analyze();

protected:
    bool isValid = false;
};

class BasicBlockPass : public Pass {
public:
    explicit BasicBlockPass(BasicBlock &bb, ostream &out = null_out)
            : Pass(out), bb(bb) {}

    bool run() override;

protected:
    BasicBlock &bb;
};

class FunctionPass : public Pass {
public:
    explicit FunctionPass(Function &fn, ostream &out = null_out)
            : Pass(out), fn(fn) {}

    bool run() override;

protected:
    Function &fn;
};

class ModulePass : public Pass {
public:
    explicit ModulePass(Module &md, ostream &out = null_out)
            : Pass(out), md(md) {}

    bool run() override;

protected:
    Module &md;
};

class AnalyzeBasicBlockPass : public AnalyzePass {
public:
    explicit AnalyzeBasicBlockPass(BasicBlock &bb, ostream &out = null_out)
            : AnalyzePass(out), bb(bb) {}

protected:
    BasicBlock &bb;
};

class AnalyzeFunctionPass : public AnalyzePass {
public:
    explicit AnalyzeFunctionPass(Function &fn, ostream &out = null_out)
            : AnalyzePass(out), fn(fn) {}

protected:
    Function &fn;
};

class AnalyzeModulePass : public AnalyzePass {
public:
    explicit AnalyzeModulePass(Module &md, ostream &out = null_out)
            : AnalyzePass(out), md(md) {}

protected:
    Module &md;
};


AnalyzeModulePasses buildModulePass(Module *md);

void invalidateModulePass(AnalyzeModulePasses &analyzePasses);

void deleteModulePass(AnalyzeModulePasses &analyzePasses);

AnalyzeFunctionPasses buildFunctionPass(Function *fn, Module *md, AnalyzeModulePasses *analyzeModulePasses);

void invalidateFunctionPass(AnalyzeFunctionPasses &analyzePasses);

void deleteFunctionPass(AnalyzeFunctionPasses &analyzePasses);

extern time_point<high_resolution_clock> compiler_start_time;


#endif //COMPILER2021_PASS_H
