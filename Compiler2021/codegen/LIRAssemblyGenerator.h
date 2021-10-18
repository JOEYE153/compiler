//
// Created by Joeye on 2021/8/3.
//

#ifndef COMPILER2021_LIRASSEMBLYGENERATOR_H
#define COMPILER2021_LIRASSEMBLYGENERATOR_H

#include "AssemblyGenerator.h"

class LIRAssemblyGenerator : public AssemblyGenerator{
public:
    LIRAssemblyGenerator(Module &md, ostream &out)
    : AssemblyGenerator(md, out) {}

private:
    void runOnFunction(Function &fn) override;

    void runOnBasicBlock(BasicBlock &bb, size_t dfn) override;

    bool preTreatLIR(LIR* lir, BasicBlock &ori, size_t dfn);

private:
    stringstream ss;
};


#endif //COMPILER2021_LIRASSEMBLYGENERATOR_H
