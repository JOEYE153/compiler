//
// Created by 陈思言 on 2021/5/21.
//

#include <sstream>
#include "Type.h"

using std::stringstream;

const shared_ptr<VoidType> VoidType::object(new VoidType());
const shared_ptr<IntegerType> IntegerType::object(new IntegerType());
const shared_ptr<BooleanType> BooleanType::object(new BooleanType());
const shared_ptr<AtomicIntegerType> AtomicIntegerType::object(new AtomicIntegerType());

string ArrayType::toString() const {
    stringstream ss;
    ss << "array<" << elementType->toString();
    if (elementNum != std::nullopt) {
        ss << ", " << elementNum.value();
    }
    ss << '>';
    return ss.str();
}

bool ArrayType::equals(const Type &other) const {
    if (Type::equals(other)) {
        const auto &other_array = *reinterpret_cast<const ArrayType *>(&other);
        return elementType->equals(*other_array.elementType) && elementNum == other_array.elementNum;
    }
    return false;
}

string FunctionType::toString() const {
    stringstream ss;
    ss << returnType->toString() << '(';
    auto iter = argTypes.begin();
    while (iter != argTypes.end()) {
        ss << (*iter)->toString();
        if (++iter != argTypes.end()) {
            ss << ',';
        }
    }
    ss << ')';
    return ss.str();
}

bool FunctionType::equals(const Type &other) const {
    if (Type::equals(other)) {
        const auto &other_array = *reinterpret_cast<const FunctionType *>(&other);
        if (argTypes.size() != other_array.argTypes.size()) {
            return false;
        }
        auto iter1 = argTypes.begin();
        auto iter2 = other_array.argTypes.begin();
        while (iter1 != argTypes.end()) {
            if (!(*iter1)->equals(*iter2->get())) {
                return false;
            }
            iter1++;
            iter2++;
        }
        return returnType->equals(*other_array.returnType);
    }
    return false;
}

string PointerType::toString() const {
    stringstream ss;
    ss << "ptr<" << elementType->toString() << '>';
    return ss.str();
}

bool PointerType::equals(const Type &other) const {
    if (Type::equals(other)) {
        const auto &other_pointer = *reinterpret_cast<const PointerType *>(&other);
        return elementType->equals(*other_pointer.elementType);
    }
    return false;
}
