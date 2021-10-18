//
// Created by 陈思言 on 2021/7/16.
//

#ifndef COMPILER2021_MIRASSEMBLYGENERATOR_H
#define COMPILER2021_MIRASSEMBLYGENERATOR_H

#include "AssemblyGenerator.h"

class MIRAssemblyGenerator : public AssemblyGenerator {
public:
    MIRAssemblyGenerator(Module &md, ostream &out)
            : AssemblyGenerator(md, out) {}

private:
    void runOnFunction(Function &fn) override;

    void runOnBasicBlock(BasicBlock &bb, size_t dfn) override;

    void loadBase(MIR *mir, const string &reg);

    void translateCommonBinaryMIR(BinaryMIR *mir, const string &op);

    void translateCompareMIR(BinaryMIR *mir, const string &op);

    void callFunction(vector<Assignment *> args, const string &func);

    string generatePhiLabel();

    void generatePhi(BasicBlock &ori, BasicBlock *bb);

    vector<Assignment *> assignments;
    int incrementer = 0;
};


#endif //COMPILER2021_MIRASSEMBLYGENERATOR_H
