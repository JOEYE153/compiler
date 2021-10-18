//
// Created by hujin on 2021/8/5.
//

#include <memory>
#include "AnalyzePointerPass.h"
#include "AnalyzeCFGPass.h"

void AnalyzePointerPass::invalidate() {
    AnalyzePass::invalidate();
    ptrNode.clear();
    ptrTree.clear();
}

bool AnalyzePointerPass::analyze() {
    struct Visitor : MIR::Visitor {
        AnalyzePointerPass *pass{};

        void visit(ValueAddressingMIR *mir) override {
            auto *value = mir->base;
            PointerNode *node;
            if (pass->ptrNode[value] == nullptr) {
                pass->ptrTree[value] = std::make_unique<PointerNode>();
                pass->ptrNode[value] = node = pass->ptrTree[value].get();
                node->base = value;
                node->rangeSize = mir->base->getType()->getSizeOfType();
            } else node = pass->ptrNode[value];
            pass->ptrNode[mir] = node;
        }

        void visit(LoadVariableMIR *mir) override {
            if (mir->getType()->getId() != Type::ID::POINTER) {
                return;
            }
            auto *value = mir->src;
            PointerNode *node;
            if (pass->ptrNode[value] == nullptr) {
                pass->ptrTree[value] = std::make_unique<PointerNode>();
                pass->ptrNode[value] = node = pass->ptrTree[value].get();
                node->base = value;
            } else node = pass->ptrNode[value];
            pass->ptrNode[mir] = node;
        }

        void visit(ArrayAddressingMIR *mir) override {
            auto *node = new PointerNode();
            auto *assignment = dynamic_cast<Assignment *>(mir->base);
            if (assignment != nullptr) {
                if (!pass->ptrNode.count(assignment)) {
                    pass->out << "unsupported pointer assignment" << std::endl;
                }
                auto *father = pass->ptrNode[assignment];
                node->rangeSize = dynamic_cast<PointerType *>(mir->getType().get())->getElementType()->getSizeOfType();
                node->father = father;
                node->base = father->base;
                father->children.emplace(node);
            } else {
                auto *value = mir->base;
                PointerNode *nodeF;
                if (pass->ptrNode[value] == nullptr) {
                    pass->ptrTree[value] = std::make_unique<PointerNode>();
                    pass->ptrNode[value] = nodeF = pass->ptrTree[value].get();
                    nodeF->base = value;
                    nodeF->rangeSize = value->getType()->getSizeOfType();
                } else nodeF = pass->ptrNode[value];

                node->base = value;
                node->father = nodeF;
                nodeF->children.emplace(node);
                node->base = mir->base;
                node->rangeSize = dynamic_cast<ArrayType *>(value->getType().get())->getElementType()->getSizeOfType();
                pass->ptrNode[mir] = node;
            }
            pass->ptrNode[mir] = node;
            auto *cnst = dynamic_cast<LoadConstantMIR *>(mir->offset);
            if (cnst != nullptr) {
                int offset = cnst->src->getValue<int>();
                if (!node->father->offsetBase.has_value()) node->offsetBase.reset();
                else
                    node->offsetBase =
                            node->father->offsetBase.value() + offset * node->rangeSize.value();
            } else {
                node->offsetBase.reset();
            }
            node->offsetAssignment = mir->offset;
        }

        void read(Assignment *assignment) const {
            if (!pass->ptrNode.count(assignment)) {
                pass->out << "Missing Pointer Assignment" << std::endl;
            }
            pass->ptrNode[assignment]->read = true;
        }

        void write(Assignment *assignment) const {
            if (!pass->ptrNode.count(assignment)) {
                pass->out << "Missing Pointer Assignment" << std::endl;
            }
            pass->ptrNode[assignment]->write = true;
        }

        void writeAll(Assignment *assignment) const {
            if (!pass->ptrNode.count(assignment)) {
                pass->out << "Missing Pointer Assignment" << std::endl;
            }
            pass->ptrNode[assignment]->write = true;
            pass->ptrNode[assignment]->writeAll = true;
        }

        void readAll(Assignment *assignment) const {
            if (!pass->ptrNode.count(assignment)) {
                pass->out << "Missing Pointer Assignment" << std::endl;
            }
            pass->ptrNode[assignment]->read = true;
            pass->ptrNode[assignment]->readAll = true;
        }

        void visit(LoadPointerMIR *mir) override {
            read(mir->src);
        }

        void visit(StorePointerMIR *mir) override {
            write(mir->dst);
        }

        void visit(CallMIR *mir) override {
            for (size_t i: pass->analyzeSideEffectPass.sideEffects[mir->func]->readByPointerArg) {
                readAll(mir->args[i]);
            }
            for (size_t i: pass->analyzeSideEffectPass.sideEffects[mir->func]->writeByPointerArg) {
                writeAll(mir->args[i]);
            }
        }

        void visit(CallWithAssignMIR *mir) override {
            visit(static_cast<CallMIR *>(mir));
        }

        void visit(MultiCallMIR *mir) override {
            visit(static_cast<CallMIR *>(mir));
        }

        void visit(MemoryCopyMIR *mir) override {
            readAll(mir->src);
            writeAll(mir->dst);
        }

        void visit(MemoryFillMIR *mir) override {
            writeAll(mir->dst);
        }
    };
    analyzeSideEffectPass.run();
    for (auto *f : md.getFunctionSetPtrOrder()) {
        if (f->isExternal) {
            continue;
        }

        AnalyzeCFGPass_MIR analyzeCfgPass = AnalyzeCFGPass_MIR(*f, out);
        analyzeCfgPass.run();
        Visitor visitor{};
        visitor.pass = this;
        for (auto *b : analyzeCfgPass.dfsSequence) {
            for (auto &mir :b->mirTable) {
                mir->accept(visitor);
            }
        }
    }


    return true;
}

