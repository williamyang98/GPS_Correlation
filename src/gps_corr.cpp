#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

#include <complex>
#include <vector>
#include <thread>

#if defined(_WIN32)
#include <io.h>
#include <fcntl.h>
#endif

#include "gps/gps_app.h"

#include <glfw/glfw3.h>
#include "imgui.h"
#include "implot.h"
#include "gui/imgui_skeleton.h"
#include "gui/imgui_config.h"
#include "gui/font_awesome_definitions.h"

#include <fmt/core.h>
#include "utility/getopt/getopt.h"
#include "utility/aligned_vector.h"
#include "utility/span.h"

constexpr struct {
    const int Fcode = 1000;
    const int Fdev = 6000;
} GPS_FIXED_PARAMS;

constexpr int SIMD_ALIGN_AMOUNT = 32;

// normalise to -1 to 1
void convert_int8_to_float(tcb::span<const int8_t> x, tcb::span<float> y, const float G=1.0f) {
    assert(x.size() == y.size());
    const size_t N = x.size();
    const float K = 1.0f/127.0f * G;
    for (int i = 0; i < N; i++) {
        float v = static_cast<float>(x[i]);
        y[i] = v*K;
    }
}

// normalise to -1 to 1
void convert_uint8_to_float(tcb::span<const uint8_t> x, tcb::span<float> y, const float G=1.0f) {
    assert(x.size() == y.size());
    const size_t N = x.size();
    const float K = 1.0f/127.5f * G;
    for (int i = 0; i < N; i++) {
        float v = static_cast<float>(x[i]) - 127.5f;
        y[i] = v*K;
    }
}

class App 
{
private:
    // buffers
    FILE* fp_in;
    const bool is_u8;
    float extra_gain = 1.0f;
    bool is_running = false;
    std::unique_ptr<std::thread> runner_thread;

    AlignedVector<std::complex<uint8_t>> buf_rd_raw_in;
    AlignedVector<std::complex<float>> buf_rd_float_in;
    GPS_App gps_app;
public:
    App(FILE* const _fp_in, const int Fs, const bool _is_u8=false) 
    : fp_in(_fp_in), is_u8(_is_u8),
      gps_app(Fs, GPS_FIXED_PARAMS.Fcode, GPS_FIXED_PARAMS.Fdev)
    {
        const int N = gps_app.GetBlockSize();
        buf_rd_raw_in = AlignedVector<std::complex<uint8_t>>(N, SIMD_ALIGN_AMOUNT);
        buf_rd_float_in = AlignedVector<std::complex<float>>(N, SIMD_ALIGN_AMOUNT);
    }
    ~App() {
        is_running = false;
        if (runner_thread) {
            runner_thread->join();
        }
    }
    void Start() {
        if (is_running) return;
        is_running = true;
        runner_thread = std::make_unique<std::thread>([this]() {
            RunnerThread();
        });
    }
public:
    auto& GetExtraGain() { return extra_gain; }
    auto& GetGPSApp() { return gps_app; }
private:
    void RunnerThread() {
        while (is_running) {
            const int N = gps_app.GetBlockSize();
            const size_t nb_read = fread(buf_rd_raw_in.data(), sizeof(std::complex<uint8_t>), N, fp_in);
            if (nb_read != (size_t)N) {
                fprintf(stderr, "Failed to read in data %zu/%zu\n", nb_read, (size_t)N);
                break;
            }

            // type conversion 
            {
                const size_t M = N*2;
                auto y = tcb::span(reinterpret_cast<float*>(&buf_rd_float_in[0]), M);
                if (is_u8) {
                    auto x = tcb::span(reinterpret_cast<uint8_t*>(&buf_rd_raw_in[0]), M);
                    convert_uint8_to_float(x, y, extra_gain);
                } else {
                    auto x = tcb::span(reinterpret_cast<int8_t*>(&buf_rd_raw_in[0]), M);
                    convert_int8_to_float(x, y, extra_gain);
                }
            }

            gps_app.Process(buf_rd_float_in);
        }

        is_running = false;
    }
};

class Renderer: public ImguiSkeleton
{
private:
    App& app;
public:
    Renderer(App& _app): app(_app) {}
    virtual GLFWwindow* Create_GLFW_Window(void) {
        return glfwCreateWindow(
            1280, 720, 
            "GPS correlation", 
            NULL, NULL);
    }
    virtual void AfterImguiContextInit() {
        ImPlot::CreateContext();
        ImguiSkeleton::AfterImguiContextInit();
        auto& io = ImGui::GetIO();
        io.IniFilename =  "imgui.ini";
        io.Fonts->AddFontFromFileTTF("res/Roboto-Regular.ttf", 15.0f);
        {
            static const ImWchar icons_ranges[] = { ICON_MIN_FA, ICON_MAX_FA };
            ImFontConfig icons_config;
            icons_config.MergeMode = true;
            icons_config.PixelSnapH = true;
            io.Fonts->AddFontFromFileTTF("res/font_awesome.ttf", 16.0f, &icons_config, icons_ranges);
        }
        ImGuiSetupCustomConfig();
    }

    virtual void Render() {
        auto& gps_app = app.GetGPSApp();

        if (ImGui::Begin("GPS")) {
            ImGui::Text("Total blocks = %d", gps_app.GetTotalBlocksRead());
            ImGui::SliderFloat(
                "Extra Gain", 
                &app.GetExtraGain(), 
                1.0f, 10.0f, "%.0f", 
                ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_ClampOnInput);
            static bool is_show_peak_line = false;
            static int selected_freq_index = 0;
            ImGui::Checkbox("Is always correlate", &gps_app.GetIsAlwaysCorrelate());
            ImGui::Checkbox("Is show peak line", &is_show_peak_line);

            enum DisplayMode {
                BEST, MODE, MANUAL
            };

            static auto get_display_mode_string = [](DisplayMode mode) {
                switch (mode) {
                case DisplayMode::MODE:     return "Mode";
                case DisplayMode::BEST:     return "Best";
                case DisplayMode::MANUAL:   return "Manual";
                default:                    return "Unknown";
                }
            };

            static auto display_mode = DisplayMode::MODE;

            if (ImGui::BeginCombo("Display mode", get_display_mode_string(display_mode))) {
                static auto render_entry = [](DisplayMode mode, DisplayMode& curr) {
                    const bool is_selected = (mode == curr);
                    if (ImGui::Selectable(get_display_mode_string(mode), is_selected)) {
                        curr = mode;
                    }
                };
                render_entry(DisplayMode::MODE, display_mode);
                render_entry(DisplayMode::BEST, display_mode);
                render_entry(DisplayMode::MANUAL, display_mode);
                ImGui::EndCombo();
            }

            if (ImGui::BeginTabBar("Correlators")) {
                auto& correlators = gps_app.GetCorrelators();
                auto& trigger_flags = gps_app.GetCorrelatorTriggerFlags();
                const int total_correlators = (int)correlators.size();
                for (int i = 0; i < total_correlators; i++) {
                    const int prn_id = i+1;
                    auto& correlator = correlators[i];
                    auto& trigger_flag = trigger_flags[i];

                    ImGui::PushID(prn_id);
                    auto tab_label = fmt::format("{}", prn_id);
                    if (ImGui::BeginTabItem(tab_label.c_str())) {
                        // NOTE: We trigger an update from the gui thread
                        //       We use an integer to overupdate
                        trigger_flag = 100;
                        auto& correlations = correlator.GetCorrelations();
                        auto& freq_offsets = correlator.GetFrequencyOffsets();
                        const int TOTAL_CORRELATIONS = (int)correlations.size();

                        int freq_index = 0;
                        switch (display_mode) {
                        case DisplayMode::BEST:
                            freq_index = correlator.GetBestFrequencyOffsetIndex();
                            break;
                        case DisplayMode::MODE:
                            freq_index = correlator.GetModeFrequencyOffsetIndex();
                            break;
                        case DisplayMode::MANUAL:
                            ImGui::SliderInt(
                                "Selected frequency", 
                                &selected_freq_index, 0, TOTAL_CORRELATIONS-1, 
                                "%d", 
                                ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_ClampOnInput); 
                            freq_index = selected_freq_index;
                            break;
                        }

                        const float freq_offset = freq_offsets[freq_index];
                        auto& x_corr = correlations[freq_index];
                        ImGui::Text("Frequency offset= %.1fkHz", freq_offset * 1e-3f);
                        if (ImPlot::BeginPlot("Correlation Peak")) {
                            ImPlot::SetupAxisLimits(ImAxis_Y1, 0.0f, 100.0f, ImPlotCond_Once);
                            ImPlot::PlotLine("Magnitude", x_corr.data(), (int)x_corr.size());

                            if (is_show_peak_line) {
                                int peak_index = 0; 
                                float peak_value = 0.0f;
                                GetCorrelationPeakIndex(x_corr, peak_index, peak_value);
                                double marker_0 = (double)peak_index;
                                double marker_1 = (double)peak_value;
                                int marker_id = 0;
                                ImPlot::DragLineX(marker_id++, &marker_0, ImVec4(1,0,0,1), 1.0f, ImPlotDragToolFlags_NoInputs);
                                ImPlot::DragLineY(marker_id++, &marker_1, ImVec4(1,0,0,1), 1.0f, ImPlotDragToolFlags_NoInputs);
                            }
                            ImPlot::EndPlot();
                        }
                        ImGui::EndTabItem();

                    }
                    ImGui::PopID();
                }

                ImGui::EndTabBar();
            }
        }
        ImGui::End();
    }
    virtual void AfterShutdown() {
        ImPlot::DestroyContext();
    }
private:
    void GetCorrelationPeakIndex(tcb::span<const float> x, int& index, float& value) {
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
};


void usage() {
    fprintf(stderr, 
        "gps_corr, Displays GPS correlation data for every PRN code\n\n"
        "\t[-i input filename (default: None)]\n"
        "\t    If no file is provided then stdin is used\n"
        "\t[-f sample rate (default: 2048000Hz)]\n"
        "\t[-F IQ format (default: u8) (options: u8, s8)]\n"
        "\t[-g extra gain (default: 1)]\n"
        "\t[-A (Always run correlation on each PRN)]\n"
        "\t[-h (show usage)]\n"
    );
}

int main(int argc, char** argv) {
    char* rd_filename = NULL;
    float extra_gain = 1.0f;
    bool is_u8 = true;
    bool is_always_correlate = false;
    int Fs = 2'048'000;

    int opt; 
    while ((opt = getopt_custom(argc, argv, "i:f:F:g:Ah")) != -1) {
        switch (opt) {
        case 'i':
            rd_filename = optarg;
            break;
        case 'f':
            Fs = (int)atof(optarg);
            break;
        case 'F':
            if (strncmp("u8", optarg, 3) == 0) {
                is_u8 = true;
            } else if (strncmp("s8", optarg, 3) == 0) {
                is_u8 = false;
            }
            break;
        case 'g':
            extra_gain = (float)atof(optarg);
            break;
        case 'A':
            is_always_correlate = true;
            break;
        case 'h':
        default:
            usage();
            return 0;
        }
    }

    if (Fs <= 0) {
        fprintf(stderr, "Got invalid sample rate %d <= 0\n", Fs);
        return 1;
    }

    if ((Fs % GPS_FIXED_PARAMS.Fcode) != 0) {
        fprintf(stderr, "WARNING: Got sample rate %d which is not a multiple of PRN code rate %d\n", 
            Fs, GPS_FIXED_PARAMS.Fcode);
        // return 1;
    }

    FILE* fp_in = stdin;
    if (rd_filename != NULL) {
        fp_in = fopen(rd_filename, "rb");
        if (fp_in == NULL) {
            fprintf(stderr, "Failed to open file for reading\n");
            return 1;
        }
    }

#ifdef _WIN32
    _setmode(_fileno(fp_in), _O_BINARY);
#endif

    auto app = App(fp_in, Fs, is_u8);
    auto& gps_app = app.GetGPSApp();
    app.GetExtraGain() = extra_gain;
    gps_app.GetIsAlwaysCorrelate() = is_always_correlate;

    auto renderer = Renderer(app);
    app.Start();
    const int rv = RenderImguiSkeleton(&renderer);
    return rv; 
}
