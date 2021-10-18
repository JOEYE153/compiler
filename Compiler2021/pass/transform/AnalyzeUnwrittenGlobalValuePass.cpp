//
// Created by hujin on 2021/8/5.
//

#include <cassert>
#include "AnalyzeUnwrittenGlobalValuePass.h"
#include "../analyze/AnalyzeSideEffectPass.h"

void MemRecorder::write(PointerNode *p, Assignment *assignment, bool writeRange) {
    std::stack<PointerNode *> pointers;
    Value *base = p->base;
    while (p != nullptr) {
        pointers.push(p);
        p = p->father;
    }
    if (!records_write.count(base)) {
        records_write[base] = std::make_unique<Node>();
    }
    Node *nodeCur = records_write[base].get();
    while (!pointers.empty()) {
        auto *pp = pointers.top();
        pointers.pop();
        auto *cnst = dynamic_cast<LoadConstantMIR *>(pp->offsetAssignment);
        if (pp->offsetAssignment == nullptr || cnst != nullptr) {
            int offset = pp->offsetAssignment == nullptr ? 0 : cnst->src->getValue<int>();
            if (!nodeCur->children.count(offset)) {
                nodeCur->children[offset] = std::make_unique<Node>();
            }
            nodeCur = nodeCur->children[offset].get();
        } else {
            nodeCur->lastOffset = pp->offsetAssignment;
            nodeCur->lastValue = assignment;
            nodeCur->children.clear();
            nodeCur->written = true;
            return;
        }
    }
    if (writeRange) {
        nodeCur->writtenAll = true;
        nodeCur->written = true;
        nodeCur->children.clear();
        nodeCur->lastOffset = nullptr;
        nodeCur->lastValue = nullptr;
    }
    nodeCur->written = true;
    nodeCur->value = assignment;
}


bool MemRecorder::isWritten(PointerNode *p) {
    std::stack<PointerNode *> pointers;
    Value *base = p->base;
    while (p != nullptr) {
        pointers.push(p);
        p = p->father;
    }
    Node *nodeCur = records_write[base].get();
    while (!pointers.empty()) {
        if (nodeCur == nullptr)
            return false;
        if (nodeCur->written)
            return true;
        auto *pp = pointers.top();
        pointers.pop();
        auto *cnst = dynamic_cast<LoadConstantMIR *>(pp->offsetAssignment);
        if (pp->offsetAssignment == nullptr || cnst != nullptr) {
            int offset = pp->offsetAssignment == nullptr ? 0 : cnst->src->getValue<int>();
            if (nodeCur->writtenAll) {
                return true;
            }
            nodeCur = nodeCur->children[offset].get();
        } else return true;//不确定范围 = 当前节点。由于已经被建立过，所以一定被写入过。
    }
    if (nodeCur == nullptr)
        return false;
    return nodeCur->written;
}

void MemRecorder::read(PointerNode *p, Assignment *assignment, bool readRange) {
    std::stack<PointerNode *> pointers;
    Value *base = p->base;
    while (p != nullptr) {
        pointers.push(p);
        p = p->father;
    }
    if (!records_read.count(base)) {
        records_read[base] = std::make_unique<Node>();
    }
    Node *nodeCur = records_read[base].get();
    while (!pointers.empty()) {
        auto *pp = pointers.top();
        pointers.pop();
        auto *cnst = dynamic_cast<LoadConstantMIR *>(pp->offsetAssignment);
        if (pp->offsetAssignment == nullptr || cnst != nullptr) {
            int offset = pp->offsetAssignment == nullptr ? 0 : cnst->src->getValue<int>();
            if (!nodeCur->children.count(offset)) {
                nodeCur->children[offset] = std::make_unique<Node>();
            }
            nodeCur = nodeCur->children[offset].get();
        } else {
            nodeCur->lastOffset = pp->offsetAssignment;
            nodeCur->lastValue = assignment;
            nodeCur->children.clear();
            nodeCur->written = true;
            return;
        }
    }
    if (readRange) {
        nodeCur->writtenAll = true;
        nodeCur->written = true;
        nodeCur->children.clear();
        nodeCur->lastOffset = nullptr;
        nodeCur->lastValue = nullptr;
    }
    nodeCur->written = true;
    nodeCur->value = assignment;
}


bool MemRecorder::isRead(PointerNode *p) {
    std::stack<PointerNode *> pointers;
    Value *base = p->base;
    while (p != nullptr) {
        pointers.push(p);
        p = p->father;
    }
    Node *nodeCur = records_read[base].get();
    while (!pointers.empty()) {
        if (nodeCur == nullptr)
            return false;
        if (nodeCur->written)
            return true;
        auto *pp = pointers.top();
        pointers.pop();
        auto *cnst = dynamic_cast<LoadConstantMIR *>(pp->offsetAssignment);
        if (pp->offsetAssignment == nullptr || cnst != nullptr) {
            int offset = pp->offsetAssignment == nullptr ? 0 : cnst->src->getValue<int>();
            if (nodeCur->writtenAll) {
                return true;
            }
            nodeCur = nodeCur->children[offset].get();
        } else return true;//不确定范围 = 当前节点。由于已经被建立过，所以一定被写入过。
    }
    if (nodeCur == nullptr)
        return false;
    return nodeCur->written;
}

bool AnalyzeUnwrittenGlobalValuePass::analyze() {
    std::map<Value *, bool> mp_write;
    std::map<Value *, bool> mp_read;
    dependency.analyzePointerPass->run();
    struct Visitor : MIR::Visitor {
        std::map<Value *, bool> &mp_read;
        std::map<Value *, bool> &mp_write;
        MemRecorder &memRecorder;
        Module &md;
        AnalyzeUnwrittenGlobalValuePass &pass;
        AnalyzePointerPass &analyzePointerPass;

        Visitor(std::map<Value *, bool> &mp_read, std::map<Value *, bool> &mp_write, MemRecorder &memRecorder,
                Module &md,
                AnalyzeUnwrittenGlobalValuePass &pass,
                AnalyzePointerPass &analyzePointerPass) :
                mp_read(mp_read), mp_write(mp_write), memRecorder(memRecorder), md(md), pass(pass),
                analyzePointerPass(analyzePointerPass) {};
        MIR *new_mir = nullptr;

        void visit(LoadVariableMIR *mir) override {
            if (mir->src->getLocation() == Value::Location::STATIC &&
                (!mp_write.count(mir->src) || !mp_write[mir->src])) {
                int v = 0;
                if (md.initializationTable.count(mir->src) && md.initializationTable[mir->src].has_value()) {
                    v = any_cast<int>(md.initializationTable[mir->src]);
                }
                auto *constant = md.declareGlobalImmediateInt(v);
                pass.constantReadMap[mir] = constant;
                //std::cout << "turned to global cnst: " << mir->src->getName() << "=" << v << std::endl;
            }
        }

        void visit(LoadPointerMIR *mir) override {
            PointerNode *p = analyzePointerPass.ptrNode[mir->src];
            if (p->offsetBase.has_value() && p->base->getLocation() == Value::Location::STATIC
                && !memRecorder.isWritten(p)) {
                int v = 0, offset = p->offsetBase.value();
                auto *keyval = dynamic_cast<Variable *>(p->base);
                if (md.initializationTable.count(keyval) && md.initializationTable[keyval].has_value()) {
                    const vector<int> &vector1 = any_cast<vector<int>>(md.initializationTable[keyval]);
                    if (offset / 4 >= vector1.size()) {
                        throw std::logic_error("Index out of bound");
                    } else v = vector1[offset / 4];
                }
                auto *constant = md.declareGlobalImmediateInt(v);
                pass.constantReadMap[mir] = constant;
//                std::cout << "turned to global cnst: " << p->base->getName() << '[' << offset / 4 << ']' << "=" << v
//                          << std::endl;
            }
        }

        void visit(StoreVariableMIR *mir) override {
            if (mir->src->getLocation() == Value::Location::STATIC &&
                (!mp_read.count(mir->src) || !mp_read[mir->src])) {
                pass.uselessWriteSet.insert(mir);
            }
        }

        void visit(StorePointerMIR *mir) override {
            PointerNode *p = analyzePointerPass.ptrNode[mir->dst];
            if (p->offsetBase.has_value() && p->base->getLocation() == Value::Location::STATIC
                && !memRecorder.isRead(p)) {
                pass.uselessWriteSet.insert(mir);
            }
        }

    };
    for (auto *f : md.getFunctionSetPtrOrder()) {
        for (auto *b : f->getBasicBlockSetPtrOrder()) {
            for (auto &mir: b->mirTable) {
                auto store = dynamic_cast<StoreVariableMIR *>(mir.get());
                if (store != nullptr) {
                    mp_write[store->dst] = true;
                }
                auto read = dynamic_cast<LoadVariableMIR *>(mir.get());
                if (read != nullptr) {
                    mp_read[read->src] = true;
                }
            }
        }
    }
    MemRecorder memRecorder{};
    for (auto &p : dependency.analyzePointerPass->ptrNode) {
        if (p.second->base->getLocation() == Value::Location::STATIC && p.second->write) {
            //std::cerr << "global write:" << p.second->base->getName() << '[' << offsettt << ']' << std::endl;
            memRecorder.write(p.second, nullptr, p.second->writeAll);
        }
        if (p.second->base->getLocation() == Value::Location::STATIC && p.second->read) {
            //std::cerr << "global write:" << p.second->base->getName() << '[' << offsettt << ']' << std::endl;
            memRecorder.read(p.second, nullptr, p.second->readAll);
        }
    }
    std::map<Assignment *, Assignment *> replacementTable = {};
    Visitor v{mp_read, mp_write, memRecorder, md, *this, *dependency.analyzePointerPass};
    for (auto *f : md.getFunctionSetPtrOrder()) {
        for (auto *b : f->getBasicBlockSetPtrOrder()) {
            vector<unique_ptr<MIR>> mirTable;
            for (auto &mir: b->mirTable) {
                mir->accept(v);
            }
        }
    }
    return true;
}

void AnalyzeUnwrittenGlobalValuePass::invalidate() {
    AnalyzePass::invalidate();
    constantReadMap.clear();
    uselessWriteSet.clear();
}
