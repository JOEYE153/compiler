//
// Created by tcyhost on 2021/8/10.
//

#ifndef COMPILER2021_CONSTUTILS_H
#define COMPILER2021_CONSTUTILS_H

#include "../IR/Module.h"
#include "ArrayToMIR.h"

// 判断一个常量数组的访问是否可以用运算替代
ConstantArrayToMIR * ArrLoad2Op(Constant* constant);

static ConstantArrayToMIR* load2And(vector<int>& values);
static ConstantArrayToMIR* load2Or(vector<int>& values);
static ConstantArrayToMIR* load2Xor(vector<int>& values);

static ConstantArrayToMIR* load2Lsl(vector<int>& values);
static ConstantArrayToMIR* load2Asr(vector<int>& values);
static ConstantArrayToMIR* load2Lsr(vector<int>& values);

static ConstantArrayToMIR* load2Add(vector<int>& values);



#endif //COMPILER2021_CONSTUTILS_H
