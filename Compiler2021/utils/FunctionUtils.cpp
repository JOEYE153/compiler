//
// Created by 陈思言 on 2021/8/1.
//

#include "IRUtils.h"
#include "FunctionUtils.h"
#include "../frontend/generate/HIRGenerator.h"
#include <stdexcept>
#include <algorithm>
#include <sstream>

using namespace frontend::generate;
using std::logic_error;
using std::vector;

Function *
extractFunction(Module *md, Function *src, AnalyzeRegionPass::NodeMin *region, AnalyzeFunctionPasses &functionPasses) {
    // step 0. find all blocks that need to be extracted
    auto etrBlocks = getAllEtrBlocks(functionPasses, region->entrance, region->exit);

    // step 1. add new prev Block in src
    auto prevBlock = createNewPrevBlock(src, etrBlocks, functionPasses);

    // step 2. find all input & output and split into scalars and arrays
    auto params = getParams(etrBlocks, functionPasses);
    vector<pair<Assignment *, bool>> scalars, arrays;
    splitArrScalar(params, scalars, arrays);

    static int tmpId = 8182;
    auto scalar_arr = src->declareLocalVariable(std::make_shared<ArrayType>(IntegerType::object, scalars.size()),
                                                ".etr_scalar_arr", false);
    auto ptr_arr = prevBlock->createValueAddressingMIR(scalar_arr->getName(), tmpId++, scalar_arr);
    storeInputScalar(src, prevBlock, ptr_arr, scalars, tmpId);

    vector<Assignment *> args = {ptr_arr}; // real params init
    vector<std::shared_ptr<Type>> argTypes = {ptr_arr->getType()};
    for (auto ssa : arrays) {
        auto ptr = prevBlock->createValueAddressingMIR(ssa.first->getName(), tmpId++, ssa.first);
        args.emplace_back(ptr);
        argTypes.emplace_back(ptr->getType());
    }
    auto new_fn = md->declareFunction(std::make_shared<FunctionType>(VoidType::object, argTypes),
                                      src->getName() + "_" + prevBlock->getName() + "_etrfn");
    new_fn->args.clear();
    for (auto &arg : args) {
        auto newArg = new_fn->declareLocalVariable(arg->getType(),
                                                   arg->getName() + std::to_string(arg->id) + "_etrfn", false, true);
        new_fn->args.emplace_back(newArg);
    }

    extractCallee(new_fn, etrBlocks, scalars, arrays, region->exit);

    prevBlock->createCallMIR(new_fn, args);

    loadOutputScalar(src, prevBlock, ptr_arr, scalars, tmpId);

    prevBlock->createJumpMIR(region->exit);

    functionPasses.analyzeCFGPass->invalidate();
    functionPasses.analyzeActivityPass->invalidate();
    functionPasses.analyzeDataFlowPass->invalidate();

    return new_fn;
}

void extractCallee(Function *new_fn, vector<BasicBlock *> &etrBlocks, vector<pair<Assignment *, bool>> &scalars,
                   vector<pair<Assignment *, bool>> &arrays, BasicBlock *oldExitBlock) {
    // 把block里的mir全移过来
    // input scalar要进行load, arrays也一样
    // 然后把mir里的都replace掉
    // 最后把output scalar对应的给store进参数里
    // 最后一个block要加返回
    auto entryBlock = new_fn->createBasicBlock(new_fn->getName() + "_entry");
    new_fn->entryBlock = entryBlock;
    vector<BasicBlock *> newBlocks;
    for (auto *b : etrBlocks) {
        auto new_b = new_fn->createBasicBlock(b->getName());
        newBlocks.emplace_back(new_b);
        new_b->mirTable.swap(b->mirTable);
    }
    int tmpId = 8182;
    std::map<Assignment *, Assignment *> replaceTable;
    for (auto idx = 0; idx < scalars.size(); idx++) {
        if (scalars[idx].second) { // load scalar input
            auto ssa = scalars[idx].first;
            auto offset = entryBlock->createLoadConstantMIR("off_" + std::to_string(idx), tmpId++,
                                                            new_fn->declareLocalImmediateInt(idx));
            auto tmp = entryBlock->createArrayAddressingMIR("addr_" + std::to_string(idx), tmpId++,
                                                            new_fn->args[0], offset);
            auto ltr = entryBlock->createLoadPointerMIR("ldr_" + std::to_string(idx), tmpId++, tmp);
            replaceTable[ssa] = ltr;
        }
    }

    // replace all arr pointer
    for (auto idx = 0; idx < arrays.size(); idx++) {
        auto arr = new_fn->args[idx + 1];
        auto ptr = entryBlock->createValueAddressingMIR(arr->getName(), tmpId++, arr);
        replaceTable[arrays[idx].first] = ptr;
    }
    entryBlock->createJumpMIR(newBlocks.front());

    for (auto *b : newBlocks) {
        for (auto &mir : b->mirTable) {
            mir->doReplacement(replaceTable);
        }
    }

    auto exitBlock = new_fn->createBasicBlock(new_fn->getName() + "_exit");
    for (auto *b : newBlocks) {
        replaceJumpBlock(b, oldExitBlock, exitBlock);
    }

    for (auto idx = 0; idx < scalars.size(); idx++) {
        if (!scalars[idx].second) { // store scalar output
            auto ssa = scalars[idx].first;
            auto offset = exitBlock->createLoadConstantMIR("off_" + std::to_string(idx), tmpId++,
                                                           new_fn->declareLocalImmediateInt(idx));
            auto tmp = exitBlock->createArrayAddressingMIR("addr_" + std::to_string(idx), tmpId++,
                                                           new_fn->args[0], offset);
            exitBlock->createStorePointerMIR(tmp, ssa);
        }
    }

    exitBlock->createReturnMIR();
}

void storeInputScalar(Function *src, BasicBlock *prevBlock, ValueAddressingMIR *ptr_arr,
                      vector<pair<Assignment *, bool>> &scalars, int &tmpId) {
    for (auto idx = 0; idx < scalars.size(); idx++) {
        if (scalars[idx].second) { // scalar input init
            auto ssa = scalars[idx].first;
            auto str = ssa;
            auto offset = prevBlock->createLoadConstantMIR("off_" + std::to_string(idx), tmpId++,
                                                           src->declareLocalImmediateInt(idx));
            auto tmp = prevBlock->createArrayAddressingMIR("addr_" + std::to_string(idx), tmpId++, ptr_arr, offset);
            auto bool_type = dynamic_cast<BooleanType *>(ssa->getType().get());
            if (bool_type != nullptr) {
                auto const1 = prevBlock->createLoadConstantMIR(ssa->getName() + "_c1", tmpId++,
                                                               src->declareLocalImmediateInt(1));
                auto const0 = prevBlock->createLoadConstantMIR(ssa->getName() + "_c0", tmpId++,
                                                               src->declareLocalImmediateInt(0));
                auto select = prevBlock->createSelectMIR(ssa->getName(), tmpId++, ssa, const1, const0);
                str = select;
            }
            prevBlock->createStorePointerMIR(tmp, str);
        }
    }
}

void loadOutputScalar(Function *src, BasicBlock *prevBlock, ValueAddressingMIR *ptr_arr,
                      vector<pair<Assignment *, bool>> &scalars, int &tmpId,
                      std::map<Assignment *, Assignment *> replaceTable) {
    for (auto idx = 0; idx < scalars.size(); idx++) {
        if (!scalars[idx].second) { // load scalar output
            auto offset = prevBlock->createLoadConstantMIR("off_" + std::to_string(idx), tmpId++,
                                                           src->declareLocalImmediateInt(idx));
            auto tmp = prevBlock->createArrayAddressingMIR("addr_" + std::to_string(idx), tmpId++, ptr_arr, offset);
            auto newLoadPtr = prevBlock->createLoadPointerMIR(tmp->getName(), tmpId++, tmp);
            auto ssa = scalars[idx].first;
            Assignment *lfr = newLoadPtr;
            auto bool_type = dynamic_cast<BooleanType *>(ssa->getType().get());
            if (bool_type != nullptr) {
                auto const0 = prevBlock->createLoadConstantMIR(ssa->getName() + "_c0", tmpId++,
                                                               src->declareLocalImmediateInt(0));
                auto cmpNe = prevBlock->createCmpNeMIR(ssa->getName() + "_ne", tmpId++, lfr, const0);
                lfr = cmpNe;
            }
            replaceTable[ssa] = lfr;
            puts(ssa->castToMIR()->toString().c_str());
            puts(lfr->castToMIR()->toString().c_str());
        }
    }
    // replace caller
    for (auto *b : src->getBasicBlockVecDictOrder()) {
        for (auto &mir : b->mirTable) {
            mir->doReplacement(replaceTable);
        }
    }
}

BasicBlock *createNewPrevBlock(Function *src, vector<BasicBlock *> &etrBlocks, AnalyzeFunctionPasses &functionPasses) {
    auto oldBlock = etrBlocks.front();
    auto oldPrevBlocks = functionPasses.analyzeCFGPass->result[oldBlock].prev;
    auto prevBlock = src->createBasicBlock(etrBlocks.front()->getName() + "_prev");
    for (auto *b : oldPrevBlocks) {
        replaceJumpBlock(b, oldBlock, prevBlock);
    }
    return prevBlock;
}

void replaceJumpBlock(BasicBlock *b, BasicBlock *old_b, BasicBlock *new_b) {
    auto mir = b->mirTable.back().get();
    auto jumpMir = dynamic_cast<JumpMIR *>(mir);
    if (jumpMir != nullptr) {
        if (jumpMir->block == old_b) jumpMir->block = new_b;
        return;
    }
    auto brMir = dynamic_cast<BranchMIR *>(mir);
    if (brMir != nullptr) {
        if (brMir->block1 == old_b) brMir->block1 = new_b;
        if (brMir->block2 == old_b) brMir->block2 = new_b;
        return;
    }
}

vector<BasicBlock *> getAllEtrBlocks(AnalyzeFunctionPasses &functionPasses, BasicBlock *entrance, BasicBlock *exit) {
    vector<BasicBlock *> etrBlocks;
    bool etr = false;
    functionPasses.analyzeCFGPass->run();
    for (auto *b : functionPasses.analyzeCFGPass->dfsSequence) {
        if (b == entrance) etr = true;
        if (b == exit) etr = false;
        if (etr) etrBlocks.emplace_back(b);
    }
    return std::move(etrBlocks);
}

vector<pair<Assignment *, bool>> getParams(vector<BasicBlock *> &etrBlocks, AnalyzeFunctionPasses &functionPasses) {
    vector<Assignment *> input, output;
    auto dataFlowPass = dynamic_cast<AnalyzeDataFlowPass_MIR *>(functionPasses.analyzeDataFlowPass);
    auto activityPass = dynamic_cast<AnalyzeActivityPass_MIR *>(functionPasses.analyzeActivityPass);
    dataFlowPass->run();
    activityPass->run();

    std::set<Assignment *> use;
    for (auto *b : etrBlocks) {
        auto &b_use = activityPass->blockActivityMap[b].use;
        std::set_union(use.begin(), use.end(), b_use.begin(), b_use.end(), std::inserter(use, use.end()));
    }
    auto &in = dataFlowPass->dataFlowMap[etrBlocks.front()].in;
    std::set_intersection(in.begin(), in.end(), use.begin(), use.end(), std::inserter(input, input.begin()));

    std::set<Assignment *> def;
    for (auto *b : etrBlocks) {
        auto &b_def = activityPass->blockActivityMap[b].def;
        std::set_union(def.begin(), def.end(), b_def.begin(), b_def.end(), std::inserter(def, def.end()));
    }
    auto &out = dataFlowPass->dataFlowMap[etrBlocks.back()].out;
    std::set_intersection(out.begin(), out.end(), def.begin(), def.end(), std::inserter(output, output.begin()));

    vector<pair<Assignment *, bool>> params;
    params.reserve(input.size() + output.size());
    for (auto *ssa : input) {
//        auto const_mir = dynamic_cast<LoadConstantMIR *>(ssa);
//        if (const_mir != nullptr) {
//            new LoadConstantMIR(*const_mir);
//        }
        params.emplace_back(std::make_pair(ssa, true));
    }
    for (auto *ssa : output) {
//        auto const_mir = dynamic_cast<LoadConstantMIR *>(ssa);
//        if (const_mir != nullptr) {
//            new LoadConstantMIR(*const_mir);
//        }
        params.emplace_back(std::make_pair(ssa, false));
    }
    return params;
}

void splitArrScalar(vector<pair<Assignment *, bool>> &input, vector<pair<Assignment *, bool>> &scalars,
                    vector<pair<Assignment *, bool>> &arrays) {
    for (auto &ssa : input) {
        auto ptr_type = dynamic_cast<PointerType *>(ssa.first->getType().get());
        if (ptr_type != nullptr) {
            arrays.emplace_back(ssa);
            continue;
        }
        auto int_type = dynamic_cast<IntegerType *>(ssa.first->getType().get());
        if (int_type != nullptr) {
            scalars.emplace_back(ssa);
            continue;
        }
        auto bool_type = dynamic_cast<BooleanType *>(ssa.first->getType().get());
        if (bool_type != nullptr) {
            scalars.emplace_back(ssa);
            continue;
        }
        throw logic_error("illegal type in extract Function");
    }
}


Function *cloneFunction(Module *md, Function *src, string_view dst_name) {
    auto dst = md->declareFunction(src->getType(), dst_name);

    dst->isExternal = src->isExternal;

    auto srcConstantTable = src->getLocalConstantVecDictOrder();
    std::map<Constant *, Constant *> constantTable;
    for (auto srcConst : srcConstantTable) {
        auto newConst = dst->declareLocalConstant(srcConst->getType(), srcConst->getName(),
                                                  srcConst->getCopyOfValue());
        constantTable[srcConst] = newConst;
    }

    auto srcVariableTable = src->getLocalVariableVecDictOrder();
    std::map<Variable *, Variable *> variableTable;
    for (auto srcVar : srcVariableTable) {
        auto newVar = dst->declareLocalVariable(srcVar->getType(), srcVar->getName(), srcVar->isReference(),
                                                srcVar->isArgument());
        variableTable[srcVar] = newVar;
    }
    for (auto i = 0; i < src->args.size(); i++) {
        dst->args[i] = dst->getLocalVariableByName(src->args[i]->getName());
    }

    // for jump
    std::map<BasicBlock *, BasicBlock *> blockTable;
    for (auto bb : src->getBasicBlockVecDictOrder()) {
        std::stringstream ss;
        ss << dst_name;
        ss << ".";
        ss << bb->getName();
        auto new_bb = dst->createBasicBlock(ss.str());
        blockTable[bb] = new_bb;
    }
    dst->entryBlock = blockTable[src->entryBlock];

    std::map<Assignment *, Assignment *> replaceTable;
    vector<unique_ptr<MIR>> mirTable;
    for (auto bb : src->getBasicBlockVecDictOrder()) {
        auto new_bb = blockTable[bb];
        mirTable.clear();
        for (auto &mir : bb->mirTable) {
            auto new_mir = cloneMIR(mir.get(), replaceTable, blockTable, constantTable, variableTable);
            if (new_mir != nullptr) {
                mirTable.emplace_back(new_mir);
            }
        }
        new_bb->mirTable.swap(mirTable);
    }
    for (auto bb : dst->getBasicBlockVecDictOrder()) {
        for (auto &mir : bb->mirTable) {
            mir->doReplacement(replaceTable);
        }
    }
    return dst;
}

BasicBlock *inlineFunction(Function *fn, BasicBlock *bb, size_t call_index) {
    if (call_index >= bb->mirTable.size()) {
        throw logic_error("Out of range");
    }
    auto call_mir = dynamic_cast<CallMIR *>(bb->mirTable[call_index].get());
    if (call_mir == nullptr) {
        throw logic_error("Not a call");
    }
    auto src = call_mir->func;
    if (src->isExternal) return nullptr;

    auto callWithAssignMir = dynamic_cast<CallWithAssignMIR *>(call_mir);
    PhiMIR *phiMir = nullptr;
    if (callWithAssignMir != nullptr) {
        phiMir = new PhiMIR(callWithAssignMir->getType(), callWithAssignMir->getName(), callWithAssignMir->id);
    }

    // 常量变量重命名拷贝
    auto srcConstantTable = src->getLocalConstantVecDictOrder();
    std::map<Constant *, Constant *> constantTable;
    for (auto srcConst : srcConstantTable) {
        auto newName = srcConst->getName() + "_" + src->getName() + "_" + bb->getName() + std::to_string(call_index);
        auto newConst = fn->declareLocalConstant(srcConst->getType(), newName,
                                                 srcConst->getCopyOfValue());
        constantTable[srcConst] = newConst;
    }

    auto srcVariableTable = src->getLocalVariableVecDictOrder();
    std::map<Variable *, Variable *> variableTable;
    for (auto srcVar : srcVariableTable) {
        if (srcVar->isArgument()) continue;
        auto newName = srcVar->getName() + "_" + src->getName() + "_" + bb->getName() + std::to_string(call_index);
        auto newVar = fn->declareLocalVariable(srcVar->getType(), newName, srcVar->isReference(),
                                               srcVar->isArgument());
        variableTable[srcVar] = newVar;
    }

    // 初始化所有内联后的block
    std::map<BasicBlock *, BasicBlock *> blockTable;
    for (auto pBlock : src->getBasicBlockVecDictOrder()) {
        auto newName = pBlock->getName() + "_" + bb->getName() + std::to_string(call_index);
        auto new_bb = fn->createBasicBlock(newName);
        blockTable[pBlock] = new_bb;
    }
    BasicBlock *src_entry = blockTable[src->entryBlock];
    auto nextBlock = fn->createBasicBlock(bb->getName() + std::to_string(call_index) + "_next");

    // 拷贝更新所有mir
    std::map<Assignment *, Assignment *> replaceTable;
    vector<unique_ptr<MIR>> mirTable;
    for (auto nbb : src->getBasicBlockVecDictOrder()) {
        auto new_bb = blockTable[nbb];
        mirTable.clear();
        for (auto &mir : nbb->mirTable) {
            auto new_mir = cloneMIR(mir.get(), replaceTable, blockTable, constantTable, variableTable, call_mir);
            if (new_mir != nullptr) {
                auto ret_mir = dynamic_cast<ReturnMIR *>(new_mir);
                if (ret_mir != nullptr) {
                    new_mir = new JumpMIR(nextBlock);
                    if (ret_mir->val != nullptr) {
                        assert(phiMir != nullptr);
                        phiMir->addIncoming(new_bb, ret_mir->val);
                    }
                }
                mirTable.emplace_back(new_mir);
            }
        }
        new_bb->mirTable.swap(mirTable);
    }

    // 更新nextBlock
    for (auto idx = call_index + 1; idx < bb->mirTable.size(); idx++) {
        nextBlock->mirTable.emplace_back(std::move(bb->mirTable[idx]));
    }

    // 重新截取bb
    bb->mirTable.resize(call_index);
    bb->createJumpMIR(src_entry);

    if (phiMir != nullptr) {
        replaceTable[callWithAssignMir] = phiMir;
        nextBlock->mirTable.insert(nextBlock->mirTable.begin(), std::unique_ptr<PhiMIR>(phiMir));
    }

    // 替换所有assign
    for (auto &nbb : fn->getBasicBlockVecDictOrder()) {
        for (auto &mir : nbb->mirTable) {
            mir->doReplacement(replaceTable);
        }
    }

    // 将所有phi中指向bb改为指向nextbb
    for (auto &nbb : fn->getBasicBlockVecDictOrder()) {
        for (auto &mir : nbb->mirTable) {
            auto phiMir2 = dynamic_cast<PhiMIR *>(mir.get());
            if (phiMir2 != nullptr && phiMir2->incomingTable.count(bb)) {
                auto ssa = phiMir2->incomingTable[bb];
                if (ssa == static_cast<Assignment *>(callWithAssignMir)) {
                    ssa = phiMir;
                }
                phiMir2->addIncoming(nextBlock, ssa);
                phiMir2->incomingTable.erase(bb);
            }
        }
    }

    return nextBlock;
}


Function *cloneFunctionReturn0(Module *md, Function *orig) {
    if (md->clonedNoRetFunctions.count(orig)) {
        return orig;
    }
    std::stringstream ss;
    ss << orig->getName();
    ss << ".NoRet";
    Function *cached = md->getFunctionByName(ss.str());
    if (cached != nullptr) {
        return cached;
    }
    auto *newFn = cloneFunction(md, orig, ss.str());
    md->clonedNoRetFunctions.insert(newFn);

    for (auto *b : newFn->getBasicBlockSetPtrOrder()) {
        auto &m = b->mirTable.back();
        auto *ret = dynamic_cast<ReturnMIR *> (m.get());
        if (ret != nullptr) {
            ret->val = nullptr;
        }
    }
    return newFn;
}