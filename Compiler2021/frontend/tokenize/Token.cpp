//
// Created by tcyhost on 2021/6/28.
//

#include "Token.h"
#include <string>

using namespace frontend::tokenize;

TOKEN_TYPE Token::getType() const {
    return this->type;
}

int Token::getNumVal() const {
    return this->numVal;
}

int Token::getLineNum() const {
    return this->lineNum;
}

string Token::getStrVal() const {
    return this->strVal;
}

string Token::toOutput() const {
    switch (this->type) {
        case TOKEN_TYPE::IDENFR:
            return "IDENFR\t" + this->strVal;
        case TOKEN_TYPE::INTCON:
            return "INTCON\t" + this->strVal;
        case TOKEN_TYPE::STRCON:
            return "STRCON\t" + this->strVal;
        case TOKEN_TYPE::CONSTTK:
            return "CONSTTK\t" + this->strVal;
        case TOKEN_TYPE::INTTK:
            return "INTTK\t" + this->strVal;
        case TOKEN_TYPE::VOIDTK:
            return "VOIDTK\t" + this->strVal;
        case TOKEN_TYPE::IFTK:
            return "IFTK\t" + this->strVal;
        case TOKEN_TYPE::ELSETK:
            return "ELSETK\t" + this->strVal;
        case TOKEN_TYPE::WHILETK:
            return "WHILETK\t" + this->strVal;
        case TOKEN_TYPE::BREAKTK:
            return "BREAKTK\t" + this->strVal;
        case TOKEN_TYPE::CONTINUETK:
            return "CONTINUETK\t" + this->strVal;
        case TOKEN_TYPE::RETURNTK:
            return "RETURNTK\t" + this->strVal;

        case TOKEN_TYPE::PLUS:
            return "PLUS\t+";
        case TOKEN_TYPE::MINU:
            return "MINU\t-";
        case TOKEN_TYPE::MULT:
            return "MULT\t*";
        case TOKEN_TYPE::DIV:
            return "DIV\t/";
        case TOKEN_TYPE::MOD:
            return "MOD\t%";
        case TOKEN_TYPE::ASSIGN:
            return "ASSIGN\t=";
        case TOKEN_TYPE::EQL:
            return "EQL\t==";
        case TOKEN_TYPE::LSS:
            return "LSS\t<";
        case TOKEN_TYPE::LEQ:
            return "LEQ\t<=";
        case TOKEN_TYPE::GRE:
            return "GRE\t>";
        case TOKEN_TYPE::GEQ:
            return "GEQ\t>=";
        case TOKEN_TYPE::NEQ:
            return "NEQ\t!=";
        case TOKEN_TYPE::NOT:
            return "NOT\t!";
        case TOKEN_TYPE::AND:
            return "AND\t&&";
        case TOKEN_TYPE::OR:
            return "OR\t||";

        case TOKEN_TYPE::SEMICN:
            return "SEMICN\t;";
        case TOKEN_TYPE::COMMA:
            return "COMMA\t,";
        case TOKEN_TYPE::LPARENT:
            return "LPARENT\t(";
        case TOKEN_TYPE::RPARENT:
            return "RPARENT\t)";
        case TOKEN_TYPE::LBRACK:
            return "LBRACK\t[";
        case TOKEN_TYPE::RBRACK:
            return "RBRACK\t]";
        case TOKEN_TYPE::LBRACE:
            return "LBRACE\t{";
        case TOKEN_TYPE::RBRACE:
            return "RBRACE\t}";
        case TOKEN_TYPE::EOFTK:
            return "EOFTK";
        default:
            return "";
    }
}


