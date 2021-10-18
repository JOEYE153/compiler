//
// Created by 陈思言 on 2021/7/16.
//

#include "EraseCommonExprPass.h"
#include "../../utils/BasicBlockUtils.h"
#include <unordered_map>

using std::unordered_map;

const int MEM_CACHE_LEN = 8;

bool EraseCommonExprPass::run() {
    analyzeDomTreePass.run();
    analyzeSideEffectPass.run();
    analyzeArrayAccessPass.run();
    currentId = 0;
    for (auto block : analyzeCFGPass.dfsSequence) {
        for (auto &mir : block->mirTable) {
            auto mir_assignment = dynamic_cast<Assignment *>(mir.get());
            if (mir_assignment != nullptr && mir_assignment->id > currentId) {
                currentId = mir_assignment->id;
            }
        }
    }
    currentId++;
    runOnNode(analyzeDomTreePass.root, {});
    return !toErase.empty();
}

void EraseCommonExprPass::runOnNode(DomTreeNode *node, std::map<Assignment *, Assignment *> replaceTable) {
    runOnBlock(node->block, replaceTable);
    auto &rearVector = analyzeCFGPass.result[node->block].rear;
    for (auto rear : rearVector) {
        replacePhiIncoming(rear, replaceTable);
    }
    for (auto &child : node->children) {
        runOnNode(child.get(), replaceTable);
    }
}


inline bool commutative(BinaryMIR::Operator op) {
    return op == BinaryMIR::Operator::ADD || op == BinaryMIR::Operator::MUL
           || op == BinaryMIR::Operator::OR || op == BinaryMIR::Operator::AND || op == BinaryMIR::Operator::XOR
           || op == BinaryMIR::Operator::CMP_EQ || op == BinaryMIR::Operator::CMP_NE;
}

const size_t mod = 999983;
const size_t mod2 = 1226959;

size_t hash_int(size_t x) {
    return (1ull * x % mod * mod2) % mod;
}

size_t hash_commutative(size_t x, size_t y) {
    return (1ull * hash_int(x + 1) * hash_int(y + 1)) % mod2;
}

size_t hash_uncommutative(size_t x, size_t y) {
    return hash_commutative(hash_int(x), y);
}

void EraseCommonExprPass::runOnBlock(BasicBlock *block, std::map<Assignment *, Assignment *> &replaceTable) {
    struct MyVisitor : MIR::Visitor {
        EraseCommonExprPass &pass;
        Assignment *firstExpr = nullptr;
        std::map<Assignment *, Assignment *> *replaceTable;

        struct HashFuncForUnary {
            size_t operator()(UnaryMIR *key) const {
                return hash_uncommutative((size_t) key->op, (size_t) (key->src->id));
            }
        };

        struct EqualFuncForUnary {
            bool operator()(UnaryMIR *a, UnaryMIR *b) const {
                return a->op == b->op && a->src == b->src;
            }
        };

        struct HashFuncForBinary {
            size_t operator()(BinaryMIR *key) const {
                size_t hash_1 = commutative(key->op) ? hash_commutative((size_t) key->src1->id,
                                                                        (size_t) (key->src2->id)) :
                                hash_uncommutative((size_t) key->src1->id, (size_t) (key->src2->id));
                return hash_uncommutative((size_t) key->op, hash_1);
            }
        };

        struct EqualFuncForBinary {
            bool operator()(BinaryMIR *a, BinaryMIR *b) const {
                return a->op == b->op && ((a->src1 == b->src1 && a->src2 == b->src2)
                                          || (commutative(a->op) && a->src1 == b->src2 &&
                                              a->src2 == b->src1)); // 考虑了具有交换律的运算
            }
        };

        struct HashFuncForSelect {
            size_t operator()(SelectMIR *key) const {
                size_t hash1 = hash_uncommutative((size_t) key->src1->id, (size_t) key->src2->id);
                return hash_uncommutative((size_t) key->cond->id, hash1);
            }
        };

        struct EqualFuncForSelect {
            bool operator()(SelectMIR *a, SelectMIR *b) const {
                return a->cond == b->cond && a->src1 == b->src1 && a->src2 == b->src2;
            }
        };

        std::map<Constant *, LoadConstantMIR *> hashMapForLoadConstant;
        unordered_map<UnaryMIR *, UnaryMIR *, HashFuncForUnary, EqualFuncForUnary> hashMapForUnary;
        unordered_map<BinaryMIR *, BinaryMIR *, HashFuncForBinary, EqualFuncForBinary> hashMapForBinary;
        std::map<Value *, ValueAddressingMIR *> hashMapForValueAddressing;
        std::map<std::pair<Value *, Assignment *>, ArrayAddressingMIR *> hashMapForArrayAddressing;
        unordered_map<SelectMIR *, SelectMIR *, HashFuncForSelect, EqualFuncForSelect> hashMapForSelect;
        std::unordered_map<Function *, std::map<std::vector<Assignment *>, CallWithAssignMIR *> > mapForStaticFunction;

        std::map<ArrayWrite *, Assignment *> memWriteCache;
        std::map<ArrayWrite *, std::map<std::vector<Assignment *>, Assignment *>> memReadCache;
        std::map<Value *, std::map<std::vector<Assignment *>, Assignment *>> memReadCacheWithoutWrite;

        explicit MyVisitor(EraseCommonExprPass &pass, std::map<Assignment *, Assignment *> *replaceTable)
                : pass(pass), replaceTable(replaceTable) {}

        enum OperateType {
            SAME, DIFFERENT, OBSCURE
        };

        OperateType isSameOffset(vector<Assignment *> &write, vector<Assignment *> &read_element) {
            int i = 0;
            for (; i < write.size() && i < read_element.size(); i++) {
                if (write[i] != read_element[i]) {
                    if (write[i] == nullptr || read_element[i] == nullptr) {
                        return OBSCURE;
                    }
                    auto w = replaceTable->count(write[i]) ? (*replaceTable)[write[i]] : write[i];
                    auto r = replaceTable->count(read_element[i]) ? (*replaceTable)[read_element[i]] : read_element[i];
                    auto *cnst1 = dynamic_cast<LoadConstantMIR * >(w);
                    auto *cnst2 = dynamic_cast<LoadConstantMIR * >(r);
                    if (cnst1 == nullptr || cnst2 == nullptr) {
                        return OBSCURE;
                    } else return DIFFERENT;
                }
            }
            return (i == write.size() && i == read_element.size()) ? SAME : OBSCURE;
        }

        void visit(LoadConstantMIR *mir) override {
            firstExpr = hashMapForLoadConstant.try_emplace(mir->src, mir).first->second;
        }

        void visit(UnaryMIR *mir) override {
            firstExpr = hashMapForUnary.try_emplace(mir, mir).first->second;
        }
        Assignment *divSame = nullptr, *modSame = nullptr, *subSame = nullptr;

        void visit(BinaryMIR *mir) override {
            if (mir->src1 == mir->src2) {
                switch (mir->op) {
                    case BinaryMIR::Operator::MOD:
                        if (modSame == nullptr) modSame = mir;
                        else {
                            firstExpr = modSame;
                            return;
                        }
                        break;
                    case BinaryMIR::Operator::DIV:
                        if (divSame == nullptr) divSame = mir;
                        else {
                            firstExpr = divSame;
                            return;
                        }
                        break;
                    case BinaryMIR::Operator::SUB:
                        if (subSame == nullptr) subSame = mir;
                        else {
                            firstExpr = subSame;
                            return;
                        }
                        break;
                    default:
                        break;
                }
            }
            firstExpr = hashMapForBinary.try_emplace(mir, mir).first->second;
        }

        void visit(ValueAddressingMIR *mir) override {
            firstExpr = hashMapForValueAddressing.try_emplace(mir->base, mir).first->second;
        }

        void visit(ArrayAddressingMIR *mir) override {
            firstExpr = hashMapForArrayAddressing.try_emplace({mir->base, mir->offset}, mir).first->second;
        }

        void visit(SelectMIR *mir) override {
            firstExpr = hashMapForSelect.try_emplace(mir, mir).first->second;
        }

        void visit(CallWithAssignMIR *mir) override {
            FunctionSideEffect *sideEffects = pass.analyzeSideEffectPass.sideEffects[mir->func];
            if (!mir->func->isExternal &&
                sideEffects->writeByPointerArg.empty() &&
                sideEffects->writeGlobalVariable.empty() &&
                sideEffects->readByPointerArg.empty() &&
                sideEffects->readGlobalVariable.empty() &&
                sideEffects->callExternalFunction.empty()) {
                if (mapForStaticFunction.count(mir->func)) {
                    auto &mp2 = mapForStaticFunction[mir->func];
                    firstExpr = mp2.try_emplace(mir->args, mir).first->second;
                } else {
                    mapForStaticFunction[mir->func] = {{mir->args, mir}};
                    firstExpr = mir;
                }
            } else firstExpr = mir;
        }

        void visit(LoadPointerMIR *mir) override {
            if (!pass.analyzeArrayAccessPass.mirReadTable.count(mir)) {
                return;
            }
            ArrayRead *read = pass.analyzeArrayAccessPass.mirReadTable[mir];
            if (read != nullptr) {
                auto *write = read->useLastWrite;
                for (int i = 0; i < MEM_CACHE_LEN; ++i) {
                    auto type = isSameOffset(read->readOffset, write->writeOffset);
                    if (type == OBSCURE) {
                        // write a[x] read a[0]
                        firstExpr = memReadCache[write].try_emplace(read->readOffset, mir).first->second;
                        return;
                    } else if (type == SAME) {
                        // write a[0] read a[0]
                        firstExpr = memWriteCache.try_emplace(write, mir).first->second;
                        if (firstExpr == mir) {
                            //missing write cache
                            firstExpr = memReadCache[write].try_emplace(read->readOffset, mir).first->second;
                        } else memReadCache[write][read->readOffset] = firstExpr;
                        return;
                    }
                    // write a[1] read a[0]
                    auto *update = dynamic_cast<ArrayWriteUpdate *> (write);
                    if (update == nullptr) {
                        break;
                    }
                    write = update->updateLastWrite;
                }
                // ...? (read a[0]) read a[0]
                if (read->readBase == nullptr) {
                    return;
                }
                firstExpr = memReadCacheWithoutWrite[read->readBase].try_emplace(read->readOffset, mir).first->second;
            }
        }
    };

    struct MemWriteVisitor : MIR::Visitor {
        std::map<ArrayWrite *, Assignment *> &memWriteCache;
        EraseCommonExprPass &pass;

        explicit MemWriteVisitor(EraseCommonExprPass &pass, std::map<ArrayWrite *, Assignment *> &memWriteCache)
                : memWriteCache(memWriteCache), pass(pass) {}

        void visit(StorePointerMIR *mir) override {
            if (pass.analyzeArrayAccessPass.mirWriteTable.count(mir)) {
                memWriteCache[pass.analyzeArrayAccessPass.mirWriteTable[mir]] = mir->src;
            }
        }

        void visit(MemoryFillMIR *mir) override {
            if (pass.analyzeArrayAccessPass.mirWriteTable.count(mir)) {
                memWriteCache[pass.analyzeArrayAccessPass.mirWriteTable[mir]] = mir->src;
            }
        }

        void visit(MemoryCopyMIR *mir) override {
            if (pass.analyzeArrayAccessPass.mirWriteTable.count(mir)) {
                // memWriteCache[pass.analyzeArrayAccessPass.mirWriteTable[mir]] = mir->src;
            }
        }

        void visit(CallMIR *mir) override {
            if (pass.analyzeArrayAccessPass.mirWriteTable.count(mir)) {
                // memWriteCache[pass.analyzeArrayAccessPass.mirWriteTable[mir]] = mir->src;
            }
        }

        void visit(CallWithAssignMIR *mir) override {
            visit(static_cast<CallMIR *>(mir));
        }
    };
    vector<unique_ptr<MIR>> mirTable;
    MyVisitor visitor(*this, &replaceTable);
    MemWriteVisitor visitorW(*this, visitor.memWriteCache);
    for (auto &mir : block->mirTable) {
        mir->doReplacement(replaceTable);
        auto expr = dynamic_cast<Assignment *>(mir.get());
        mir->accept(visitorW);
        if (expr == nullptr) {
            mirTable.emplace_back(std::move(mir));
            continue;
        }
        visitor.firstExpr = expr;
        mir->accept(visitor);
        if (visitor.firstExpr == expr) {
            mirTable.emplace_back(std::move(mir));
        } else {
            replaceTable[expr] = visitor.firstExpr;
            toErase.emplace_back(std::move(mir));
        }
    }
    block->mirTable.swap(mirTable);
    mirTable.clear();
}
