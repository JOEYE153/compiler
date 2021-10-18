//
// Created by 陈思言 on 2021/8/3.
//

#ifndef COMPILER2021_ANALYZEDATAFLOWPASS_H
#define COMPILER2021_ANALYZEDATAFLOWPASS_H

#include "AnalyzeActivityPass.h"

template<typename T>
struct DataFlow {
    std::set<T *> in;
    std::set<T *> out;
};

class AnalyzeDataFlowPass : public AnalyzeFunctionPass {
protected:
    AnalyzeDataFlowPass(Function &fn, AnalyzeFunctionPasses &dependency, ostream &out = null_out)
            : AnalyzeFunctionPass(fn, out),
              analyzeCFGPass(*dependency.analyzeCFGPass) {}

public:
    virtual void printDataFlowMap() const = 0;

protected:
    bool analyze() override;

    virtual void initOutSet() = 0;

    virtual void iterateOutSet() = 0;

    virtual void calculateInSet() = 0;

protected:
    AnalyzeCFGPass &analyzeCFGPass;
};

class AnalyzeDataFlowPass_MIR : public AnalyzeDataFlowPass {
public:
    AnalyzeDataFlowPass_MIR(Function &fn, AnalyzeFunctionPasses &dependency, ostream &out = null_out)
            : AnalyzeDataFlowPass(fn, dependency, out),
              analyzeActivityPass(*dynamic_cast<AnalyzeActivityPass_MIR *>(dependency.analyzeActivityPass)) {}

    void invalidate() override;

    void printDataFlowMap() const override;

private:
    bool analyze() override;

    void initOutSet() override;

    void iterateOutSet() override;

    void calculateInSet() override;

private:
    AnalyzeActivityPass_MIR &analyzeActivityPass;

public:
    std::map<BasicBlock *, DataFlow<Assignment>> dataFlowMap;
};

class AnalyzeDataFlowPass_LIR : public AnalyzeDataFlowPass {
public:
    AnalyzeDataFlowPass_LIR(Function &fn, AnalyzeFunctionPasses &dependency, ostream &out = null_out)
            : AnalyzeDataFlowPass(fn, dependency, out),
              analyzeActivityPass(*dynamic_cast<AnalyzeActivityPass_LIR *>(dependency.analyzeActivityPass)) {}

    void invalidate() override;

    void printDataFlowMap() const override;

private:
    bool analyze() override;

    void initOutSet() override;

    void iterateOutSet() override;

    void calculateInSet() override;

private:
    AnalyzeActivityPass_LIR &analyzeActivityPass;

public:
    std::map<BasicBlock *, DataFlow<CoreRegAssign>> dataFlowMapCoreReg;
    std::map<BasicBlock *, DataFlow<NeonRegAssign>> dataFlowMapNeonReg;

    std::map<BasicBlock *, DataFlow<StatusRegAssign>> dataFlowMapStatusReg;

    std::map<BasicBlock *, DataFlow<CoreMemAssign>> dataFlowMapCoreMem;
    std::map<BasicBlock *, DataFlow<NeonMemAssign>> dataFlowMapNeonMem;
};


#endif //COMPILER2021_ANALYZEDATAFLOWPASS_H
