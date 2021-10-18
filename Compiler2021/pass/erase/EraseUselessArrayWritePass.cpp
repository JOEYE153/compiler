//
// Created by hujin on 2021/8/15.
//

#include "EraseUselessArrayWritePass.h"
#include "../analyze/AnalyzeArrayAccessPass.h"

bool EraseUselessArrayWritePass::run() {
    analyzeArrayAccessPass.run();
    bool altered = false;
    for (auto &x : analyzeArrayAccessPass.functionReadTable) {
        for (auto *read: x.second) {
            use(read->useLastWrite);
        }
    }
    for (auto &x : analyzeArrayAccessPass.mirReadTable) {
        use(x.second->useLastWrite);
    }
    for (auto *b : fn.getBasicBlockSetPtrOrder()) {
        std::vector<unique_ptr<MIR>> table;

        for (auto &mir : b->mirTable) {
            if (analyzeArrayAccessPass.mirWriteTable.count(mir.get())) {
                auto w = analyzeArrayAccessPass.mirWriteTable[mir.get()];
                if (w->writeBase->getLocation() == Value::Location::STACK && !used.count(w)) {
                    altered = true;
                    continue;
                }
            }
            table.emplace_back(std::move(mir));
        }

        b->mirTable.swap(table);
    }
    if (altered) {
        analyzeArrayAccessPass.invalidate();
    }
    return altered;
}

void EraseUselessArrayWritePass::use(ArrayWrite *write) {
    if (used.count(write))
        return;
    used.insert(write);
    auto update = dynamic_cast<ArrayWriteUpdate *>(write);
    if (update != nullptr)use(update->updateLastWrite);
    auto phi = dynamic_cast<ArrayAccessPhi *>(write);
    if (phi != nullptr) {
        for (auto x : phi->incomingTable) {
            use(x.second);
        }
    }
}