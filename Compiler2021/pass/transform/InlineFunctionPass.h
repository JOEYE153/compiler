//
// Created by tcyhost on 2021/8/9.
//

#ifndef COMPILER2021_INLINEFUNCTIONPASS_H
#define COMPILER2021_INLINEFUNCTIONPASS_H

#include "../Pass.h"
#include "../analyze/AnalyzeCallGraphPass.h"
#include "../analyze/AnalyzeSideEffectPass.h"

class InlineFunctionPass : public ModulePass {
public:
    InlineFunctionPass(Module &md, ostream &out = null_out) : ModulePass(md, out) {
        dependency = new AnalyzeModulePasses();
        dependency->analyzeCallGraphPass = new AnalyzeCallGraphPass_MIR(md, out);
        dependency->analyzeSideEffectPass = new AnalyzeSideEffectPass(md, *dependency, out);
    }

    ~InlineFunctionPass() override {
        delete dependency->analyzeCallGraphPass;
        delete dependency->analyzeSideEffectPass;
        delete dependency;
    }

    bool run() override;

    AnalyzeModulePasses *dependency;
    const size_t max_block_count = 5;
    const size_t max_mir_count = 30;

private:
    // 将函数内所有调用（除递归外）内联展开
    bool inlineAll(Function *fn);

    // 判断某被调函数是否可展开
    bool canInline(Function *call_fn) const;
};


#endif //COMPILER2021_INLINEFUNCTIONPASS_H
