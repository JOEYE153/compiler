//
// Created by tcyhost on 2021/6/28.
//

#ifndef COMPILER2021_TOKEN_H
#define COMPILER2021_TOKEN_H

#include <vector>
#include <string>

namespace frontend::tokenize {
    using std::string;

    enum class TOKEN_TYPE {
        IDENFR,

        INTCON, STRCON,

        CONSTTK, INTTK, VOIDTK, IFTK, ELSETK,
        WHILETK, BREAKTK, CONTINUETK, RETURNTK,

        PLUS, MINU, MULT, DIV, MOD,
        LSS, LEQ, GRE, GEQ, EQL, NEQ, AND, OR, NOT,
        ASSIGN,

        SEMICN, COMMA,
        LPARENT, RPARENT,
        LBRACK, RBRACK,
        LBRACE, RBRACE,

        EOFTK, UNKNOWNTK
    };

    class Token {
        TOKEN_TYPE type;
        string strVal;
        int numVal;
        int lineNum; // 行号

    public:
        Token() {
            this->type = TOKEN_TYPE::UNKNOWNTK;
            this->lineNum = -1;
            this->numVal = 0;
        }

        explicit Token(int _lineNum) {
            this->lineNum = _lineNum;
            this->numVal = 0;
            this->type = TOKEN_TYPE::UNKNOWNTK;
        }

        explicit Token(TOKEN_TYPE _type) : type(_type) {
            this->numVal = 0;
            this->lineNum = -1;
        }

        Token(TOKEN_TYPE _type, string _str, int _lineNum) : type(_type), strVal(std::move(_str)), lineNum(_lineNum) {
            this->numVal = 0;
        }

        Token(TOKEN_TYPE _type, string _str, int _num, int _lineNum) {
            this->type = _type;
            this->strVal = std::move(_str);
            this->numVal = _num;
            this->lineNum = _lineNum;
        }

        [[nodiscard]] TOKEN_TYPE getType() const;

        [[nodiscard]] int getNumVal() const;

        [[nodiscard]] int getLineNum() const;

        [[nodiscard]] string getStrVal() const;

        [[nodiscard]] string toOutput() const;

        void setType(TOKEN_TYPE _type) {
            this->type = _type;
        }

        void setNumVal(int _num) {
            this->numVal = _num;
        }

        void setLineNum(int _lineNum) {
            this->lineNum = _lineNum;
        }

        void setStr(string _s) {
            this->strVal = std::move(_s);
        }
    };

}

#endif //COMPILER2021_TOKEN_H
