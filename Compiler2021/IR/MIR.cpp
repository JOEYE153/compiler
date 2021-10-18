//
// Created by 陈思言 on 2021/6/4.
//

#include <sstream>
#include "Function.h"
#include "MIR.h"


using std::stringstream;

string UninitializedMIR::toString() const {
    stringstream ss;
    ss << getNameAndIdString() << " = [[uninitialized]]";
    return ss.str();
}

void UninitializedMIR::accept(MIR::Visitor &visitor) {
    visitor.visit(this);
}

string LoadConstantMIR::toString() const {
    stringstream ss;
    ss << getNameAndIdString() << " = load.const " << src->getName();
    return ss.str();
}

void LoadConstantMIR::accept(MIR::Visitor &visitor) {
    visitor.visit(this);
}

string LoadVariableMIR::toString() const {
    stringstream ss;
    ss << getNameAndIdString() << " = load.var " << src->getName();
    return ss.str();
}

void LoadVariableMIR::accept(MIR::Visitor &visitor) {
    visitor.visit(this);
}

string LoadPointerMIR::toString() const {
    stringstream ss;
    ss << getNameAndIdString() << " = load.ptr " << src->getNameAndIdString();
    return ss.str();
}

void LoadPointerMIR::accept(MIR::Visitor &visitor) {
    visitor.visit(this);
}

void LoadPointerMIR::doReplacement(const std::map<Assignment *, Assignment *> &replaceTable) {
    auto new_src = replaceTable.find(src);
    if (new_src != replaceTable.end()) {
        src = new_src->second;
    }
}

string StoreVariableMIR::toString() const {
    stringstream ss;
    ss << "store.var " << dst->getName() << ", " << src->getNameAndIdString();
    return ss.str();
}

void StoreVariableMIR::accept(MIR::Visitor &visitor) {
    visitor.visit(this);
}

void StoreVariableMIR::doReplacement(const std::map<Assignment *, Assignment *> &replaceTable) {
    auto new_src = replaceTable.find(src);
    if (new_src != replaceTable.end()) {
        src = new_src->second;
    }
}

string StorePointerMIR::toString() const {
    stringstream ss;
    ss << "store.ptr " << dst->getNameAndIdString() << ", " << src->getNameAndIdString();
    return ss.str();
}

void StorePointerMIR::accept(MIR::Visitor &visitor) {
    visitor.visit(this);
}

void StorePointerMIR::doReplacement(const std::map<Assignment *, Assignment *> &replaceTable) {
    auto new_dst = replaceTable.find(dst);
    if (new_dst != replaceTable.end()) {
        dst = new_dst->second;
    }
    auto new_src = replaceTable.find(src);
    if (new_src != replaceTable.end()) {
        src = new_src->second;
    }
}

string MemoryFillMIR::toString() const {
    stringstream ss;
    ss << "fill " << dst->getNameAndIdString() << ", " << src->getNameAndIdString();
    return ss.str();
}

void MemoryFillMIR::accept(MIR::Visitor &visitor) {
    visitor.visit(this);
}

void MemoryFillMIR::doReplacement(const std::map<Assignment *, Assignment *> &replaceTable) {
    auto new_dst = replaceTable.find(dst);
    if (new_dst != replaceTable.end()) {
        dst = new_dst->second;
    }
    auto new_src = replaceTable.find(src);
    if (new_src != replaceTable.end()) {
        src = new_src->second;
    }
}

string MemoryCopyMIR::toString() const {
    stringstream ss;
    ss << "copy " << dst->getNameAndIdString() << ", " << src->getNameAndIdString();
    return ss.str();
}

void MemoryCopyMIR::accept(MIR::Visitor &visitor) {
    visitor.visit(this);
}

void MemoryCopyMIR::doReplacement(const std::map<Assignment *, Assignment *> &replaceTable) {
    auto new_dst = replaceTable.find(dst);
    if (new_dst != replaceTable.end()) {
        dst = new_dst->second;
    }
    auto new_src = replaceTable.find(src);
    if (new_src != replaceTable.end()) {
        src = new_src->second;
    }
}

string UnaryMIR::toString() const {
    stringstream ss;
    ss << getNameAndIdString() << " = ";
    switch (op) {
        case Operator::NEG:
            ss << "neg ";
            break;
        case Operator::NOT:
            ss << "not ";
            break;
        case Operator::REV:
            ss << "rev ";
            break;
    }
    ss << src->getNameAndIdString();
    return ss.str();
}

void UnaryMIR::accept(MIR::Visitor &visitor) {
    visitor.visit(this);
}

void UnaryMIR::doReplacement(const std::map<Assignment *, Assignment *> &replaceTable) {
    auto new_src = replaceTable.find(src);
    if (new_src != replaceTable.end()) {
        src = new_src->second;
    }
}

string BinaryMIR::toString() const {
    stringstream ss;
    ss << getNameAndIdString() << " = ";
    switch (op) {
        case Operator::ADD:
            ss << "add ";
            break;
        case Operator::SUB:
            ss << "sub ";
            break;
        case Operator::MUL:
            ss << "mul ";
            break;
        case Operator::DIV:
            ss << "div ";
            break;
        case Operator::MOD:
            ss << "mod ";
            break;
        case Operator::AND:
            ss << "and ";
            break;
        case Operator::OR:
            ss << "or ";
            break;
        case Operator::XOR:
            ss << "xor ";
            break;
        case Operator::LSL:
            ss << "lsl ";
            break;
        case Operator::LSR:
            ss << "lsr ";
            break;
        case Operator::ASR:
            ss << "asr ";
            break;
        case Operator::CMP_EQ:
            ss << "cmp.eq ";
            break;
        case Operator::CMP_NE:
            ss << "cmp.ne ";
            break;
        case Operator::CMP_GT:
            ss << "cmp.gt ";
            break;
        case Operator::CMP_LT:
            ss << "cmp.lt ";
            break;
        case Operator::CMP_GE:
            ss << "cmp.ge ";
            break;
        case Operator::CMP_LE:
            ss << "cmp.le ";
            break;
    }
    ss << src1->getNameAndIdString() << ", " << src2->getNameAndIdString();
    return ss.str();
}

void BinaryMIR::accept(MIR::Visitor &visitor) {
    visitor.visit(this);
}

void BinaryMIR::doReplacement(const std::map<Assignment *, Assignment *> &replaceTable) {
    auto new_src1 = replaceTable.find(src1);
    if (new_src1 != replaceTable.end()) {
        src1 = new_src1->second;
    }
    auto new_src2 = replaceTable.find(src2);
    if (new_src2 != replaceTable.end()) {
        src2 = new_src2->second;
    }
}

string ValueAddressingMIR::toString() const {
    stringstream ss;
    ss << getNameAndIdString() << " = addr.scalar " << base->getName();
    return ss.str();
}

void ValueAddressingMIR::accept(MIR::Visitor &visitor) {
    visitor.visit(this);
}

string ArrayAddressingMIR::toString() const {
    stringstream ss;
    ss << getNameAndIdString() << " = addr.array ";
    auto base_ptr = dynamic_cast<Assignment *>(base);
    if (base_ptr != nullptr) {
        ss << base_ptr->getNameAndIdString();
    } else {
        ss << base->getName();
    }
    ss << ", " << offset->getNameAndIdString();
    return ss.str();
}

void ArrayAddressingMIR::accept(MIR::Visitor &visitor) {
    visitor.visit(this);
}

void ArrayAddressingMIR::doReplacement(const std::map<Assignment *, Assignment *> &replaceTable) {
    auto new_offset = replaceTable.find(offset);
    if (new_offset != replaceTable.end()) {
        offset = new_offset->second;
    }
    auto base_ptr = dynamic_cast<Assignment *>(base);
    if (base_ptr != nullptr) {
        auto new_base = replaceTable.find(base_ptr);
        if (new_base != replaceTable.end()) {
            base = new_base->second;
        }
    }
}

string SelectMIR::toString() const {
    stringstream ss;
    ss << getNameAndIdString() << " = select " << cond->getNameAndIdString() << " ? ";
    ss << src1->getNameAndIdString() << ", " << src2->getNameAndIdString();
    return ss.str();
}

void SelectMIR::accept(MIR::Visitor &visitor) {
    visitor.visit(this);
}

void SelectMIR::doReplacement(const std::map<Assignment *, Assignment *> &replaceTable) {
    auto new_cond = replaceTable.find(cond);
    if (new_cond != replaceTable.end()) {
        cond = new_cond->second;
    }
    auto new_src1 = replaceTable.find(src1);
    if (new_src1 != replaceTable.end()) {
        src1 = new_src1->second;
    }
    auto new_src2 = replaceTable.find(src2);
    if (new_src2 != replaceTable.end()) {
        src2 = new_src2->second;
    }
}

string JumpMIR::toString() const {
    stringstream ss;
    ss << "goto " << block->getName();
    return ss.str();
}

void JumpMIR::accept(MIR::Visitor &visitor) {
    visitor.visit(this);
}

string BranchMIR::toString() const {
    stringstream ss;
    ss << "if (" << cond->getNameAndIdString() << ") goto ";
    ss << block1->getName() << "; else goto " << block2->getName();
    return ss.str();
}

void BranchMIR::accept(MIR::Visitor &visitor) {
    visitor.visit(this);
}

void BranchMIR::doReplacement(const std::map<Assignment *, Assignment *> &replaceTable) {
    auto new_cond = replaceTable.find(cond);
    if (new_cond != replaceTable.end()) {
        cond = new_cond->second;
    }
}

string CallMIR::toString() const {
    stringstream ss;
    ss << "call " << func->getName() << '(';
    auto iter = args.begin();
    while (iter != args.end()) {
        ss << (*iter)->getNameAndIdString();
        if (++iter != args.end()) {
            ss << ", ";
        }
    }
    ss << ')';
    return ss.str();
}

void CallMIR::accept(MIR::Visitor &visitor) {
    visitor.visit(this);
}

void CallMIR::doReplacement(const std::map<Assignment *, Assignment *> &replaceTable) {
    for (auto &arg : args) {
        auto new_arg = replaceTable.find(arg);
        if (new_arg != replaceTable.end()) {
            arg = new_arg->second;
        }
    }
}

string CallWithAssignMIR::toString() const {
    stringstream ss;
    ss << getNameAndIdString() << " = " << CallMIR::toString();
    return ss.str();
}

void CallWithAssignMIR::accept(MIR::Visitor &visitor) {
    visitor.visit(this);
}

string MultiCallMIR::toString() const {
    return CallMIR::toString() + " @multi-thread, atomic_var = " + atomic_var->getName();
}

void MultiCallMIR::accept(MIR::Visitor &visitor) {
    visitor.visit(this);
}

string ReturnMIR::toString() const {
    if (val == nullptr) {
        return "return;";
    }
    stringstream ss;
    ss << "return " << val->getNameAndIdString();
    return ss.str();
}

void ReturnMIR::accept(MIR::Visitor &visitor) {
    visitor.visit(this);
}

void ReturnMIR::doReplacement(const std::map<Assignment *, Assignment *> &replaceTable) {
    if (val != nullptr) {
        auto new_val = replaceTable.find(val);
        if (new_val != replaceTable.end()) {
            val = new_val->second;
        }
    }
}

string PhiMIR::toString() const {
    stringstream ss;
    ss << getNameAndIdString() << " = " << "phi ";
    auto iter = incomingTable.begin();
    while (iter != incomingTable.end()) {
        ss << '[' << (iter->first)->getName() << ", " << (iter->second)->getNameAndIdString() << ']';
        if (++iter != incomingTable.end()) {
            ss << ", ";
        }
    }
    return ss.str();
}

void PhiMIR::accept(MIR::Visitor &visitor) {
    visitor.visit(this);
}

void PhiMIR::doReplacement(const std::map<Assignment *, Assignment *> &replaceTable) {
    for (auto &incoming : incomingTable) {
        auto new_incoming = replaceTable.find(incoming.second);
        if (new_incoming != replaceTable.end()) {
            incoming.second = new_incoming->second;
        }
    }
}

string AtomicLoopCondMIR::toString() const {
    stringstream ss;
    ss << "@AtomicLoopCondMIR: ";
    ss << "if (!compare_op(" << atomic_var->getName() << ", " << border->getNameAndIdString() << ")) ";
    ss << "goto " << exit->getName() << "; ";
    ss << getNameAndIdString() << " = " << atomic_var->getName() << "; ";
    ss << atomic_var->getName() << " = update_op(" << atomic_var->getName() << ", " << step->getNameAndIdString() << "); ";
    ss << "goto " << body->getName() << ';';
    return ss.str();
}

void AtomicLoopCondMIR::accept(MIR::Visitor &visitor) {
    visitor.visit(this);
}

void AtomicLoopCondMIR::doReplacement(const std::map<Assignment *, Assignment *> &replaceTable) {
    auto new_step = replaceTable.find(step);
    if (new_step != replaceTable.end()) {
        step = new_step->second;
    }
    auto new_border = replaceTable.find(border);
    if (new_border != replaceTable.end()) {
        border = new_border->second;
    }
}
