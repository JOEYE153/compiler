//
// Created by 陈思言 on 2021/8/13.
//

#ifndef COMPILER2021_ANALYZEPARALLELLOOPPASS_H
#define COMPILER2021_ANALYZEPARALLELLOOPPASS_H

#include "../analyze/AnalyzeLoopParamPass.h"
#include "../analyze/AnalyzeRegionPass.h"
#include "../analyze/AnalyzeArrayAccessPass.h"
#include "../analyze/AnalyzeActivityPass.h"

class AnalyzeParallelLoopPass : public AnalyzeFunctionPass {
public:
    AnalyzeParallelLoopPass(Function &fn, AnalyzeFunctionPasses &dependency, ostream &out = null_out)
            : AnalyzeFunctionPass(fn, out),
              analyzeCFGPass(*dependency.analyzeCFGPass),
              analyzeDomTreePass(*dependency.analyzeDomTreePass),
              analyzeLoopPass(*dependency.analyzeLoopPass),
              analyzeLoopParamPass(*dependency.analyzeLoopParamPass),
              analyzeRegionPass(*dependency.analyzeRegionPass),
              analyzeArrayAccessPass(*dependency.analyzeArrayAccessPass),
              analyzeActivityPass(*dynamic_cast<AnalyzeActivityPass_MIR *>(dependency.analyzeActivityPass)),
              analyzeSideEffectPass(*dependency.analyzeModulePasses->analyzeSideEffectPass){}

protected:
    bool analyze() override;

public:
    void invalidate() override;

private:
    enum class OffsetAttribute {
        STABLE, INDUCED, RANDOM
    };

    void checkLoop(Loop *loop, const LoopParam &param);

    OffsetAttribute getOffsetAttribute(Loop *loop, Assignment *induction, Assignment *offset);

    bool checkArrayAccess(Loop *loop, Assignment *induction, const vector<MIR *> &allMir);

public:
    std::map<Loop *, RegionMin *> parallelLoop;

private:
    AnalyzeCFGPass &analyzeCFGPass;
    AnalyzeDomTreePass &analyzeDomTreePass;
    AnalyzeLoopPass &analyzeLoopPass;
    AnalyzeLoopParamPass &analyzeLoopParamPass;
    AnalyzeRegionPass &analyzeRegionPass;
    AnalyzeArrayAccessPass &analyzeArrayAccessPass;
    AnalyzeActivityPass_MIR &analyzeActivityPass;
    AnalyzeSideEffectPass &analyzeSideEffectPass;
    std::map<Assignment *, OffsetAttribute> offsetAttributeCache;
};


#endif //COMPILER2021_ANALYZEPARALLELLOOPPASS_H
