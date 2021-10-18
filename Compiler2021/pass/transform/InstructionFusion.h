//
// Created by Joeye on 2021/8/10.
//

#ifndef COMPILER2021_INSTRUCTIONFUSION_H
#define COMPILER2021_INSTRUCTIONFUSION_H

#include "../analyze/AnalyzeCallGraphPass.h"
#include "../analyze/AnalyzeActivityPass.h"
#include "../analyze/AnalyzeDataFlowPass.h"
#include "../analyze/AnalyzeDomTreePass.h"
#include <algorithm>

struct PRegActivity {
    std::set<int> def;
    std::set<int> use;
};

class InstructionFusion : public FunctionPass {
public:
    InstructionFusion(Function &fn, AnalyzeFunctionPasses &dependency, ostream &out = null_out)
            : FunctionPass(fn, out), analyzeCFGPass(*dependency.analyzeCFGPass),
              analyzeDataFlowPass(*dynamic_cast<AnalyzeDataFlowPass_LIR *>(dependency.analyzeDataFlowPass)),
              analyzeActivityPass(*dynamic_cast<AnalyzeActivityPass_LIR *>(dependency.analyzeActivityPass)),
              analyzeDomTreePass(*dependency.analyzeDomTreePass){}

    bool run() override;

private:
    void fuse(DomTreeNode *node, std::map<CoreRegAssign *, CoreRegAssign *> replaceTable);

    void collectDefUsePos(BasicBlock &bb, std::map<int, PRegActivity> &regActivity,
                          std::map<CoreRegAssign *, CoreRegAssign *> &replaceTable);

    static std::map<LIR *, LIR *> getFusionInstruction(BasicBlock &bb, std::map<int, PRegActivity> &regActivity);

    static uint32_t getBinaryCalculateRes(ArmInst::BinaryOperator op,  uint32_t x, uint32_t y);

private:
    AnalyzeCFGPass &analyzeCFGPass;
    AnalyzeDomTreePass &analyzeDomTreePass;
    AnalyzeDataFlowPass_LIR &analyzeDataFlowPass;
    AnalyzeActivityPass_LIR &analyzeActivityPass;
    vector<unique_ptr<LIR>> toErase;
    int originInstrCnt = 0;
    int instrCnt = 0;
    int newInstrCnt = 0;
};


#endif //COMPILER2021_INSTRUCTIONFUSION_H
