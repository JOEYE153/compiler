//
// Created by hujin on 2021/7/3.
//

#ifndef COMPILER2021_ANALYZECALLGRAPHPASS_H
#define COMPILER2021_ANALYZECALLGRAPHPASS_H

#include "../Pass.h"

class AnalyzeCallGraphPass : public AnalyzeModulePass {
protected:
    explicit AnalyzeCallGraphPass(Module &fn, ostream &out = null_out)
            : AnalyzeModulePass(fn, out) {};

    bool analyze() override;

    virtual vector<Function *> extractCalledFunctions(BasicBlock *basicBlock) = 0;

public:
    void invalidate() override;

    std::map<Function *, std::set<Function *>> callGraph;

    bool recursive(Function *f) {
        return callGraph[f].count(f);
    }
};

class AnalyzeCallGraphPass_HIR final : public AnalyzeCallGraphPass {
public:
    explicit AnalyzeCallGraphPass_HIR(Module &fn, ostream &out = null_out)
            : AnalyzeCallGraphPass(fn, out) {};

protected:
    vector<Function *> extractCalledFunctions(BasicBlock *basicBlock) override;
};

class AnalyzeCallGraphPass_MIR final : public AnalyzeCallGraphPass {
public:
    explicit AnalyzeCallGraphPass_MIR(Module &fn, ostream &out = null_out)
            : AnalyzeCallGraphPass(fn, out) {};

protected:
    vector<Function *> extractCalledFunctions(BasicBlock *basicBlock) override;
};

class AnalyzeCallGraphPass_LIR final : public AnalyzeCallGraphPass {
public:
    explicit AnalyzeCallGraphPass_LIR(Module &fn, ostream &out = null_out)
            : AnalyzeCallGraphPass(fn, out) {};

protected:
    vector<Function *> extractCalledFunctions(BasicBlock *basicBlock) override;
};


#endif //COMPILER2021_ANALYZECALLGRAPHPASS_H
