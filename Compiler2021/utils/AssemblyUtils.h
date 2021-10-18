//
// Created by 陈思言 on 2021/8/1.
//

#ifndef COMPILER2021_ASSEMBLYUTILS_H
#define COMPILER2021_ASSEMBLYUTILS_H

// 检查一个整型是否2的幂
template<typename T>
inline bool isPowerOfTwo(T x) {
    return !(x & (x - 1));
}

// 检查一个立即数是否符合8位循环移动偶数位的要求
bool checkImmediate(int imm);

#endif //COMPILER2021_ASSEMBLYUTILS_H
