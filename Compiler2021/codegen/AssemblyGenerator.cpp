//
// Created by Joeye on 2021/7/18.
//

#include "AssemblyGenerator.h"
#include "../utils/AssemblyUtils.h"

bool AssemblyGenerator::run() {
    auto functionTable = md.getFunctionVecDictOrder();
    for (auto fn : functionTable) {
        if (fn->isExternal) {
            continue;
        }
        runOnFunction(*fn);
        codeSection.addFunction(*fn, out);
    }
    if (md.useThreadPool) {
        dataSection.createThreadPool();
    }
    auto variableTable = md.getGlobalVariableVecDictOrder();
    dataSection.addVariable(variableTable, md.initializationTable);
    auto constantTable = md.getGlobalConstantVecDictOrder();
    dataSection.addConstant(constantTable);
    /**************************** _mymemset ****************************/
    codeSection.textSection << "\n\n\t.align 3\n"
                            << "\t.global _mymemset\n"
                            << "\t.type _mymemset, %function\n"
                            << "_mymemset:\n"
                            << "\t@ r0 = array address\n"
                            << "\t@ r1 = value\n"
                            << "\t@ r2 = array len, len > 0, aligned by 4\n"
                            << "\tadd r2, r0, r2\n"
                            << "_assign:\n"
                            << "\tstr r1, [r0], #4\n"
                            << "\tcmp r2, r0\n"
                            << "\tbne _assign\n"
                            << "\tbx lr\n"
                            << "\t.size _mymemset, .-_mymemset\n";
    /**************************** _mymemset ****************************/
    /**************************** _mymemcpy ****************************/
    codeSection.textSection << "\n\n\t.align 3\n"
                            << "\t.global _mymemcpy\n"
                            << "\t.type _mymemcpy, %function\n"
                            << "_mymemcpy:\n"
                            << "\t@ r0 = dst address\n"
                            << "\t@ r1 = src address\n"
                            << "\t@ r2 = array len, len > 0, aligned by 4\n"
                            << "\tldr r3, [r1], #4\n"
                            << "\tsubs r2, r2, #4\n"
                            << "\tstr r3, [r0], #4\n"
                            << "\tbne _mymemcpy\n"
                            << "\tbx lr\n"
                            << "\t.size _mymemcpy, .-_mymemcpy\n";
    /**************************** _mymemcpy ****************************/
    if (md.useThreadPool) {
        codeSection.createThreadPool();
    }
    Pass::out << codeSection.textSection.str() << '\n';
    Pass::out << dataSection.dataSection.str() << '\n';
    Pass::out << dataSection.rodataSection.str() << '\n';
    return false;
}

void AssemblyGenerator::allocateFrame(const string &tmpReg) {
    if (checkImmediate(frameSize)) {
        out << "\tsub sp, sp, #" << frameSize << "\n";
    } else {
        out << "\tmovw " << tmpReg << ", #:lower16:" << frameSize << "\n";
        out << "\tmovt " << tmpReg << ", #:upper16:" << frameSize << "\n";
        out << "\tsub sp, sp, " << tmpReg << "\n";
    }
}

void AssemblyGenerator::freeFrame(const string &tmpReg) {
    if (checkImmediate(frameSize)) {
        out << "\tadd sp, sp, #" << frameSize << "\n";
    } else {
        out << "\tmovw " << tmpReg << ", #:lower16:" << frameSize << "\n";
        out << "\tmovt " << tmpReg << ", #:upper16:" << frameSize << "\n";
        out << "\tadd sp, sp, " << tmpReg << "\n";
    }
}

void AssemblyGenerator::loadImmediate(int im, const string &reg) {
    if (checkImmediate(im)) {
        out << "\tmov " << reg << ", #" << im << "\n";
    } else {
        out << "\tmovw " << reg << ", #:lower16:" << im << "\n";
        out << "\tmovt " << reg << ", #:upper16:" << im << "\n";
    }
}

void AssemblyGenerator::loadLabel(const string &label, const string &tmpReg) {
    out << "\tmovw " << tmpReg << ", #:lower16:" << label << "\n";
    out << "\tmovt " << tmpReg << ", #:upper16:" << label << "\n";
}

string AssemblyGenerator::preTreatStackOffset(int offset, const string &tmpReg) {
    if (offset <= 4095) {
        return "sp, #" + std::to_string(offset);
    }
    out << "\tmovw " << tmpReg << ", #:lower16:" << offset << "\n";
    out << "\tmovt " << tmpReg << ", #:upper16:" << offset << "\n";
    return "sp, " + tmpReg;
}

void AssemblyGenerator::load(Value *value, const string &reg) {
    if (value == nullptr) return;
    switch (value->getLocation()) {
        case Value::Location::STATIC:
            loadLabel(value->getName(), reg);
            out << "\tldr " << reg << ", [" << reg << "]\n";
            break;
        case Value::Location::CONSTANT:
            loadImmediate(dynamic_cast<Constant *>(value)->getValue<int>(), reg);
            break;
        case Value::Location::STACK:
        case Value::Location::ARGUMENT:{
            auto variable = dynamic_cast<Variable *>(value);
            int offset = valueOffsetTable.find(variable)->second;
            string res = preTreatStackOffset(offset, "r11");
            out << "\tldr " << reg << ", [" << res << "]\n";
            if (variable->isReference()) {
                out << "\tldr " << reg << ", [" << reg << "]\n";
            }
            break;
        }
        case Value::Location::REGISTER: {
            auto assignment = dynamic_cast<Assignment *>(value);
            int offset = valueOffsetTable.find(assignment)->second;
            string res = preTreatStackOffset(offset, "r11");
            out << "\tldr " << reg << ", [" << res << "]\n";
            break;
        }
        default:
            break;
    }
}

void AssemblyGenerator::loadArg(Value *arg, const string &reg) {
    if (arg == nullptr) return;
    if (arg->getType()->getId() == Type::ID::ARRAY) {
        // pass address if it is array
        auto iter = valueOffsetTable.find(arg);
        if (iter == valueOffsetTable.end()) {
            // global array
            loadLabel(arg->getName(), reg);
        } else {
            // local array
            auto variable = dynamic_cast<Variable *>(arg);
            if (variable != nullptr && variable->isReference()) {
                string res = preTreatStackOffset(iter->second, "r11");
                out << "\tldr " << reg << ", [" << res << "]\n";
            } else {
                loadImmediate(iter->second, "r11");
                out << "\tadd " << reg << ", sp, r11\n";
            }
        }
    } else {
        load(arg, reg);
    }
}

void AssemblyGenerator::store(Value *value, const string &reg) {
    if (value == nullptr) return;
    switch (value->getLocation()) {
        case Value::Location::STATIC: {
            loadLabel(value->getName(), "r11");
            out << "\tstr " << reg << ", [r11]\n";
            break;
        }
        case Value::Location::STACK:
        case Value::Location::ARGUMENT: {
            auto variable = dynamic_cast<Variable *>(value);
            int offset = valueOffsetTable.find(variable)->second;
            string res = preTreatStackOffset(offset, "r11");
            if (variable->isReference()) {
                out << "\tldr r11, [" << res << "]\n";
                out << "\tstr " << reg << ", [r11]\n";
            } else {
                out << "\tstr " << reg << ", [" << res << "]\n";
            }
            break;
        }
        case Value::Location::REGISTER: {
            auto assignment = dynamic_cast<Assignment *>(value);
            int offset = valueOffsetTable.find(assignment)->second;
            string res = preTreatStackOffset(offset, "r11");
            out << "\tstr " << reg << ", [" << res << "]\n";
            break;
        }
        default:
            break;
    }
}



