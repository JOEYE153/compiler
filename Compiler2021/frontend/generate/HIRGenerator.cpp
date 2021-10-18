//
// Created by tcyhost on 2021/7/3.
//

#include "HIRGenerator.h"

using namespace frontend::generate;

Constant *HIRGenerator::declareConstant(shared_ptr<Type> type, const string &constant_name, std::any value) {
    assert(!isdigit(constant_name[0]));
    Constant *pConst;
    if (isInGlobal()) {
        pConst = module->declareGlobalConstant(std::move(type), constant_name, std::move(value));
    } else {
        string newName = constant_name + layer2Str();
        pConst = curFunc->declareLocalConstant(type, newName, value);
    }
    symbolMap[constant_name].emplace_back(pConst);
    return pConst;
}

string HIRGenerator::layer2Str() const {
    int curLayer = layerStack.back();
    return "_.l" + std::to_string(curLayer);
}

Variable *HIRGenerator::declareGlobalVariable(shared_ptr<Type> type, const string &variable_name,
                                              std::any value) {
    Variable *pVar;
    pVar = module->declareGlobalVariable(std::move(type), variable_name, std::move(value));
    symbolMap[variable_name].emplace_back(pVar);
    return pVar;
}

Variable *HIRGenerator::declareLocalVariable(shared_ptr<Type> type, const string &variable_name, bool is_ref,
                                             bool is_arg) {
    string newName = variable_name + layer2Str();
    Variable *pVar;
    pVar = curFunc->declareLocalVariable(type, newName, is_ref, is_arg);
    symbolMap[variable_name].emplace_back(pVar);
    return pVar;
}

UnaryHIR *HIRGenerator::createAssignHIR(Variable *dst, Value *src) {
    return curBlock->createAssignHIR(dst, src);
}

string HIRGenerator::genTmpSymbolName(const string &prefix) {
    static int idx = 0;
    return "@" + prefix + "_" + std::to_string(idx++);
}

BinaryHIR *HIRGenerator::createAddressingHIR(Variable *dst, Value *src1, Value *src2) {
    return curBlock->createAddressingHIR(dst, src1, src2);
}

// create entry block automatically
void HIRGenerator::enterFunc() {
    isGlobal = false;
    assert(curFunc != nullptr);
    inLayer(); // for fParams
    curBlock = curFunc->createBasicBlock(curFunc->getName() + ".entry");
    curFunc->entryBlock = curBlock;
}

void HIRGenerator::exitFunc() {
    if (curFunc->getReturnType() == VoidType::object) {
        createReturnHIR();
    } else {
        createReturnHIR(declareImmediateInt(0));
    }
    outLayer();
    auto blockTable = curFunc->getBasicBlockVecDictOrder();
    for (auto bb : blockTable) {
        for (auto i = 0; i < bb->hirTable.size(); i++) {
            if (dynamic_cast<JumpHIR *>(bb->hirTable[i].get()) != nullptr ||
                dynamic_cast<BranchHIR *>(bb->hirTable[i].get()) != nullptr ||
                dynamic_cast<ReturnHIR *>(bb->hirTable[i].get()) != nullptr) {
                bb->hirTable.resize(i + 1);
                break;
            }
        }
    }
    isGlobal = true;
    curFunc = nullptr;
    curBlock = nullptr;
}

Function *HIRGenerator::declareFunction(shared_ptr<FunctionType> type, const string &function_name) {
    curFunc = module->declareFunction(std::move(type), function_name);
    return curFunc;
}

void HIRGenerator::inLayer() {
    layerStack.emplace_back(nextLayerIdx++);
}

void HIRGenerator::outLayer() {
    string pat = layer2Str();
    layerStack.pop_back();
    for (auto iter = symbolMap.begin(); iter != symbolMap.end();) {
        auto &symbols = iter->second;
        while (!symbols.empty() && endsWith(symbols.back()->getName(), pat)) {
            symbols.pop_back();
        }
        if (symbols.empty()) symbolMap.erase(iter++);
        else ++iter;
    }
}

bool HIRGenerator::endsWith(string_view src, string_view pat) {
    if (src.length() < pat.length()) return false;
    assert(pat.length() <= src.length());
    for (ptrdiff_t i = pat.length() - 1; i >= 0; i--) {
        if (src[i + src.length() - pat.length()] != pat[i]) return false;
    }
    return true;
}

ReturnHIR *HIRGenerator::createReturnHIR(Value *val) {
    return curBlock->createReturnHIR(val);
}

// create BasicBlock automatically
// also let it be the curBlock
BasicBlock *HIRGenerator::declareBasicBlock(const string &suffix, bool setCur) {
    static int blockIdx = 0;
    auto cb = curFunc->createBasicBlock(curFunc->getName() + "." + std::to_string(++blockIdx) + "_" + suffix);
    if (setCur) curBlock = cb;
    return cb;
}

BasicBlock *HIRGenerator::getNearestWhileCond() {
    return whileStack.back().at(0);
}

BasicBlock *HIRGenerator::getNearestWhileBody() {
    return whileStack.back().at(1);
}

BasicBlock *HIRGenerator::getNearestWhileEnd() {
    return whileStack.back().at(2);
}

BasicBlock *HIRGenerator::enterWhileCond() {
    auto condBlock = declareBasicBlock("while_cond", false);
    createJumpHIR(condBlock);
    curBlock = condBlock;
    return curBlock;
}

void HIRGenerator::enterWhileBody(BasicBlock *condBlock, BasicBlock *bodyBlock, BasicBlock *endBlock) {
    vector<BasicBlock *> blocks = {condBlock, bodyBlock, endBlock};
    whileStack.emplace_back(blocks);
    curBlock = bodyBlock;
}

void HIRGenerator::exitWhile() {
    createJumpHIR(getNearestWhileCond());
    curBlock = getNearestWhileEnd();
    whileStack.pop_back();
}

JumpHIR *HIRGenerator::createJumpHIR(BasicBlock *block) {
    return curBlock->createJumpHIR(block);
}

BranchHIR *HIRGenerator::createBranchHIR(Value *cond, BasicBlock *block1, BasicBlock *block2) {
    return curBlock->createBranchHIR(cond, block1, block2);
}

void HIRGenerator::setCurBlock(BasicBlock *block) {
    this->curBlock = block;
}

BasicBlock *HIRGenerator::getCurBlock() {
    return curBlock;
}

Value *HIRGenerator::getNearestSymbol(const string &symbolName) {
    Value *retSym = nullptr;
    if (this->symbolMap.count(symbolName)) {
        retSym = symbolMap[symbolName].back();
    }
    return retSym;
}

BinaryHIR *HIRGenerator::createAddHIR(Variable *dst, Value *src1, Value *src2) {
    return curBlock->createAddHIR(dst, src1, src2);
}

BinaryHIR *HIRGenerator::createSubHIR(Variable *dst, Value *src1, Value *src2) {
    return curBlock->createSubHIR(dst, src1, src2);
}

BinaryHIR *HIRGenerator::createCmpEqHIR(Variable *dst, Value *src1, Value *src2) {
    return curBlock->createCmpEqHIR(dst, src1, src2);
}

BinaryHIR *HIRGenerator::createCmpNeHIR(Variable *dst, Value *src1, Value *src2) {
    return curBlock->createCmpNeHIR(dst, src1, src2);
}


BinaryHIR *HIRGenerator::createCmpGtHIR(Variable *dst, Value *src1, Value *src2) {
    return curBlock->createCmpGtHIR(dst, src1, src2);
}

BinaryHIR *HIRGenerator::createCmpLtHIR(Variable *dst, Value *src1, Value *src2) {
    return curBlock->createCmpLtHIR(dst, src1, src2);
}

BinaryHIR *HIRGenerator::createCmpGeHIR(Variable *dst, Value *src1, Value *src2) {
    return curBlock->createCmpGeHIR(dst, src1, src2);
}

BinaryHIR *HIRGenerator::createCmpLeHIR(Variable *dst, Value *src1, Value *src2) {
    return curBlock->createCmpLeHIR(dst, src1, src2);
}

BinaryHIR *HIRGenerator::createMulHIR(Variable *dst, Value *src1, Value *src2) {
    return curBlock->createMulHIR(dst, src1, src2);
}

BinaryHIR *HIRGenerator::createDivHIR(Variable *dst, Value *src1, Value *src2) {
    return curBlock->createDivHIR(dst, src1, src2);
}

BinaryHIR *HIRGenerator::createModHIR(Variable *dst, Value *src1, Value *src2) {
    return curBlock->createModHIR(dst, src1, src2);
}

CallHIR *HIRGenerator::createCallHIR(Variable *ret, string_view func_name, vector<Value *> args) {
    return curBlock->createCallHIR(ret, func_name, std::move(args));
}

CallHIR *HIRGenerator::createCallHIR(const string &func_name, vector<Value *> args) {
    if (func_name == "starttime" || func_name == "stoptime") {
        return curBlock->createCallHIR("_sysy_" + func_name, std::move(args));
    }
    return curBlock->createCallHIR(func_name, std::move(args));
}

PutfHIR *HIRGenerator::createPutfHIR(string_view format, vector<Value *> args) {
    return curBlock->createPutfHIR(format, std::move(args));
}

Function *HIRGenerator::getFunctionByName(const string &name) {
    if (name == "starttime" || name == "stoptime") {
        return module->getFunctionByName("_sysy_" + name);
    }
    return module->getFunctionByName(name);
}

Value *HIRGenerator::createMidSymbol(Value *left, Value *right, frontend::tokenize::TOKEN_TYPE opType) {
    Value *sym = nullptr;
    if (left->isConstant() && right->isConstant()) {
        int val = -1;
        switch (opType) {
            case tokenize::TOKEN_TYPE::EQL:
                val = dynamic_cast<Constant *>(left)->getValue<int>() ==
                      dynamic_cast<Constant *>(right)->getValue<int>();
                break;
            case tokenize::TOKEN_TYPE::NEQ:
                val = dynamic_cast<Constant *>(left)->getValue<int>() !=
                      dynamic_cast<Constant *>(right)->getValue<int>();
                break;
            case tokenize::TOKEN_TYPE::LSS:
                val = dynamic_cast<Constant *>(left)->getValue<int>() <
                      dynamic_cast<Constant *>(right)->getValue<int>();
                break;
            case tokenize::TOKEN_TYPE::LEQ:
                val = dynamic_cast<Constant *>(left)->getValue<int>() <=
                      dynamic_cast<Constant *>(right)->getValue<int>();
                break;
            case tokenize::TOKEN_TYPE::GRE:
                val = dynamic_cast<Constant *>(left)->getValue<int>() >
                      dynamic_cast<Constant *>(right)->getValue<int>();
                break;
            case tokenize::TOKEN_TYPE::GEQ:
                val = dynamic_cast<Constant *>(left)->getValue<int>() >=
                      dynamic_cast<Constant *>(right)->getValue<int>();
                break;

            case tokenize::TOKEN_TYPE::PLUS:
                val = dynamic_cast<Constant *>(left)->getValue<int>() +
                      dynamic_cast<Constant *>(right)->getValue<int>();
                break;
            case tokenize::TOKEN_TYPE::MINU:
                val = dynamic_cast<Constant *>(left)->getValue<int>() -
                      dynamic_cast<Constant *>(right)->getValue<int>();
                break;
            case tokenize::TOKEN_TYPE::MULT:
                val = dynamic_cast<Constant *>(left)->getValue<int>() *
                      dynamic_cast<Constant *>(right)->getValue<int>();
                break;
            case tokenize::TOKEN_TYPE::DIV:
                val = dynamic_cast<Constant *>(right)->getValue<int>();
                if (val != 0) {
                    val = dynamic_cast<Constant *>(left)->getValue<int>() / val;
                }
                break;
            case tokenize::TOKEN_TYPE::MOD:
                val = dynamic_cast<Constant *>(right)->getValue<int>();
                if (val != 0) {
                    val = dynamic_cast<Constant *>(left)->getValue<int>() % val;
                }
                break;
            default:
                std::cerr << "not a illegal operator! --" << std::endl;
                break;
        }
        sym = declareImmediateInt(val);
    } else { // must be local not global
        switch (opType) {
            case tokenize::TOKEN_TYPE::EQL:
                sym = declareLocalVariable(BooleanType::object, genTmpSymbolName("var"), false);
                createCmpEqHIR(dynamic_cast<Variable *>(sym), left, right);
                break;
            case tokenize::TOKEN_TYPE::NEQ:
                sym = declareLocalVariable(BooleanType::object, genTmpSymbolName("var"), false);
                createCmpNeHIR(dynamic_cast<Variable *>(sym), left, right);
                break;
            case tokenize::TOKEN_TYPE::LSS:
                sym = declareLocalVariable(BooleanType::object, genTmpSymbolName("var"), false);
                createCmpLtHIR(dynamic_cast<Variable *>(sym), left, right);
                break;
            case tokenize::TOKEN_TYPE::LEQ:
                sym = declareLocalVariable(BooleanType::object, genTmpSymbolName("var"), false);
                createCmpLeHIR(dynamic_cast<Variable *>(sym), left, right);
                break;
            case tokenize::TOKEN_TYPE::GRE:
                sym = declareLocalVariable(BooleanType::object, genTmpSymbolName("var"), false);
                createCmpGtHIR(dynamic_cast<Variable *>(sym), left, right);
                break;
            case tokenize::TOKEN_TYPE::GEQ:
                sym = declareLocalVariable(BooleanType::object, genTmpSymbolName("var"), false);
                createCmpGeHIR(dynamic_cast<Variable *>(sym), left, right);
                break;

            case tokenize::TOKEN_TYPE::PLUS:
                sym = declareLocalVariable(IntegerType::object, genTmpSymbolName("var"), false);
                createAddHIR(dynamic_cast<Variable *>(sym), left, right);
                break;
            case tokenize::TOKEN_TYPE::MINU:
                sym = declareLocalVariable(IntegerType::object, genTmpSymbolName("var"), false);
                createSubHIR(dynamic_cast<Variable *>(sym), left, right);
                break;
            case tokenize::TOKEN_TYPE::MULT:
                sym = declareLocalVariable(IntegerType::object, genTmpSymbolName("var"), false);
                createMulHIR(dynamic_cast<Variable *>(sym), left, right);
                break;
            case tokenize::TOKEN_TYPE::DIV:
                sym = declareLocalVariable(IntegerType::object, genTmpSymbolName("var"), false);
                createDivHIR(dynamic_cast<Variable *>(sym), left, right);
                break;
            case tokenize::TOKEN_TYPE::MOD:
                sym = declareLocalVariable(IntegerType::object, genTmpSymbolName("var"), false);
                createModHIR(dynamic_cast<Variable *>(sym), left, right);
                break;
            default:
                std::cerr << "not a illegal operator! --" << std::endl;
                break;
        }
    }
    return sym;
}

Value *HIRGenerator::createUniSymbol(Value *src, TOKEN_TYPE opType) {
    Value *sym = nullptr;
    if (opType == tokenize::TOKEN_TYPE::PLUS) {
        return src;
    }
    if (src->isConstant()) {
        int val = -1;
        switch (opType) {
            case tokenize::TOKEN_TYPE::NOT:
                val = !dynamic_cast<Constant *>(src)->getValue<int>();
                break;
            case tokenize::TOKEN_TYPE::MINU:
                val = -dynamic_cast<Constant *>(src)->getValue<int>();
                break;
            default:
                std::cerr << "not a illegal operator! --" << std::endl;
                break;
        }
        sym = declareImmediateInt(val);
    } else {
        switch (opType) {
            case tokenize::TOKEN_TYPE::NOT:
                sym = declareLocalVariable(BooleanType::object, genTmpSymbolName("var"), false);
                createNotHIR(dynamic_cast<Variable *>(sym), src);
                break;
            case tokenize::TOKEN_TYPE::MINU:
                sym = declareLocalVariable(BooleanType::object, genTmpSymbolName("var"), false);
                createNegHIR(dynamic_cast<Variable *>(sym), src);
                break;
            default:
                std::cerr << "not a illegal operator! --" << std::endl;
                break;
        }
    }
    return sym;
}

UnaryHIR *HIRGenerator::createNegHIR(Variable *dst, Value *src) {
    return curBlock->createNegHIR(dst, src);
}

UnaryHIR *HIRGenerator::createNotHIR(Variable *dst, Value *src) {
    return curBlock->createNotHIR(dst, src);
}

void HIRGenerator::declareExternalFunction() {
    auto getintType = make_shared<FunctionType>(IntegerType::object);
    module->declareFunction(getintType, "getint")->isExternal = true;
    auto getChType = make_shared<FunctionType>(IntegerType::object);
    module->declareFunction(getChType, "getch")->isExternal = true;
    auto getArrayType = make_shared<FunctionType>(
            IntegerType::object, vector < shared_ptr < Type >> {make_shared<ArrayType>(IntegerType::object)});
    module->declareFunction(getArrayType, "getarray")->isExternal = true;
    auto putIntType = make_shared<FunctionType>(
            VoidType::object, vector < shared_ptr < Type >> {IntegerType::object});
    module->declareFunction(putIntType, "putint")->isExternal = true;
    auto putChType = make_shared<FunctionType>(
            VoidType::object, vector < shared_ptr < Type >> {IntegerType::object});
    module->declareFunction(putChType, "putch")->isExternal = true;
    auto putArrayType = make_shared<FunctionType>(
            VoidType::object, vector < shared_ptr < Type >> {IntegerType::object,
                                                             make_shared<ArrayType>(IntegerType::object)});
    module->declareFunction(putArrayType, "putarray")->isExternal = true;
    // no putf defined
    auto startTimeType = make_shared<FunctionType>(VoidType::object,
                                                   vector < shared_ptr < Type >> {IntegerType::object});
    module->declareFunction(startTimeType, "_sysy_starttime")->isExternal = true;
    auto stopTimeType = make_shared<FunctionType>(VoidType::object,
                                                  vector < shared_ptr < Type >> {IntegerType::object});
    module->declareFunction(stopTimeType, "_sysy_stoptime")->isExternal = true;
}

Constant *HIRGenerator::declareImmediateInt(int value) {
    if (isGlobal) {
        return module->declareGlobalImmediateInt(value);
    } else {
        return curFunc->declareLocalImmediateInt(value);
    }
}