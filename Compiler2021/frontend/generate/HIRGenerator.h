//
// Created by tcyhost on 2021/7/3.
//

#ifndef COMPILER2021_HIRGENERATOR_H
#define COMPILER2021_HIRGENERATOR_H

#include <memory>
#include <utility>
#include <vector>
#include <map>
#include <cassert>
#include <tuple>
#include <iostream>

#include "../../IR/Module.h"
#include "../tokenize/Token.h"

namespace frontend::generate {
    using std::make_unique;
    using std::make_shared;
    using std::unique_ptr;
    using std::shared_ptr;
    using std::map;
    using std::string_view;
    using std::pair;
    using std::make_pair;
    using std::make_tuple;
    using std::tuple;
    using namespace frontend::tokenize;

    typedef shared_ptr<Value> ValueSPtr;

    class HIRGenerator {
        shared_ptr<Module> module;
        Function *curFunc;
        BasicBlock *curBlock;

        bool isGlobal;
        int nextLayerIdx;
        vector<int> layerStack;
        map<string, vector<Value *>> symbolMap;
        vector<vector<BasicBlock *>> whileStack;

        [[nodiscard]] string layer2Str() const;


    public:
        explicit HIRGenerator(const string &moduleName) {
            module = make_shared<Module>(moduleName);
            curFunc = nullptr;
            curBlock = nullptr;
            isGlobal = true;
            nextLayerIdx = 0;
        }

        ~HIRGenerator() = default;

        shared_ptr<Module> getModule() { return module; }

        static bool endsWith(string_view src, string_view pat);

        static string genTmpSymbolName(const string &prefix = "");

        [[nodiscard]] bool isInGlobal() const { return isGlobal; }

        void declareExternalFunction();

        void enterFunc();

        void exitFunc();

        void inLayer();

        void outLayer(); // remove all the symbols at this layer from symbolMap

        BasicBlock *getNearestWhileCond();

        BasicBlock *getNearestWhileBody();

        BasicBlock *getNearestWhileEnd();

        BasicBlock *enterWhileCond();

        void enterWhileBody(BasicBlock *condBlock, BasicBlock *bodyBlock, BasicBlock *endBlock);

        void exitWhile();

        Value *getNearestSymbol(const string &symbolName);

        Function *getFunctionByName(const string &name);

        void setCurBlock(BasicBlock *block);

        BasicBlock *getCurBlock();

        // symbol declaration

        Constant *declareConstant(shared_ptr<Type> type, const string &constant_name, std::any value);

        Constant *declareImmediateInt(int value);

        Variable *declareGlobalVariable(shared_ptr<Type> type, const string &variable_name, std::any value = {});

        Variable *
        declareLocalVariable(shared_ptr<Type> type, const string &variable_name, bool is_ref, bool is_arg = false);

        Function *declareFunction(shared_ptr<FunctionType> type, const string &function_name);

        BasicBlock *declareBasicBlock(const string &suffix = "", bool setCur = true);

        Value *createMidSymbol(Value *left, Value *right, TOKEN_TYPE opType);

        Value *createUniSymbol(Value *src, TOKEN_TYPE opType);

        // HIR creation

        UnaryHIR *createAssignHIR(Variable *dst, Value *src);

        UnaryHIR *createNegHIR(Variable *dst, Value *src);

        UnaryHIR *createNotHIR(Variable *dst, Value *src);

        BinaryHIR *createAddressingHIR(Variable *dst, Value *src1, Value *src2);

        ReturnHIR *createReturnHIR(Value *val = nullptr);

        JumpHIR *createJumpHIR(BasicBlock *block);

        BranchHIR *createBranchHIR(Value *cond, BasicBlock *block1, BasicBlock *block2);

        BinaryHIR *createAddHIR(Variable *dst, Value *src1, Value *src2);

        BinaryHIR *createSubHIR(Variable *dst, Value *src1, Value *src2);

        BinaryHIR *createMulHIR(Variable *dst, Value *src1, Value *src2);

        BinaryHIR *createDivHIR(Variable *dst, Value *src1, Value *src2);

        BinaryHIR *createModHIR(Variable *dst, Value *src1, Value *src2);

        BinaryHIR *createCmpEqHIR(Variable *dst, Value *src1, Value *src2);

        BinaryHIR *createCmpNeHIR(Variable *dst, Value *src1, Value *src2);

        BinaryHIR *createCmpGtHIR(Variable *dst, Value *src1, Value *src2);

        BinaryHIR *createCmpLtHIR(Variable *dst, Value *src1, Value *src2);

        BinaryHIR *createCmpGeHIR(Variable *dst, Value *src1, Value *src2);

        BinaryHIR *createCmpLeHIR(Variable *dst, Value *src1, Value *src2);

        CallHIR *createCallHIR(Variable *ret, string_view func_name, vector<Value *> args = {});

        CallHIR *createCallHIR(const string &func_name, vector<Value *> args = {});

        PutfHIR *createPutfHIR(string_view format, vector<Value *> args = {});

    };

    typedef unique_ptr<HIRGenerator> HIRGeneratorUPtr;
}

#endif //COMPILER2021_HIRGENERATOR_H
