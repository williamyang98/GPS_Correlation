#include "gps_correlator.h"
#include <assert.h>
#include <cmath>
#include "dsp/simd/c32_vec_mul.h"
#include "dsp/calculate_fft.h"
#include "dsp/fftshift.h"

static void ApplyFrequencyShift(
    tcb::span<const std::complex<float>> x, 
    tcb::span<std::complex<float>> y, 
    const float k) 
{
    assert(x.size() == y.size());
    const size_t N = x.size();
    constexpr float PI = 3.14159265f;
    const float step = 2.0f * PI * k;
    float dt = 0.0f;
    for (size_t i = 0; i < N; i++) {
        const auto pll = std::complex<float>(std::cos(dt), std::sin(dt));
        dt = std::fmod(dt + step, 2.0f * PI);
        y[i] = x[i] * pll;
    }
};

// NOTE: AVX2 is 256bit = 32bytes
constexpr size_t SIMD_ALIGN_AMOUNT = 32u;

GPS_Correlator::GPS_Correlator(
    tcb::span<uint8_t> _logical_prn_code, 
    const int _block_size, 
    const int _Fcode, const int _Fs, const int _Fdev_max)
:   block_size(_block_size), 
    Fcode(_Fcode), Fs(_Fs), Fdev_max(_Fdev_max),
    Ts(1.0f/(float)_Fs)
{
    // Possible frequency shifts we should search for when correlating
    const int Fshift_step = Fcode/2;
    for (int i = -Fdev_max; i <= +Fdev_max; i+=Fshift_step) {
        freq_offsets.push_back((float)i);
    }
    const int TOTAL_FREQ_OFFSETS = (int)freq_offsets.size();

    freq_offset_index_histogram = std::make_unique<Histogram>(TOTAL_FREQ_OFFSETS);

    // Allocate buffers
    for (int i = 0; i < TOTAL_FREQ_OFFSETS; i++) {
        freq_shifted_prn_codes.push_back({ (size_t)block_size, SIMD_ALIGN_AMOUNT });
        freq_shifted_prn_ffts.push_back({ (size_t)block_size, SIMD_ALIGN_AMOUNT });
        freq_shifted_correlation_output.push_back({ (size_t)block_size, SIMD_ALIGN_AMOUNT });
    }
    // correlation fft buffer
    corr_buf = AlignedVector<std::complex<float>>(block_size, SIMD_ALIGN_AMOUNT);
    ifft_buf = AlignedVector<std::complex<float>>(block_size, SIMD_ALIGN_AMOUNT);

    // nearest neighbour upsampling of code to sampling frequency
    auto prn_code = std::vector<std::complex<float>>(block_size);
    {
        const int N_src = (int)_logical_prn_code.size();
        const int N_dst = block_size;
        const float x_scale = (float)(N_src-1) / (float)(N_dst-1);
        for (int i = 0; i < N_dst; i++) {
            const int i_scaled = (int)((float)i * x_scale);
            // NOTE: Reverse so we perform the correlation correctly
            const int i_reverse = (N_src-1)-i_scaled;
            const float v = (float)_logical_prn_code[i_reverse];
            const float v_norm = 2.0f*v - 1.0f;
            prn_code[i] = std::complex<float>{ v_norm, 0.0f };
        }
    }

    // Generate frequency shifted prn codes and their associated FFT
    for (int i = 0; i < TOTAL_FREQ_OFFSETS; i++) {
        const float freq_offset = freq_offsets[i];
        auto& freq_shifted_prn_code = freq_shifted_prn_codes[i];
        auto& freq_shifted_prn_fft = freq_shifted_prn_ffts[i];

        const float k = freq_offset / (float)Fs;
        ApplyFrequencyShift(prn_code, freq_shifted_prn_code, k);
        CalculateFFT(freq_shifted_prn_code, freq_shifted_prn_fft);
    }
}

void GPS_Correlator::Process(tcb::span<const std::complex<float>> x_in_fft) {
    assert(x_in_fft.size() == (size_t)block_size);

    const size_t TOTAL_FREQ_OFFSETS = freq_offsets.size();

    // Get correlation for each frequency offset
    const float K_norm_fft = 1.0f / (float)(2*block_size + 1);
    for (size_t i = 0; i < TOTAL_FREQ_OFFSETS; i++) {
        auto& freq_shifted_prn_fft = freq_shifted_prn_ffts[i];
        auto& freq_shifted_corr_out = freq_shifted_correlation_output[i];

        // multiplication in frequency domain
        c32_vec_mul_auto(x_in_fft.data(), freq_shifted_prn_fft.data(), corr_buf.data(), block_size);
        // ifft to get impulse response in time domain
        CalculateIFFT(corr_buf, ifft_buf);
        InplaceFFTShift<std::complex<float>>(ifft_buf);
        for (int j = 0; j < block_size; j++) {
            freq_shifted_corr_out[j] = std::abs(ifft_buf[j]) * K_norm_fft;
        }
    }

    // Find best frequency offset
    float largest_peak = 0.0f;
    int freq_offset_index = 0;
    for (int i = 0; i < TOTAL_FREQ_OFFSETS; i++) {
        auto& y_corr = freq_shifted_correlation_output[i];
        float v_max = 0.0f;
        for (int j = 0; j < block_size; j++) {
            v_max = std::max(v_max, y_corr[j]);
        }
        if (v_max > largest_peak) {
            largest_peak = v_max;
            freq_offset_index = i;
        }
    }

    best_frequency_offset_index = freq_offset_index;
    freq_offset_index_histogram->PushIndex(freq_offset_index);
}

int GPS_Correlator::GetModeFrequencyOffsetIndex() const {
    return freq_offset_index_histogram->GetMode();
}

void GPS_Correlator::FindCorrelationPeak(tcb::span<const float> x, int& index, float& value) {
        int peak_index = 0;
        float peak_value = x[0];
        const size_t N = x.size();
        for (size_t i = 0; i < N; i++) {
            const float v = x[i];
            if (v > peak_value) {
                peak_value = v;
                peak_index = (int)i;
            }
        }

        index = peak_index;
        value = peak_value;
    }