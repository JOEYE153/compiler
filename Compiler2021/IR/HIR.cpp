//
// Created by 陈思言 on 2021/5/22.
//

#include <sstream>
#include "BasicBlock.h"

using std::stringstream;

string UnaryHIR::toString() const {
    stringstream ss;
    ss << dst->getName() << " = ";
    switch (op) {
        case Operator::ASSIGN:
            break;
        case Operator::NEG:
            ss << '-';
            break;
        case Operator::NOT:
            ss << '!';
            break;
        case Operator::REV:
            ss << '~';
            break;
    }
    ss << src->getName() << ';';
    return ss.str();
}

void UnaryHIR::accept(HIR::Visitor &visitor) {
    visitor.visit(this);
}

string BinaryHIR::toString() const {
    stringstream ss;
    if (op == Operator::ADDRESSING) {
        ss << '&' << dst->getName() << " = &" << src1->getName() << '[' << src2->getName() << "];";
        return ss.str();
    }
    ss << dst->getName() << " = " << src1->getName();
    switch (op) {
        case Operator::ADD:
            ss << " + ";
            break;
        case Operator::SUB:
            ss << " - ";
            break;
        case Operator::MUL:
            ss << " * ";
            break;
        case Operator::DIV:
            ss << " / ";
            break;
        case Operator::MOD:
            ss << " % ";
            break;
        case Operator::AND:
            ss << " & ";
            break;
        case Operator::OR:
            ss << " | ";
            break;
        case Operator::XOR:
            ss << " ^ ";
            break;
        case Operator::LSL:
            ss << " << ";
            break;
        case Operator::LSR:
            ss << " >> ";
            break;
        case Operator::ASR:
            ss << " >>> ";
            break;
        case Operator::CMP_EQ:
            ss << " == ";
            break;
        case Operator::CMP_NE:
            ss << " != ";
            break;
        case Operator::CMP_GT:
            ss << " > ";
            break;
        case Operator::CMP_LT:
            ss << " < ";
            break;
        case Operator::CMP_GE:
            ss << " >= ";
            break;
        case Operator::CMP_LE:
            ss << " <= ";
            break;
        default:
            return "error";
    }
    ss << src2->getName() << ';';
    return ss.str();
}

void BinaryHIR::accept(HIR::Visitor &visitor) {
    visitor.visit(this);
}

string JumpHIR::toString() const {
    stringstream ss;
    ss << "goto " << block->getName() << ';';
    return ss.str();
}

void JumpHIR::accept(HIR::Visitor &visitor) {
    visitor.visit(this);
}

string BranchHIR::toString() const {
    stringstream ss;
    ss << "if (" << cond->getName() << ") goto " << block1->getName() << "; else goto " << block2->getName() << ';';
    return ss.str();
}

void BranchHIR::accept(HIR::Visitor &visitor) {
    visitor.visit(this);
}

string CallHIR::toString() const {
    stringstream ss;
    if (ret != nullptr) {
        ss << ret->getName() << " = ";
    }
    ss << func_name << '(';
    auto iter = args.begin();
    while (iter != args.end()) {
        ss << (*iter)->getName();
        if (++iter != args.end()) {
            ss << ", ";
        }
    }
    ss << ");";
    return ss.str();
}

void CallHIR::accept(HIR::Visitor &visitor) {
    visitor.visit(this);
}

string ReturnHIR::toString() const {
    if (val == nullptr) {
        return "return;";
    }
    stringstream ss;
    ss << "return " << val->getName() << ';';
    return ss.str();
}

void ReturnHIR::accept(HIR::Visitor &visitor) {
    visitor.visit(this);
}

string PutfHIR::toString() const {
    stringstream ss;
    ss << "putf(" << format;
    auto iter = args.begin();
    if (iter != args.end()) {
        ss << ", ";
    }
    while (iter != args.end()) {
        ss << (*iter)->getName();
        if (++iter != args.end()) {
            ss << ", ";
        }
    }
    ss << ");";
    return ss.str();
}

void PutfHIR::accept(HIR::Visitor &visitor) {
    visitor.visit(this);
}
