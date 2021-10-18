//
// Created by Joeye on 2021/7/31.
//

#ifndef COMPILER2021_ERASEPSEUDOINSTLIRPASS_H
#define COMPILER2021_ERASEPSEUDOINSTLIRPASS_H


#include "../analyze/AnalyzeDomTreePass.h"
#include "../analyze/AnalyzeCallGraphPass.h"
#include "../analyze/AnalyzeActivityPass.h"
#include "../analyze/AnalyzeDataFlowPass.h"

class ErasePseudoInstLIRPass : public FunctionPass {
public:
    ErasePseudoInstLIRPass(Function &fn, AnalyzeFunctionPasses &dependency,
                           AnalyzeCallGraphPass_LIR &analyzeCallGraphPass, ostream &out = std::cout)
            : FunctionPass(fn, out), analyzeDomTreePass(*dependency.analyzeDomTreePass),
              analyzeActivityPass(*dynamic_cast<AnalyzeActivityPass_LIR *>(dependency.analyzeActivityPass)),
              analyzeDataFlowPass(*dynamic_cast<AnalyzeDataFlowPass_LIR *>(dependency.analyzeDataFlowPass)),
              analyzeCFGPass(*dependency.analyzeCFGPass), analyzeCallGraphPass(analyzeCallGraphPass) {}

    bool run() override;

private:
    int getMemOffsetAllocation(int base);

    void addConflictEdge(CoreMemAssign *x, CoreMemAssign *y);

    void addSuccessor(CoreMemAssign *from, LIR *to);

    void eliminatePseudo(DomTreeNode *node, std::map<CoreRegAssign *, CoreRegAssign *> replaceTable);

    vector<LIR *> revisePseudo(LIR *lir, std::map<CoreRegAssign *, CoreRegAssign *> &replaceTable);

    void checkNeedStore();

    Rematerialization *dfsCheckRematerialization(CoreMemAssign *ssa);

    bool dfsCheckNeedStore(std::set<CoreMemAssign *> &allMemAssign, CoreMemAssign *ssa, bool userNeedStore);

private:
    AnalyzeCFGPass &analyzeCFGPass;
    AnalyzeCallGraphPass_LIR &analyzeCallGraphPass;
    AnalyzeActivityPass_LIR &analyzeActivityPass;
    AnalyzeDataFlowPass_LIR &analyzeDataFlowPass;
    AnalyzeDomTreePass &analyzeDomTreePass;


    struct ConflictEdge {
        std::set<CoreMemAssign *> next;
    };
    std::vector<ConflictEdge> conflictEdges;
    std::map<CoreMemAssign *, int> conflictGraph;
    std::map<CoreMemAssign *, vector<LIR *>> successorTable;
    vector<std::set<CoreMemAssign *>> connects;
    std::set<CoreMemPhiLIR *> phiTable;
    vector<CoreMemAssign *> starts;
    std::map<Value *, int> valueOffset;
    std::set<int> regList;
    vector<unique_ptr<LIR>> toErase;
    bool hasConflict = false;
    int frameSize = 0;
    int memInstrCnt = 0;
};


#endif //COMPILER2021_ERASEPSEUDOINSTLIRPASS_H
