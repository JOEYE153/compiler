//
// Created by tcyhost on 2021/6/28.
//

#ifndef COMPILER2021_ERROR_H
#define COMPILER2021_ERROR_H

#include <string>

namespace frontend::errorhandle {
    using std::string;

    enum class ERROR_TYPE {
        ILLEGAL_TOKEN,
        REDEF_IDENFR,
        UNDEF_IDENFR,
        PARA_NUM_UNALIGN,
        PARA_TYPE_UNALIGN,
        ILLEGAL_COND_TYPE,
        VOID_RET_UNALIGN,
        RET_RET_UNALIGN,
        IDX_TYPE_UNALIGN,
        CONST_VAL_CHANGED,
        SEMICN_MISS,
        RPARENT_MISS,
        RBRACK_MISS,
        ARR_INIT_UNALIGN,
        CONST_TYPE_UNALIGN,
        DEFAULT_MISS,
        UNKNOWN_ERROR
    };

    class Error {
        int lineNum;
        ERROR_TYPE type;

    public:
        Error(int _lineNum, ERROR_TYPE _type) {
            this->lineNum = _lineNum;
            this->type = _type;
        }

        [[nodiscard]] int getLineNum() const;

        [[nodiscard]] string toOutput() const;
    };
}

#endif //COMPILER2021_ERROR_H
