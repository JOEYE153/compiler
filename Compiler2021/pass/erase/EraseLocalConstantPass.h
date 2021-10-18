//
// Created by hujin on 2021/8/6.
//

#ifndef COMPILER2021_ERASELOCALCONSTANTPASS_H
#define COMPILER2021_ERASELOCALCONSTANTPASS_H


#include "../Pass.h"
#include "../analyze/AnalyzeArrayAccessPass.h"

class EraseLocalConstantPass : public ModulePass {
public:
    EraseLocalConstantPass(Module &md, ostream &out = null_out) : ModulePass(md, out) {};

    std::map<int, Constant *> globalIntCnstMap = {};

    std::map<std::pair<vector<int>, vector<int>>, Constant *> globalVecCnstMap = {};

    bool run() override;

protected:

    void insert_cnst(Constant *cnst, const string &basicString);

    Constant *getReplaceConstant(Constant *old);

};

class EraseConstantArrReadPass : public EraseLocalConstantPass {
public:

    EraseConstantArrReadPass(Module &md, std::map<Function *, AnalyzeFunctionPasses> &dependencies, ostream &out)
            : EraseLocalConstantPass(md), dependencies(dependencies) {}

    bool run() override;

private:
    std::map<Function *, AnalyzeFunctionPasses> &dependencies;
    std::map<ArrayWrite *, Constant *> arrWriteMap = {};

    void insert_cnst(ArrayWrite *write, vector<int> &val, const string &basicString);
};

#endif //COMPILER2021_ERASELOCALCONSTANTPASS_H
