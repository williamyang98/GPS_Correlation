#pragma once

#include <complex>
#include "gps_correlator.h"
#include "utility/basic_thread_pool.h"
#include "utility/aligned_vector.h"
#include "utility/span.h"

class GPS_App 
{
private:
    const int block_size;
    AlignedVector<std::complex<float>> fft_buf;
    std::vector<GPS_Correlator> gps_correlators;
    BasicThreadPool gps_correlator_thread_pool;

    int total_blocks_read = 0;
    bool is_always_correlate = false;
    std::vector<int> gps_correlator_trigger_flags;
public:
    GPS_App(const int _Fs, const int _Fcode, const int _Fdev_max);
    void Process(tcb::span<const std::complex<float>> x);
public:
    int GetBlockSize() const { return block_size; }
    int GetTotalBlocksRead() const { return total_blocks_read; }
    auto& GetCorrelators() { return gps_correlators; }
    auto& GetCorrelatorTriggerFlags() { return gps_correlator_trigger_flags; }
    auto& GetIsAlwaysCorrelate() { return is_always_correlate; }
};