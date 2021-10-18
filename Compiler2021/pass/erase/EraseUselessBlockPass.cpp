//
// Created by 陈思言 on 2021/6/5.
//

#include "EraseUselessBlockPass.h"

bool EraseUselessBlockPass::run() {
    analyzeCFGPass.run();
    for (auto block : analyzeCFGPass.dfsSequence) {
        for (auto &mir : block->mirTable) {
            auto mir_phi = dynamic_cast<PhiMIR *>(mir.get());
            if (mir_phi == nullptr) {
                break;
            }
            for (auto useless_incoming : analyzeCFGPass.remainBlocks) {
                mir_phi->incomingTable.erase(useless_incoming);
            }
        }
    }
    if (analyzeCFGPass.remainBlocks.empty()) {
        return false;
    }
    for (auto block : analyzeCFGPass.remainBlocks) {
        //out << "Erase useless block \"" << block->getName() << "\" in function \"" << fn.getName() << "\"\n";
        fn.eraseBasicBlockByName(block->getName());
    }
    analyzeCFGPass.invalidate();
    return true;
}
