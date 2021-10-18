//
// Created by tcyhost on 2021/6/28.
//

#include "Error.h"

using std::to_string;
using namespace frontend::errorhandle;

int Error::getLineNum() const {
    return lineNum;
}

std::string Error::toOutput() const {
    string out = to_string(this->lineNum) + " ";
    switch (this->type) {
        case ERROR_TYPE::ILLEGAL_TOKEN:
            out += "a";
            break;
        case ERROR_TYPE::REDEF_IDENFR:
            out += "b";
            break;
        case ERROR_TYPE::UNDEF_IDENFR:
            out += "c";
            break;
        case ERROR_TYPE::PARA_NUM_UNALIGN:
            out += "d";
            break;
        case ERROR_TYPE::PARA_TYPE_UNALIGN:
            out += "e";
            break;
        case ERROR_TYPE::ILLEGAL_COND_TYPE:
            out += "f";
            break;
        case ERROR_TYPE::VOID_RET_UNALIGN:
            out += "g";
            break;
        case ERROR_TYPE::RET_RET_UNALIGN:
            out += "h";
            break;
        case ERROR_TYPE::IDX_TYPE_UNALIGN:
            out += "i";
            break;
        case ERROR_TYPE::CONST_VAL_CHANGED:
            out += "j";
            break;
        case ERROR_TYPE::SEMICN_MISS:
            out += "k";
            break;
        case ERROR_TYPE::RPARENT_MISS:
            out += "l";
            break;
        case ERROR_TYPE::RBRACK_MISS:
            out += "m";
            break;
        case ERROR_TYPE::ARR_INIT_UNALIGN:
            out += "n";
            break;
        case ERROR_TYPE::CONST_TYPE_UNALIGN:
            out += "o";
            break;
        case ERROR_TYPE::DEFAULT_MISS:
            out += "p";
            break;
        case ERROR_TYPE::UNKNOWN_ERROR:
            out += "<unknown error has occurred>";
            break;
        default:
            out += "error uninitialized!!!";
            break;
    }
    return out;
}


