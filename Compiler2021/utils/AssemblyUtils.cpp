//
// Created by 陈思言 on 2021/8/1.
//

#include "AssemblyUtils.h"

bool checkImmediate(int imm) {
    for (int i = 0; i < 32; i += 2) {
        unsigned int tmp = imm << i;
        tmp |= (imm & (0xffffffff << (32 - i))) >> (32 - i);
        if (tmp <= 0x000000ff) {
            return true;
        }
    }
    return false;
}
