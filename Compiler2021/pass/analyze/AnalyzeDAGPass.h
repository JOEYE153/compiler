//
// Created by 陈思言 on 2021/7/20.
//

#ifndef COMPILER2021_ANALYZEDAGPASS_H
#define COMPILER2021_ANALYZEDAGPASS_H

#include "../Pass.h"

class AnalyzeDAGPass : public AnalyzeBasicBlockPass {
public:
    enum DependencyType {
        NONE = 0x00,
        DEF_USE = 0x01,
        STORE_LOAD = 0x02,
        LOAD_STORE = 0x04,
        STORE_STORE = 0x08,
        CONTROL_FLOW = 0x10,
        SYSTEM_CALL = 0x20,
        INDIRECT = 0x40
    };

protected:
    AnalyzeDAGPass(BasicBlock &bb, bool run_floyd, ostream &out = null_out)
            : AnalyzeBasicBlockPass(bb, out), run_floyd(run_floyd) {}

    bool analyze() override;

    void invalidate() override;

    void addAt(size_t i, size_t j, int type) {
        edgeMat[i * exprNum + j] = 1;
        typeMat[i * exprNum + j] |= type;
    }

public:
    [[nodiscard]] int getEdgeAt(size_t i, size_t j) const {
        return edgeMat[i * exprNum + j];
    }

    [[nodiscard]] int getTypeAt(size_t i, size_t j) const {
        return typeMat[i * exprNum + j];
    }

    void print() const;

public:
    // run_floyd为false时为邻接矩阵，1表示结点i是结点j的直接前驱，-1表示直接后继，0表示没有直接前驱后继关系
    // run_floyd为true时为可达性矩阵，正数表示直接或间接前驱，负数表示直接或间接后继，0表示没有前驱后继关系
    vector<int> edgeMat;
    vector<int> typeMat;
    size_t phiNum = 0; // 用于快速跳过phi，IR表中的下标减去此值为DAG图结点下标
    size_t exprNum = 0;

private:
    bool run_floyd;
};

class AnalyzeDAGPass_LIR : public AnalyzeDAGPass {
public:
    AnalyzeDAGPass_LIR(BasicBlock &bb, bool run_floyd, ostream &out = null_out)
            : AnalyzeDAGPass(bb, run_floyd, out) {}

protected:
    bool analyze() override;
};

#endif //COMPILER2021_ANALYZEDAGPASS_H
