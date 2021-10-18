//
// Created by 陈思言 on 2021/6/17.
//

#ifndef COMPILER2021_ERASEUSELESSVALUEPASS_H
#define COMPILER2021_ERASEUSELESSVALUEPASS_H

#include "../Pass.h"
#include <queue>

class EraseUselessGlobalValuePass : public ModulePass {
protected:
    explicit EraseUselessGlobalValuePass(Module &md, ostream &out = null_out)
            : ModulePass(md, out) {}

    void eraseUselessValue(std::set<Value *> &uselessValueSet);
};

class EraseUselessLocalValuePass : public FunctionPass {
protected:
    explicit EraseUselessLocalValuePass(Function &fn, ostream &out = null_out)
            : FunctionPass(fn, out) {}

    void eraseUselessValue(std::set<Value *> &uselessValueSet);
};

class EraseUselessGlobalValuePass_HIR : public EraseUselessGlobalValuePass {
public:
    explicit EraseUselessGlobalValuePass_HIR(Module &md, ostream &out = null_out)
            : EraseUselessGlobalValuePass(md, out) {}

    bool run() override;
};

class EraseUselessLocalValuePass_HIR : public EraseUselessLocalValuePass {
public:
    explicit EraseUselessLocalValuePass_HIR(Function &fn, ostream &out = null_out)
            : EraseUselessLocalValuePass(fn, out) {}

    bool run() override;
};

class EraseUselessGlobalValuePass_MIR : public EraseUselessGlobalValuePass {
public:
    explicit EraseUselessGlobalValuePass_MIR(Module &md, ostream &out = null_out)
            : EraseUselessGlobalValuePass(md, out) {}

    bool run() override;
};

class EraseUselessLocalValuePass_MIR : public EraseUselessLocalValuePass {
public:
    EraseUselessLocalValuePass_MIR(Function &fn, ostream &out = null_out)
            : EraseUselessLocalValuePass(fn, out) {}

    bool run() override;

protected:
    virtual void initUselessAssignmentSet();

    void updateUselessAssignmentSet();

    std::set<BasicBlock *> blockSet;
    std::set<Assignment *> uselessAssignmentSet;
    std::queue<Assignment *> usefulAssignmentQueue;
};


class SimplifyFunctionCallPass : public EraseUselessLocalValuePass_MIR {
public:
    SimplifyFunctionCallPass(Module *moduleIn, Function &fn, AnalyzeFunctionPasses &dependency,
                             std::queue<Function *> &toInsert, ostream &out = null_out)
            : EraseUselessLocalValuePass_MIR(fn, out),
              analyzeSideEffectPass(*dependency.analyzeModulePasses->analyzeSideEffectPass),
              moduleIn(moduleIn), toInsert(toInsert) {}


public:
    bool run() override;

private:
    void initUselessAssignmentSet() override;

    std::set<CallMIR *> uselessCallVoid;

public:
    AnalyzeSideEffectPass &analyzeSideEffectPass;
    Module *moduleIn;
    std::queue<Function *> &toInsert;
};


class EraseUselessGlobalValuePass_LIR : public EraseUselessGlobalValuePass {
public:
    explicit EraseUselessGlobalValuePass_LIR(Module &md, ostream &out = null_out)
            : EraseUselessGlobalValuePass(md, out) {}

    bool run() override;
};

class EraseUselessLocalValuePass_LIR : public EraseUselessLocalValuePass {
public:
    EraseUselessLocalValuePass_LIR(Function &fn, ostream &out = null_out)
            : EraseUselessLocalValuePass(fn, out) {}

    bool run() override;

private:
    void initUselessAssignmentSet();

    void updateUselessAssignmentSet();

private:
    std::set<BasicBlock *> blockSet;
    std::set<RegAssign *> uselessAssignmentSet;
    std::queue<RegAssign *> usefulAssignmentQueue;
};


#endif //COMPILER2021_ERASEUSELESSVALUEPASS_H
