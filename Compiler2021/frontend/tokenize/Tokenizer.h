//
// Created by tcyhost on 2021/6/28.
//

#ifndef COMPILER2021_TOKENIZER_H
#define COMPILER2021_TOKENIZER_H

#include "Token.h"
#include "../errorhandle/ErrorHandler.h"

#include <map>
#include <fstream>
#include <cstring>

namespace frontend::tokenize {
    using namespace frontend::errorhandle;
    using std::ifstream;
    using std::map;

    class Tokenizer {
        map<string, TOKEN_TYPE> tokenDict;
        ifstream inFile;
        ErrorHandlerSPtr errHandler;
        int curC;
        int curLineNum;
        vector<Token> tokens;

        void makeDict();

        Token nextToken();

        inline void getNextChar() { curC = inFile.get(); };

        inline int peekNextChar() { return inFile.peek(); };

        inline void putBackChar() { inFile.unget(); };

    public:
        Tokenizer(const string &inputFileName, ErrorHandlerSPtr &errHandler);

        vector<Token> &getTokens();

        static int toNum(string s);

        static bool isNonDigit(char c);

        static bool isAlphaNum(char c);

        static bool isOctNum(char c);

        static bool isHexNum(char c);
    };
}

#endif //COMPILER2021_TOKENIZER_H
