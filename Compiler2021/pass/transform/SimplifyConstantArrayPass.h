//
// Created by 陈思言 on 2021/8/14.
//

#ifndef COMPILER2021_SIMPLIFYCONSTANTARRAYPASS_H
#define COMPILER2021_SIMPLIFYCONSTANTARRAYPASS_H

#include "../Pass.h"
#include "../../utils/ConstUtils.h"

class SimplifyConstantArrayPass : public ModulePass {
public:
    explicit SimplifyConstantArrayPass(Module &md, ostream &out = null_out)
            : ModulePass(md, out) {}

    bool run() override;

private:
    bool runOnBasicBlock(BasicBlock *bb, std::map<Assignment *, Assignment *> &replaceTable);

private:
    vector<unique_ptr<MIR>> toErase;
};


#endif //COMPILER2021_SIMPLIFYCONSTANTARRAYPASS_H
