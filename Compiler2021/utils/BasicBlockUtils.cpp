//
// Created by 陈思言 on 2021/8/1.
//

#include "BasicBlockUtils.h"
#include "IRUtils.h"
void replacePhiIncoming(BasicBlock *bb,
                        const std::map<Assignment *, Assignment *> &replaceTable) {
    for (auto &mir : bb->mirTable) {
        auto mir_phi = dynamic_cast<PhiMIR *>(mir.get());
        if (mir_phi == nullptr) {
            break;
        }
        mir_phi->doReplacement(replaceTable);
    }
}

void replacePhiIncoming(BasicBlock *bb,
                        const std::map<CoreRegAssign *, CoreRegAssign *> &replaceTableCoreReg,
                        const std::map<NeonRegAssign *, NeonRegAssign *> &replaceTableNeonReg,
                        const std::map<StatusRegAssign *, StatusRegAssign *> &replaceTableStatusReg) {
    for (auto &lir : bb->lirTable) {
        auto lir_phi = dynamic_cast<PhiLIR *>(lir.get());
        if (lir_phi == nullptr) {
            break;
        }
        lir_phi->doReplacement(replaceTableCoreReg, replaceTableNeonReg, replaceTableStatusReg);
    }
}

BasicBlock *cloneBlock(BasicBlock *block,
                       std::map<Assignment *, Assignment *> &replaceTable,
                       std::map<BasicBlock *, BasicBlock *> &blockTable,
                       std::map<Constant *, Constant *> &constantTable,
                       std::map<Variable *, Variable *> &variableTable, string_view newName) {
    auto *b = new BasicBlock(newName);
    for (auto &mir : block->mirTable) {
        auto *new_mir = cloneMIR(mir.get(), replaceTable, blockTable, constantTable, variableTable);
        b->mirTable.emplace_back(new_mir);
    }
    blockTable[block] = b;
    return b;
}


std::map<BasicBlock *, BasicBlock *>
cloneSetOfBlocks(Function *fnIn, const std::set<BasicBlock *> &bb, std::map<BasicBlock *, BasicBlock *> &blockTable,
                 std::map<Assignment *, Assignment *> &replaceTable,
                 const string &suffix, bool replaceSuffix) {
    std::map<BasicBlock *, BasicBlock *> ret{};

    for (auto *b: bb) {
        auto name = b->getName();
        if (replaceSuffix) {
            name = name.substr(0, name.rfind(".clone_"));
        }
        ret[b] = fnIn->createBasicBlock(name.append(".clone_").append(suffix));
        blockTable[b] = ret[b];
    }
    std::map<Constant *, Constant *> constantTable = {};
    std::map<Variable *, Variable *> variableTable = {};
    std::vector<MIR *> new_mirs = {};
    for (auto *b: bb) {
        for (auto &mir : b->mirTable) {
            auto *new_mir = cloneMIR(mir.get(), replaceTable, blockTable, constantTable, variableTable);
            ret[b]->mirTable.emplace_back(new_mir);
            new_mirs.push_back(new_mir);
        }
    }
    for (auto *ir : new_mirs) {
        ir->doReplacement(replaceTable);
    }
    return ret;
}



