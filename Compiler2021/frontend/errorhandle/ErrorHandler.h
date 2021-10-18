//
// Created by tcyhost on 2021/6/28.
//

#ifndef COMPILER2021_ERRORHANDLER_H
#define COMPILER2021_ERRORHANDLER_H

#include "Error.h"
#include <vector>
#include <memory>

namespace frontend::errorhandle {
    using std::vector;
    using std::shared_ptr;

    class ErrorHandler {
        vector<Error> errors;

    public:
        ErrorHandler() = default;

        [[nodiscard]] int getErrCnt();

        void raiseErr(int lineNum, ERROR_TYPE errType) {
            this->errors.emplace_back(lineNum, errType);
        }

        [[nodiscard]] string toOutput(); // 输出按行号排序

    };

    typedef shared_ptr<ErrorHandler> ErrorHandlerSPtr;
}
#endif //COMPILER2021_ERRORHANDLER_H
