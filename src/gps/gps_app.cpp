#include "gps_app.h"
#include "gps_prn_constants.h"
#include "prn_code.h"
#include "dsp/calculate_fft.h"
#include <stdint.h>
#include <assert.h>

// NOTE: AVX2 requires 256bit = 32byte alignment
constexpr int SIMD_ALIGN_AMOUNT = 32;

GPS_App::GPS_App(const int _Fs, const int _Fcode, const int _Fdev_max)
: block_size(_Fs/_Fcode)
{
    assert(block_size > 0);
    assert(_Fs > 0);
    assert(_Fcode > 0);

    auto prn_code = std::vector<uint8_t>(PRN_CODE_LENGTH);
    gps_correlators.reserve(TOTAL_PRN_CODES);
    gps_correlator_trigger_flags.resize(TOTAL_PRN_CODES, 0);
    for (int prn_id = 0; prn_id < TOTAL_PRN_CODES; prn_id++) {
        generate_prn_code<uint8_t>(prn_code, PRN_OUTPUT_TAPS[prn_id]);
        auto corr = GPS_Correlator(prn_code, block_size, _Fcode, _Fs, _Fdev_max);
        gps_correlators.push_back(std::move(corr));
    }

    fft_buf = AlignedVector<std::complex<float>>(block_size, SIMD_ALIGN_AMOUNT);
}

void GPS_App::Process(tcb::span<const std::complex<float>> x) {
    assert(x.size() == (size_t)block_size);
    assert(((uintptr_t)x.data() % SIMD_ALIGN_AMOUNT) == 0u);

    CalculateFFT(x, fft_buf);
    const size_t total_correlators = gps_correlators.size();
    for (size_t i = 0; i < total_correlators; i++) {
        auto& correlator = gps_correlators[i];
        auto& trigger_flag = gps_correlator_trigger_flags[i];

        bool is_correlate = false;
        if (trigger_flag > 0) {
            is_correlate = true;
            trigger_flag--;
        }
        is_correlate = is_correlate || is_always_correlate;

        if (is_correlate) {
            gps_correlator_thread_pool.PushTask([&correlator, this]() {
                correlator.Process(fft_buf);
            });
        }
    }

    gps_correlator_thread_pool.WaitAll();
    total_blocks_read++;
}