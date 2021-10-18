//
// Created by 陈思言 on 2021/6/5.
//

#ifndef COMPILER2021_HIR2MIRPASS_H
#define COMPILER2021_HIR2MIRPASS_H

#include "../Pass.h"

class HIR2MIRPass : public ModulePass {
public:
    explicit HIR2MIRPass(Module &md, ostream &out = null_out)
            : ModulePass(md, out) {}

    bool run() override;

private:
    void runOnBasicBlock(BasicBlock &bb);

    Assignment *getValuePtr(BasicBlock &bb, Value *src);

    Assignment *createLoad(BasicBlock &bb, Value *src);

    void createStore(BasicBlock &bb, Variable *dst, Assignment *src);

private:
    size_t currentId = 0;
    std::map<Variable *, Variable *> ref2ptrMap;
};


#endif //COMPILER2021_HIR2MIRPASS_H
