#pragma once
#include <stdint.h>
#include "mod_sum_table.h"
#include "utility/span.h"

// Source: https://natronics.github.io/blag/2014/gps-prn/
// Generates a binary code that is psuedorandom noise 
// It is a shift register with arbitrary feedback and output using mod2 sum of a masked state
template <typename T>
void generate_prn_code(tcb::span<T> code_out, tcb::span<const int> output_taps) {
    constexpr int TOTAL_REG_BITS = 10;
    constexpr int TOTAL_STATES = 1 << TOTAL_REG_BITS;
    static const auto XOR_SUM_LUT = generate_mod_sum_table(TOTAL_REG_BITS);

    // registers
    uint16_t R1 = (1u << TOTAL_REG_BITS) - 1u;
    uint16_t R2 = (1u << TOTAL_REG_BITS) - 1u;
    // feedback taps
    const uint16_t G1 = 0b0'010'000'001;
    const uint16_t G2 = 0b0'110'010'111;
    // output taps
    const uint16_t O1 = 0b0'000'000'001;
    uint16_t O2 = 0u;
    for (const int tap: output_taps) {
        const int shift = TOTAL_REG_BITS-tap;
        O2 |= (1u << shift);
    }
    // make sure resulting mask gits into xor sum lut
    O2 &= ((1u << TOTAL_REG_BITS) - 1u);

    // create code
    const size_t N = code_out.size();
    for (size_t i = 0; i < N; i++) {
        // output 
        const uint16_t sum = XOR_SUM_LUT[(R1 & O1)] ^ XOR_SUM_LUT[(R2 & O2)];
        code_out[i] = static_cast<T>(sum);
        // feedback
        const uint16_t F1 = XOR_SUM_LUT[R1 & G1];
        const uint16_t F2 = XOR_SUM_LUT[R2 & G2];
        R1 = (R1 >> 1) | (F1 << (TOTAL_REG_BITS-1));
        R2 = (R2 >> 1) | (F2 << (TOTAL_REG_BITS-1));
    }
}
