//
// Created by Joeye on 2021/7/18.
//

#ifndef COMPILER2021_ASSEMBLYGENERATOR_H
#define COMPILER2021_ASSEMBLYGENERATOR_H

#include "CodeSectionGenerator.h"
#include "DataSectionGenerator.h"
#include "../pass/analyze/AnalyzeCFGPass.h"
#include "../pass/analyze/AnalyzeCallGraphPass.h"
#include <algorithm>

class AssemblyGenerator : public ModulePass {
public:
    AssemblyGenerator(Module &md, ostream &out)
            : ModulePass(md, out) {}

    bool run() override;

protected:
    virtual void runOnFunction(Function &fn) = 0;

    virtual void runOnBasicBlock(BasicBlock &bb, size_t dfn) = 0;

    virtual void allocateFrame(const string &tmpReg);

    virtual void freeFrame(const string &tmpReg);

    virtual void load(Value *value, const string &reg);

    virtual void store(Value *value, const string &reg);

    virtual void loadArg(Value *arg, const string &reg);

    virtual void loadImmediate(int im, const string &reg);

    virtual void loadLabel(const string &label, const string &tmpReg);

    virtual string preTreatStackOffset(int offset, const string &tmpReg);

    stringstream out;
    int frameSize = 0;
    std::map<Value *, int> valueOffsetTable;
    vector<BasicBlock *> dfsSequence;
private:
    CodeSectionGenerator codeSection;
    DataSectionGenerator dataSection;
};


#endif //COMPILER2021_ASSEMBLYGENERATOR_H
