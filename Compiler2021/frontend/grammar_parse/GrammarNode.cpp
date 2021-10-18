//
// Created by tcyhost on 2021/6/30.
//

#include "GrammarNode.h"

using namespace frontend::grammar_parse;

std::string GNode::toString() {
    return "<virtual func> no illegal str";
}

std::string GNode::toOutput() {
    return "<virtual func> no illegal output";
}

std::string LGNode::toString() {
    return this->tok.toOutput();
}

std::string LGNode::toOutput() {
    return this->tok.toOutput();
}

std::string BGNode::toOutput() {
    string outStr;
    for (auto &iter : children) {
        outStr += iter->toOutput() + "\n";
    }
    outStr += this->toString();
    return outStr;
}

std::string BGNode::toString() {
    switch (this->getType()) {
        case G_NODE_TYPE::COMP_UNIT:
            return "<compUnit>";
        case G_NODE_TYPE::DECL:
            return "<decl>";
        case G_NODE_TYPE::FUNC_DEF:
            return "<funcDef>";
        case G_NODE_TYPE::CONST_DECL:
            return "<constDef>";
        case G_NODE_TYPE::VAR_DECL:
            return "<varDecl>";
        case G_NODE_TYPE::CONST_DEF:
            return "<constDef>";
        case G_NODE_TYPE::VAR_DEF:
            return "<varDef>";
        case G_NODE_TYPE::CONST_INIT:
            return "<constInitVal>";
        case G_NODE_TYPE::VAR_INIT:
            return "<varInitVal>";
        case G_NODE_TYPE::CONST_EXPR:
            return "<constExp>";
        case G_NODE_TYPE::FUNC_TYPE:
            return "<funcType>";
        case G_NODE_TYPE::FUNC_F_PARAM:
            return "<funcFParam>";
        case G_NODE_TYPE::FUNC_F_PARAM_TABLE:
            return "<funcFParams>";
        case G_NODE_TYPE::BLOCK:
            return "<block>";
        case G_NODE_TYPE::BLOCK_ITEM:
            return "<blockItem>";
        case G_NODE_TYPE::STMT:
            return "<stmt>";
        case G_NODE_TYPE::IF_STMT:
            return "<if_stmt>";
        case G_NODE_TYPE::WHILE_STMT:
            return "<while_stmt>";
        case G_NODE_TYPE::COND:
            return "<cond>";
        case G_NODE_TYPE::EXPR:
            return "<expr>";
        case G_NODE_TYPE::ADD_EXPR:
            return "<addExpr>";
        case G_NODE_TYPE::MUL_EXPR:
            return "<mulExpr>";
        case G_NODE_TYPE::UNARY_EXPR:
            return "<unaryExpr>";
        case G_NODE_TYPE::PRIMARY_EXPR:
            return "<primaryExpr>";
        case G_NODE_TYPE::LOR_EXPR:
            return "<LOrExpr>";
        case G_NODE_TYPE::LAND_EXPR:
            return "<LAndExpr>";
        case G_NODE_TYPE::EQ_EXPR:
            return "<EqExpr>";
        case G_NODE_TYPE::REL_EXPR:
            return "<RelExpr>";
        case G_NODE_TYPE::L_VAL:
            return "<lVal>";
        case G_NODE_TYPE::FUNC_R_PARAM_TABLE:
            return "<funcRParamTable>";
        case G_NODE_TYPE::UNARY_OP:
            return "<unaryOp>";
        case G_NODE_TYPE::TOKEN:
            return "<token but this is not supposed to appear>";
        default:
            return "<unknown ele>";
    }
}
