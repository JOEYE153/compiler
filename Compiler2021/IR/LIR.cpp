//
// Created by 陈思言 on 2021/6/18.
//

#include <sstream>
#include <iostream>
#include "Function.h"
#include "LIR.h"


using std::stringstream;

string StatusRegAssign::getRegString(Format format) const {
    stringstream ss;
    ss << "cpsr";
    if (format == Format::VREG || format == Format::ALL) {
        ss << "{%" << vReg << '}';
    }
    return ss.str();
}

string CoreRegAssign::getRegString(Format format) const {
    static const char *apcs[16] = {
            "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", "r8", "r9", "r10", "r11", "ip", "sp", "lr", "pc"
    };
    stringstream ss;
    switch (format) {
        case Format::VREG:
            ss << "%r" << vReg;
            break;
        case Format::COLOR:
            if (color.has_value()) {
                ss << 'r' << (unsigned) color.value();
            }
            break;
        case Format::PREG:
            if (pReg.has_value() && pReg.value() < 16) {
                return apcs[pReg.value()];
            }
            break;
        case Format::ALL:
            ss << getRegString(Format::VREG) << '{';
            if (color.has_value()) {
                ss << (unsigned) color.value();
            }
            ss << ',';
            if (pReg.has_value() && pReg.value() < 16) {
                ss << (unsigned) pReg.value();
            }
            ss << '}';
            break;
    }
    return ss.str();
}

string NeonRegAssign::getRegString(Format format) const {
    stringstream ss;
    switch (format) {
        case Format::VREG:
            ss << (form == NeonForm::D ? "%d" : "%q") << vReg;
            break;
        case Format::COLOR:
            if (color.has_value()) {
                ss << (form == NeonForm::D ? 'd' : 'q') << (unsigned) color.value();
            }
            break;
        case Format::PREG:
            if (pReg.has_value() && pReg.value() < 32) {
                ss << (form == NeonForm::D ? 'd' : 'q') << (unsigned) pReg.value();
            }
            break;
        case Format::ALL:
            ss << getRegString(Format::VREG) << '{';
            if (color.has_value()) {
                ss << (unsigned) color.value();
            }
            ss << ',';
            if (pReg.has_value() && pReg.value() < 16) {
                ss << (unsigned) pReg.value();
            }
            ss << '}';
            break;
    }
    return ss.str();
}

string CoreMemAssign::getMemString() const {
    stringstream ss;
    ss << "$coreSpill[" << vMem << ',';
    if (offset.has_value()) {
        ss << offset.value();
    }
    ss << ']';
    return ss.str();
}

string NeonMemAssign::getMemString() const {
    stringstream ss;
    ss << "$neonSpill[" << vMem << ',';
    if (offset.has_value()) {
        ss << offset.value();
    }
    ss << ']';
    return ss.str();
}

const char *ArmInst::getString(ShiftOperator op) {
    switch (op) {
        case ShiftOperator::LSL:
            return "lsl";
        case ShiftOperator::LSR:
            return "lsr";
        case ShiftOperator::ASR:
            return "asr";
    }
    return nullptr;
}

const char *ArmInst::getString(UnaryOperator op) {
    switch (op) {
        case UnaryOperator::MOV:
            return "mov";
        case UnaryOperator::MVN:
            return "mvn";
    }
    return nullptr;
}

const char *ArmInst::getString(BinaryOperator op) {
    switch (op) {
        case BinaryOperator::AND:
            return "and";
        case BinaryOperator::EOR:
            return "eor";
        case BinaryOperator::SUB:
            return "sub";
        case BinaryOperator::RSB:
            return "rsb";
        case BinaryOperator::ADD:
            return "add";
        case BinaryOperator::ORR:
            return "orr";
        case BinaryOperator::BIC:
            return "bic";
    }
    return nullptr;
}

const char *ArmInst::getString(CompareOperator op) {
    switch (op) {
        case CompareOperator::TST:
            return "tst";
        case CompareOperator::TEQ:
            return "teq";
        case CompareOperator::CMP:
            return "cmp";
        case CompareOperator::CMN:
            return "cmn";
    }
    return nullptr;
}

const char *CondInst::getCondString(CondInst::Cond cond) {
    switch (cond) {
        case Cond::EQ:
            return "eq";
        case Cond::NE:
            return "ne";
        case Cond::CS:
            return "cs";
        case Cond::CC:
            return "cc";
        case Cond::MI:
            return "mi";
        case Cond::PL:
            return "pl";
        case Cond::VS:
            return "vs";
        case Cond::VC:
            return "vc";
        case Cond::HI:
            return "hi";
        case Cond::LS:
            return "ls";
        case Cond::GE:
            return "ge";
        case Cond::LT:
            return "lt";
        case Cond::GT:
            return "gt";
        case Cond::LE:
            return "le";
        case Cond::AL:
            return "";
    }
    return nullptr;
}

CondInst::Cond CondInst::getContrary(Cond cond) {
    switch (cond) {
        case Cond::EQ:
            return Cond::NE;
        case Cond::NE:
            return Cond::EQ;
        case Cond::CS:
            return Cond::CC;
        case Cond::CC:
            return Cond::CS;
        case Cond::MI:
            return Cond::PL;
        case Cond::PL:
            return Cond::MI;
        case Cond::VS:
            return Cond::VC;
        case Cond::VC:
            return Cond::VS;
        case Cond::HI:
            return Cond::LS;
        case Cond::LS:
            return Cond::HI;
        case Cond::GE:
            return Cond::LT;
        case Cond::LT:
            return Cond::GE;
        case Cond::GT:
            return Cond::LE;
        case Cond::LE:
            return Cond::GT;
        default:
            throw std::logic_error("No contrary");
    }
}

void CondInst::doReplacement(const std::map<CoreRegAssign *, CoreRegAssign *> &replaceTableCoreReg,
                             const std::map<NeonRegAssign *, NeonRegAssign *> &replaceTableNeonReg,
                             const std::map<StatusRegAssign *, StatusRegAssign *> &replaceTableStatusReg) {
    if (cpsr != nullptr) {
        auto new_cpsr = replaceTableStatusReg.find(cpsr);
        if (new_cpsr != replaceTableStatusReg.end()) {
            cpsr = new_cpsr->second;
        }
    }
    if (rd != nullptr) {
        auto new_rd = replaceTableCoreReg.find(rd);
        if (new_rd != replaceTableCoreReg.end()) {
            rd = new_rd->second;
        }
    }
}

void CondInst::addComment(stringstream &ss) const {
    if (cpsr != nullptr) {
        ss << " @cpsr = " << cpsr->getRegString(RegAssign::Format::VREG);
    }
    if (rd != nullptr) {
        ss << " @rd = " << rd->getRegString(RegAssign::Format::ALL);
    }
}

string UnaryRegInst::toString(RegAssign::Format format) const {
    stringstream ss;
    ss << getString(op) << getCondString(cond) << ' ';
    ss << getRegString(format) << ", " << rm->getRegString(format);
    if (shiftImm != 0) {
        ss << ", " << getString(shiftOp) << " #" << (unsigned) shiftImm;
    }
    addComment(ss);
    return ss.str();
}

void UnaryRegInst::accept(LIR::Visitor &visitor) {
    visitor.visit(this);
}

void UnaryRegInst::doReplacement(const std::map<CoreRegAssign *, CoreRegAssign *> &replaceTableCoreReg,
                                 const std::map<NeonRegAssign *, NeonRegAssign *> &replaceTableNeonReg,
                                 const std::map<StatusRegAssign *, StatusRegAssign *> &replaceTableStatusReg) {
    CondInst::doReplacement(replaceTableCoreReg, replaceTableNeonReg, replaceTableStatusReg);
    if (rm != nullptr) {
        auto new_rm = replaceTableCoreReg.find(rm);
        if (new_rm != replaceTableCoreReg.end()) {
            rm = new_rm->second;
        }
    }
}

string BinaryRegInst::toString(RegAssign::Format format) const {
    stringstream ss;
    ss << getString(op) << getCondString(cond) << ' ';
    ss << getRegString(format) << ", " << rn->getRegString(format) << ", " << rm->getRegString(format);
    if (shiftImm != 0) {
        ss << ", " << getString(shiftOp) << " #" << (unsigned) shiftImm;
    }
    addComment(ss);
    return ss.str();
}

void BinaryRegInst::accept(LIR::Visitor &visitor) {
    visitor.visit(this);
}

void BinaryRegInst::doReplacement(const std::map<CoreRegAssign *, CoreRegAssign *> &replaceTableCoreReg,
                                  const std::map<NeonRegAssign *, NeonRegAssign *> &replaceTableNeonReg,
                                  const std::map<StatusRegAssign *, StatusRegAssign *> &replaceTableStatusReg) {
    CondInst::doReplacement(replaceTableCoreReg, replaceTableNeonReg, replaceTableStatusReg);
    auto new_rm = replaceTableCoreReg.find(rm);
    if (new_rm != replaceTableCoreReg.end()) {
        rm = new_rm->second;
    }
    auto new_rn = replaceTableCoreReg.find(rn);
    if (new_rn != replaceTableCoreReg.end()) {
        rn = new_rn->second;
    }
}

string CompareRegInst::toString(RegAssign::Format format) const {
    stringstream ss;
    ss << getString(op) << getCondString(cond) << ' ';
    ss << rn->getRegString(format) << ", " << rm->getRegString(format);
    if (shiftImm != 0) {
        ss << ", " << getString(shiftOp) << " #" << (unsigned) shiftImm;
    }
    addComment(ss);
    return ss.str();
}

void CompareRegInst::accept(LIR::Visitor &visitor) {
    visitor.visit(this);
}

void CompareRegInst::doReplacement(const std::map<CoreRegAssign *, CoreRegAssign *> &replaceTableCoreReg,
                                   const std::map<NeonRegAssign *, NeonRegAssign *> &replaceTableNeonReg,
                                   const std::map<StatusRegAssign *, StatusRegAssign *> &replaceTableStatusReg) {
    CondInst::doReplacement(replaceTableCoreReg, replaceTableNeonReg, replaceTableStatusReg);
    auto new_rm = replaceTableCoreReg.find(rm);
    if (new_rm != replaceTableCoreReg.end()) {
        rm = new_rm->second;
    }
    auto new_rn = replaceTableCoreReg.find(rn);
    if (new_rn != replaceTableCoreReg.end()) {
        rn = new_rn->second;
    }
}

string UnaryShiftInst::toString(RegAssign::Format format) const {
    stringstream ss;
    ss << getString(op) << getCondString(cond) << ' ';
    ss << getRegString(format) << ", " << rm->getRegString(format);
    ss << ", " << getString(shiftOp) << ' ' << rs->getRegString(format);
    addComment(ss);
    return ss.str();
}

void UnaryShiftInst::accept(LIR::Visitor &visitor) {
    visitor.visit(this);
}

void UnaryShiftInst::doReplacement(const std::map<CoreRegAssign *, CoreRegAssign *> &replaceTableCoreReg,
                                   const std::map<NeonRegAssign *, NeonRegAssign *> &replaceTableNeonReg,
                                   const std::map<StatusRegAssign *, StatusRegAssign *> &replaceTableStatusReg) {
    CondInst::doReplacement(replaceTableCoreReg, replaceTableNeonReg, replaceTableStatusReg);
    auto new_rm = replaceTableCoreReg.find(rm);
    if (new_rm != replaceTableCoreReg.end()) {
        rm = new_rm->second;
    }
    auto new_rs = replaceTableCoreReg.find(rs);
    if (new_rs != replaceTableCoreReg.end()) {
        rs = new_rs->second;
    }
}

string BinaryShiftInst::toString(RegAssign::Format format) const {
    stringstream ss;
    ss << getString(op) << getCondString(cond) << ' ';
    ss << getRegString(format) << ", " << rn->getRegString(format) << ", " << rm->getRegString(format);
    ss << ", " << getString(shiftOp) << ' ' << rs->getRegString(format);
    addComment(ss);
    return ss.str();
}

void BinaryShiftInst::accept(LIR::Visitor &visitor) {
    visitor.visit(this);
}

void BinaryShiftInst::doReplacement(const std::map<CoreRegAssign *, CoreRegAssign *> &replaceTableCoreReg,
                                    const std::map<NeonRegAssign *, NeonRegAssign *> &replaceTableNeonReg,
                                    const std::map<StatusRegAssign *, StatusRegAssign *> &replaceTableStatusReg) {
    CondInst::doReplacement(replaceTableCoreReg, replaceTableNeonReg, replaceTableStatusReg);
    auto new_rm = replaceTableCoreReg.find(rm);
    if (new_rm != replaceTableCoreReg.end()) {
        rm = new_rm->second;
    }
    auto new_rn = replaceTableCoreReg.find(rn);
    if (new_rn != replaceTableCoreReg.end()) {
        rn = new_rn->second;
    }
    auto new_rs = replaceTableCoreReg.find(rs);
    if (new_rs != replaceTableCoreReg.end()) {
        rs = new_rs->second;
    }
}

string CompareShiftInst::toString(RegAssign::Format format) const {
    stringstream ss;
    ss << getString(op) << getCondString(cond) << ' ';
    ss << rn->getRegString(format) << ", " << rm->getRegString(format);
    ss << ", " << getString(shiftOp) << ' ' << rs->getRegString(format);
    addComment(ss);
    return ss.str();
}

void CompareShiftInst::accept(LIR::Visitor &visitor) {
    visitor.visit(this);
}

void CompareShiftInst::doReplacement(const std::map<CoreRegAssign *, CoreRegAssign *> &replaceTableCoreReg,
                                     const std::map<NeonRegAssign *, NeonRegAssign *> &replaceTableNeonReg,
                                     const std::map<StatusRegAssign *, StatusRegAssign *> &replaceTableStatusReg) {
    CondInst::doReplacement(replaceTableCoreReg, replaceTableNeonReg, replaceTableStatusReg);
    auto new_rm = replaceTableCoreReg.find(rm);
    if (new_rm != replaceTableCoreReg.end()) {
        rm = new_rm->second;
    }
    auto new_rn = replaceTableCoreReg.find(rn);
    if (new_rn != replaceTableCoreReg.end()) {
        rn = new_rn->second;
    }
    auto new_rs = replaceTableCoreReg.find(rs);
    if (new_rs != replaceTableCoreReg.end()) {
        rs = new_rs->second;
    }
}

string UnaryImmInst::toString(RegAssign::Format format) const {
    stringstream ss;
    ss << getString(op) << getCondString(cond) << ' ';
    ss << getRegString(format) << ", #" << imm12;
    addComment(ss);
    return ss.str();
}

void UnaryImmInst::accept(LIR::Visitor &visitor) {
    visitor.visit(this);
}

void UnaryImmInst::doReplacement(const std::map<CoreRegAssign *, CoreRegAssign *> &replaceTableCoreReg,
                                 const std::map<NeonRegAssign *, NeonRegAssign *> &replaceTableNeonReg,
                                 const std::map<StatusRegAssign *, StatusRegAssign *> &replaceTableStatusReg) {
    CondInst::doReplacement(replaceTableCoreReg, replaceTableNeonReg, replaceTableStatusReg);
}

string BinaryImmInst::toString(RegAssign::Format format) const {
    stringstream ss;
    ss << getString(op) << getCondString(cond) << ' ';
    ss << getRegString(format) << ", " << rn->getRegString(format) << ", #" << imm12;
    addComment(ss);
    return ss.str();
}

void BinaryImmInst::accept(LIR::Visitor &visitor) {
    visitor.visit(this);
}

void BinaryImmInst::doReplacement(const std::map<CoreRegAssign *, CoreRegAssign *> &replaceTableCoreReg,
                                  const std::map<NeonRegAssign *, NeonRegAssign *> &replaceTableNeonReg,
                                  const std::map<StatusRegAssign *, StatusRegAssign *> &replaceTableStatusReg) {
    CondInst::doReplacement(replaceTableCoreReg, replaceTableNeonReg, replaceTableStatusReg);
    auto new_rn = replaceTableCoreReg.find(rn);
    if (new_rn != replaceTableCoreReg.end()) {
        rn = new_rn->second;
    }
}

string CompareImmInst::toString(RegAssign::Format format) const {
    stringstream ss;
    ss << getString(op) << getCondString(cond) << ' ';
    ss << rn->getRegString(format) << ", #" << imm12;
    addComment(ss);
    return ss.str();
}

void CompareImmInst::accept(LIR::Visitor &visitor) {
    visitor.visit(this);
}

void CompareImmInst::doReplacement(const std::map<CoreRegAssign *, CoreRegAssign *> &replaceTableCoreReg,
                                   const std::map<NeonRegAssign *, NeonRegAssign *> &replaceTableNeonReg,
                                   const std::map<StatusRegAssign *, StatusRegAssign *> &replaceTableStatusReg) {
    CondInst::doReplacement(replaceTableCoreReg, replaceTableNeonReg, replaceTableStatusReg);
    auto new_rn = replaceTableCoreReg.find(rn);
    if (new_rn != replaceTableCoreReg.end()) {
        rn = new_rn->second;
    }
}

string SetFlagInst::toString(RegAssign::Format format) const {
    return getRegString(format) + " = $set_flag";
}

void SetFlagInst::accept(LIR::Visitor &visitor) {
    visitor.visit(this);
}

string LoadRegInst::toString(RegAssign::Format format) const {
    stringstream ss;
    ss << "ldr" << getCondString(cond) << ' ' << getRegString(format);
    ss << ", [" << rn->getRegString(format);
    if (postIndexed) {
        ss << "], " << (subIndexing ? '-' : '+') << rm->getRegString(format);
        if (shiftImm != 0) {
            ss << ", " << getString(shiftOp) << " #" << (unsigned) shiftImm;
        }
    } else {
        ss << ", " << (subIndexing ? '-' : '+') << rm->getRegString(format);
        if (shiftImm != 0) {
            ss << ", " << getString(shiftOp) << " #" << (unsigned) shiftImm;
        }
        ss << ']';
    }
    addComment(ss);
    return ss.str();
}

void LoadRegInst::accept(LIR::Visitor &visitor) {
    visitor.visit(this);
}

void LoadRegInst::doReplacement(const std::map<CoreRegAssign *, CoreRegAssign *> &replaceTableCoreReg,
                                const std::map<NeonRegAssign *, NeonRegAssign *> &replaceTableNeonReg,
                                const std::map<StatusRegAssign *, StatusRegAssign *> &replaceTableStatusReg) {
    CondInst::doReplacement(replaceTableCoreReg, replaceTableNeonReg, replaceTableStatusReg);
    auto new_rm = replaceTableCoreReg.find(rm);
    if (new_rm != replaceTableCoreReg.end()) {
        rm = new_rm->second;
    }
    auto new_rn = replaceTableCoreReg.find(rn);
    if (new_rn != replaceTableCoreReg.end()) {
        rn = new_rn->second;
    }
}

string LoadImmInst::toString(RegAssign::Format format) const {
    stringstream ss;
    ss << "ldr" << getCondString(cond) << ' ' << getRegString(format);
    ss << ", [" << rn->getRegString(format);
    if (postIndexed) {
        ss << "], #" << (subIndexing ? '-' : '+') << imm12;
    } else {
        ss << ", #" << (subIndexing ? '-' : '+') << imm12 << ']';
    }
    addComment(ss);
    return ss.str();
}

void LoadImmInst::accept(LIR::Visitor &visitor) {
    visitor.visit(this);
}

void LoadImmInst::doReplacement(const std::map<CoreRegAssign *, CoreRegAssign *> &replaceTableCoreReg,
                                const std::map<NeonRegAssign *, NeonRegAssign *> &replaceTableNeonReg,
                                const std::map<StatusRegAssign *, StatusRegAssign *> &replaceTableStatusReg) {
    CondInst::doReplacement(replaceTableCoreReg, replaceTableNeonReg, replaceTableStatusReg);
    auto new_rn = replaceTableCoreReg.find(rn);
    if (new_rn != replaceTableCoreReg.end()) {
        rn = new_rn->second;
    }
}

string StoreRegInst::toString(RegAssign::Format format) const {
    stringstream ss;
    ss << "str" << getCondString(cond) << ' ' << rt->getRegString(format);
    ss << ", [" << rn->getRegString(format);
    if (postIndexed) {
        ss << "], " << (subIndexing ? '-' : '+') << rm->getRegString(format);
        if (shiftImm != 0) {
            ss << ", " << getString(shiftOp) << " #" << (unsigned) shiftImm;
        }
    } else {
        ss << ", " << (subIndexing ? '-' : '+') << rm->getRegString(format);
        if (shiftImm != 0) {
            ss << ", " << getString(shiftOp) << " #" << (unsigned) shiftImm;
        }
        ss << ']';
    }
    addComment(ss);
    return ss.str();
}

void StoreRegInst::accept(LIR::Visitor &visitor) {
    visitor.visit(this);
}

void StoreRegInst::doReplacement(const std::map<CoreRegAssign *, CoreRegAssign *> &replaceTableCoreReg,
                                 const std::map<NeonRegAssign *, NeonRegAssign *> &replaceTableNeonReg,
                                 const std::map<StatusRegAssign *, StatusRegAssign *> &replaceTableStatusReg) {
    CondInst::doReplacement(replaceTableCoreReg, replaceTableNeonReg, replaceTableStatusReg);
    auto new_rt = replaceTableCoreReg.find(rt);
    if (new_rt != replaceTableCoreReg.end()) {
        rt = new_rt->second;
    }
    auto new_rm = replaceTableCoreReg.find(rm);
    if (new_rm != replaceTableCoreReg.end()) {
        rm = new_rm->second;
    }
    auto new_rn = replaceTableCoreReg.find(rn);
    if (new_rn != replaceTableCoreReg.end()) {
        rn = new_rn->second;
    }
}

string StoreImmInst::toString(RegAssign::Format format) const {
    stringstream ss;
    ss << "str" << getCondString(cond) << ' ' << rt->getRegString(format);
    ss << ", [" << rn->getRegString(format);
    if (postIndexed) {
        ss << "], #" << (subIndexing ? '-' : '+') << imm12;
    } else {
        ss << ", #" << (subIndexing ? '-' : '+') << imm12 << ']';
    }
    addComment(ss);
    return ss.str();
}

void StoreImmInst::accept(LIR::Visitor &visitor) {
    visitor.visit(this);
}

void StoreImmInst::doReplacement(const std::map<CoreRegAssign *, CoreRegAssign *> &replaceTableCoreReg,
                                 const std::map<NeonRegAssign *, NeonRegAssign *> &replaceTableNeonReg,
                                 const std::map<StatusRegAssign *, StatusRegAssign *> &replaceTableStatusReg) {
    CondInst::doReplacement(replaceTableCoreReg, replaceTableNeonReg, replaceTableStatusReg);
    auto new_rt = replaceTableCoreReg.find(rt);
    if (new_rt != replaceTableCoreReg.end()) {
        rt = new_rt->second;
    }
    auto new_rn = replaceTableCoreReg.find(rn);
    if (new_rn != replaceTableCoreReg.end()) {
        rn = new_rn->second;
    }
}

string WriteBackAddressInst::toString(RegAssign::Format format) const {
    stringstream ss;
    ss << getRegString(format) << " = $wba";
    return ss.str();
}

void WriteBackAddressInst::accept(LIR::Visitor &visitor) {
    visitor.visit(this);
}

string MultiplyInst::toString(RegAssign::Format format) const {
    stringstream ss;
    switch (op) {
        case Operator::MUL:
            ss << "mul " << getRegString(format);
            ss << ", " << rn->getRegString(format) << ", " << rm->getRegString(format);
            break;
        case Operator::MLA:
            ss << "mla " << getRegString(format);
            ss << ", " << rn->getRegString(format) << ", " << rm->getRegString(format);
            ss << ", " << ra->getRegString(format);
            break;
        case Operator::MLS:
            ss << "mls " << getRegString(format);
            ss << ", " << rn->getRegString(format) << ", " << rm->getRegString(format);
            ss << ", " << ra->getRegString(format);
            break;
    }
    addComment(ss);
    return ss.str();
}

void MultiplyInst::accept(LIR::Visitor &visitor) {
    visitor.visit(this);
}

void MultiplyInst::doReplacement(const std::map<CoreRegAssign *, CoreRegAssign *> &replaceTableCoreReg,
                                 const std::map<NeonRegAssign *, NeonRegAssign *> &replaceTableNeonReg,
                                 const std::map<StatusRegAssign *, StatusRegAssign *> &replaceTableStatusReg) {
    CondInst::doReplacement(replaceTableCoreReg, replaceTableNeonReg, replaceTableStatusReg);
    auto new_rm = replaceTableCoreReg.find(rm);
    if (new_rm != replaceTableCoreReg.end()) {
        rm = new_rm->second;
    }
    auto new_rn = replaceTableCoreReg.find(rn);
    if (new_rn != replaceTableCoreReg.end()) {
        rn = new_rn->second;
    }
    if (ra != nullptr) {
        auto new_ra = replaceTableCoreReg.find(ra);
        if (new_ra != replaceTableCoreReg.end()) {
            ra = new_ra->second;
        }
    }
}

string Multiply64GetHighInst::toString(RegAssign::Format format) const {
    stringstream ss;
    ss << "smull " << "r12, " << getRegString(format);
    ss << ", " << rn->getRegString(format) << ", " << rm->getRegString(format);
    addComment(ss);
    return ss.str();
}

void Multiply64GetHighInst::accept(LIR::Visitor &visitor) {
    visitor.visit(this);
}

void Multiply64GetHighInst::doReplacement(const std::map<CoreRegAssign *, CoreRegAssign *> &replaceTableCoreReg,
                                          const std::map<NeonRegAssign *, NeonRegAssign *> &replaceTableNeonReg,
                                          const std::map<StatusRegAssign *, StatusRegAssign *> &replaceTableStatusReg) {
    CondInst::doReplacement(replaceTableCoreReg, replaceTableNeonReg, replaceTableStatusReg);
    auto new_rm = replaceTableCoreReg.find(rm);
    if (new_rm != replaceTableCoreReg.end()) {
        rm = new_rm->second;
    }
    auto new_rn = replaceTableCoreReg.find(rn);
    if (new_rn != replaceTableCoreReg.end()) {
        rn = new_rn->second;
    }
}

string DivideInst::toString(RegAssign::Format format) const {
    stringstream ss;
    ss << "sdiv " << getRegString(format) << ", " << rn->getRegString(format) << ", " << rm->getRegString(format);
    addComment(ss);
    return ss.str();
}

void DivideInst::accept(LIR::Visitor &visitor) {
    visitor.visit(this);
}

void DivideInst::doReplacement(const std::map<CoreRegAssign *, CoreRegAssign *> &replaceTableCoreReg,
                               const std::map<NeonRegAssign *, NeonRegAssign *> &replaceTableNeonReg,
                               const std::map<StatusRegAssign *, StatusRegAssign *> &replaceTableStatusReg) {
    CondInst::doReplacement(replaceTableCoreReg, replaceTableNeonReg, replaceTableStatusReg);
    auto new_rm = replaceTableCoreReg.find(rm);
    if (new_rm != replaceTableCoreReg.end()) {
        rm = new_rm->second;
    }
    auto new_rn = replaceTableCoreReg.find(rn);
    if (new_rn != replaceTableCoreReg.end()) {
        rn = new_rn->second;
    }
}

string MovwInst::toString(RegAssign::Format format) const {
    stringstream ss;
    ss << "movw " << getRegString(format) << ", #";
    if (label.empty()) {
        ss << imm16;
    } else {
        ss << ":lower16:" << label;
    }
    addComment(ss);
    return ss.str();
}

void MovwInst::accept(LIR::Visitor &visitor) {
    visitor.visit(this);
}

void MovwInst::doReplacement(const std::map<CoreRegAssign *, CoreRegAssign *> &replaceTableCoreReg,
                             const std::map<NeonRegAssign *, NeonRegAssign *> &replaceTableNeonReg,
                             const std::map<StatusRegAssign *, StatusRegAssign *> &replaceTableStatusReg) {
    CondInst::doReplacement(replaceTableCoreReg, replaceTableNeonReg, replaceTableStatusReg);
}

string MovtInst::toString(RegAssign::Format format) const {
    stringstream ss;
    ss << "movt " << getRegString(format) << ", #";
    if (label.empty()) {
        ss << imm16;
    } else {
        ss << ":upper16:" << label;
    }
    addComment(ss);
    return ss.str();
}

void MovtInst::accept(LIR::Visitor &visitor) {
    visitor.visit(this);
}

void MovtInst::doReplacement(const std::map<CoreRegAssign *, CoreRegAssign *> &replaceTableCoreReg,
                             const std::map<NeonRegAssign *, NeonRegAssign *> &replaceTableNeonReg,
                             const std::map<StatusRegAssign *, StatusRegAssign *> &replaceTableStatusReg) {
    CondInst::doReplacement(replaceTableCoreReg, replaceTableNeonReg, replaceTableStatusReg);
}

string BranchInst::toString(RegAssign::Format format) const {
    stringstream ss;
    ss << "b" << getCondString(cond) << ' ' << block->getName();
    addComment(ss);
    return ss.str();
}

void BranchInst::accept(LIR::Visitor &visitor) {
    visitor.visit(this);
}

void BranchInst::doReplacement(const std::map<CoreRegAssign *, CoreRegAssign *> &replaceTableCoreReg,
                               const std::map<NeonRegAssign *, NeonRegAssign *> &replaceTableNeonReg,
                               const std::map<StatusRegAssign *, StatusRegAssign *> &replaceTableStatusReg) {
    CondInst::doReplacement(replaceTableCoreReg, replaceTableNeonReg, replaceTableStatusReg);
}

string BranchAndLinkInst::toString(RegAssign::Format format) const {
    stringstream ss;
    ss << "bl" << getCondString(cond) << ' ' << func->getName();
    addComment(ss);
    return ss.str();
}

void BranchAndLinkInst::accept(LIR::Visitor &visitor) {
    visitor.visit(this);
}

void BranchAndLinkInst::doReplacement(const std::map<CoreRegAssign *, CoreRegAssign *> &replaceTableCoreReg,
                                      const std::map<NeonRegAssign *, NeonRegAssign *> &replaceTableNeonReg,
                                      const std::map<StatusRegAssign *, StatusRegAssign *> &replaceTableStatusReg) {
    CondInst::doReplacement(replaceTableCoreReg, replaceTableNeonReg, replaceTableStatusReg);
}

string BranchAndExchangeInst::toString(RegAssign::Format format) const {
    stringstream ss;
    ss << "bx" << getCondString(cond) << ' ' << rm->getRegString(format);
    addComment(ss);
    return ss.str();
}

void BranchAndExchangeInst::accept(LIR::Visitor &visitor) {
    visitor.visit(this);
}

void BranchAndExchangeInst::doReplacement(const std::map<CoreRegAssign *, CoreRegAssign *> &replaceTableCoreReg,
                                          const std::map<NeonRegAssign *, NeonRegAssign *> &replaceTableNeonReg,
                                          const std::map<StatusRegAssign *, StatusRegAssign *> &replaceTableStatusReg) {
    CondInst::doReplacement(replaceTableCoreReg, replaceTableNeonReg, replaceTableStatusReg);
    auto new_rm = replaceTableCoreReg.find(rm);
    if (new_rm != replaceTableCoreReg.end()) {
        rm = new_rm->second;
    }
}

string PushInst::toString(RegAssign::Format format) const {
    stringstream ss;
    ss << "push {";
    int i = 0;
    for (auto reg:regList) {
        ss << "r" + std::to_string(reg);
        i++;
        if (i != regList.size()) {
            ss << ",";
        } else {
            ss << "}";
        }
    }
    return ss.str();
}

void PushInst::accept(LIR::Visitor &visitor) {
    visitor.visit(this);
}

string PopInst::toString(RegAssign::Format format) const {
    stringstream ss;
    ss << "pop {";
    int i = 0;
    for (auto reg:regList) {
        ss << "r" + std::to_string(reg);
        i++;
        if (i != regList.size()) {
            ss << ",";
        } else {
            ss << "}";
        }
    }
    return ss.str();
}

void PopInst::accept(LIR::Visitor &visitor) {
    visitor.visit(this);
}

string LoadImm32LIR::toString(RegAssign::Format format) const {
    stringstream ss;
    ss << getRegString(format) << " = #" << imm32;
    return ss.str();
}

void LoadImm32LIR::accept(LIR::Visitor &visitor) {
    visitor.visit(this);
}

string ValueAddressingLIR::toString(RegAssign::Format format) const {
    stringstream ss;
    ss << getRegString(format) << " = &" << base->getName();
    return ss.str();
}

void ValueAddressingLIR::accept(LIR::Visitor &visitor) {
    visitor.visit(this);
}

string GetArgumentLIR::toString(RegAssign::Format format) const {
    stringstream ss;
    ss << getRegString(format) << " = $arg" << index;
    return ss.str();
}

void GetArgumentLIR::accept(LIR::Visitor &visitor) {
    visitor.visit(this);
}

string SetArgumentLIR::toString(RegAssign::Format format) const {
    stringstream ss;
    ss << "$arg" << index << " = " << src->getRegString(format);
    return ss.str();
}

void SetArgumentLIR::accept(LIR::Visitor &visitor) {
    visitor.visit(this);
}

void SetArgumentLIR::doReplacement(const std::map<CoreRegAssign *, CoreRegAssign *> &replaceTableCoreReg,
                                   const std::map<NeonRegAssign *, NeonRegAssign *> &replaceTableNeonReg,
                                   const std::map<StatusRegAssign *, StatusRegAssign *> &replaceTableStatusReg) {
    auto new_src = replaceTableCoreReg.find(src);
    if (new_src != replaceTableCoreReg.end()) {
        src = new_src->second;
    }
}

string GetReturnValueLIR::toString(RegAssign::Format format) const {
    stringstream ss;
    ss << getRegString(format) << " = $val " << call->func->getName();
    return ss.str();
}

void GetReturnValueLIR::accept(LIR::Visitor &visitor) {
    visitor.visit(this);
}

string ReturnLIR::toString(RegAssign::Format format) const {
    if (val == nullptr) {
        return "ret";
    }
    stringstream ss;
    ss << "ret " << val->getRegString(format);
    return ss.str();
}

void ReturnLIR::accept(LIR::Visitor &visitor) {
    visitor.visit(this);
}

void ReturnLIR::doReplacement(const std::map<CoreRegAssign *, CoreRegAssign *> &replaceTableCoreReg,
                              const std::map<NeonRegAssign *, NeonRegAssign *> &replaceTableNeonReg,
                              const std::map<StatusRegAssign *, StatusRegAssign *> &replaceTableStatusReg) {
    if (val != nullptr) {
        auto new_val = replaceTableCoreReg.find(val);
        if (new_val != replaceTableCoreReg.end()) {
            val = new_val->second;
        }
    }
}

string LoadScalarLIR::toString(RegAssign::Format format) const {
    stringstream ss;
    ss << getRegString(format) << " = " << src->getMemString();
    return ss.str();
}

void LoadScalarLIR::accept(LIR::Visitor &visitor) {
    visitor.visit(this);
}

string StoreScalarLIR::toString(RegAssign::Format format) const {
    stringstream ss;
    ss << getMemString() << " = " << src->getRegString(format);
    return ss.str();
}

void StoreScalarLIR::accept(LIR::Visitor &visitor) {
    visitor.visit(this);
}

void StoreScalarLIR::doReplacement(const std::map<CoreRegAssign *, CoreRegAssign *> &replaceTableCoreReg,
                                   const std::map<NeonRegAssign *, NeonRegAssign *> &replaceTableNeonReg,
                                   const std::map<StatusRegAssign *, StatusRegAssign *> &replaceTableStatusReg) {
    auto new_src = replaceTableCoreReg.find(src);
    if (new_src != replaceTableCoreReg.end()) {
        src = new_src->second;
    }
}

//string LoadVectorLIR::toString(RegAssign::Format format) const {
//    stringstream ss;
//    ss << getRegString(format) << " = " << src->getMemString();
//    return ss.str();
//}
//
//void LoadVectorLIR::accept(LIR::Visitor &visitor) {
//    visitor.visit(this);
//}
//
//string StoreVectorLIR::toString(RegAssign::Format format) const {
//    stringstream ss;
//    ss << getMemString() << " = " << src->getRegString(format);
//    return ss.str();
//}
//
//void StoreVectorLIR::accept(LIR::Visitor &visitor) {
//    visitor.visit(this);
//}
//
//void StoreVectorLIR::doReplacement(const std::map<CoreRegAssign *, CoreRegAssign *> &replaceTableCoreReg,
//                                   const std::map<NeonRegAssign *, NeonRegAssign *> &replaceTableNeonReg,
//                                   const std::map<StatusRegAssign *, StatusRegAssign *> &replaceTableStatusReg) {
//    auto new_src = replaceTableNeonReg.find(src);
//    if (new_src != replaceTableNeonReg.end()) {
//        src = new_src->second;
//    }
//}

string AtomicLoopCondLIR::toString(RegAssign::Format format) const {
    stringstream ss;
    ss << "@AtomicLoopCondLIR: " << atomic_var_ptr->getRegString(format);
    return ss.str();
}

void AtomicLoopCondLIR::accept(LIR::Visitor &visitor) {
    visitor.visit(this);
}

void AtomicLoopCondLIR::doReplacement(const std::map<CoreRegAssign *, CoreRegAssign *> &replaceTableCoreReg,
                                      const std::map<NeonRegAssign *, NeonRegAssign *> &replaceTableNeonReg,
                                      const std::map<StatusRegAssign *, StatusRegAssign *> &replaceTableStatusReg) {
    auto new_atomic = replaceTableCoreReg.find(atomic_var_ptr);
    if (new_atomic != replaceTableCoreReg.end()) {
        atomic_var_ptr = new_atomic->second;
    }
    auto new_step = replaceTableCoreReg.find(step);
    if (new_step != replaceTableCoreReg.end()) {
        step = new_step->second;
    }
    auto new_border = replaceTableCoreReg.find(border);
    if (new_border != replaceTableCoreReg.end()) {
        border = new_border->second;
    }
}

string UninitializedCoreReg::toString(RegAssign::Format format) const {
    stringstream ss;
    ss << getRegString(format) << " = [[uninitialized]]";
    return ss.str();
}

void UninitializedCoreReg::accept(LIR::Visitor &visitor) {
    visitor.visit(this);
}

//string UninitializedNeonReg::toString(RegAssign::Format format) const {
//    stringstream ss;
//    ss << getRegString(format) << " = [[uninitialized]]";
//    return ss.str();
//}
//
//void UninitializedNeonReg::accept(LIR::Visitor &visitor) {
//    visitor.visit(this);
//}

string CoreRegPhiLIR::toString(RegAssign::Format format) const {
    stringstream ss;
    ss << getRegString(format) << " = " << "phi ";
    auto iter = incomingTable.begin();
    while (iter != incomingTable.end()) {
//        std::cout << '[' << (iter->first)->getName() << ", " << (iter->second)->getRegString(format) << ']';
        ss << '[' << (iter->first)->getName() << ", " << (iter->second)->getRegString(format) << ']';
        if (++iter != incomingTable.end()) {
            ss << ", ";
        }
    }
    return ss.str();
}

void CoreRegPhiLIR::accept(LIR::Visitor &visitor) {
    visitor.visit(this);
}

void CoreRegPhiLIR::doReplacement(const std::map<CoreRegAssign *, CoreRegAssign *> &replaceTableCoreReg,
                                  const std::map<NeonRegAssign *, NeonRegAssign *> &replaceTableNeonReg,
                                  const std::map<StatusRegAssign *, StatusRegAssign *> &replaceTableStatusReg) {
    for (auto &incoming : incomingTable) {
        auto new_incoming = replaceTableCoreReg.find(incoming.second);
        if (new_incoming != replaceTableCoreReg.end()) {
            incoming.second = new_incoming->second;
        }
    }
}

void CoreRegPhiLIR::replaceBlock(BasicBlock *dst, BasicBlock *src) {
    auto iter = incomingTable.find(src);
    if (iter != incomingTable.end()) {
        auto second = iter->second;
        incomingTable.erase(iter);
        incomingTable.emplace(dst, second);
    }
}

//string NeonRegPhiLIR::toString(RegAssign::Format format) const {
//    stringstream ss;
//    ss << getRegString(format) << " = " << "phi ";
//    auto iter = incomingTable.begin();
//    while (iter != incomingTable.end()) {
//        ss << '[' << (iter->first)->getName() << ", " << (iter->second)->getRegString(format) << ']';
//        if (++iter != incomingTable.end()) {
//            ss << ", ";
//        }
//    }
//    return ss.str();
//}
//
//void NeonRegPhiLIR::accept(LIR::Visitor &visitor) {
//    visitor.visit(this);
//}
//
//void NeonRegPhiLIR::doReplacement(const std::map<CoreRegAssign *, CoreRegAssign *> &replaceTableCoreReg,
//                                  const std::map<NeonRegAssign *, NeonRegAssign *> &replaceTableNeonReg,
//                                  const std::map<StatusRegAssign *, StatusRegAssign *> &replaceTableStatusReg) {
//    for (auto &incoming : incomingTable) {
//        auto new_incoming = replaceTableNeonReg.find(incoming.second);
//        if (new_incoming != replaceTableNeonReg.end()) {
//            incoming.second = new_incoming->second;
//        }
//    }
//}
//
//void NeonRegPhiLIR::replaceBlock(BasicBlock *dst, BasicBlock *src) {
//    auto iter = incomingTable.find(src);
//    if (iter != incomingTable.end()) {
//        auto second = iter->second;
//        incomingTable.erase(iter);
//        incomingTable.emplace(dst, second);
//    }
//}

string CoreMemPhiLIR::toString(RegAssign::Format format) const {
    stringstream ss;
    ss << getMemString() << " = " << "phi ";
    auto iter = incomingTable.begin();
    while (iter != incomingTable.end()) {
        ss << '[' << (iter->first)->getName() << ", " << (iter->second)->vMem << ']';
        if (++iter != incomingTable.end()) {
            ss << ", ";
        }
    }
    return ss.str();
}

void CoreMemPhiLIR::accept(LIR::Visitor &visitor) {
    visitor.visit(this);
}

void CoreMemPhiLIR::replaceBlock(BasicBlock *dst, BasicBlock *src) {
    auto iter = incomingTable.find(src);
    if (iter != incomingTable.end()) {
        auto second = iter->second;
        incomingTable.erase(iter);
        incomingTable.emplace(dst, second);
    }
}

//void NeonMemPhiLIR::accept(LIR::Visitor &visitor) {
//    visitor.visit(this);
//}
//
//string NeonMemPhiLIR::toString(RegAssign::Format format) const {
//    stringstream ss;
//    ss << getMemString() << " = " << "phi ";
//    auto iter = incomingTable.begin();
//    while (iter != incomingTable.end()) {
//        ss << '[' << (iter->first)->getName() << ", " << (iter->second)->vMem << ']';
//        if (++iter != incomingTable.end()) {
//            ss << ", ";
//        }
//    }
//    return ss.str();
//}
//
//void NeonMemPhiLIR::replaceBlock(BasicBlock *dst, BasicBlock *src) {
//    auto iter = incomingTable.find(src);
//    if (iter != incomingTable.end()) {
//        auto second = iter->second;
//        incomingTable.erase(iter);
//        incomingTable.emplace(dst, second);
//    }
//}
