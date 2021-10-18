//
// Created by 陈思言 on 2021/8/13.
//

#include "ExtractMultiCallPass.h"
#include "../erase/EraseUselessBlockPass.h"

bool ExtractMultiCallPass::run() {
    auto fnVec = md.getFunctionVecDictOrder();
    for (auto fn : fnVec) {
        if (fn->isExternal) {
            continue;
        }

        AnalyzeFunctionPasses analyzePasses{};
        analyzePasses.fn = fn;
        analyzePasses.analyzeModulePasses = &analyzeModulePasses;
        analyzePasses.analyzeCFGPass = new AnalyzeCFGPass_MIR(*fn, std::cerr);
        analyzePasses.analyzeDomTreePass = new AnalyzeDomTreePass(*fn, analyzePasses, std::cerr);
        analyzePasses.analyzeActivityPass = new AnalyzeActivityPass_MIR(*fn, analyzePasses, std::cerr);
        analyzePasses.analyzeLoopPass = new AnalyzeLoopPass(*fn, analyzePasses, std::cerr);
        analyzePasses.analyzeRegionPass = new AnalyzeRegionPass(*fn, analyzePasses, std::cerr);
        analyzePasses.analyzeArrayAccessPass = new AnalyzeArrayAccessPass(*fn, md, analyzePasses, std::cerr);
        analyzePasses.analyzeLoopParamPass = new AnalyzeLoopParamPass(*fn, analyzePasses, std::cerr);
        analyzePasses.analyzeDataFlowPass = new AnalyzeDataFlowPass_MIR(*fn, analyzePasses, std::cerr);

        AnalyzeParallelLoopPass analyzeParallelLoopPass(*fn, analyzePasses, std::cout);
        analyzeParallelLoopPass.run();
        if (!analyzeParallelLoopPass.parallelLoop.empty()) {
            auto best_iter = analyzeParallelLoopPass.parallelLoop.begin();
            size_t max_n = 0;
            auto iter = analyzeParallelLoopPass.parallelLoop.begin();
            while (iter != analyzeParallelLoopPass.parallelLoop.end()) {
                size_t n = 0;
                for (auto bb : iter->first->nodes) {
                    n += bb->mirTable.size();
                }
                if (n > max_n) {
                    max_n = n;
                    best_iter = iter;
                }
                iter++;
            }
            extractLoopFunc(&md, fn, best_iter->first, analyzePasses, analyzeParallelLoopPass);
            invalidateFunctionPass(analyzePasses);
            EraseUselessBlockPass(*fn, analyzePasses).run();
        }

        delete analyzePasses.analyzeDomTreePass;
        delete analyzePasses.analyzeCFGPass;
        delete analyzePasses.analyzeActivityPass;
        delete analyzePasses.analyzeLoopPass;
        delete analyzePasses.analyzeRegionPass;
        delete analyzePasses.analyzeArrayAccessPass;
    }
    return false;
}

Function *
ExtractMultiCallPass::extractLoopFunc(Module *md, Function *src, Loop *loop, AnalyzeFunctionPasses &functionPasses,
                                      AnalyzeParallelLoopPass &parallelLoopPass) {
    auto region = parallelLoopPass.parallelLoop[loop];
    // step 0. find all blocks that need to be extracted
    auto etrBlocks = getAllEtrBlocks(functionPasses, region->entrance, region->exit);

    // step 1. add new prev Block in src
    auto prevBlock = createNewPrevBlock(src, etrBlocks, functionPasses);

    // step 2. find all input & output and split into scalars and arrays
    auto params = getParams(etrBlocks, functionPasses);
    vector<pair<Assignment *, bool>> scalars, arrays;
    splitArrScalar(params, scalars, arrays);

    static int tmpId = 8282;
    auto scalar_arr = src->declareLocalVariable(std::make_shared<ArrayType>(IntegerType::object, scalars.size()),
                                                ".etr_scalar_arr", false);
    auto ptr_arr = prevBlock->createValueAddressingMIR(scalar_arr->getName(), tmpId++, scalar_arr);
    storeInputScalar(src, prevBlock, ptr_arr, scalars, tmpId);

    vector<Assignment *> args = {ptr_arr}; // real params init
    vector<std::shared_ptr<Type>> argTypes = {ptr_arr->getType()};
    for (auto ssa : arrays) {
        // auto ptr = prevBlock->createValueAddressingMIR(ssa.first->getName(), tmpId++, ssa.first);
        args.emplace_back(ssa.first);
        argTypes.emplace_back(ssa.first->getType());
    }
    auto new_fn = md->declareFunction(std::make_shared<FunctionType>(VoidType::object, argTypes),
                                      src->getName() + "_" + prevBlock->getName() + "_etrfn");
    new_fn->args.clear();
    // declare atomic formal param
    new_fn->hasAtomicVar = true;
    auto atomic_var = new_fn->declareLocalVariable(IntegerType::object,
                                                   ".atomic_var" + std::to_string(tmpId++), true, true);

    for (auto &arg : args) {
        auto newArg = new_fn->declareLocalVariable(arg->getType(),
                                                   arg->getName() + std::to_string(arg->id) + "_etrfn", false, true);
        new_fn->args.emplace_back(newArg);
    }
    new_fn->args.emplace_back(atomic_var);

    // declare atomic real param
    auto src_atomic = src->declareLocalVariable(IntegerType::object,
                                                ".atomic_var" + std::to_string(tmpId++), false);
    auto loopParam = functionPasses.analyzeLoopParamPass->result[loop];
    // auto phiMir = loopParam.varMap.begin()->first;
    auto inductionVar = loopParam.varMap.begin()->second;
    prevBlock->createStoreVariableMIR(src_atomic, inductionVar.init);
    auto multiCallMir = new MultiCallMIR(new_fn, args, src_atomic);
    prevBlock->mirTable.emplace_back(multiCallMir);

    extractLoopCallee(new_fn, etrBlocks, scalars, arrays, prevBlock, region->exit, loop, functionPasses);

    // 替换所有的phiMir
    auto oldPrev = etrBlocks.front();
    for (auto &nbb : src->getBasicBlockVecDictOrder()) {
        for (auto &mir : nbb->mirTable) {
            auto phiMir2 = dynamic_cast<PhiMIR *>(mir.get());
            if (phiMir2 != nullptr && phiMir2->incomingTable.count(oldPrev)) {
                auto ssa = phiMir2->incomingTable[oldPrev];
                phiMir2->addIncoming(prevBlock, ssa);
                phiMir2->incomingTable.erase(oldPrev);
            }
        }
    }

    auto out_atomic = prevBlock->createLoadVariableMIR(src_atomic->getName(), tmpId++, src_atomic);
    loadOutputScalar(src, prevBlock, ptr_arr, scalars, tmpId, {{inductionVar.phi, out_atomic}});

    prevBlock->createJumpMIR(region->exit);

    functionPasses.analyzeCFGPass->invalidate();
    functionPasses.analyzeActivityPass->invalidate();
    functionPasses.analyzeDataFlowPass->invalidate();

    return new_fn;
}

void ExtractMultiCallPass::extractLoopCallee(Function *new_fn, vector<BasicBlock *> &etrBlocks,
                                             vector<pair<Assignment *, bool>> &scalars,
                                             vector<pair<Assignment *, bool>> &arrays,
                                             BasicBlock *prevBlock, BasicBlock *oldExitBlock,
                                             Loop *loop, AnalyzeFunctionPasses &functionPasses) {
    auto entryBlock = new_fn->createBasicBlock(new_fn->getName() + "_entry");
    new_fn->entryBlock = entryBlock;
    vector<BasicBlock *> newBlocks;
    std::map<BasicBlock *, BasicBlock *> blockTable;

    for (auto *b : etrBlocks) {
        auto new_b = new_fn->createBasicBlock("etrfn_" + b->getName());
        newBlocks.emplace_back(new_b);
        new_b->mirTable.swap(b->mirTable);
        blockTable.emplace(b, new_b);
    }

    int tmpId = 8282;
    std::map<Assignment *, Assignment *> replaceTable;

    // auto atomic_ssa = entryBlock->createLoadVariableMIR("atm_" + new_fn->args[0]->getName(), tmpId++, new_fn->args[0]);

    auto base_ptr = entryBlock->createLoadVariableMIR(new_fn->args[0]->getName(), tmpId++, new_fn->args[0]);
    for (auto idx = 0; idx < scalars.size(); idx++) {
        if (scalars[idx].second) { // load scalar input
            auto ssa = scalars[idx].first;
            auto offset = entryBlock->createLoadConstantMIR("off_" + std::to_string(idx), tmpId++,
                                                            new_fn->declareLocalImmediateInt(idx));
            auto tmp = entryBlock->createArrayAddressingMIR("addr_" + std::to_string(idx), tmpId++,
                                                            base_ptr, offset);
            auto ltr = entryBlock->createLoadPointerMIR("ldr_" + std::to_string(idx), tmpId++, tmp);
            replaceTable[ssa] = ltr;
        }
    }

    // replace all arr pointer
    for (auto idx = 0; idx < arrays.size(); idx++) {
        auto arr = new_fn->args[idx + 1];
        // auto ptr = entryBlock->createValueAddressingMIR(arr->getName(), tmpId++, arr);
        auto load_arr = entryBlock->createLoadVariableMIR(arr->getName(), tmpId++, arr);
        replaceTable[arrays[idx].first] = load_arr;
    }
    entryBlock->createJumpMIR(newBlocks.front());

    // 插入循环归纳变量原子操作
    auto entrance = newBlocks.front();
    auto &loopParam = functionPasses.analyzeLoopParamPass->result[loop];
    auto &phiMir = loopParam.varMap.begin()->first;
    auto &inductionVar = loopParam.varMap.begin()->second;
    auto &exitCond = *inductionVar.exitCond.begin();
    vector<unique_ptr<MIR>> mirTable;
    for (auto &mir : entrance->mirTable) {
        if (mir.get() == phiMir ||
            mir.get() == exitCond.first->castToMIR() ||
            mir.get() == exitCond.second) {
            toErase.emplace_back(std::move(mir));
            continue;
        }
        mirTable.emplace_back(std::move(mir));
    }
    auto updateMir = dynamic_cast<BinaryMIR *>(inductionVar.update->castToMIR());
    auto step = (updateMir->src1 == phiMir) ? updateMir->src2 : updateMir->src1;
    auto cmpMir = dynamic_cast<BinaryMIR *>(exitCond.first->castToMIR());
    auto border = (cmpMir->src1 == phiMir) ? cmpMir->src2 : cmpMir->src1;
    auto body = (exitCond.second->block1 == oldExitBlock) ?
                blockTable[exitCond.second->block2] : blockTable[exitCond.second->block1];
    auto exitBlock = new_fn->createBasicBlock(new_fn->getName() + "_exit");
    auto atomicLoopCondMir = new AtomicLoopCondMIR("atomicLoopCondMir", tmpId++, new_fn->args.back(),
                                                   updateMir->op, step, cmpMir->op, border, body, exitBlock);
    mirTable.emplace_back(atomicLoopCondMir);
    entrance->mirTable.swap(mirTable);

    replaceTable[phiMir] = atomicLoopCondMir;
    for (auto *b : newBlocks) {
        for (auto &mir : b->mirTable) {
            mir->doReplacement(replaceTable);
        }
    }

    // 删除update
    for (auto *b : newBlocks) {
        mirTable.clear();
        for (auto &mir : b->mirTable) {
            if (mir.get() == inductionVar.update->castToMIR()) {
                toErase.emplace_back(std::move(mir));
                continue;
            }
            mirTable.emplace_back(std::move(mir));
        }
        b->mirTable.swap(mirTable);
    }

    blockTable.emplace(oldExitBlock, exitBlock);
    blockTable.emplace(prevBlock, entrance);
    for (auto *b : newBlocks) {
        for (auto &item : blockTable) {
//            std::cerr << b->toString(BasicBlock::LevelOfIR::MEDIUM) << " -- "
//                      << item.first->toString(BasicBlock::LevelOfIR::MEDIUM) << " -- "
//                      << item.second->toString(BasicBlock::LevelOfIR::MEDIUM) << std::endl;
            replaceJumpBlock(b, item.first, item.second);
        }
    }

    // 替换所有的phiMir
    for (auto &nbb : newBlocks) {
        for (auto &mir : nbb->mirTable) {
            auto phiMir2 = dynamic_cast<PhiMIR *>(mir.get());
            if (phiMir2 != nullptr) {
                std::map<BasicBlock *, Assignment*> tmpIncomings;
                for (auto &iter : phiMir2->incomingTable) {
                    tmpIncomings[blockTable[iter.first]] = iter.second;
                }
                phiMir2->incomingTable.swap(tmpIncomings);
            }
        }
    }

    exitBlock->createReturnMIR();
}
