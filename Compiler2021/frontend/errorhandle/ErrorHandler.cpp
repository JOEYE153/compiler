//
// Created by tcyhost on 2021/6/28.
//

#include "ErrorHandler.h"
#include <algorithm>

using std::sort;
using namespace frontend::errorhandle;

int ErrorHandler::getErrCnt() {
    return errors.size();
}

bool cmp(Error e1, Error e2) {
    return e1.getLineNum() < e2.getLineNum();
}

std::string ErrorHandler::toOutput() {
    sort(errors.begin(), errors.end(), cmp);
    string out;
    for (auto &error : this->errors) {
        out += error.toOutput() + "\n";
    }
    return out;
}
