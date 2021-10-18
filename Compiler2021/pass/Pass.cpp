//
// Created by 陈思言 on 2021/5/23.
//

#include "Pass.h"

null_ostream null_out;

bool Pass::run() {
    out << "Run a pass\n";
    return false;
}

bool AnalyzePass::run() {
    if (!isValid) {
        isValid = analyze();
    }
    return false;
}

bool AnalyzePass::analyze() {
    out << "analyze\n";
    return true;
}

bool ModulePass::run() {
    out << "Run a pass on Module " << md.getName() << "\n";
    return false;
}

bool FunctionPass::run() {
    out << "Run a pass on Function " << fn.getName() << "\n";
    return false;
}

bool BasicBlockPass::run() {
    out << "Run a pass on BasicBlock " << bb.getName() << "\n";
    return false;
}
