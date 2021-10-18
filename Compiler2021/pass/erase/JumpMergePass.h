//
// Created by tcyhost on 2021/8/5.
//

#ifndef COMPILER2021_JUMPMERGEPASS_H
#define COMPILER2021_JUMPMERGEPASS_H

#include "../analyze/AnalyzeCFGPass.h"

class JumpMergePass : public FunctionPass {
public:
    JumpMergePass(Function &fn, AnalyzeFunctionPasses &dependency, ostream &out = null_out, bool afterForward = false) :
            FunctionPass(fn, out), analyzeCFGPass(*dependency.analyzeCFGPass), afterForward(afterForward) {}

    bool run() override;

private:
    bool afterForward;

    AnalyzeCFGPass &analyzeCFGPass;

    BasicBlock *findNextJumpBlock(BasicBlock *src, BasicBlock *jumpBlock, std::set<BasicBlock *> &remainBlocks,
                                  std::map<PhiMIR *, std::set<BasicBlock *>> &phiErase,
                                  std::map<PhiMIR *, std::map<BasicBlock *, Assignment *>> &phiNew);

    void mergeBranchMIR(BasicBlock *block, BranchMIR *mir2, std::set<BasicBlock *> &remainBlocks,
                        std::map<PhiMIR *, std::set<BasicBlock *>> &phiErase,
                        std::map<PhiMIR *, std::map<BasicBlock *, Assignment *>> &phiNew, size_t currentId);
};


#endif //COMPILER2021_JUMPMERGEPASS_H
