//
// Created by 陈思言 on 2021/8/13.
//

#include "EraseUselessFunctionPass.h"
#include <queue>

bool EraseUselessFunctionPass::run() {
    analyzeCallGraphPass.run();
    std::queue<Function *> usefulFunction;
    usefulFunction.push(md.getFunctionByName("main"));
    auto uselessFunction = md.getFunctionSetPtrOrder();
    while (!usefulFunction.empty()) {
        uselessFunction.erase(usefulFunction.front());
        auto &call_set = analyzeCallGraphPass.callGraph[usefulFunction.front()];
        for (auto fn : call_set) {
            usefulFunction.push(fn);
        }
        call_set.clear();
        usefulFunction.pop();
    }
    for (auto fn : uselessFunction) {
        if (fn->isExternal) {
            continue;
        }
        md.eraseFunctionByName(fn->getName());
    }
    analyzeCallGraphPass.invalidate();
    return !uselessFunction.empty();
}
