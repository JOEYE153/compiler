//
// Created by hujin on 2021/8/6.
//

#include <stack>
#include <cassert>
#include "EraseLocalConstantPass.h"
#include "../analyze/AnalyzeArrayAccessPass.h"
#include "../../utils/IRUtils.h"

std::vector<int> getDimensions(ArrayType *type) {
    vector<int> ret;
    do {
        ret.emplace_back((int) (type->getElementNum().value()));
        type = dynamic_cast<ArrayType *>(type->getElementType().get());
    } while (type != nullptr);
    return std::move(ret);
}

bool EraseLocalConstantPass::run() {
    struct Visitor : MIR::Visitor {
        EraseLocalConstantPass *pass{};

        void visit(LoadConstantMIR *mir) override {
            mir->src = pass->getReplaceConstant(mir->src);
        }

        void visit(ValueAddressingMIR *mir) override {
            auto cnst = dynamic_cast<Constant *> (mir->base);
            if (cnst != nullptr) {
                mir->base = pass->getReplaceConstant(cnst);
            }
        }

        void visit(ArrayAddressingMIR *mir) override {
            auto cnst = dynamic_cast<Constant *> (mir->base);
            if (cnst != nullptr) {
                mir->base = pass->getReplaceConstant(cnst);
            }
        }
    };
    bool altered = false;
    for (auto &c : md.getGlobalConstantSetPtrOrder()) {
        if (c->getType()->getId() == Type::ID::INTEGER) {
            globalIntCnstMap[c->getValue<int>()] = c;
        }
        if (c->getType()->getId() == Type::ID::ARRAY) {
            ArrayType *arrType = dynamic_cast<ArrayType *>(c->getType().get());
            globalVecCnstMap[std::make_pair(getDimensions(arrType), c->getValue<vector<int>>())] = c;
        }
    }
    for (auto *fn : md.getFunctionVecDictOrder()) {
        for (auto &c : fn->getLocalConstantVecDictOrder()) {
            insert_cnst(c, fn->getName());
        }
    }

    for (auto *fn : md.getFunctionVecDictOrder()) {
        for (auto *b : fn->getBasicBlockSetPtrOrder()) {
            for (auto &mir : b->mirTable) {
                Visitor v;
                v.pass = this;
                mir->accept(v);
            }
        }
    }
    return altered;
}

void EraseLocalConstantPass::insert_cnst(Constant *cnst, const string &basicString) {
    if (cnst->getType()->getId() == Type::ID::INTEGER) {
        int val = cnst->getValue<int>();
        if (!globalIntCnstMap.count(val)) {
            auto *cnstNew = md.declareGlobalImmediateInt(val);
            globalIntCnstMap[val] = cnstNew;
        }
    } else if (cnst->getType()->getId() == Type::ID::ARRAY) {
        auto val = cnst->getValue<vector<int>>();
        ArrayType *arrType = dynamic_cast<ArrayType *>(cnst->getType().get());
        if (!globalVecCnstMap.count(std::make_pair(getDimensions(arrType), val))) {
            auto cnstName = cnst->getName();
            auto it = cnstName.find('@');
            cnstName.replace(it, 1, "_");
            it = cnstName.find('.');
            cnstName.erase(it, 1);

            cnstName = "Extracted_" + basicString + cnstName;
            auto *cnstNew = md.declareGlobalConstant(cnst->getType(), cnstName, val);
            std::vector<int> d = getDimensions(dynamic_cast<ArrayType *>(cnst->getType().get()));
            globalVecCnstMap[std::make_pair(d, val)] = cnstNew;
        }
    }
}

void EraseConstantArrReadPass::insert_cnst(ArrayWrite *write, std::vector<int> &val, const string &basicString) {
    Constant *constant;
    std::vector<int> d = getDimensions(dynamic_cast<ArrayType *>(write->writeBase->getType().get()));
    if (!globalVecCnstMap.count(std::make_pair(d, val))) {
        auto cnstName = "WriteResult_" + write->writeBase->getName() + basicString;
        constant = md.declareGlobalConstant(write->writeBase->getType(), cnstName, val);
        globalVecCnstMap[std::make_pair(d, val)] = constant;
    } else constant = globalVecCnstMap[std::make_pair(d, val)];
    arrWriteMap[write] = constant;
}

Constant *EraseLocalConstantPass::getReplaceConstant(Constant *old) {
    if (old->getType()->getId() == Type::ID::INTEGER) {
        int val = old->getValue<int>();
        if (globalIntCnstMap.count(val)) {
            return globalIntCnstMap[val];
        }
    } else if (old->getType()->getId() == Type::ID::ARRAY) {
        auto val = old->getValue<vector<int>>();
        std::vector<int> d = getDimensions(dynamic_cast<ArrayType *>(old->getType().get()));
        if (globalVecCnstMap.count(std::make_pair(d, val))) {
            return globalVecCnstMap[std::make_pair(d, val)];
        }
    }
    return old;
}

bool EraseConstantArrReadPass::run() {
    std::set<ArrayWrite *> vis = {};
    bool altered = EraseLocalConstantPass::run();
    for (auto *fn : md.getFunctionSetPtrOrder()) {
        if (fn->isExternal)continue;
        AnalyzePointerPass *analyzePtr = dependencies[fn].analyzeModulePasses->analyzePointerPass;
        analyzePtr->invalidate();
        AnalyzeArrayAccessPass *analyzeArr = dependencies[fn].analyzeArrayAccessPass;
        analyzeArr->invalidate();
        analyzeArr->run();
        for (auto &x : analyzeArr->mirReadTable) {
            if (vis.count(x.second->useLastWrite))continue;
            vis.insert(x.second->useLastWrite);
            if (x.second->useLastWrite->isConst && !arrWriteMap.count(x.second->useLastWrite)) {
                ArrayWrite *write = x.second->useLastWrite;
                std::stack<ArrayWrite *> updates;
                std::vector<int> v(write->writeBase->getType()->getSizeOfType() / 4);
                while (write != nullptr) {
                    updates.push(write);
                    if (arrWriteMap.count(write)) {
                        break;
                    }
                    auto update = dynamic_cast<ArrayWriteUpdate * >(write);
                    if (update != nullptr) {
                        write = update->updateLastWrite;
                    } else break;
                }
                while (!updates.empty()) {
                    write = updates.top();
                    updates.pop();
                    if (arrWriteMap.count(write)) {
                        v = arrWriteMap[write]->getValue<vector<int>>();
                        continue;
                    }
                    MIR *pMir = analyzeArr->writeMIRTable[write];
                    auto *store = dynamic_cast<StorePointerMIR *> (pMir);
                    auto *copy = dynamic_cast<MemoryCopyMIR *> (pMir);
                    if (store != nullptr) {
                        int offset = analyzePtr->ptrNode[store->dst]->offsetBase.value() / 4;
                        int val = dynamic_cast<LoadConstantMIR *> (store->src)->src->getValue<int>();
                        assert(v.size() > offset);
                        v[offset] = val;
                    } else if (copy != nullptr) {
                        int offset = analyzePtr->ptrNode[copy->dst]->offsetBase.value() / 4;
                        auto *cnst = dynamic_cast<Constant *>(dynamic_cast<ValueAddressingMIR *> (copy->src)->base);
                        auto v2 = cnst->getValue<vector<int>>();
                        for (int i = 0; i < v2.size(); i++) v[i + offset] = v2[i];
                    }
                }
                static size_t uuid = 987654321;
                insert_cnst(x.second->useLastWrite, v, std::to_string(uuid++));
            }
        }
    }


    for (auto *fn : md.getFunctionVecDictOrder()) {
        for (auto *b : fn->getBasicBlockSetPtrOrder()) {
            std::vector<unique_ptr<MIR>> table;
            auto *fake = new BasicBlock("");
            for (auto &mir : b->mirTable) {
                if (dependencies[fn].analyzeArrayAccessPass->mirReadTable.count(mir.get())) {
                    auto read = dependencies[fn].analyzeArrayAccessPass->mirReadTable[mir.get()];
                    if (arrWriteMap.count(read->useLastWrite)) {
                        auto load = dynamic_cast<LoadPointerMIR *> (mir.get());
                        if (load != nullptr) {
                            altered = true;
                            auto base = arrWriteMap[read->useLastWrite];
                            int id = load->id;
                            auto ptr = fake->createArrayAddressingMIR("addr_" + base->getName(), id++, base,
                                                                      read->readOffset[0]);
                            for (int i = 1; i < read->readOffset.size(); ++i) {
                                ptr = fake->createArrayAddressingMIR("addr_" + base->getName(), id++, ptr,
                                                                     read->readOffset[i]);
                            }
                            load->src = ptr;
                        }
                    }
                }
                fake->mirTable.emplace_back(std::move(mir));
            }
            b->mirTable.swap(fake->mirTable);
        }
    }
    return altered;
}
