//
// Created by tcyhost on 2021/6/28.
//

#include "Tokenizer.h"
#include <cassert>
#include <iostream>
#include <stdexcept>

using std::stoi;
using std::runtime_error;
using namespace frontend::tokenize;

Tokenizer::Tokenizer(const string &inputFileName, ErrorHandlerSPtr &errHandler) {
    curC = 0;
    inFile.open(inputFileName);
    if (!inFile.is_open()) {
        throw runtime_error("Cannot open file " + inputFileName);
    }
    curLineNum = 1;
    this->errHandler = errHandler;
    makeDict();
}

void Tokenizer::makeDict() {
    tokenDict["const"] = TOKEN_TYPE::CONSTTK;
    tokenDict["int"] = TOKEN_TYPE::INTTK;
    tokenDict["void"] = TOKEN_TYPE::VOIDTK;
    tokenDict["if"] = TOKEN_TYPE::IFTK;
    tokenDict["else"] = TOKEN_TYPE::ELSETK;
    tokenDict["while"] = TOKEN_TYPE::WHILETK;
    tokenDict["break"] = TOKEN_TYPE::BREAKTK;
    tokenDict["continue"] = TOKEN_TYPE::CONTINUETK;
    tokenDict["return"] = TOKEN_TYPE::RETURNTK;
}

int Tokenizer::toNum(string s) {
    assert(!s.empty());
    int numVal;
    if (s[0] != '0') {
        numVal = (int)stoll(s, nullptr, 10);
    } else {
        if (s.size() > 1 && (s[1] == 'X') || (s[1] == 'x')) {
            numVal = (int)stoll(s, nullptr, 16);
        } else {
            numVal = (int)stoll(s, nullptr, 8);
        }
    }
    return numVal;
}

/*
 * [_a-zA-Z]
 */
bool Tokenizer::isNonDigit(char c) {
    return isalpha(c) || (c == '_');
}

/*
 * [_a-zA-Z0-9]
 */
bool Tokenizer::isAlphaNum(char c) {
    return isalnum(c) || (c == '_');
}

std::vector<Token> &Tokenizer::getTokens() {
    getNextChar();
    Token newTok;
    do {
        newTok = nextToken();
        // std::cout << newTok.toOutput() << std::endl;
        if (newTok.getType() == tokenize::TOKEN_TYPE::UNKNOWNTK) continue;
        tokens.emplace_back(newTok);
    } while (newTok.getType() != TOKEN_TYPE::EOFTK);
    inFile.close();
    return tokens;
}

Token Tokenizer::nextToken() {
    string curTokStr;
    curTokStr.clear();
    while (isspace(curC)) {
        if (curC == '\n')
            curLineNum++;
        getNextChar();
    }
    if (isNonDigit(curC)) { // identifier or tokenize
        do {
            curTokStr.append(1, curC);
            getNextChar();
        } while (isAlphaNum(curC));
        auto it = tokenDict.find(curTokStr);
        if (it != tokenDict.end()) { // basic tokenize type
            return Token(it->second, curTokStr, curLineNum);
        } else { // identifier
            return Token(TOKEN_TYPE::IDENFR, curTokStr, curLineNum);
        }
    } else if (isdigit(curC)) { // number
        if (curC == '0') {
            curTokStr.append(1, curC);
            getNextChar();
            if (curC == 'x' || curC == 'X') { // hexadecimal
                do {
                    curTokStr.append(1, curC);
                    getNextChar();
                } while (isHexNum(curC));
            } else if (isOctNum(curC)) { // octal
                do {
                    curTokStr.append(1, curC);
                    getNextChar();
                } while (isOctNum(curC));
            }
        } else { // decimal
            do {
                curTokStr.append(1, curC);
                getNextChar();
            } while (isdigit(curC));
        }
        return Token(TOKEN_TYPE::INTCON, curTokStr, toNum(curTokStr), curLineNum);
    } else {
        Token newTok(curLineNum);
        switch (curC) {
            case '"':
                getNextChar();
                while (curC != '"' && curC != '\n') { // also check for '\n' here
                    curTokStr.append(1, curC);
                    getNextChar();
                }
                if (curC == '"') {
                    newTok.setStr(curTokStr);
                    newTok.setType(tokenize::TOKEN_TYPE::STRCON);
                } else {
                    errHandler->raiseErr(curLineNum, errorhandle::ERROR_TYPE::ILLEGAL_TOKEN);
                }
                break;
            case '+':
                newTok.setType(tokenize::TOKEN_TYPE::PLUS);
                break;
            case '-':
                newTok.setType(tokenize::TOKEN_TYPE::MINU);
                break;
            case '*':
                newTok.setType(tokenize::TOKEN_TYPE::MULT);
                break;
            case '/':
                getNextChar();
                if (curC == '/') {
                    do {
                        getNextChar();
                    } while (curC != '\n');
                    curLineNum++;
                } else if (curC == '*') {
                    getNextChar();
                    if (curC == '\n') curLineNum++;
                    char lastC;
                    do {
                        lastC = curC;
                        getNextChar();
                        if (curC == '\n') curLineNum++;
                    } while (!(lastC == '*' && curC == '/'));
                } else {
                    putBackChar();
                    newTok.setType(tokenize::TOKEN_TYPE::DIV);
                }
                break;
            case '%':
                newTok.setType(tokenize::TOKEN_TYPE::MOD);
                break;
            case '=':
                if (peekNextChar() == '=') {
                    getNextChar();
                    newTok.setType(tokenize::TOKEN_TYPE::EQL);
                } else {
                    newTok.setType(tokenize::TOKEN_TYPE::ASSIGN);
                }
                break;
            case '<':
                if (peekNextChar() == '=') {
                    getNextChar();
                    newTok.setType(tokenize::TOKEN_TYPE::LEQ);
                } else {
                    newTok.setType(tokenize::TOKEN_TYPE::LSS);
                }
                break;
            case '>':
                if (peekNextChar() == '=') {
                    getNextChar();
                    newTok.setType(tokenize::TOKEN_TYPE::GEQ);
                } else {
                    newTok.setType(tokenize::TOKEN_TYPE::GRE);
                }
                break;
            case '!':
                if (peekNextChar() == '=') {
                    getNextChar();
                    newTok.setType(tokenize::TOKEN_TYPE::NEQ);
                } else {
                    newTok.setType(tokenize::TOKEN_TYPE::NOT);
                }
                break;
            case '&':
                getNextChar();
                if (curC == '&') {
                    newTok.setType(tokenize::TOKEN_TYPE::AND);
                } else {
                    errHandler->raiseErr(curLineNum, errorhandle::ERROR_TYPE::ILLEGAL_TOKEN);
                }
                break;
            case '|':
                getNextChar();
                if (curC == '|') {
                    newTok.setType(tokenize::TOKEN_TYPE::OR);
                } else {
                    errHandler->raiseErr(curLineNum, errorhandle::ERROR_TYPE::ILLEGAL_TOKEN);
                }
                break;
            case ';':
                newTok.setType(tokenize::TOKEN_TYPE::SEMICN);
                break;
            case ',':
                newTok.setType(tokenize::TOKEN_TYPE::COMMA);
                break;
            case '(':
                newTok.setType(tokenize::TOKEN_TYPE::LPARENT);
                break;
            case ')':
                newTok.setType(tokenize::TOKEN_TYPE::RPARENT);
                break;
            case '[':
                newTok.setType(tokenize::TOKEN_TYPE::LBRACK);
                break;
            case ']':
                newTok.setType(tokenize::TOKEN_TYPE::RBRACK);
                break;
            case '{':
                newTok.setType(tokenize::TOKEN_TYPE::LBRACE);
                break;
            case '}':
                newTok.setType(tokenize::TOKEN_TYPE::RBRACE);
                break;
            case EOF:
                newTok.setType(tokenize::TOKEN_TYPE::EOFTK);
                break;
            default:
                errHandler->raiseErr(curLineNum, errorhandle::ERROR_TYPE::ILLEGAL_TOKEN);
                break;
        }
        getNextChar();
        return newTok;
    }
}

bool Tokenizer::isOctNum(char c) {
    return c >= '0' && c <= '7';
}

bool Tokenizer::isHexNum(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}
