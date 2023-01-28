#pragma once
#include <assert.h>
#include <complex>

// NOTE: Assumes arrays are aligned
// Multiply and accumulate vector of complex floats with vector of floats

static inline
void c32_vec_mul_scalar(
    const std::complex<float>* x0, 
    const std::complex<float>* x1, 
    std::complex<float>* y, 
    const int N) 
{
    for (int i = 0; i < N; i++) {
        y[i] += x0[i] * x1[i];
    }
}

// TODO: Modify code to support ARM platforms like Raspberry PI using NEON
#include <immintrin.h>
#include "simd_config.h"
#include "data_packing.h"
#include "c32_mul.h"

#if defined(_DSP_SSSE3)
static inline
void c32_vec_mul_ssse3(
    const std::complex<float>* x0, 
    const std::complex<float>* x1, 
    std::complex<float>* y, 
    const int N) 
{
    // 128bits = 16bytes = 2*8bytes
    constexpr int K = 2;
    const int M = N/K;

    for (int i = 0; i < M; i++) {
        __m128 a0 = _mm_load_ps(reinterpret_cast<const float*>(&x0[i*K]));
        __m128 a1 = _mm_load_ps(reinterpret_cast<const float*>(&x1[i*K]));
        __m128 b1 = c32_mul_ssse3(a0, a1);
        _mm_store_ps(reinterpret_cast<float*>(&y[i*K]), b1);
    }

    const int N_vector = M*K;
    const int N_remain = N-N_vector;
    c32_vec_mul_scalar(&x0[N_vector], &x1[N_vector], &y[N_vector], N_remain);
}
#endif

#if defined(_DSP_AVX2)
static inline
void c32_vec_mul_avx2(
    const std::complex<float>* x0, 
    const std::complex<float>* x1, 
    std::complex<float>* y, 
    const int N) 
{
    // 256bits = 32bytes = 4*8bytes
    constexpr int K = 4;
    const int M = N/K;

    for (int i = 0; i < M; i++) {
        __m256 a0 = _mm256_load_ps(reinterpret_cast<const float*>(&x0[i*K]));
        __m256 a1 = _mm256_load_ps(reinterpret_cast<const float*>(&x1[i*K]));
        __m256 b1 = c32_mul_avx2(a0, a1);
        _mm256_store_ps(reinterpret_cast<float*>(&y[i*K]), b1);
    }

    const int N_vector = M*K;
    const int N_remain = N-N_vector;
    c32_vec_mul_scalar(&x0[N_vector], &x1[N_vector], &y[N_vector], N_remain);
}
#endif

inline static 
void c32_vec_mul_auto(
    const std::complex<float>* x0, 
    const std::complex<float>* x1, 
    std::complex<float>* y, 
    const int N) 
{
    #if defined(_DSP_AVX2)
    return c32_vec_mul_avx2(x0, x1, y, N);
    #elif defined(_DSP_SSSE3)
    return c32_vec_mul_ssse3(x0, x1, y, N);
    #else
    return c32_vec_mul_scalar(x0, x1, y, N);
    #endif
}
