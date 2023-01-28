#pragma once

#include <stdint.h>
#include <complex>
#include <vector>
#include <memory>
#include "histogram.h"
#include "utility/aligned_vector.h"
#include "utility/joint_allocate.h"
#include "utility/span.h"

class GPS_Correlator 
{
private:
    const int block_size;
    const int Fcode;
    const int Fs;
    const float Ts;
    const int Fdev_max;

    // frequency shifted correlation data
    std::vector<float> freq_offsets;
    std::vector<AlignedVector<std::complex<float>>> freq_shifted_prn_codes;
    std::vector<AlignedVector<std::complex<float>>> freq_shifted_prn_ffts;
    std::vector<AlignedVector<float>> freq_shifted_correlation_output;

    AlignedVector<std::complex<float>> corr_buf;
    AlignedVector<std::complex<float>> ifft_buf;
    // frequency offset and peak detection 
    int best_frequency_offset_index = 0;
    std::unique_ptr<Histogram> freq_offset_index_histogram;
public:
    GPS_Correlator(
        tcb::span<uint8_t> _logical_prn_code, 
        const int _block_size, 
        const int _Fcode, const int _Fs, const int _Fdev_max);
    void Process(tcb::span<const std::complex<float>> x_in_fft);
    void FindCorrelationPeak(tcb::span<const float> x, int& index, float& value);
public:
    auto GetBestFrequencyOffsetIndex() const { return best_frequency_offset_index; }
    int GetModeFrequencyOffsetIndex() const;
    auto& GetFrequencyOffsets() { return freq_offsets; }
    auto& GetCorrelations() { return freq_shifted_correlation_output; }
};