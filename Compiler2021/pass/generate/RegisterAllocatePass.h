//
// Created by 陈思言 on 2021/7/27.
//

#ifndef COMPILER2021_REGISTERALLOCATEPASS_H
#define COMPILER2021_REGISTERALLOCATEPASS_H

#include "RegisterColoringPass.h"
#include "../analyze/AnalyzeDomTreePass.h"

class RegisterAllocatePass : public FunctionPass {
public:
    RegisterAllocatePass(Function &fn, AnalyzeFunctionPasses &dependency, ostream &out = null_out)
            : FunctionPass(fn, out),
              analyzeCFGPass(*dependency.analyzeCFGPass),
              analyzeDomTreePass(*dependency.analyzeDomTreePass),
              analyzeActivityPass(
                      *dynamic_cast<AnalyzeActivityPass_LIR *>(dependency.analyzeActivityPass)),
              analyzeDataFlowPass(
                      *dynamic_cast<AnalyzeDataFlowPass_LIR *>(dependency.analyzeDataFlowPass)) {}

    bool run() override;

private:
    void scheduleUnusedVReg(vector<std::map<size_t, std::set<size_t>>> &coreRegSpill);

    void allocatePReg(BasicBlock *bb);

    void dealWithEdge(BasicBlock *prev, BasicBlock *rear);

private:
    AnalyzeCFGPass &analyzeCFGPass;
    AnalyzeDomTreePass &analyzeDomTreePass;
    AnalyzeActivityPass_LIR &analyzeActivityPass;
    AnalyzeDataFlowPass_LIR &analyzeDataFlowPass;
    std::map<BasicBlock *, RegisterColoringPass *> registerColoringPasses;
    std::map<BasicBlock *, std::map<size_t, uint8_t>> pRegInputMap;
    std::map<BasicBlock *, std::map<size_t, uint8_t>> pRegOutputMap;
};


#endif //COMPILER2021_REGISTERALLOCATEPASS_H
