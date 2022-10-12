#pragma once
/* Fix-Point Real Arithmetic Implimentation
 * Used by threads
 * We use a 32 bits int to represent a fix-point number. Using 17 bits for the integer part and 14 bits for the fractional part, i.e. 17.14 format.
 */
#include <stdint.h>
#define FP32_F (1<<14)
typedef int fp32;

inline fp32 fp32_create(int n) {
    return n * (1 << 14);
}

inline int fp32_round(fp32 x) {
    if (x >= 0) {
        return (x + (1 << 13)) / (1 << 14);
    } else {
        return (x - (1 << 13)) / (1 << 14);
    }
}

inline int fp32_to_int(fp32 x) {
    return x / (1 << 14);
}

inline fp32 fp32_add(fp32 x, fp32 y) {
    return x + y;
}

inline fp32 fp32_sub(fp32 x, fp32 y) {
    return x - y;
}

inline fp32 fp32_add_int(fp32 x,int y)
{
    return x + y * FP32_F;
}

inline fp32 fp32_sub_int(fp32 x,int y)
{
    return x - y * FP32_F;
}

inline fp32 fp32_mul(fp32 x,fp32 y)
{
    return ((int64_t)x) * y / FP32_F;
}

inline fp32 fp32_mul_int(fp32 x,int y)
{
    return x * y;
}

inline fp32 fp32_div(fp32 x,fp32 y)
{
    return ((int64_t)x) * FP32_F / y;
}

inline fp32 fp32_div_int(fp32 x,int y)
{
    return x / y;
}