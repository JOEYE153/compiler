//
// Created by 陈思言 on 2021/8/3.
//

#include "AnalyzeDataFlowPass.h"
#include <algorithm>

bool AnalyzeDataFlowPass::analyze() {
    initOutSet();
    iterateOutSet();
    calculateInSet();
    return true;
}

void AnalyzeDataFlowPass_MIR::invalidate() {
    AnalyzePass::invalidate();
    dataFlowMap.clear();
}

void AnalyzeDataFlowPass_MIR::printDataFlowMap() const {
    for (auto &item : dataFlowMap) {
        out << item.first->getName() << ":\n";
        out << "\tin =\n";
        for (auto &item_in : item.second.in) {
            out << "\t\t" << item_in->getNameAndIdString() << '\n';
        }
        out << "\tout =\n";
        for (auto &item_out : item.second.out) {
            out << "\t\t" << item_out->getNameAndIdString() << '\n';
        }
        out << '\n';
    }
}

bool AnalyzeDataFlowPass_MIR::analyze() {
    analyzeActivityPass.run();
    return AnalyzeDataFlowPass::analyze();
}

template<typename T>
static void AnalyzeDataFlowPass_initOutSet(AnalyzeCFGPass &analyzeCFGPass,
                                           std::map<BasicBlock *, BlockActivity<T>> &blockActivityMap,
                                           std::map<BasicBlock *, DataFlow<T>> &dataFlowMap) {
    for (auto bb : analyzeCFGPass.dfsSequence) {
        auto &dataFlow = dataFlowMap[bb];
        auto &prevSet = analyzeCFGPass.result[bb].prev;
        auto &activity = blockActivityMap[bb];
        for (auto prev : prevSet) {
            auto &prevOut = dataFlowMap[prev].out;
            for (auto &item : activity.useByPhi) {
                if (item.second.find(prev) != item.second.end()) {
                    prevOut.insert(item.first);
                }
            }
            prevOut.insert(activity.use.begin(), activity.use.end());
        }
    }
}

void AnalyzeDataFlowPass_MIR::initOutSet() {
    AnalyzeDataFlowPass_initOutSet(analyzeCFGPass, analyzeActivityPass.blockActivityMap, dataFlowMap);
}

template<typename T>
static void AnalyzeDataFlowPass_iterateOutSet(AnalyzeCFGPass &analyzeCFGPass,
                                              std::map<BasicBlock *, BlockActivity<T>> &blockActivityMap,
                                              std::map<BasicBlock *, DataFlow<T>> &dataFlowMap) {
    size_t lastSize, currSize = 0;
    do {
        lastSize = currSize;
        for (auto bb : analyzeCFGPass.dfsSequence) {
            auto &dataFlow = dataFlowMap[bb];
            auto &prevSet = analyzeCFGPass.result[bb].prev;
            auto &activity = blockActivityMap[bb];
            std::set<T *> tmp;
            std::set_difference(dataFlow.out.begin(), dataFlow.out.end(),
                                activity.def.begin(), activity.def.end(),
                                std::inserter(tmp, tmp.end()));
            for (auto prev : prevSet) {
                dataFlowMap[prev].out.insert(tmp.begin(), tmp.end());
            }
        }
        currSize = 0;
        for (auto bb : analyzeCFGPass.dfsSequence) {
            currSize += dataFlowMap[bb].out.size();
        }
    } while (lastSize < currSize);
}

void AnalyzeDataFlowPass_MIR::iterateOutSet() {
    AnalyzeDataFlowPass_iterateOutSet(analyzeCFGPass, analyzeActivityPass.blockActivityMap, dataFlowMap);
}

template<typename T>
static void AnalyzeDataFlowPass_calculateInSet(AnalyzeCFGPass &analyzeCFGPass,
                                               std::map<BasicBlock *, BlockActivity<T>> &blockActivityMap,
                                               std::map<BasicBlock *, DataFlow<T>> &dataFlowMap) {
    for (auto bb : analyzeCFGPass.dfsSequence) {
        auto &dataFlow = dataFlowMap[bb];
        auto &prevSet = analyzeCFGPass.result[bb].prev;
        auto &activity = blockActivityMap[bb];
        std::set<T *> tmp;
        std::set_difference(dataFlow.out.begin(), dataFlow.out.end(),
                            activity.def.begin(), activity.def.end(),
                            std::inserter(tmp, tmp.end()));

        for (auto &item : activity.useByPhi) {
            dataFlow.in.insert(item.first);
        }
        dataFlow.in.insert(tmp.begin(), tmp.end());
        dataFlow.in.insert(activity.use.begin(), activity.use.end());
    }
}

void AnalyzeDataFlowPass_MIR::calculateInSet() {
    AnalyzeDataFlowPass_calculateInSet(analyzeCFGPass, analyzeActivityPass.blockActivityMap, dataFlowMap);
}

void AnalyzeDataFlowPass_LIR::invalidate() {
    AnalyzePass::invalidate();
    dataFlowMapCoreReg.clear();
//    dataFlowMapNeonReg.clear();
    dataFlowMapStatusReg.clear();
    dataFlowMapCoreMem.clear();
//    dataFlowMapNeonMem.clear();
}

template<typename T>
static void LIR_reg_printDataFlowMap(ostream &out, const std::map<BasicBlock *, DataFlow<T>> &dataFlowMap) {
    for (auto &item : dataFlowMap) {
        out << item.first->getName() << ":\n";
        out << "\tin =\n";
        for (auto &item_in : item.second.in) {
            out << "\t\t" << item_in->getRegString(RegAssign::Format::VREG) << '\n';
        }
        out << "\tout =\n";
        for (auto &item_out : item.second.out) {
            out << "\t\t" << item_out->getRegString(RegAssign::Format::VREG) << '\n';
        }
        out << '\n';
    }
}

template<typename T>
static void LIR_mem_printDataFlowMap(ostream &out, const std::map<BasicBlock *, DataFlow<T>> &dataFlowMap) {
    for (auto &item : dataFlowMap) {
        out << item.first->getName() << ":\n";
        out << "\tin =\n";
        for (auto &item_in : item.second.in) {
            out << "\t\t" << item_in->getMemString() << '\n';
        }
        out << "\tout =\n";
        for (auto &item_out : item.second.out) {
            out << "\t\t" << item_out->getMemString() << '\n';
        }
        out << '\n';
    }
}

void AnalyzeDataFlowPass_LIR::printDataFlowMap() const {
    LIR_reg_printDataFlowMap(out, dataFlowMapCoreReg);
//    LIR_reg_printDataFlowMap(out, dataFlowMapNeonReg);
    LIR_reg_printDataFlowMap(out, dataFlowMapStatusReg);
    LIR_mem_printDataFlowMap(out, dataFlowMapCoreMem);
//    LIR_mem_printDataFlowMap(out, dataFlowMapNeonMem);
}

bool AnalyzeDataFlowPass_LIR::analyze() {
    analyzeActivityPass.run();
    return AnalyzeDataFlowPass::analyze();
}

void AnalyzeDataFlowPass_LIR::initOutSet() {
    AnalyzeDataFlowPass_initOutSet(analyzeCFGPass,
                                   analyzeActivityPass.blockActivityMapCoreReg, dataFlowMapCoreReg);
//    AnalyzeDataFlowPass_initOutSet(analyzeCFGPass,
//                                   analyzeActivityPass.blockActivityMapNeonReg, dataFlowMapNeonReg);
    AnalyzeDataFlowPass_initOutSet(analyzeCFGPass,
                                   analyzeActivityPass.blockActivityMapStatusReg, dataFlowMapStatusReg);
    AnalyzeDataFlowPass_initOutSet(analyzeCFGPass,
                                   analyzeActivityPass.blockActivityMapCoreMem, dataFlowMapCoreMem);
//    AnalyzeDataFlowPass_initOutSet(analyzeCFGPass,
//                                   analyzeActivityPass.blockActivityMapNeonMem, dataFlowMapNeonMem);
}

void AnalyzeDataFlowPass_LIR::iterateOutSet() {
    AnalyzeDataFlowPass_iterateOutSet(analyzeCFGPass,
                                      analyzeActivityPass.blockActivityMapCoreReg, dataFlowMapCoreReg);
//    AnalyzeDataFlowPass_iterateOutSet(analyzeCFGPass,
//                                      analyzeActivityPass.blockActivityMapNeonReg, dataFlowMapNeonReg);
    AnalyzeDataFlowPass_iterateOutSet(analyzeCFGPass,
                                      analyzeActivityPass.blockActivityMapStatusReg, dataFlowMapStatusReg);
    AnalyzeDataFlowPass_iterateOutSet(analyzeCFGPass,
                                      analyzeActivityPass.blockActivityMapCoreMem, dataFlowMapCoreMem);
//    AnalyzeDataFlowPass_iterateOutSet(analyzeCFGPass,
//                                      analyzeActivityPass.blockActivityMapNeonMem, dataFlowMapNeonMem);
}

void AnalyzeDataFlowPass_LIR::calculateInSet() {
    AnalyzeDataFlowPass_calculateInSet(analyzeCFGPass,
                                       analyzeActivityPass.blockActivityMapCoreReg,dataFlowMapCoreReg);
//    AnalyzeDataFlowPass_calculateInSet(analyzeCFGPass,
//                                       analyzeActivityPass.blockActivityMapNeonReg,dataFlowMapNeonReg);
    AnalyzeDataFlowPass_calculateInSet(analyzeCFGPass,
                                       analyzeActivityPass.blockActivityMapStatusReg,dataFlowMapStatusReg);
    AnalyzeDataFlowPass_calculateInSet(analyzeCFGPass,
                                       analyzeActivityPass.blockActivityMapCoreMem,dataFlowMapCoreMem);
//    AnalyzeDataFlowPass_calculateInSet(analyzeCFGPass,
//                                       analyzeActivityPass.blockActivityMapNeonMem,dataFlowMapNeonMem);
}
