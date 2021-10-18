//
// Created by tcyhost on 2021/8/10.
//

#ifndef COMPILER2021_ARRAYTOMIR_H
#define COMPILER2021_ARRAYTOMIR_H

#include "../IR/Module.h"

class ConstantArrayToMIR {
public:
    explicit ConstantArrayToMIR(int imm) : imm(imm) {}

    virtual Assignment *operator()(Assignment *src1_or_src2, Module *md, BasicBlock *bb) const = 0;

protected:
    Assignment *createLoadConstantMIR(Module *md, BasicBlock *bb) const {
        auto constant = md->declareGlobalImmediateInt(imm);
        return bb->createLoadConstantMIR("caml_" + constant->getName(), 18373636, constant);
    }

private:
    int imm;
};

class ConstantArrayToMIRAdd : public ConstantArrayToMIR {
public:
    explicit ConstantArrayToMIRAdd(int imm) : ConstantArrayToMIR(imm) {}

    Assignment *operator()(Assignment *src1, Module *md, BasicBlock *bb) const override {
        auto ssa = createLoadConstantMIR(md, bb);
        return bb->createAddMIR("add_" + ssa->getName(), 63637381, src1, ssa);
    }
};

class ConstantArrayToMIRAnd : public ConstantArrayToMIR {
public:
    explicit ConstantArrayToMIRAnd(int imm) : ConstantArrayToMIR(imm) {}

    Assignment *operator()(Assignment *src1, Module *md, BasicBlock *bb) const override {
        auto ssa = createLoadConstantMIR(md, bb);
        return bb->createAndMIR("and_" + ssa->getName(), 63637381, src1, ssa);
    }
};

class ConstantArrayToMIROr : public ConstantArrayToMIR {
public:
    explicit ConstantArrayToMIROr(int imm) : ConstantArrayToMIR(imm) {}

    Assignment *operator()(Assignment *src1, Module *md, BasicBlock *bb) const override {
        auto ssa = createLoadConstantMIR(md, bb);
        return bb->createOrMIR("or_" + ssa->getName(), 63637381, src1, ssa);
    }
};

class ConstantArrayToMIRXor : public ConstantArrayToMIR {
public:
    explicit ConstantArrayToMIRXor(int imm) : ConstantArrayToMIR(imm) {}

    Assignment *operator()(Assignment *src1, Module *md, BasicBlock *bb) const override {
        auto ssa = createLoadConstantMIR(md, bb);
        return bb->createXorMIR("xor_" + ssa->getName(), 63637381, src1, ssa);
    }
};

class ConstantArrayToMIRLsl : public ConstantArrayToMIR {
public:
    explicit ConstantArrayToMIRLsl(int imm) : ConstantArrayToMIR(imm) {}

    Assignment *operator()(Assignment *src2, Module *md, BasicBlock *bb) const override {
        auto ssa = createLoadConstantMIR(md, bb);
        return bb->createLslMIR("lsl_" + ssa->getName(), 63637381, ssa, src2);
    }
};

class ConstantArrayToMIRLsr : public ConstantArrayToMIR {
public:
    explicit ConstantArrayToMIRLsr(int imm) : ConstantArrayToMIR(imm) {}

    Assignment *operator()(Assignment *src2, Module *md, BasicBlock *bb) const override {
        auto ssa = createLoadConstantMIR(md, bb);
        return bb->createLsrMIR("lsr_" + ssa->getName(), 63637381, ssa, src2);
    }
};

class ConstantArrayToMIRAsr : public ConstantArrayToMIR {
public:
    explicit ConstantArrayToMIRAsr(int imm) : ConstantArrayToMIR(imm) {}

    Assignment *operator()(Assignment *src2, Module *md, BasicBlock *bb) const override {
        auto ssa = createLoadConstantMIR(md, bb);
        return bb->createAsrMIR("asr_" + ssa->getName(), 63637381, ssa, src2);
    }
};


#endif //COMPILER2021_ARRAYTOMIR_H
