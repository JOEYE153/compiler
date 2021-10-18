cmake_minimum_required(VERSION 3.19)
project(Compiler2021)

set(CMAKE_CXX_STANDARD 17)

add_executable(compiler main.cpp
        frontend/tokenize/Token.cpp frontend/tokenize/Token.h
        frontend/tokenize/Tokenizer.cpp frontend/tokenize/Tokenizer.h
        frontend/errorhandle/ErrorHandler.cpp frontend/errorhandle/ErrorHandler.h
        frontend/errorhandle/Error.cpp frontend/errorhandle/Error.h
        frontend/grammar_parse/Gparser.cpp frontend/grammar_parse/Gparser.h
        frontend/grammar_parse/GrammarNode.cpp frontend/grammar_parse/GrammarNode.h
        frontend/generate/HIRGenerator.cpp frontend/generate/HIRGenerator.h
        IR/Module.cpp IR/Module.h
        IR/Type.cpp IR/Type.h
        IR/Value.cpp IR/Value.h
        IR/Function.cpp IR/Function.h
        IR/BasicBlock.cpp IR/BasicBlock.h
        IR/HIR.cpp IR/HIR.h
        IR/MIR.cpp IR/MIR.h
        IR/LIR.cpp IR/LIR.h
        pass/Pass.cpp pass/Pass.h
        pass/debug/PrintIRPass.cpp pass/debug/PrintIRPass.h
        pass/erase/EraseUselessBlockPass.cpp pass/erase/EraseUselessBlockPass.h
        pass/generate/HIR2MIRPass.cpp pass/generate/HIR2MIRPass.h
        pass/analyze/AnalyzeCFGPass.cpp pass/analyze/AnalyzeCFGPass.h
        pass/transform/Mem2RegPass.cpp pass/transform/Mem2RegPass.h
        pass/analyze/AnalyzeDomTreePass.cpp pass/analyze/AnalyzeDomTreePass.h
        pass/erase/EraseUselessValuePass.cpp pass/erase/EraseUselessValuePass.h
        pass/analyze/AnalyzeCallGraphPass.cpp pass/analyze/AnalyzeCallGraphPass.h
        pass/transform/ConstantFoldingPass.cpp pass/transform/ConstantFoldingPass.h
        pass/erase/EraseCommonExprPass.cpp pass/erase/EraseCommonExprPass.h
        pass/analyze/AnalyzeSideEffectPass.cpp pass/analyze/AnalyzeSideEffectPass.h
        pass/analyze/AnalyzeActivityPass.cpp pass/analyze/AnalyzeActivityPass.h
        pass/analyze/AnalyzeDataFlowPass.cpp pass/analyze/AnalyzeDataFlowPass.h
        pass/analyze/AnalyzeDAGPass.cpp pass/analyze/AnalyzeDAGPass.h
        pass/generate/MIR2LIRPass.cpp pass/generate/MIR2LIRPass.h
        pass/generate/RegisterColoringPass.cpp pass/generate/RegisterColoringPass.h
        pass/generate/RegisterAllocatePass.cpp pass/generate/RegisterAllocatePass.h
        pass/analyze/AnalyzeLoopPass.cpp pass/analyze/AnalyzeLoopPass.h
        pass/debug/ResetIRIDPass.cpp pass/debug/ResetIRIDPass.h
        pass/analyze/AnalyzeRegionPass.cpp pass/analyze/AnalyzeRegionPass.h
        pass/erase/ErasePseudoInstLIRPass.cpp pass/erase/ErasePseudoInstLIRPass.h
        pass/transform/ForwardMovingPass.cpp pass/transform/ForwardMovingPass.h
        pass/transform/BackwardMovingPass.cpp pass/transform/BackwardMovingPass.h
        pass/analyze/AnalyzePointerPass.cpp pass/analyze/AnalyzePointerPass.h
        pass/transform/AnalyzeUnwrittenGlobalValuePass.cpp pass/transform/AnalyzeUnwrittenGlobalValuePass.h
        pass/erase/EraseLocalConstantPass.cpp pass/erase/EraseLocalConstantPass.h
        pass/erase/JumpMergePass.cpp pass/erase/JumpMergePass.h
        pass/analyze/AnalyzeArrayAccessPass.cpp pass/analyze/AnalyzeArrayAccessPass.h
        pass/transform/InlineFunctionPass.cpp pass/transform/InlineFunctionPass.h
        pass/analyze/AnalyzeLoopParamPass.cpp pass/analyze/AnalyzeLoopParamPass.h
        pass/transform/LoopUnrollingPass.cpp pass/transform/LoopUnrollingPass.h
        pass/erase/EraseUselessFunctionPass.cpp pass/erase/EraseUselessFunctionPass.h
        pass/parallel/ExtractMultiCallPass.cpp pass/parallel/ExtractMultiCallPass.h
        pass/parallel/AnalyzeParallelLoopPass.cpp pass/parallel/AnalyzeParallelLoopPass.h
        pass/transform/InstructionFusion.cpp pass/transform/InstructionFusion.h
        pass/parallel/AnalyzeParallelFunctionPass.cpp pass/parallel/AnalyzeParallelFunctionPass.h
        pass/transform/SimplifyConstantArrayPass.cpp pass/transform/SimplifyConstantArrayPass.h
        pass/erase/JumpMergePass.cpp pass/erase/JumpMergePass.h
        pass/transform/InlineFunctionPass.cpp pass/transform/InlineFunctionPass.h
        pass/transform/InstructionFusion.cpp pass/transform/InstructionFusion.h
        pass/transform/InstructionSchedule.cpp pass/transform/InstructionSchedule.h
        pass/erase/EraseUselessArrayWritePass.cpp pass/erase/EraseUselessArrayWritePass.h
        utils/ConstUtils.cpp utils/ConstUtils.h utils/ArrayToMIR.h
        utils/FunctionUtils.cpp utils/FunctionUtils.h
        utils/BasicBlockUtils.cpp utils/BasicBlockUtils.h
        utils/IRUtils.cpp utils/IRUtils.h
        utils/AssemblyUtils.cpp utils/AssemblyUtils.h
        utils/ConstUtils.cpp utils/ConstUtils.h
        utils/ArrayToMIR.h
        codegen/DataSectionGenerator.cpp codegen/DataSectionGenerator.h
        codegen/CodeSectionGenerator.cpp codegen/CodeSectionGenerator.h
        codegen/AssemblyGenerator.cpp codegen/AssemblyGenerator.h
        codegen/HIRAssemblyGenerator.cpp codegen/HIRAssemblyGenerator.h
        codegen/MIRAssemblyGenerator.cpp codegen/MIRAssemblyGenerator.h
        codegen/LIRAssemblyGenerator.cpp codegen/LIRAssemblyGenerator.h
        )





SET(CMAKE_CXX_FLAGS_RELEASE "-O2 ${CMAKE_CXX_FLAGS_RELEASE}")

add_definitions(-DSHOW_IR)
add_definitions(-DTEST_OPTIMIZE)
add_definitions(-DRANDOM_TEST)
