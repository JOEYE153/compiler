//
// Created by tcyhost on 2021/6/30.
//

#ifndef COMPILER2021_GPARSER_H
#define COMPILER2021_GPARSER_H

#include "../tokenize/Token.h"
#include "../errorhandle/ErrorHandler.h"
#include "GrammarNode.h"
#include "../generate/HIRGenerator.h"

#include <memory>
#include <utility>
#include <vector>
#include <array>

namespace frontend::grammar_parse {
    using std::shared_ptr;
    using std::string;
    using std::vector;
    using std::array;
    using std::min;
    using std::max;
    using std::make_shared;
    using namespace frontend::tokenize;
    using namespace frontend::errorhandle;
    using namespace frontend::generate;

    class GParser {
        vector<Token> tokens;
        size_t curTokIndex;

        GNodeSPtr root = nullptr;
        ErrorHandlerSPtr errorHandler;
        HIRGeneratorUPtr hirGenerator;

        inline void readNextToken() { curTokIndex = min(curTokIndex + 1, tokens.size()); }

        inline TOKEN_TYPE getCurTokType() { return tokens[curTokIndex].getType(); }

        inline int getCurTokLineNum() { return tokens[curTokIndex].getLineNum(); }

        inline int getCurTokVal() { return tokens[curTokIndex].getNumVal(); }

        inline string getLastTokName() { return tokens[curTokIndex - 1].getStrVal(); }

        bool checkTokType(size_t pIdx, TOKEN_TYPE type);

        bool checkTokType(size_t pIdx, size_t type_cnt, ...);

        bool findTok(TOKEN_TYPE trg, TOKEN_TYPE end);

        static shared_ptr<Type> dims2Type(vector<int> &dims);

        int constValInit(const GNodeSPtr &initNode, vector<int> &dims, vector<int> &values, int layerIdx);

        int valInit(const GNodeSPtr &initNode, vector<int> &dims, vector<Value *> &values, int layerIdx);

        void localArrAssign(Variable* variable, const std::any& value, vector<int> &dims);

        void error();

        static inline void addBGNode(const GNodeSPtr &par, GNodeSPtr child) {
            par->addChild(std::move(child));
        };

        void addLGNode(const GNodeSPtr &par, TOKEN_TYPE type);

        GNodeSPtr parseConstDecl();

        GNodeSPtr parseVarDecl();

        GNodeSPtr parseConstDef();

        GNodeSPtr parseConstInitVal(std::any &value, vector<int> &dims);

        GNodeSPtr parseVarDef();

        GNodeSPtr parseVarInitVal(std::any &value, vector<int> &dims);

        GNodeSPtr parseFuncDef();

        GNodeSPtr parseFuncFParams(vector<shared_ptr<Type>> &args_type, vector<string> &argNames);

        GNodeSPtr parseFuncFParam(shared_ptr<Type> &argType, string &argName);

        GNodeSPtr parseBlock();

        GNodeSPtr parseStmt();

        GNodeSPtr parseIfStmt();

        GNodeSPtr parseWhileStmt();

        GNodeSPtr parseCond(BasicBlock **trueBlock, BasicBlock **falseBlock);

        GNodeSPtr parseLOrExpr(BasicBlock **trueBlock, BasicBlock **falseBlock);

        GNodeSPtr parseLAndExpr(BasicBlock **trueBlock, BasicBlock **falseBlock);

        GNodeSPtr parseEqExpr(Value **symbol);

        GNodeSPtr parseRelExpr(Value **symbol);

        GNodeSPtr parseConstExpr(Value **symbol);

        GNodeSPtr parseExpr(Value **symbol);

        GNodeSPtr parseMulExpr(Value **symbol);

        GNodeSPtr parseUnaryExpr(Value **symbol);

        GNodeSPtr parsePrimaryExpr(Value **symbol);

        GNodeSPtr parseFuncRParams(vector<Value *> &args);

        GNodeSPtr parseLVal(Value **symbol);

    public:
        GParser(vector<Token> &tokens, ErrorHandlerSPtr &errorHandler, const string &moduleName) {
            this->tokens = tokens;
            curTokIndex = 0;
            root = make_shared<BGNode>(G_NODE_TYPE::COMP_UNIT);
            this->errorHandler = errorHandler;
            this->hirGenerator = make_unique<HIRGenerator>(moduleName);
        }

        ~GParser() = default;

        GNodeSPtr parseCompUnit();

        shared_ptr<Module> getModule() { return hirGenerator->getModule(); }
    };
}
#endif //COMPILER2021_GPARSER_H
