//
// Created by 陈思言 on 2021/6/9.
//

#ifndef COMPILER2021_ANALYZECFGPASS_H
#define COMPILER2021_ANALYZECFGPASS_H

#include "../Pass.h"

struct EdgeSetOfCFG {
    BasicBlock *block;
    BasicBlock *father;
    std::set<BasicBlock *> prev;
    vector<BasicBlock *> rear;
};

class AnalyzeCFGPass : public AnalyzeFunctionPass {
protected:
    explicit AnalyzeCFGPass(Function &fn, ostream &out = null_out)
            : AnalyzeFunctionPass(fn, out) {}

    virtual vector<BasicBlock *> getRear(BasicBlock *block) = 0;

    bool analyze() override;

public:
    void invalidate() override;

    [[nodiscard]] std::map<BasicBlock *, size_t> getDfnMap() const;

    [[nodiscard]] size_t calculateCriticalPathLength() const;

private:
    void dfs(BasicBlock *block, BasicBlock *father);

public:
    std::map<BasicBlock *, EdgeSetOfCFG> result;
    std::set<BasicBlock *> remainBlocks;
    vector<BasicBlock *> dfsSequence;
};

class AnalyzeCFGPass_HIR final : public AnalyzeCFGPass {
public:
    explicit AnalyzeCFGPass_HIR(Function &fn, ostream &out = null_out)
            : AnalyzeCFGPass(fn, out) {}

protected:
    vector<BasicBlock *> getRear(BasicBlock *block) override;
};

class AnalyzeCFGPass_MIR final : public AnalyzeCFGPass {
public:
    explicit AnalyzeCFGPass_MIR(Function &fn, ostream &out = null_out)
            : AnalyzeCFGPass(fn, out) {}

protected:
    vector<BasicBlock *> getRear(BasicBlock *block) override;
};

class AnalyzeCFGPass_LIR final : public AnalyzeCFGPass {
public:
    explicit AnalyzeCFGPass_LIR(Function &fn, ostream &out = null_out)
            : AnalyzeCFGPass(fn, out) {}

protected:
    vector<BasicBlock *> getRear(BasicBlock *block) override;
};


#endif //COMPILER2021_ANALYZECFGPASS_H
