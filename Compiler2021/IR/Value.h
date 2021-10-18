//
// Created by 陈思言 on 2021/5/21.
//

#ifndef COMPILER2021_VALUE_H
#define COMPILER2021_VALUE_H

#include <utility>
#include <any>
#include "Type.h"

using std::any_cast;
using std::string_view;

class Value {
public:
    enum class Location {
        REGISTER, STACK, STATIC, ARGUMENT, CONSTANT
    };

    [[nodiscard]] shared_ptr<Type> getType() const {
        return type;
    }

    [[nodiscard]] string getName() const {
        return name;
    }

    [[nodiscard]] Location getLocation() const {
        return location;
    }

    [[nodiscard]] bool isConstant() const {
        return location == Location::CONSTANT;
    }

    virtual ~Value() = default;

protected:
    Value(shared_ptr<Type> type, string_view name, Location location)
            : type(std::move(type)), name(name), location(location) {}

    string name;
private:
    shared_ptr<Type> type;
    Location location;
};

class Variable final : public Value {
public:
    Variable(shared_ptr<Type> type, string_view name, Location location, bool is_ref)
            : Value(std::move(type), name, location), is_ref(is_ref) {}

    [[nodiscard]] bool isReference() const {
        return is_ref;
    }

    [[nodiscard]] bool isArgument() const {
        return getLocation() == Location::ARGUMENT;
    }

    [[nodiscard]] bool isStatic() const {
        return getLocation() == Location::STATIC;
    }

private:
    bool is_ref;
};

class Constant final : public Value {
public:
    Constant(shared_ptr<Type> type, string_view name, std::any value)
            : Value(std::move(type), name, Location::CONSTANT),
              value(std::move(value)) {}

    template<class T>
    [[nodiscard]] T getValue() const {
        return any_cast<T>(value);
    }

    [[nodiscard]] string getValueString() const;

    [[nodiscard]] std::any getCopyOfValue() const {
        return std::any(value);
    }

private:
    std::any value;
};

class Assignment : public Value {
public:
    Assignment(shared_ptr<Type> type, string_view name, size_t id)
            : Value(std::move(type), name, Location::REGISTER), id(id) {}

    [[nodiscard]] string getNameAndIdString() const;

    virtual class MIR *castToMIR() { return nullptr; }

public:
    size_t id;
};


#endif //COMPILER2021_VALUE_H
