#include "frontend/errorhandle/ErrorHandler.h"
#include "frontend/generate/HIRGenerator.h"
#include "frontend/grammar_parse/Gparser.h"
#include "frontend/tokenize/Tokenizer.h"

#include "pass/erase/JumpMergePass.h"
#include "pass/erase/EraseUselessBlockPass.h"
#include "pass/generate/HIR2MIRPass.h"
#include "pass/transform/Mem2RegPass.h"
#include "pass/erase/EraseUselessValuePass.h"
#include "pass/transform/ConstantFoldingPass.h"
#include "pass/erase/EraseCommonExprPass.h"
#include "pass/generate/MIR2LIRPass.h"
#include "pass/generate/RegisterAllocatePass.h"
#include "pass/analyze/AnalyzeLoopPass.h"
#include "pass/debug/ResetIRIDPass.h"
#include "pass/analyze/AnalyzeRegionPass.h"
#include "pass/erase/ErasePseudoInstLIRPass.h"
#include "pass/transform/ForwardMovingPass.h"
#include "pass/transform/BackwardMovingPass.h"
#include "pass/transform/AnalyzeUnwrittenGlobalValuePass.h"
#include "pass/erase/EraseLocalConstantPass.h"
#include "pass/transform/InlineFunctionPass.h"
#include "pass/transform/SimplifyConstantArrayPass.h"
#include "pass/analyze/AnalyzeLoopParamPass.h"
#include "pass/transform/LoopUnrollingPass.h"
#include "pass/erase/EraseUselessFunctionPass.h"
#include "pass/parallel/ExtractMultiCallPass.h"
#include "pass/transform/InstructionFusion.h"
#include "pass/transform/InstructionSchedule.h"
#include "pass/erase/EraseUselessArrayWritePass.h"


#ifdef SHOW_IR

#include "pass/debug/PrintIRPass.h"

#endif

#include "codegen/HIRAssemblyGenerator.h"
#include "codegen/MIRAssemblyGenerator.h"
#include "codegen/LIRAssemblyGenerator.h"


#ifndef TEST_OPTIMIZE
#define TEST_OPTIMIZE
#endif

using std::make_unique;
using std::make_shared;
using std::ofstream;

using namespace frontend;

time_point<high_resolution_clock> compiler_start_time;

shared_ptr<Module> runFrontend(const string &srcFile) {
    auto errHandler = make_shared<errorhandle::ErrorHandler>();
    tokenize::Tokenizer tokenizer(srcFile, errHandler);
    vector<tokenize::Token> tokens = tokenizer.getTokens();

    grammar_parse::GParser gParser(tokens, errHandler, srcFile);
    auto root = gParser.parseCompUnit();

#ifdef SHOW_GRAMMAR_TREE
    std::cout << base->toOutput() << std::endl;
#endif

    return gParser.getModule();
}

void runOptimizePassesOnHIR(Module *md) {
    AnalyzeModulePasses analyzeModulePasses;
    AnalyzeCallGraphPass_HIR analyzeCallGraphPass(*md);
    analyzeModulePasses.analyzeCallGraphPass = &analyzeCallGraphPass;
    EraseUselessFunctionPass(*md, analyzeModulePasses).run();
    auto functionTable = md->getFunctionVecDictOrder();
    for (auto fn : functionTable) {
        if (fn->isExternal) {
            continue;
        }
        AnalyzeFunctionPasses analyzePasses;
        AnalyzeCFGPass_HIR analyzeCFGPass(*fn, std::cerr);
        analyzePasses.analyzeCFGPass = &analyzeCFGPass;
        EraseUselessBlockPass(*fn, analyzePasses).run();
        EraseUselessLocalValuePass_HIR(*fn).run();
    }
    EraseUselessGlobalValuePass_HIR(*md).run();
}

AnalyzeModulePasses buildModulePass(Module *md) {
    AnalyzeModulePasses analyzePasses{md};
    analyzePasses.analyzeCallGraphPass = new AnalyzeCallGraphPass_MIR(*md, std::cerr);
    analyzePasses.analyzeSideEffectPass = new AnalyzeSideEffectPass(*md, analyzePasses, std::cerr);
    analyzePasses.analyzePointerPass = new AnalyzePointerPass(*md, analyzePasses, std::cerr);
    analyzePasses.analyzeUnwrittenGlobalValuePass = new AnalyzeUnwrittenGlobalValuePass(*md, analyzePasses, std::cerr);
    return analyzePasses;
}

void invalidateModulePass(AnalyzeModulePasses &analyzePasses) {
    analyzePasses.analyzeCallGraphPass->invalidate();
    analyzePasses.analyzeSideEffectPass->invalidate();
    analyzePasses.analyzePointerPass->invalidate();
    analyzePasses.analyzeUnwrittenGlobalValuePass->invalidate();
}

void deleteModulePass(AnalyzeModulePasses &analyzePasses) {
    delete analyzePasses.analyzeCallGraphPass;
    delete analyzePasses.analyzeSideEffectPass;
    delete analyzePasses.analyzePointerPass;
    delete analyzePasses.analyzeUnwrittenGlobalValuePass;
}

AnalyzeFunctionPasses buildFunctionPass(Function *fn, Module *md, AnalyzeModulePasses *analyzeModulePasses) {
    AnalyzeFunctionPasses analyzePasses{};
    analyzePasses.fn = fn;
    analyzePasses.analyzeModulePasses = analyzeModulePasses;
    analyzePasses.analyzeCFGPass = new AnalyzeCFGPass_MIR(*fn, std::cerr);
    analyzePasses.analyzeDomTreePass = new AnalyzeDomTreePass(*fn, analyzePasses, std::cerr);
    analyzePasses.analyzeActivityPass = new AnalyzeActivityPass_MIR(*fn, analyzePasses, std::cerr);
    analyzePasses.analyzeDataFlowPass = new AnalyzeDataFlowPass_MIR(*fn, analyzePasses, std::cerr);
    analyzePasses.analyzeLoopPass = new AnalyzeLoopPass(*fn, analyzePasses, std::cerr);
    analyzePasses.analyzeRegionPass = new AnalyzeRegionPass(*fn, analyzePasses, std::cerr);
    analyzePasses.analyzeArrayAccessPass = new AnalyzeArrayAccessPass(*fn, *md, analyzePasses, std::cerr);
    analyzePasses.analyzeLoopParamPass = new AnalyzeLoopParamPass(*fn, analyzePasses, std::cerr);
    return analyzePasses;
}

void invalidateFunctionPass(AnalyzeFunctionPasses &analyzePasses) {
    analyzePasses.analyzeDomTreePass->invalidate();
    analyzePasses.analyzeCFGPass->invalidate();
    analyzePasses.analyzeActivityPass->invalidate();
    analyzePasses.analyzeLoopPass->invalidate();
    analyzePasses.analyzeRegionPass->invalidate();
    analyzePasses.analyzeArrayAccessPass->invalidate();
    analyzePasses.analyzeDataFlowPass->invalidate();
    analyzePasses.analyzeLoopParamPass->invalidate();
}

void deleteFunctionPass(AnalyzeFunctionPasses &analyzePasses) {
    delete analyzePasses.analyzeDomTreePass;
    delete analyzePasses.analyzeCFGPass;
    delete analyzePasses.analyzeActivityPass;
    delete analyzePasses.analyzeLoopPass;
    delete analyzePasses.analyzeRegionPass;
    delete analyzePasses.analyzeArrayAccessPass;
    delete analyzePasses.analyzeDataFlowPass;
    delete analyzePasses.analyzeLoopParamPass;
}

bool
run_optimize_in_function(AnalyzeModulePasses &analyzeModulePasses, Function *fn, AnalyzeFunctionPasses &analyzePasses);

// 局部消减
bool runLocalOptimizePassesOnMIR(Module *md, Function *fn,
                                 AnalyzeModulePasses &analyzeModulePasses,
                                 AnalyzeFunctionPasses &analyzeFunctionPasses) {
    bool notConverged = false;
    invalidateFunctionPass(analyzeFunctionPasses);
    RETRY:
    if (EraseUselessBlockPass(*fn, analyzeFunctionPasses).run()) {
        invalidateFunctionPass(analyzeFunctionPasses);
        invalidateModulePass(analyzeModulePasses);
        notConverged = true;
        goto RETRY;
    }
    if (EraseUselessLocalValuePass_MIR(*fn).run()) {
        invalidateFunctionPass(analyzeFunctionPasses);
        invalidateModulePass(analyzeModulePasses);
        notConverged = true;
        goto RETRY;
    }
    if (ConstantFoldingPass(*fn, analyzeFunctionPasses).run()) {
        invalidateFunctionPass(analyzeFunctionPasses);
        invalidateModulePass(analyzeModulePasses);
        notConverged = true;
        goto RETRY;
    }
    if (EraseCommonExprPass(*fn, analyzeFunctionPasses).run()) {
        invalidateFunctionPass(analyzeFunctionPasses);
        invalidateModulePass(analyzeModulePasses);
        notConverged = true;
        goto RETRY;
    }
    if (Mem2RegGlobalPass(*fn, analyzeFunctionPasses).run()) {
        invalidateFunctionPass(analyzeFunctionPasses);
        invalidateModulePass(analyzeModulePasses);
        notConverged = true;
        goto RETRY;
    }

    if (EraseUselessArrayWritePass(*fn, analyzeFunctionPasses, std::cerr).run()) {
        invalidateFunctionPass(analyzeFunctionPasses);
        invalidateModulePass(analyzeModulePasses);
        notConverged = true;
        goto RETRY;
    }
    if (EraseUselessLocalValuePass_MIR(*fn).run()) {
        invalidateFunctionPass(analyzeFunctionPasses);
        invalidateModulePass(analyzeModulePasses);
        notConverged = true;
        goto RETRY;
    }
    return notConverged;
}

bool runLocalOptimizePassesOnMIR(Module *md, AnalyzeModulePasses &analyzeModulePasses,
                                 std::map<Function *, AnalyzeFunctionPasses> analyzeFunctionPassesMap) {
    bool notConverged = false;
    invalidateModulePass(analyzeModulePasses);
    for (auto *fn : md->getFunctionVecDictOrder()) {
        if (fn->isExternal) {
            continue;
        }
        AnalyzeFunctionPasses &analyzeFunctionPasses = analyzeFunctionPassesMap[fn];
        runLocalOptimizePassesOnMIR(md, fn, analyzeModulePasses, analyzeFunctionPasses);
    }
    return notConverged;
}

void runOptimizePassesOnMIR(Module *md) {
    std::map<Function *, AnalyzeFunctionPasses> analyzeFunctionPassesMap;
    AnalyzeModulePasses analyzeModulePasses = buildModulePass(md);

    // 构造SSA
    for (auto *fn : md->getFunctionVecDictOrder()) {
        if (fn->isExternal) {
            continue;
        }
        AnalyzeFunctionPasses analyzeFunctionPasses = buildFunctionPass(fn, md, &analyzeModulePasses);
        analyzeFunctionPassesMap[fn] = analyzeFunctionPasses;
        analyzeFunctionPasses.analyzeCFGPass = new AnalyzeCFGPass_MIR(*fn, std::cerr);
        analyzeFunctionPasses.analyzeDomTreePass = new AnalyzeDomTreePass(*fn, analyzeFunctionPasses, std::cerr);
        EraseUselessLocalValuePass_MIR(*fn, std::cerr).run();
        JumpMergePass(*fn, analyzeFunctionPasses, std::cerr).run();
        EraseUselessBlockPass(*fn, analyzeFunctionPasses, std::cerr).run();
        Mem2RegPass(*fn, analyzeFunctionPasses).run();
        EraseUselessLocalValuePass_MIR(*fn, std::cerr).run();
        invalidateFunctionPass(analyzeFunctionPasses);
    }
    ResetModulePass(*md, BasicBlock::LevelOfIR::MEDIUM).run();
    EraseLocalConstantPass(*md).run();
    invalidateModulePass(analyzeModulePasses);

    // 向前迭代
    bool notConverged;
    do {
        bool functionInserted;
        do {
            analyzeModulePasses.analyzeSideEffectPass->run();
            auto &functSeq = analyzeModulePasses.analyzeSideEffectPass->funcTopoSeq;
            auto fnSizeOld = md->getFunctionSetPtrOrder().size();
            std::queue<Function *> fnque = {};
            functionInserted = false;
            for (auto *x : functSeq) if (!x->isExternal)fnque.push(x);
            //可以添加函数的优化
            while (!fnque.empty()) {
                auto *fn = fnque.front();
                fnque.pop();
                if (!analyzeFunctionPassesMap.count(fn)) {
                    analyzeFunctionPassesMap[fn] = buildFunctionPass(fn, md, &analyzeModulePasses);
                }
                AnalyzeFunctionPasses &analyzePasses = analyzeFunctionPassesMap[fn];
                if (SimplifyFunctionCallPass(md, *fn, analyzePasses, fnque, std::cerr).run()) {
                    invalidateFunctionPass(analyzeFunctionPassesMap[fn]);
                }
                invalidateModulePass(analyzeModulePasses);
                if (fnSizeOld != md->getFunctionSetPtrOrder().size()) {
                    functionInserted = true;
                }
            }
        } while (functionInserted);

        notConverged = runLocalOptimizePassesOnMIR(md, analyzeModulePasses,
                                                   analyzeFunctionPassesMap);

        if (InlineFunctionPass(*md, std::cerr).run()) {
            analyzeModulePasses.analyzeCallGraphPass->invalidate();
            EraseUselessFunctionPass(*md, analyzeModulePasses).run();
            invalidateModulePass(analyzeModulePasses);
            std::set<Function *> usefulFunction = md->getFunctionSetPtrOrder();
            std::set<Function *> uselessFunction;
            for (auto &analyzeFunctionPasses : analyzeFunctionPassesMap) {
                if (usefulFunction.count(analyzeFunctionPasses.first) == 0) {
                    uselessFunction.insert(analyzeFunctionPasses.first);
                }
            }
            for (auto fn : uselessFunction) {
                deleteFunctionPass(analyzeFunctionPassesMap[fn]);
                analyzeFunctionPassesMap.erase(fn);
            }
            for (auto &analyzeFunctionPasses : analyzeFunctionPassesMap) {
                invalidateFunctionPass(analyzeFunctionPasses.second);
            }
            notConverged = true;
            runLocalOptimizePassesOnMIR(md, analyzeModulePasses, analyzeFunctionPassesMap);
        }

        invalidateModulePass(analyzeModulePasses);
        for (auto *fn : md->getFunctionVecDictOrder()) {
            if (fn->isExternal) {
                continue;
            }
            AnalyzeFunctionPasses &analyzeFunctionPasses = analyzeFunctionPassesMap[fn];
            invalidateFunctionPass(analyzeFunctionPasses);
            if (ForwardMovingPass(*fn, analyzeFunctionPasses, std::cerr).run()) {
                invalidateFunctionPass(analyzeFunctionPasses);
                invalidateModulePass(analyzeModulePasses);
                if (JumpMergePass(*fn, analyzeFunctionPasses, std::cerr, true).run()) {
                    EraseUselessBlockPass(*fn, analyzeFunctionPasses).run();
                    invalidateFunctionPass(analyzeFunctionPasses);
                    invalidateModulePass(analyzeModulePasses);
                }
                runLocalOptimizePassesOnMIR(md, fn, analyzeModulePasses, analyzeFunctionPasses);
                notConverged = true;
            }
            if (LoopUnrollingPass(*analyzeModulePasses.md, *fn, analyzeFunctionPasses).run()) {
                invalidateFunctionPass(analyzeFunctionPasses);
                invalidateModulePass(analyzeModulePasses);
                runLocalOptimizePassesOnMIR(md, fn, analyzeModulePasses, analyzeFunctionPasses);
                notConverged = true;
            }
        }
        if (EraseConstantArrReadPass(*md, analyzeFunctionPassesMap, std::cerr).run()) {
            invalidateModulePass(analyzeModulePasses);
            for (auto &analyzeFunctionPasses : analyzeFunctionPassesMap) {
                invalidateFunctionPass(analyzeFunctionPasses.second);
            }
            notConverged = true;
        }
    } while (notConverged);

    // 向后迭代
    do {
        notConverged = false;
        invalidateModulePass(analyzeModulePasses);
        for (auto *fn : md->getFunctionVecDictOrder()) {
            if (fn->isExternal) {
                continue;
            }
            AnalyzeFunctionPasses &analyzeFunctionPasses = analyzeFunctionPassesMap[fn];
            invalidateFunctionPass(analyzeFunctionPasses);
            if (BackwardMovingPass(*fn, analyzeFunctionPasses, std::cerr).run()) {
                invalidateFunctionPass(analyzeFunctionPasses);
                invalidateModulePass(analyzeModulePasses);
                if (JumpMergePass(*fn, analyzeFunctionPasses, std::cerr, false).run()) {
                    EraseUselessBlockPass(*fn, analyzeFunctionPasses).run();
                    invalidateFunctionPass(analyzeFunctionPasses);
                    invalidateModulePass(analyzeModulePasses);
                }
                runLocalOptimizePassesOnMIR(md, fn, analyzeModulePasses, analyzeFunctionPasses);
                notConverged = true;
            }
        }
    } while (notConverged);

    SimplifyConstantArrayPass(*md, std::cerr).run();
    EraseUselessGlobalValuePass_MIR(*md).run();
    EraseUselessFunctionPass(*md, analyzeModulePasses).run();

    ResetModulePass(*md, BasicBlock::LevelOfIR::MEDIUM).run();
    ExtractMultiCallPass(*md, analyzeModulePasses, std::cerr).run();
    deleteModulePass(analyzeModulePasses);
}

void runOptimizePassesOnLIR(Module *md) {
    EraseUselessGlobalValuePass_LIR(*md).run();
    AnalyzeCallGraphPass_LIR analyzeCallGraphPass(*md, std::cerr);
    auto functionTable = md->getFunctionVecDictOrder();
    for (auto fn : functionTable) {
        if (fn->isExternal) {
            continue;
        }
        AnalyzeFunctionPasses analyzePasses;
        AnalyzeCFGPass_LIR analyzeCFGPass(*fn, std::cerr);
        analyzePasses.analyzeCFGPass = &analyzeCFGPass;
        AnalyzeDomTreePass analyzeDomTreePass(*fn, analyzePasses, std::cerr);
        analyzePasses.analyzeDomTreePass = &analyzeDomTreePass;
        EraseUselessLocalValuePass_LIR(*fn).run();
        AnalyzeActivityPass_LIR analyzeActivityPass(*fn, analyzePasses, std::cerr);
        analyzePasses.analyzeActivityPass = &analyzeActivityPass;
        AnalyzeDataFlowPass_LIR analyzeDataFlowPass(*fn, analyzePasses, std::cerr);
        analyzePasses.analyzeDataFlowPass = &analyzeDataFlowPass;
        ResetFunctionPass_LIR(*fn, analyzePasses).run();
        RegisterAllocatePass(*fn, analyzePasses).run();
        ErasePseudoInstLIRPass(*fn, analyzePasses, analyzeCallGraphPass).run();
        InstructionFusion(*fn, analyzePasses).run();
//        InstructionSchedule(*fn, analyzePasses).run();
    }
}

int main(int argc, char **argv) {
    compiler_start_time = high_resolution_clock::now();
    stringstream temp;
    ofstream asm_out;

    try {
        assert(argc >= 5);
        string dstFile = argv[3];
        string srcFile = argv[4];
        asm_out.open(dstFile);

        int optimizeLevel = 0;
#ifdef TEST_OPTIMIZE
        if (argc > 5) {
            string os = argv[5];
            optimizeLevel = os[os.length() - 1] - '0';
        }
#endif
        auto t0 = high_resolution_clock::now();
        auto md = runFrontend(srcFile);
        runOptimizePassesOnHIR(md.get());
        auto t1 = high_resolution_clock::now();
        std::cout << "SRC -> HIR : " << duration_cast<milliseconds>(t1 - t0).count() << "ms" << std::endl;

#ifdef SHOW_IR
        ofstream hir_out("HIR.txt");
        PrintModulePass(*md, BasicBlock::LevelOfIR::HIGH, hir_out).run();
        hir_out.close();
#endif

        HIRAssemblyGenerator(*md, temp).run();
        if (optimizeLevel == 0) {
            HIRAssemblyGenerator(*md, asm_out).run();
            auto t2 = high_resolution_clock::now();
            std::cout << "HIR -> ARM : " << duration_cast<milliseconds>(t2 - t1).count() << "ms" << std::endl;
        } else {
            HIR2MIRPass(*md).run();
            runOptimizePassesOnMIR(md.get());
            auto t2 = high_resolution_clock::now();
            std::cout << "HIR -> MIR : " << duration_cast<milliseconds>(t2 - t1).count() << "ms" << std::endl;

#ifdef SHOW_IR
            ofstream mir_out("MIR.txt");
            ResetModulePass(*md, BasicBlock::LevelOfIR::MEDIUM).run();
            PrintModulePass(*md, BasicBlock::LevelOfIR::MEDIUM, mir_out).run();
            mir_out.close();
#endif

            if (optimizeLevel == 1) {
                MIRAssemblyGenerator(*md, asm_out).run();
                auto t3 = high_resolution_clock::now();
                std::cout << "MIR -> ARM : " << duration_cast<milliseconds>(t3 - t2).count() << "ms" << std::endl;
            } else {
                MIR2LIRPass(*md).run();
                runOptimizePassesOnLIR(md.get());
                auto t3 = high_resolution_clock::now();
                std::cout << "MIR -> LIR : " << duration_cast<milliseconds>(t3 - t2).count() << "ms" << std::endl;

#ifdef SHOW_IR
                ofstream lir_out("LIR.txt");
//                ResetModulePass(*md, BasicBlock::LevelOfIR::LOW).run();
                PrintModulePass(*md, BasicBlock::LevelOfIR::LOW, lir_out).run();
                lir_out.close();
#endif

                if (optimizeLevel == 2) {
                    LIRAssemblyGenerator(*md, asm_out).run();
                    auto t4 = high_resolution_clock::now();
                    std::cout << "LIR -> ARM : " << duration_cast<milliseconds>(t4 - t3).count() << "ms" << std::endl;
                }
            }
        }
        asm_out.close();
        std::cout << "finished" << std::endl;

    } catch (std::exception &e) {
        asm_out << temp.str();
    }
    return 0;
}
