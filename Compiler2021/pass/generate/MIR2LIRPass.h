//
// Created by 陈思言 on 2021/7/20.
//

#ifndef COMPILER2021_MIR2LIRPASS_H
#define COMPILER2021_MIR2LIRPASS_H

#include "../analyze/AnalyzeDomTreePass.h"
#include "../analyze/AnalyzeLoopPass.h"

class MIR2LIRPass : public ModulePass {
public:
    explicit MIR2LIRPass(Module &md, ostream &out = null_out)
            : ModulePass(md, out) {}

    bool run() override;

private:
    void runOnNode(Function &fn, DomTreeNode *node, std::set<DivideInst *> divideCache,
                   std::map<std::pair<CoreRegAssign *, int>, CoreRegAssign*> fastDivideCache,
                   std::set<ValueAddressingLIR *> valueAddressingCache);

    void addPhiIncoming(BasicBlock *bb);

    static uint8_t floorLog(uint32_t x);

    static uint32_t abs(int x);

    static int findCeilPowerOfTwo(int x);

    void prepareMultiThread();

    ValueAddressingLIR *createAsyncCall(Function &fn, BasicBlock &bb, CallMIR *call_mir);

    Function *createFunctionWrapper(Function *callee, size_t num_args);

    ValueAddressingLIR *createMultiCall(Function &fn, BasicBlock &bb, MultiCallMIR *call_mir, CoreRegAssign *atomic_var_ptr);

    Function *createMultiFunctionWrapper(Function *callee, size_t num_args);

    void syncLastCall(BasicBlock &bb, ValueAddressingLIR *buf_ptr);

private:
    size_t vReg = 0;
    std::map<Assignment *, CoreRegAssign *> mapCoreRegAssign;
    std::map<Assignment *, NeonRegAssign *> mapNeonRegAssign;
    std::map<BasicBlock *, vector<std::pair<size_t, size_t>>> callSyncTimeVecMap;
    std::map<BasicBlock *, size_t> dfnMap;
    std::map<BasicBlock *, std::vector<int>> loopIn;
    std::vector<Loop> loops;
};


#endif //COMPILER2021_MIR2LIRPASS_H
