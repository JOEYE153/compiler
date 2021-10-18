//
// Created by 陈思言 on 2021/5/21.
//

#include <sstream>
#include <cassert>

#include "Value.h"

using std::stringstream;
using std::to_string;

string Assignment::getNameAndIdString() const {
    stringstream ss;
    ss << '%' << id;
    if (!name.empty()) {
        ss << '{' << name << '}';
    }
    return ss.str();
}

string Constant::getValueString() const {
    stringstream ss;
    switch (getType()->getId()) {
        case Type::ID::VOID:
            ss << "[[void]]";
            break;
        case Type::ID::INTEGER:
        case Type::ID::POINTER:
            ss << getValue<int>();
            break;
        case Type::ID::BOOLEAN:
            ss << (getValue<int>() ? "true" : "false");
            break;
        case Type::ID::ARRAY: {
            ss << "{ ";
            auto values = getValue<vector<int>>();
            assert(!values.empty());
            ss << to_string(values[0]);
            for (auto i = 1; i < values.size(); i++)
                ss << ", " << to_string(values[i]);
            ss << " }";
            break;
        }
        case Type::ID::FUNCTION:
            ss << getValue<string>();
            break;
    }
    return ss.str();
}
