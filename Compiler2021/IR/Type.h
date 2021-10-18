//
// Created by 陈思言 on 2021/5/21.
//

#ifndef COMPILER2021_TYPE_H
#define COMPILER2021_TYPE_H

#include <string>
#include <memory>
#include <utility>
#include <vector>
#include <list>
#include <optional>

using std::string;
using std::vector;
using std::list;
using std::shared_ptr;
using std::optional;

class Type {
public:
    enum class ID {
        VOID, INTEGER, BOOLEAN, ARRAY, FUNCTION, POINTER
    };

    [[nodiscard]] ID getId() const {
        return id;
    }

    virtual ~Type() = default;

    [[nodiscard]] virtual string toString() const = 0;

    [[nodiscard]] virtual bool equals(const Type &other) const {
        return getId() == other.getId();
    }

    [[nodiscard]] virtual unsigned getSizeOfType() const {
        return 0;
    }

protected:
    Type(ID id) : id(id) {}

private:
    ID id;
};

class VoidType final : public Type {
public:
    VoidType() : Type(ID::VOID) {}

    static const shared_ptr<VoidType> object;

    [[nodiscard]] string toString() const override {
        return "void";
    }
};

class IntegerType : public Type {
public:
    IntegerType() : Type(ID::INTEGER) {}

    static const shared_ptr<IntegerType> object;

    [[nodiscard]] string toString() const override {
        return "int";
    }

    [[nodiscard]] unsigned getSizeOfType() const override {
        return 4;
    }
};

class BooleanType final : public Type {
public:
    BooleanType() : Type(ID::BOOLEAN) {}

    static const shared_ptr<BooleanType> object;

    [[nodiscard]] string toString() const override {
        return "bool";
    }

    [[nodiscard]] unsigned getSizeOfType() const override {
        return 4;
    }
};

class ArrayType final : public Type {
public:
    explicit ArrayType(shared_ptr<Type> elementType, optional<unsigned> elementNum = std::nullopt)
            : Type(ID::ARRAY), elementType(std::move(elementType)), elementNum(elementNum) {}

    [[nodiscard]] string toString() const override;

    [[nodiscard]] bool equals(const Type &other) const override;

    [[nodiscard]] shared_ptr<Type> getElementType() const {
        return elementType;
    }

    [[nodiscard]] optional<unsigned> getElementNum() const {
        return elementNum;
    }

    [[nodiscard]] unsigned getSizeOfType() const override {
        return elementNum.has_value() ? elementType->getSizeOfType() * elementNum.value() : 0;
    }

private:
    shared_ptr<Type> elementType;
    optional<unsigned> elementNum;
};

class FunctionType final : public Type {
public:
    explicit FunctionType(shared_ptr<Type> returnType, vector<shared_ptr<Type>> argTypes = {})
            : Type(ID::FUNCTION), returnType(std::move(returnType)), argTypes(std::move(argTypes)) {}

    [[nodiscard]] string toString() const override;

    [[nodiscard]] bool equals(const Type &other) const override;

    [[nodiscard]] shared_ptr<Type> getReturnType() const {
        return returnType;
    }

    [[nodiscard]] const vector<shared_ptr<Type>> &getArgTypes() const {
        return argTypes;
    }

private:
    shared_ptr<Type> returnType;
    vector<shared_ptr<Type>> argTypes;
};

class PointerType final : public Type {
public:
    explicit PointerType(shared_ptr<Type> elementType)
            : Type(ID::POINTER), elementType(std::move(elementType)) {}

    [[nodiscard]] string toString() const override;

    [[nodiscard]] bool equals(const Type &other) const override;

    [[nodiscard]] shared_ptr<Type> getElementType() const {
        return elementType;
    }

    [[nodiscard]] unsigned getSizeOfType() const override {
        return 4;
    }

private:
    shared_ptr<Type> elementType;
};

class AtomicIntegerType final : public IntegerType {
public:
    AtomicIntegerType() = default;

    static const shared_ptr<AtomicIntegerType> object;

    [[nodiscard]] string toString() const override {
        return "atomic_int";
    }
};


#endif //COMPILER2021_TYPE_H
