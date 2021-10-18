//
// Created by hujin on 2021/8/5.
//

#ifndef COMPILER2021_ANALYZEUNWRITTENGLOBALVALUEPASS_H
#define COMPILER2021_ANALYZEUNWRITTENGLOBALVALUEPASS_H

#include "../analyze/AnalyzePointerPass.h"
#include <stack>

class MemRecorder {

public:
    struct Node {
        Assignment *value = nullptr;
        bool written = false;
        bool writtenAll = false;
        std::map<int, unique_ptr<MemRecorder::Node>> children;
        Assignment *lastOffset, *lastValue;
    };
    std::map<Value *, unique_ptr<Node>> records_write;
    std::map<Value *, unique_ptr<Node>> records_read;

    void write(PointerNode *p, Assignment *assignment, bool writeRange);

    bool isWritten(PointerNode *p);

    void read(PointerNode *p, Assignment *assignment, bool readRange);

    bool isRead(PointerNode *p);

};

class AnalyzeUnwrittenGlobalValuePass : public AnalyzeModulePass {
public:
    AnalyzeUnwrittenGlobalValuePass(Module &md, AnalyzeModulePasses &dependency, ostream &out)
            : AnalyzeModulePass(md, out),
              dependency(dependency) {}

    void invalidate() override;

protected:

    bool analyze() override;

public:

    AnalyzeModulePasses dependency;
    std::map<MIR *, Constant *> constantReadMap;
    std::set<MIR *> uselessWriteSet;
};


#endif //COMPILER2021_ANALYZEUNWRITTENGLOBALVALUEPASS_H
