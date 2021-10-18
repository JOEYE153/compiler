//
// Created by tcyhost on 2021/8/5.
//

#include "JumpMergePass.h"
#include <cassert>

bool JumpMergePass::run() {
    size_t currentId = 0;
    for (auto block : analyzeCFGPass.dfsSequence) {
        for (auto &mir : block->mirTable) {
            auto mir_assignment = dynamic_cast<Assignment *>(mir.get());
            if (mir_assignment != nullptr && mir_assignment->id > currentId) {
                currentId = mir_assignment->id;
            }
        }
    }
    currentId++;
    analyzeCFGPass.run();
    std::set<BasicBlock *> remainBlocks;
    auto entry = fn.entryBlock;
    while (!entry->mirTable.empty() && entry->mirTable.size() <= 1) {
        auto mir = dynamic_cast<JumpMIR *>(entry->mirTable.back().get());
        if (mir != nullptr) {
            remainBlocks.insert(entry);
            entry = mir->block;
        } else {
            break;
        }
    }
    if (remainBlocks.size() > 1)
        fn.entryBlock = entry;


    std::map<PhiMIR *, std::set<BasicBlock *>> phiErase;
    std::map<PhiMIR *, std::map<BasicBlock *, Assignment *>> phiNew;


    for (auto block : analyzeCFGPass.dfsSequence) {
        if (block->mirTable.size() <= 1) continue;
        if (afterForward && analyzeCFGPass.result[block].rear.size() != 1) continue;
        auto mir = dynamic_cast<JumpMIR *>(block->mirTable.back().get());
        if (mir != nullptr) {
            mir->block = findNextJumpBlock(block, mir->block, remainBlocks, phiErase, phiNew);
            if (mir->block->mirTable.size() == 1) {
                auto retMir = dynamic_cast<ReturnMIR *>(mir->block->mirTable.back().get());
                if (retMir != nullptr) {
                    remainBlocks.insert(mir->block);
                    block->mirTable.resize(block->mirTable.size() - 1);
                    block->mirTable.emplace_back(new ReturnMIR(retMir->val));
                }
            }
            continue;
        }
        auto mir2 = dynamic_cast<BranchMIR *>(block->mirTable.back().get());
        if (mir2 != nullptr) {
            mergeBranchMIR(block, mir2, remainBlocks, phiErase, phiNew, currentId);
            continue;
        }

    }
    if (!afterForward) {
        for (auto &m : phiErase) {
            for (auto *b : m.second) {
                m.first->incomingTable.erase(b);
            }
        }
    }
    for (auto &m : phiNew) {
        for (auto &b : m.second) {
            m.first->incomingTable.erase(b.first);
            m.first->addIncoming(b.first, b.second);
        }
    }
    if (remainBlocks.empty()) {
        return false;
    }

    analyzeCFGPass.invalidate();

    analyzeCFGPass.run();
    for (auto *b : analyzeCFGPass.dfsSequence) {
        for (auto &mir :b->mirTable) {
            auto *phi = dynamic_cast<PhiMIR *>(mir.get());
            if (phi != nullptr) {
                std::set<BasicBlock *> erase;
                for (auto &bb : phi->incomingTable) {
                    if (!analyzeCFGPass.result[b].prev.count(bb.first)) {
                        erase.insert(bb.first);
                    }
                }
                for (auto *bb: erase)phi->incomingTable.erase(bb);
            } else break;
        }
    }
    return true;
}

BasicBlock *
JumpMergePass::findNextJumpBlock(BasicBlock *src, BasicBlock *jumpBlock, std::set<BasicBlock *> &remainBlocks,
                                 std::map<PhiMIR *, std::set<BasicBlock *>> &phiErase,
                                 std::map<PhiMIR *, std::map<BasicBlock *, Assignment *>> &phiNew) {
    auto oriBlock = jumpBlock;
    auto lastBlock = src;
    JumpMIR *tmp;
    int loopCnt = 0;
    while (jumpBlock->mirTable.size() <= 1) {
        loopCnt++;
        if (loopCnt == INT16_MAX) {
            out << "[JumpMerge] achieve max loop cnt " + std::to_string(loopCnt);
            break;
        }
        if (!jumpBlock->mirTable.empty()) {
            tmp = dynamic_cast<JumpMIR *>(jumpBlock->mirTable.back().get());
            if (tmp != nullptr) {
                remainBlocks.insert(jumpBlock);
                lastBlock = jumpBlock;
                jumpBlock = tmp->block;
                continue;
            }
            break;
        } else {
            assert(false);
            // something must be wrong here
        }
    }

    if (jumpBlock != oriBlock) {
        for (auto &mmir : jumpBlock->mirTable) {
            auto mir_phi = dynamic_cast<PhiMIR *>(mmir.get());
            if (mir_phi != nullptr && mir_phi->incomingTable.count(lastBlock)) {
                auto ssa = mir_phi->incomingTable[lastBlock];
                //mir_phi->addIncoming(src, ssa);
                phiNew[mir_phi][src] = ssa;
                phiErase[mir_phi].insert(lastBlock);
            }
        }
    }
    return jumpBlock;
}

void
JumpMergePass::mergeBranchMIR(BasicBlock *block, BranchMIR *mir2, std::set<BasicBlock *> &remainBlocks,
                              std::map<PhiMIR *, std::set<BasicBlock *>> &phiErase,
                              std::map<PhiMIR *, std::map<BasicBlock *, Assignment *>> &phiNew, size_t currentId) {
    std::map<PhiMIR *, std::set<BasicBlock *>> phiEraseTemp1;
    std::map<PhiMIR *, std::set<BasicBlock *>> phiEraseTemp2;
    auto *block1 = findNextJumpBlock(block, mir2->block1, remainBlocks, phiEraseTemp1, phiNew);
    auto *block2 = findNextJumpBlock(block, mir2->block2, remainBlocks, phiEraseTemp2, phiNew);

    bool samePhi = false;
    if (block1 == block2) {
        auto *cond = mir2->cond;
        std::vector<SelectMIR *> select;
        for (auto &x : phiEraseTemp1) {
            if (phiEraseTemp2.count(x.first)) {
                //a--b-->d, a--c->d
                samePhi = true;
                auto &xx = phiEraseTemp2[x.first];
                BasicBlock *bb = *x.second.begin();
                BasicBlock *cc = *xx.begin();
                auto *p1 = x.first->incomingTable[bb];
                auto *p2 = x.first->incomingTable[cc];
                select.push_back(new SelectMIR(x.first->getType(), "", currentId++, cond, p1, p2));
                phiNew[x.first].erase(bb);
                phiNew[x.first].erase(cc);
                phiErase[x.first].insert(bb);
                phiErase[x.first].insert(cc);
                phiNew[x.first][block] = select.back();
            }
        }

        if (!samePhi)
            for (auto &phi1 : phiEraseTemp1) {
                if (phi1.first->incomingTable.count(block)) {
                    //a--b-->d, a-->d
                    samePhi = true;
                    BasicBlock *bb = *phi1.second.begin();
                    auto *p1 = phi1.first->incomingTable[bb];
                    auto *p2 = phi1.first->incomingTable[block];
                    select.push_back(new SelectMIR(phi1.first->getType(), "", currentId++, cond, p1, p2));
                    phiNew[phi1.first].erase(bb);
                    phiErase[phi1.first].insert(bb);
                    phiNew[phi1.first][block] = select.back();
                }
            }
        if (!samePhi)
            for (auto &phi2 : phiEraseTemp2) {
                if (phi2.first->incomingTable.count(block)) {
                    //a-->d, a--c->d
                    samePhi = true;
                    BasicBlock *cc = *phi2.second.begin();
                    auto *p1 = phi2.first->incomingTable[block];
                    auto *p2 = phi2.first->incomingTable[cc];
                    select.push_back(new SelectMIR(phi2.first->getType(), "", currentId++, cond, p1, p2));
                    phiNew[phi2.first].erase(cc);
                    phiErase[phi2.first].insert(cc);
                    phiNew[phi2.first][block] = select.back();
                }
            }

        if (samePhi) {
            block->mirTable.erase(block->mirTable.end() - 1);
            for (auto *s : select) {
                block->mirTable.emplace_back(s);
            }
            block->createJumpMIR(block1);
            return;
        }
    }
    mir2->block1 = block1;
    mir2->block2 = block2;
    for (auto &x : phiEraseTemp1) {
        phiErase[x.first].insert(x.second.begin(), x.second.end());
    }
    for (auto &x : phiEraseTemp2) {
        phiErase[x.first].insert(x.second.begin(), x.second.end());
    }
}
