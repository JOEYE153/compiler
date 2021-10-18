//
// Created by tcyhost on 2021/6/30.
//

#include <cstdarg>
#include <iostream>
#include <cassert>

#include "Gparser.h"

using namespace frontend::tokenize;
using namespace frontend::grammar_parse;
using std::dynamic_pointer_cast;
using std::to_string;

// CompUnit ::= (Decl | FuncDef) {Decl | FuncDef}
// Decl ::= ConstDecl | VarDecl
GNodeSPtr GParser::parseCompUnit() {
    hirGenerator->declareExternalFunction();
    while (!(checkTokType(0, TOKEN_TYPE::EOFTK))) {
        if (checkTokType(0, 2, TOKEN_TYPE::INTTK, TOKEN_TYPE::VOIDTK)
            && checkTokType(1, TOKEN_TYPE::IDENFR)
            && checkTokType(2, TOKEN_TYPE::LPARENT)) {
            addBGNode(root, parseFuncDef());
        } else if (checkTokType(0, tokenize::TOKEN_TYPE::CONSTTK)) {
            addBGNode(root, parseConstDecl());
        } else {
            addBGNode(root, parseVarDecl());
        }
    }
    return root;
}

bool GParser::checkTokType(size_t pIdx, size_t type_cnt, ...) {
    bool flag = false;
    if (curTokIndex + pIdx < tokens.size()) {
        TOKEN_TYPE type = tokens[curTokIndex + pIdx].getType();
        va_list tokTypes;
        va_start(tokTypes, type_cnt);
        for (auto i = 0; i < type_cnt; i++) {
            flag |= va_arg(tokTypes, TOKEN_TYPE) == type;
        }
        va_end(tokTypes);
    }
    return flag;
}

bool GParser::checkTokType(size_t pIdx, TOKEN_TYPE type) {
    if (curTokIndex + pIdx < tokens.size()) {
        return tokens[curTokIndex + pIdx].getType() == type;
    }
    return false;
}

bool GParser::findTok(TOKEN_TYPE trg, TOKEN_TYPE end) {
    for (auto i = curTokIndex; i < tokens.size(); i++) {
        if (tokens[i].getType() == trg) {
            return true;
        }
        if (tokens[i].getType() == end) {
            return false;
        }
    }
    return false;
}

void GParser::error() {
    std::cout << "unexpected error on " + to_string(getCurTokLineNum()) << std::endl;
}

// 添加一个token，并自动向后读取
void GParser::addLGNode(const GNodeSPtr &par, TOKEN_TYPE type) {
    if (checkTokType(0, type)) {
        auto leaf = make_shared<LGNode>(tokens[curTokIndex]);
        par->addChild(leaf);
        readNextToken();
    } else {
        error();
    }
}

// ConstDecl ::= **'const'** **'int'** ConstDef { ',' ConstDef } ';'
GNodeSPtr GParser::parseConstDecl() {
    auto constDecl = make_shared<BGNode>(G_NODE_TYPE::CONST_DECL);
    addLGNode(constDecl, tokenize::TOKEN_TYPE::CONSTTK);
    addLGNode(constDecl, tokenize::TOKEN_TYPE::INTTK);
    addBGNode(constDecl, parseConstDef());
    while (checkTokType(0, tokenize::TOKEN_TYPE::COMMA)) {
        addLGNode(constDecl, tokenize::TOKEN_TYPE::COMMA);
        addBGNode(constDecl, parseConstDef());
    }
    addLGNode(constDecl, tokenize::TOKEN_TYPE::SEMICN);
    return constDecl;
}

// VarDecl ::= **'int'** VarDef { ',' VarDef } ';'
GNodeSPtr GParser::parseVarDecl() {
    auto varDecl = make_shared<BGNode>(G_NODE_TYPE::VAR_DECL);
    addLGNode(varDecl, tokenize::TOKEN_TYPE::INTTK);
    addBGNode(varDecl, parseVarDef());
    while (checkTokType(0, tokenize::TOKEN_TYPE::COMMA)) {
        addLGNode(varDecl, tokenize::TOKEN_TYPE::COMMA);
        addBGNode(varDecl, parseVarDef());
    }
    addLGNode(varDecl, tokenize::TOKEN_TYPE::SEMICN);
    return varDecl;
}

// ConstDef ::= **Ident** { '[' ConstExp ']' } '=' ConstInitVal
GNodeSPtr GParser::parseConstDef() {
    auto constDef = make_shared<BGNode>(G_NODE_TYPE::CONST_DEF);
    addLGNode(constDef, tokenize::TOKEN_TYPE::IDENFR);
    auto ideName = getLastTokName();
    vector<int> dims;
    Value *pVal;
    while (checkTokType(0, tokenize::TOKEN_TYPE::LBRACK)) {
        addLGNode(constDef, tokenize::TOKEN_TYPE::LBRACK);
        addBGNode(constDef, parseConstExpr(&pVal));
        auto cpVal = dynamic_cast<Constant *>(pVal);
        dims.emplace_back(cpVal->getValue<int>());
        addLGNode(constDef, tokenize::TOKEN_TYPE::RBRACK);
    }
    auto type = dims2Type(dims);
    std::any value;
    addLGNode(constDef, tokenize::TOKEN_TYPE::ASSIGN);
    addBGNode(constDef, parseConstInitVal(value, dims));
    hirGenerator->declareConstant(type, ideName, value);
    return constDef;
}

// VarDef ::= **Ident** { '[' ConstExp ']' }  [ '=' InitVal ]
GNodeSPtr GParser::parseVarDef() {
    auto varDef = make_shared<BGNode>(G_NODE_TYPE::VAR_DEF);
    addLGNode(varDef, tokenize::TOKEN_TYPE::IDENFR);
    auto ideName = getLastTokName();
    // get dims
    vector<int> dims;
    Value *pVal;
    while (checkTokType(0, tokenize::TOKEN_TYPE::LBRACK)) {
        addLGNode(varDef, tokenize::TOKEN_TYPE::LBRACK);
        addBGNode(varDef, parseConstExpr(&pVal));
        auto cpVal = dynamic_cast<Constant *>(pVal);
        dims.emplace_back(cpVal->getValue<int>());
        addLGNode(varDef, tokenize::TOKEN_TYPE::RBRACK);
    }
    auto type = dims2Type(dims);
    // get init value if exist
    std::any value;
    bool needAssign = false;
    if (checkTokType(0, TOKEN_TYPE::ASSIGN)) {
        needAssign = true;
        addLGNode(varDef, tokenize::TOKEN_TYPE::ASSIGN);
        if (hirGenerator->isInGlobal()) {
            addBGNode(varDef, parseConstInitVal(value, dims));
        } else {
            addBGNode(varDef, parseVarInitVal(value, dims));
        }
    }
    // declare and init variable here
    if (hirGenerator->isInGlobal()) {
        if (!needAssign) { // let them all be zero by default
            hirGenerator->declareGlobalVariable(type, ideName);
        } else {
            // check if all zero
            bool all0 = true;
            switch (type->getId()) {
                case Type::ID::INTEGER:
                    all0 = std::any_cast<int>(value) == 0;
                    break;
                case Type::ID::ARRAY:
                    if (value.has_value()) {
                        auto vecValue = std::any_cast<vector<int>>(value);
                        for (int val : vecValue) {
                            if (val != 0) {
                                all0 = false;
                                break;
                            }
                        }
                    }
                    break;
                default:
                    break;
            }
            if (all0) {
                hirGenerator->declareGlobalVariable(type, ideName);
            } else {
                hirGenerator->declareGlobalVariable(type, ideName, value);
            }
        }
    } else {
        auto variable = hirGenerator->declareLocalVariable(type, ideName, false);
        if (needAssign) {
            if (type == IntegerType::object) {
                auto pValue = std::any_cast<Value *>(value);
                hirGenerator->createAssignHIR(variable, pValue);
            } else {
                localArrAssign(variable, value, dims);
            }
        }
    }
    return varDef;
}

// ConstInitVal ::= ConstExp | '{' [ ConstInitVal { ',' ConstInitVal } ] '}'
GNodeSPtr GParser::parseConstInitVal(std::any &value, vector<int> &dims) {
    auto constInitVal = make_shared<BGNode>(G_NODE_TYPE::CONST_INIT);
    if (checkTokType(0, tokenize::TOKEN_TYPE::LBRACE)) { // array
        vector<int> values;
        constValInit(constInitVal, dims, values, 0);
        value = values;
    } else { // scalar
        Value *pVal;
        addBGNode(constInitVal, parseConstExpr(&pVal));
        auto cpVal = dynamic_cast<Constant *>(pVal);
        value = cpVal->getValue<int>();
    }
    return constInitVal;
}

// InitVal ::= Exp | '{' [ InitVal { ',' InitVal } ] '}'
GNodeSPtr GParser::parseVarInitVal(std::any &value, vector<int> &dims) {
    auto varInitVal = make_shared<BGNode>(G_NODE_TYPE::VAR_INIT);
    if (checkTokType(0, tokenize::TOKEN_TYPE::LBRACE)) {
        vector<Value *> values;
        valInit(varInitVal, dims, values, 0);
        value = values;
    } else {
        Value *pVal;
        addBGNode(varInitVal, parseExpr(&pVal));
        value = pVal;
    }
    return varInitVal;
}

// FuncDef ::= FuncType **Ident** '(' [FuncFParams] ')' Block
// FuncType ::= **'void'** |  **'int'**
GNodeSPtr GParser::parseFuncDef() {
    auto funcDef = make_shared<BGNode>(G_NODE_TYPE::FUNC_DEF);
    shared_ptr<Type> retType;
    if (checkTokType(0, tokenize::TOKEN_TYPE::VOIDTK)) {
        addLGNode(funcDef, tokenize::TOKEN_TYPE::VOIDTK);
        retType = VoidType::object;
    } else {
        addLGNode(funcDef, tokenize::TOKEN_TYPE::INTTK);
        retType = IntegerType::object;
    }
    addLGNode(funcDef, tokenize::TOKEN_TYPE::IDENFR);
    auto funcName = getLastTokName();
    addLGNode(funcDef, tokenize::TOKEN_TYPE::LPARENT);
    vector<shared_ptr<Type>> args_type;
    vector<string> argNames;
    if (!checkTokType(0, tokenize::TOKEN_TYPE::RPARENT)) {
        addBGNode(funcDef, parseFuncFParams(args_type, argNames));
    }
    addLGNode(funcDef, tokenize::TOKEN_TYPE::RPARENT);
    shared_ptr<FunctionType> fn_type;
    if (args_type.empty()) {
        fn_type = make_shared<FunctionType>(retType);
    } else {
        fn_type = make_shared<FunctionType>(retType, args_type);
    }
    auto fn = hirGenerator->declareFunction(fn_type, funcName);
    hirGenerator->enterFunc();
    for (auto i = 0; i < argNames.size(); i++) {
        bool is_ref = args_type[i] != IntegerType::object;
        fn->args[i] = hirGenerator->declareLocalVariable(args_type[i], argNames[i], is_ref, true);
    }
    addBGNode(funcDef, parseBlock());
    hirGenerator->exitFunc();
    return funcDef;
}

// FuncFParams ::= FuncFParam { ',' FuncFParam }
GNodeSPtr GParser::parseFuncFParams(vector<shared_ptr<Type>> &args_type, vector<string> &argNames) {
    auto funcFParams = make_shared<BGNode>(G_NODE_TYPE::FUNC_F_PARAM_TABLE);
    shared_ptr<Type> argType;
    string ideName;
    addBGNode(funcFParams, parseFuncFParam(argType, ideName));
    args_type.emplace_back(argType);
    argNames.emplace_back(ideName);
    while (checkTokType(0, tokenize::TOKEN_TYPE::COMMA)) {
        addLGNode(funcFParams, tokenize::TOKEN_TYPE::COMMA);
        addBGNode(funcFParams, parseFuncFParam(argType, ideName));
        args_type.emplace_back(argType);
        argNames.emplace_back(ideName);
    }
    return funcFParams;
}

// FuncFParam ::= **'int'** **Ident** [ '[' ']' { '[' Exp ']' } ]
GNodeSPtr GParser::parseFuncFParam(shared_ptr<Type> &argType, string &argName) {
    auto funcFParam = make_shared<BGNode>(G_NODE_TYPE::FUNC_F_PARAM);
    addLGNode(funcFParam, tokenize::TOKEN_TYPE::INTTK);
    addLGNode(funcFParam, tokenize::TOKEN_TYPE::IDENFR);
    argName = getLastTokName();
    argType = IntegerType::object;
    if (checkTokType(0, tokenize::TOKEN_TYPE::LBRACK)) {
        vector<int> dims;
        Value *var;
        addLGNode(funcFParam, tokenize::TOKEN_TYPE::LBRACK);
        addLGNode(funcFParam, tokenize::TOKEN_TYPE::RBRACK);
        while (checkTokType(0, tokenize::TOKEN_TYPE::LBRACK)) {
            addLGNode(funcFParam, tokenize::TOKEN_TYPE::LBRACK);
            addBGNode(funcFParam, parseConstExpr(&var));
            auto cVal = dynamic_cast<Constant *>(var);
            dims.emplace_back(cVal->getValue<int>());
            addLGNode(funcFParam, tokenize::TOKEN_TYPE::RBRACK);
        }
        argType = make_shared<ArrayType>(dims2Type(dims));
    }
    // bool is_ref = argType != IntegerType::object;
    // hirGenerator->declareLocalVariable(argType, argName, is_ref, true);
    return funcFParam;
}

// Block ::= '{' { BlockItem } '}'
// BlockItem ::= Decl | Stmt
// Decl ::= ConstDecl | VarDecl
GNodeSPtr GParser::parseBlock() {
    auto block = make_shared<BGNode>(G_NODE_TYPE::BLOCK);
    addLGNode(block, tokenize::TOKEN_TYPE::LBRACE);
    hirGenerator->inLayer();
    while (!checkTokType(0, tokenize::TOKEN_TYPE::RBRACE)) {
        if (checkTokType(0, tokenize::TOKEN_TYPE::CONSTTK)) {
            addBGNode(block, parseConstDecl());
        } else if (checkTokType(0, tokenize::TOKEN_TYPE::INTTK)) {
            addBGNode(block, parseVarDecl());
        } else {
            addBGNode(block, parseStmt());
        }
    }
    addLGNode(block, tokenize::TOKEN_TYPE::RBRACE);
    hirGenerator->outLayer();
    return block;
}

// Stmt ::= LVal '=' Exp ';' | [Exp] ';' |
//          Block | if_stmt | while_stmt |
//          **'break'** ';' | **'continue'** ';' | **'return'** [Exp] ';'
GNodeSPtr GParser::parseStmt() {
    auto stmt = make_shared<BGNode>(G_NODE_TYPE::STMT);
    if (checkTokType(0, tokenize::TOKEN_TYPE::BREAKTK)) {
        addLGNode(stmt, tokenize::TOKEN_TYPE::BREAKTK);
        hirGenerator->createJumpHIR(hirGenerator->getNearestWhileEnd());
        addLGNode(stmt, tokenize::TOKEN_TYPE::SEMICN);
    } else if (checkTokType(0, tokenize::TOKEN_TYPE::CONTINUETK)) {
        addLGNode(stmt, tokenize::TOKEN_TYPE::CONTINUETK);
        hirGenerator->createJumpHIR(hirGenerator->getNearestWhileCond());
        addLGNode(stmt, tokenize::TOKEN_TYPE::SEMICN);
    } else if (checkTokType(0, tokenize::TOKEN_TYPE::RETURNTK)) {
        addLGNode(stmt, tokenize::TOKEN_TYPE::RETURNTK);
        if (!checkTokType(0, tokenize::TOKEN_TYPE::SEMICN)) {
            Value *retVal;
            addBGNode(stmt, parseExpr(&retVal));
            hirGenerator->createReturnHIR(retVal);
        } else {
            hirGenerator->createReturnHIR();
        }
        hirGenerator->declareBasicBlock();
        addLGNode(stmt, tokenize::TOKEN_TYPE::SEMICN);
    } else if (checkTokType(0, tokenize::TOKEN_TYPE::IFTK)) {
        addBGNode(stmt, parseIfStmt());
    } else if (checkTokType(0, tokenize::TOKEN_TYPE::WHILETK)) {
        addBGNode(stmt, parseWhileStmt());
    } else if (checkTokType(0, tokenize::TOKEN_TYPE::LBRACE)) {
        addBGNode(stmt, parseBlock());
    } else if (findTok(tokenize::TOKEN_TYPE::ASSIGN, tokenize::TOKEN_TYPE::SEMICN)) {
        Value *lVal, *rVal;
        addBGNode(stmt, parseLVal(&lVal));
        addLGNode(stmt, tokenize::TOKEN_TYPE::ASSIGN);
        addBGNode(stmt, parseExpr(&rVal));
        hirGenerator->createAssignHIR(dynamic_cast<Variable *>(lVal), rVal);
        addLGNode(stmt, tokenize::TOKEN_TYPE::SEMICN);
    } else {
        if (!checkTokType(0, tokenize::TOKEN_TYPE::SEMICN)) {
            Value *rVal;
            addBGNode(stmt, parseExpr(&rVal));
        }
        addLGNode(stmt, tokenize::TOKEN_TYPE::SEMICN);
    }
    return stmt;
}

// if_stmt ::= **'if'** '( Cond ')' Stmt [ **'else'** Stmt ]
GNodeSPtr GParser::parseIfStmt() {
    auto ifStmt = make_shared<BGNode>(G_NODE_TYPE::IF_STMT);
    addLGNode(ifStmt, tokenize::TOKEN_TYPE::IFTK);
    addLGNode(ifStmt, tokenize::TOKEN_TYPE::LPARENT);
    BasicBlock *bodyBlock = nullptr;
    BasicBlock *nextBlock = nullptr;
    addBGNode(ifStmt, parseCond(&bodyBlock, &nextBlock));
    hirGenerator->setCurBlock(bodyBlock);
    addLGNode(ifStmt, tokenize::TOKEN_TYPE::RPARENT);
    addBGNode(ifStmt, parseStmt());
    if (checkTokType(0, tokenize::TOKEN_TYPE::ELSETK)) {
        addLGNode(ifStmt, tokenize::TOKEN_TYPE::ELSETK);
        BasicBlock *endBlock = hirGenerator->declareBasicBlock("if_end", false);
        hirGenerator->createJumpHIR(endBlock);
        hirGenerator->setCurBlock(nextBlock);
        addBGNode(ifStmt, parseStmt());
        hirGenerator->createJumpHIR(endBlock);
        hirGenerator->setCurBlock(endBlock);
    } else {
        hirGenerator->createJumpHIR(nextBlock);
        hirGenerator->setCurBlock(nextBlock);
    }
    return ifStmt;
}

// while_stmt ::= **'while'** '(' Cond ')' Stmt
GNodeSPtr GParser::parseWhileStmt() {
    auto whileStmt = make_shared<BGNode>(G_NODE_TYPE::WHILE_STMT);
    addLGNode(whileStmt, tokenize::TOKEN_TYPE::WHILETK);
    addLGNode(whileStmt, tokenize::TOKEN_TYPE::LPARENT);
    auto condBlock = hirGenerator->enterWhileCond();
    BasicBlock *bodyBlock = nullptr, *endBlock = nullptr;
    addBGNode(whileStmt, parseCond(&bodyBlock, &endBlock));
    addLGNode(whileStmt, tokenize::TOKEN_TYPE::RPARENT);
    hirGenerator->enterWhileBody(condBlock, bodyBlock, endBlock);
    addBGNode(whileStmt, parseStmt());
    hirGenerator->exitWhile();
    return whileStmt;
}

// ConstExp ::= AddExp 	*注：使用的 Ident 必须是常量*
GNodeSPtr GParser::parseConstExpr(Value **symbol) {
    auto constExpr = make_shared<BGNode>(G_NODE_TYPE::CONST_EXPR);
    addBGNode(constExpr, parseExpr(symbol));
    if (!(*symbol)->isConstant()) error();
    return constExpr;
}

// Cond ::= LOrExp
// allocate the trueBlock and falseBlock
// note that the curBlock after CondParse **must** be set manually by caller
GNodeSPtr GParser::parseCond(BasicBlock **trueBlock, BasicBlock **falseBlock) {
    auto cond = make_shared<BGNode>(G_NODE_TYPE::COND);
    addBGNode(cond, parseLOrExpr(trueBlock, falseBlock));
    return cond;
}

// LOrExp ::= LAndExp { '||' LAndExp }
/*
 * allocate the trueBlock and falseBlock if not provided
 * note that the curBlock after LorExpParse **must** be set manually by caller
 * trueBlock: the block jumped to when the LOrExp is true (any of the LAndExp)
 * falseBlock: the block jumped to when the LOrExp is false (all of the LAndExp)
 */
GNodeSPtr GParser::parseLOrExpr(BasicBlock **trueBlock, BasicBlock **falseBlock) {
    auto lOrExpr = make_shared<BGNode>(G_NODE_TYPE::LOR_EXPR);
    BasicBlock *tb = nullptr, *fb = nullptr;
    addBGNode(lOrExpr, parseLAndExpr(&tb, &fb));
    while (checkTokType(0, tokenize::TOKEN_TYPE::OR)) {
        addLGNode(lOrExpr, tokenize::TOKEN_TYPE::OR);
        hirGenerator->setCurBlock(fb);
        addBGNode(lOrExpr, parseLAndExpr(&tb, &fb));
    }
    *falseBlock = fb;
    *trueBlock = tb;
    return lOrExpr;
}

// LAndExp ::= EqExp { '&&' EqExp }
/*
 * allocate the trueBlock if not provided
 * always allocate the falseBlock for each LAndExp once
 * trueBlock: the block jumped to when the LAndExp is true (all of the EqExp)
 * falseBlock: the block jumped to when the LAndExp is false (any of the EqExp)
 */
GNodeSPtr GParser::parseLAndExpr(BasicBlock **trueBlock, BasicBlock **falseBlock) {
    Value *symbol;
    auto lAndExpr = make_shared<BGNode>(G_NODE_TYPE::LAND_EXPR);
    addBGNode(lAndExpr, parseEqExpr(&symbol));
    auto fb = hirGenerator->declareBasicBlock("false", false);
    BasicBlock *tb = nullptr;
    while (checkTokType(0, tokenize::TOKEN_TYPE::AND)) {
        addLGNode(lAndExpr, tokenize::TOKEN_TYPE::AND);
        tb = hirGenerator->declareBasicBlock("true", false);
        hirGenerator->createBranchHIR(symbol, tb, fb);
        hirGenerator->setCurBlock(tb);
        addBGNode(lAndExpr, parseEqExpr(&symbol));
    }
    if ((*trueBlock) == nullptr) {
        tb = hirGenerator->declareBasicBlock("true", false);
        *trueBlock = tb;
    }
    hirGenerator->createBranchHIR(symbol, *trueBlock, fb);
    *falseBlock = fb;
    return lAndExpr;
}

// EqExp ::= RelExp { ('==' | '!=') RelExp }
GNodeSPtr GParser::parseEqExpr(Value **symbol) {
    auto eqExpr = make_shared<BGNode>(G_NODE_TYPE::EQ_EXPR);
    Value *left;
    addBGNode(eqExpr, parseRelExpr(&left));
    while (checkTokType(0, 2, tokenize::TOKEN_TYPE::EQL, tokenize::TOKEN_TYPE::NEQ)) {
        Value *right;
        auto tokType = getCurTokType();
        addLGNode(eqExpr, tokType);
        addBGNode(eqExpr, parseRelExpr(&right));
        left = hirGenerator->createMidSymbol(left, right, tokType);
    }
    *symbol = left;
    return eqExpr;
}

// RelExp ::= AddExp { ('<' | '>' | '<=' | '>=') AddExp }
GNodeSPtr GParser::parseRelExpr(Value **symbol) {
    auto RelExpr = make_shared<BGNode>(G_NODE_TYPE::REL_EXPR);
    Value *left;
    addBGNode(RelExpr, parseExpr(&left));
    while (checkTokType(0, 4, tokenize::TOKEN_TYPE::LSS, tokenize::TOKEN_TYPE::LEQ,
                        tokenize::TOKEN_TYPE::GEQ, tokenize::TOKEN_TYPE::GRE)) {
        auto tokType = getCurTokType();
        Value *right;
        addLGNode(RelExpr, tokType);
        addBGNode(RelExpr, parseExpr(&right));
        left = hirGenerator->createMidSymbol(left, right, tokType);
    }
    *symbol = left;
    return RelExpr;
}

// Exp ::= AddExp 	*注：SysY 表达式是 int 型表达式*
// AddExp ::= MulExp { ('+' | '−') MulExp }
GNodeSPtr GParser::parseExpr(Value **symbol) {
    auto expr = make_shared<BGNode>(G_NODE_TYPE::EXPR);
    Value *left;
    addBGNode(expr, parseMulExpr(&left));
    while (checkTokType(0, 2, TOKEN_TYPE::PLUS, TOKEN_TYPE::MINU)) {
        auto tokType = getCurTokType();
        addLGNode(expr, tokType);
        Value *right;
        addBGNode(expr, parseMulExpr(&right));
        left = hirGenerator->createMidSymbol(left, right, tokType);
    }
    *symbol = left;
    return expr;
}

// MulExp ::= UnaryExp { ('*' | '/' | '%') UnaryExp }
GNodeSPtr GParser::parseMulExpr(Value **symbol) {
    auto mulExpr = make_shared<BGNode>(G_NODE_TYPE::MUL_EXPR);
    Value *left;
    addBGNode(mulExpr, parseUnaryExpr(&left));
    while (checkTokType(0, 3, tokenize::TOKEN_TYPE::MULT, tokenize::TOKEN_TYPE::DIV,
                        tokenize::TOKEN_TYPE::MOD)) {
        auto tokType = getCurTokType();
        addLGNode(mulExpr, tokType);
        Value *right;
        addBGNode(mulExpr, parseUnaryExpr(&right));
        left = hirGenerator->createMidSymbol(left, right, tokType);
    }
    *symbol = left;
    return mulExpr;
}

// LVal ::= **Ident** {'[' Exp ']'}
GNodeSPtr GParser::parseLVal(Value **symbol) {
    auto lVal = make_shared<BGNode>(G_NODE_TYPE::L_VAL);
    addLGNode(lVal, tokenize::TOKEN_TYPE::IDENFR);
    auto localVar = hirGenerator->getNearestSymbol(getLastTokName());
    assert(localVar != nullptr);
    while (checkTokType(0, tokenize::TOKEN_TYPE::LBRACK)) {
        Value *idx, *newVar;
        addLGNode(lVal, tokenize::TOKEN_TYPE::LBRACK);
        addBGNode(lVal, parseExpr(&idx));
        auto subType = dynamic_pointer_cast<ArrayType>(localVar->getType())->getElementType();
        if (localVar->isConstant() && idx->isConstant()) { // const fold for array here
            auto constVar = dynamic_cast<Constant *>(localVar);
            auto values = constVar->getValue<vector<int>>();
            int iIdx = dynamic_cast<Constant *>(idx)->getValue<int>();
            if (subType == IntegerType::object) {
                newVar = hirGenerator->declareImmediateInt(values[iIdx]);
            } else {
                // element must all be integer
                auto subNum = subType->getSizeOfType() / IntegerType::object->getSizeOfType();
                vector<int> newVals;
                newVals.reserve(subNum);
                for (auto i = 0; i < subNum; i++)
                    newVals.emplace_back(values[i + iIdx * subNum]);
                newVar = hirGenerator->declareConstant(subType, hirGenerator->genTmpSymbolName("const_arr"), newVals);
            }
        } else {
            newVar = hirGenerator->declareLocalVariable(subType, hirGenerator->genTmpSymbolName("var_arr"), true);
            hirGenerator->createAddressingHIR(dynamic_cast<Variable *>(newVar), localVar, idx);
        }
        localVar = newVar;
        addLGNode(lVal, tokenize::TOKEN_TYPE::RBRACK);
    }
    *symbol = localVar;
    return lVal;
}

// UnaryExp ::= PrimaryExp | **Ident** '(' [FuncRParams] ')' | UnaryOp UnaryExp
// UnaryOp ::= '+' | '−' | '!' 	*注：'!'仅出现在条件表达式中*
GNodeSPtr GParser::parseUnaryExpr(Value **symbol) {
    auto unaryExpr = make_shared<BGNode>(G_NODE_TYPE::UNARY_EXPR);
    if (checkTokType(0, tokenize::TOKEN_TYPE::IDENFR) &&
        checkTokType(1, tokenize::TOKEN_TYPE::LPARENT)) {
        addLGNode(unaryExpr, tokenize::TOKEN_TYPE::IDENFR);
        auto funcName = getLastTokName();
        auto callFunc = hirGenerator->getFunctionByName(funcName);
        vector<Value *> args = {};
        string format;
        addLGNode(unaryExpr, tokenize::TOKEN_TYPE::LPARENT);
        if (!checkTokType(0, tokenize::TOKEN_TYPE::RPARENT)) {
            if (funcName == "putf") {
                addLGNode(unaryExpr, tokenize::TOKEN_TYPE::STRCON);
                format = getLastTokName();
                if (checkTokType(0, tokenize::TOKEN_TYPE::COMMA)) {
                    addLGNode(unaryExpr, tokenize::TOKEN_TYPE::COMMA);
                    addBGNode(unaryExpr, parseFuncRParams(args));
                }
            } else {
                addBGNode(unaryExpr, parseFuncRParams(args));
            }
        }
        if (funcName == "putf") {
            hirGenerator->createPutfHIR(format, args);
            *symbol = nullptr;
        } else {
            if (funcName == "starttime" || funcName == "stoptime") {
                auto line = hirGenerator->declareImmediateInt(getCurTokLineNum());
                args.emplace_back(line);
            }
            if (callFunc->getType()->getReturnType() == VoidType::object) {
                hirGenerator->createCallHIR(funcName, args);
                *symbol = nullptr;
            } else {
                Variable *var = hirGenerator->declareLocalVariable(IntegerType::object,
                                                                   hirGenerator->genTmpSymbolName("ret"),
                                                                   false);
                hirGenerator->createCallHIR(var, funcName, args);
                *symbol = var;
            }
        }
        addLGNode(unaryExpr, tokenize::TOKEN_TYPE::RPARENT);
    } else if (checkTokType(0, 3, TOKEN_TYPE::PLUS, TOKEN_TYPE::MINU, TOKEN_TYPE::NOT)) {
        auto tokType = getCurTokType();
        addLGNode(unaryExpr, tokType);
        Value *tmpVal;
        addBGNode(unaryExpr, parseUnaryExpr(&tmpVal));
        *symbol = hirGenerator->createUniSymbol(tmpVal, tokType);
    } else {
        addBGNode(unaryExpr, parsePrimaryExpr(symbol));
    }
    return unaryExpr;
}

// PrimaryExp ::= '(' Exp ')' | LVal | **IntConst**
GNodeSPtr GParser::parsePrimaryExpr(Value **symbol) {
    auto primaryExp = make_shared<BGNode>(G_NODE_TYPE::PRIMARY_EXPR);
    if (checkTokType(0, tokenize::TOKEN_TYPE::LPARENT)) {
        addLGNode(primaryExp, tokenize::TOKEN_TYPE::LPARENT);
        addBGNode(primaryExp, parseExpr(symbol));
        addLGNode(primaryExp, tokenize::TOKEN_TYPE::RPARENT);
    } else if (checkTokType(0, tokenize::TOKEN_TYPE::INTCON)) {
        *symbol = hirGenerator->declareImmediateInt(getCurTokVal());
        addLGNode(primaryExp, tokenize::TOKEN_TYPE::INTCON);
    } else {
        addBGNode(primaryExp, parseLVal(symbol));
    }
    return primaryExp;
}

// FuncRParams ::= Exp { ',' Exp }
GNodeSPtr GParser::parseFuncRParams(vector<Value *> &args) {
    auto funcRParams = make_shared<BGNode>(G_NODE_TYPE::FUNC_R_PARAM_TABLE);
    Value *curArg;
    addBGNode(funcRParams, parseExpr(&curArg));
    args.emplace_back(curArg);
    while (checkTokType(0, tokenize::TOKEN_TYPE::COMMA)) {
        addLGNode(funcRParams, tokenize::TOKEN_TYPE::COMMA);
        addBGNode(funcRParams, parseExpr(&curArg));
        args.emplace_back(curArg);
    }
    return funcRParams;
}

shared_ptr<Type> GParser::dims2Type(vector<int> &dims) {
    if (dims.empty()) {
        return IntegerType::object;
    } else {
        int idx = dims.size() - 1;
        shared_ptr<Type> curType = IntegerType::object;
        do {
            curType = make_shared<ArrayType>(curType, dims[idx]);
            idx--;
        } while (idx >= 0);
        return curType;
    }
}

int GParser::constValInit(const GNodeSPtr &initNode, vector<int> &dims, vector<int> &values, int layerIdx) {
    int totNum = 1;
    int curNum = 0;
    Value *pVal;
    for (auto i = layerIdx; i < dims.size(); i++)
        totNum *= dims[i];
    addLGNode(initNode, tokenize::TOKEN_TYPE::LBRACE);
    while (!checkTokType(0, tokenize::TOKEN_TYPE::RBRACE)) {
        if (checkTokType(0, tokenize::TOKEN_TYPE::LBRACE)) {
            curNum += constValInit(initNode, dims, values, layerIdx + 1);
        } else {
            addBGNode(initNode, parseConstExpr(&pVal));
            auto cpVal = dynamic_cast<Constant *>(pVal);
            values.emplace_back(cpVal->getValue<int>());
            curNum++;
        }
        if (checkTokType(0, tokenize::TOKEN_TYPE::COMMA)) {
            addLGNode(initNode, tokenize::TOKEN_TYPE::COMMA);
        }
    }
    addLGNode(initNode, tokenize::TOKEN_TYPE::RBRACE);
    while (curNum < totNum) {
        values.emplace_back(0);
        curNum++;
    }
    return curNum;
}

int GParser::valInit(const GNodeSPtr &initNode, vector<int> &dims, vector<Value *> &values, int layerIdx) {
    int totNum = 1;
    int curNum = 0;
    Value *pVal;
    for (auto i = layerIdx; i < dims.size(); i++)
        totNum *= dims[i];
    addLGNode(initNode, tokenize::TOKEN_TYPE::LBRACE);
    while (!checkTokType(0, tokenize::TOKEN_TYPE::RBRACE)) {
        if (checkTokType(0, tokenize::TOKEN_TYPE::LBRACE)) {
            curNum += valInit(initNode, dims, values, layerIdx + 1);
        } else {
            addBGNode(initNode, parseExpr(&pVal));
            values.emplace_back(pVal);
            curNum++;
        }
        if (checkTokType(0, tokenize::TOKEN_TYPE::COMMA)) {
            addLGNode(initNode, tokenize::TOKEN_TYPE::COMMA);
        }
    }
    addLGNode(initNode, tokenize::TOKEN_TYPE::RBRACE);
    while (curNum < totNum) {
        auto val = hirGenerator->declareImmediateInt(0);
        values.emplace_back(val);
        curNum++;
    }
    return curNum;
}

void GParser::localArrAssign(Variable *variable, const std::any &value, vector<int> &dims) {
    auto pValues = std::any_cast<vector<Value *>>(value);
    auto type = variable->getType();
    vector<int> intVals;
    for (auto &pValue : pValues) {
        if (pValue->isConstant()) {
            intVals.emplace_back(dynamic_cast<Constant *>(pValue)->getValue<int>());
        }
    }
    if (intVals.size() == pValues.size()) { // all constant
        auto tmpArr = hirGenerator->declareConstant(
                type, hirGenerator->genTmpSymbolName("const_arr"), intVals);
        hirGenerator->createAssignHIR(variable, tmpArr);
    } else {
        int idx = 0;
        vector<int> fillDims = {0};
        vector<Variable *> stack = {variable};
        while (idx < pValues.size()) {
            if (stack.size() == dims.size()) { // last dim
                auto dim = dims.back();
                auto baseAddress = stack.back();
                intVals.clear();
                for (auto i = 0; i < dim; i++) {
                    if (pValues[idx + i]->isConstant()) {
                        intVals.emplace_back(dynamic_cast<Constant *>(pValues[idx + i])->getValue<int>());
                    }
                }
                if (intVals.size() == dim) {
                    auto tmpArr = hirGenerator->declareConstant(
                            baseAddress->getType(), hirGenerator->genTmpSymbolName("const_arr"), intVals);
                    hirGenerator->createAssignHIR(baseAddress, tmpArr);
                } else {
                    for (auto i = 0; i < dim; i++) {
                        auto offset = hirGenerator->declareImmediateInt(i);
                        auto tmpAddrName = hirGenerator->genTmpSymbolName("addr");
                        auto tmpAddress = hirGenerator->declareLocalVariable(IntegerType::object,
                                                                             tmpAddrName,
                                                                             true);
                        hirGenerator->createAddressingHIR(tmpAddress, baseAddress, offset);
                        hirGenerator->createAssignHIR(tmpAddress, pValues[idx + i]);
                    }
                }
                idx += dim;
                stack.pop_back();
                fillDims.pop_back();
                if (!fillDims.empty()) {
                    fillDims[fillDims.size() - 1]++;
                }
            } else {
                if (fillDims[stack.size() - 1] < dims[stack.size() - 1]) {
                    auto curType = dynamic_pointer_cast<ArrayType>(
                            stack.back()->getType())->getElementType();
                    auto tmpAddrName = hirGenerator->genTmpSymbolName("addr");
                    auto relVar = hirGenerator->declareLocalVariable(curType, tmpAddrName, true);
                    auto offset = hirGenerator->declareImmediateInt(fillDims[stack.size() - 1]);
                    hirGenerator->createAddressingHIR(relVar, stack.back(), offset);
                    stack.emplace_back(relVar);
                    fillDims.emplace_back(0);
                } else {
                    stack.pop_back();
                    fillDims.pop_back();
                    if (!fillDims.empty()) {
                        fillDims[fillDims.size() - 1]++;
                    }
                }
            }
        }
    }
}

