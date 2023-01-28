#pragma once
#include <stdint.h>
#include <vector>

std::vector<uint8_t> generate_mod_sum_table(const int total_bits) {
    const uint16_t total_states = (1u << total_bits);
    auto arr = std::vector<uint8_t>((int)total_states);
    for (uint16_t i = 0u; i < total_states; i++) {
        uint16_t reg = i;
        uint16_t sum = 0u;
        for (int j = 0; j < total_bits; j++) {
            sum = sum ^ (reg & 0b1);
            reg = reg >> 1;
        }
        arr[i] = (uint8_t)sum;
    }
    return arr;
};