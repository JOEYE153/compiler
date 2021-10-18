//
// Created by 陈思言 on 2021/7/8.
//

#ifndef COMPILER2021_HIRASSEMBLYGENERATOR_H
#define COMPILER2021_HIRASSEMBLYGENERATOR_H


#include "AssemblyGenerator.h"

class HIRAssemblyGenerator : public AssemblyGenerator {
public:
    HIRAssemblyGenerator(Module &md, ostream &out)
            : AssemblyGenerator(md, out) {}

private:
    void runOnFunction(Function &fn) override;

    void runOnBasicBlock(BasicBlock &bb, size_t dfn) override;

    void translateCommonBinaryHIR(BinaryHIR *hir, const string &op);

    void translateCompareHIR(BinaryHIR *hir, const string &op);
};


#endif //COMPILER2021_HIRASSEMBLYGENERATOR_H
