//
// Created by tcyhost on 2021/8/10.
//

#include "ConstUtils.h"

static int currentId = 0;

ConstantArrayToMIR *ArrLoad2Op(Constant *constant) {
    if (constant->getType() == IntegerType::object) return nullptr;
    auto values = constant->getValue<vector<int>>();
    ConstantArrayToMIR *src;
    if ((src = load2And(values)) != nullptr) return src;
    if ((src = load2Or(values)) != nullptr) return src;
    if ((src = load2Xor(values)) != nullptr) return src;
    if ((src = load2Lsl(values)) != nullptr) return src;
    if ((src = load2Asr(values)) != nullptr) return src;
    if ((src = load2Lsr(values)) != nullptr) return src;
    if ((src = load2Add(values)) != nullptr) return src;
    return nullptr;
}

// offset & i
ConstantArrayToMIR *load2And(vector<int> &values) {
    if (values[0] != 0) return nullptr;
    auto tot = values.size() - 1;
    int n = 1;
    while (((1 << n) - 1) <= tot) n++;
    n--;
    auto idx = (1 << n) - 1;
    auto val = values[idx] & idx;
    if (idx != tot)
        val += values[tot] & (1 << n);
    for (auto i = 1; i < values.size(); i++) {
        if (values[i] != (val & i)) return nullptr;
    }
    return new ConstantArrayToMIRAnd(val);
}

// offset | i
ConstantArrayToMIR *load2Or(vector<int> &values) {
    int offset = values[0];
    for (auto i = 1; i < values.size(); i++) {
        if (values[i] != (offset | i)) return nullptr;
    }
    return new ConstantArrayToMIROr(offset);
}

// offset ^ i
ConstantArrayToMIR *load2Xor(vector<int> &values) {
    int offset = values[0];
    for (auto i = 1; i < values.size(); i++) {
        if (values[i] != (offset ^ i)) return nullptr;
    }
    return new ConstantArrayToMIRXor(offset);
}

// offset << i
ConstantArrayToMIR *load2Lsl(vector<int> &values) {
    int offset = values[0];
    for (auto i = 1; i < values.size(); i++) {
        if (values[i] != (offset << i)) return nullptr;
    }
    return new ConstantArrayToMIRLsl(offset);
}

// ((unsigned) offset) >> i
ConstantArrayToMIR *load2Lsr(vector<int> &values) {
    unsigned int offset = values[0];
    for (auto i = 1; i < values.size(); i++) {
        if (values[i] != (offset >> i)) return nullptr;
    }
    return new ConstantArrayToMIRLsr(offset);
}

// offset >> i
ConstantArrayToMIR *load2Asr(vector<int> &values) {
    int offset = values[0];
    for (auto i = 1; i < values.size(); i++) {
        if (values[i] != (offset >> i)) return nullptr;
    }
    return new ConstantArrayToMIRAsr(offset);
}

ConstantArrayToMIR *load2Add(vector<int> &values) {
    int offset = values[0];
    for (auto i = 1; i < values.size(); i++) {
        if (values[i] != offset + i) return nullptr;
    }
    return new ConstantArrayToMIRAdd(offset);
}