#pragma once

// Source: https://natronics.github.io/blag/2014/gps-prn/
constexpr int PRN_CODE_LENGTH = 1023;
constexpr int TOTAL_PRN_CODES = 32;
constexpr int PRN_OUTPUT_TAPS[TOTAL_PRN_CODES][2] = {
    {2,6},
    {3,7},
    {4,8},
    {5,9},
    {1,9},
    {2,10},
    {1,8},
    {2,9},
    {3,10},
    {2,3},
    {3,4},
    {5,6},
    {6,7},
    {7,8},
    {8,9},
    {9,10},
    {1,4},
    {2,5},
    {3,6},
    {4,7},
    {5,8},
    {6,9},
    {1,3},
    {4,6},
    {5,7},
    {6,8},
    {7,9},
    {8,10},
    {1,6},
    {2,7},
    {3,8},
    {4,9},
};