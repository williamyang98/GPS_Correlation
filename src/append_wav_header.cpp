#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <vector>

#if defined(_WIN32)
#include <io.h>
#include <fcntl.h>
#endif

#include "utility/getopt/getopt.h"

// Source: http://soundfile.sapp.org/doc/WaveFormat/
struct WavHeader {
    char     ChunkID[4];
    int32_t  ChunkSize;
    char     Format[4];
    // Subchunk 1 = format information
    char     Subchunk1ID[4];
    int32_t  Subchunk1Size;
    int16_t  AudioFormat;
    int16_t  NumChannels;
    int32_t  SampleRate;
    int32_t  ByteRate;
    int16_t  BlockAlign;
    int16_t  BitsPerSample;
    // Subchunk 2 = data 
    char     Subchunk2ID[4];
    int32_t  Subchunk2Size;
} header;

void init_header(WavHeader& header, const int nb_data_bytes, const int32_t SampleRate) {
    const int32_t Subchunk2Size = nb_data_bytes;
    const int32_t ChunkSize = 36 + Subchunk2Size;
    const int16_t NumChannels = 2;
    const int32_t BitsPerSample = 8;
    
    // Header
    strncpy(header.ChunkID, "RIFF", 4);
    header.ChunkSize = ChunkSize;
    strncpy(header.Format, "WAVE", 4);
    
    // Subchunk 1
    strncpy(header.Subchunk1ID, "fmt ", 4);
    header.Subchunk1Size = 16;  // size of PCM format fields 
    header.AudioFormat = 1;     // Linear quantisation
    header.NumChannels = NumChannels;
    header.SampleRate = SampleRate;
    header.BitsPerSample = BitsPerSample;
    header.ByteRate = header.SampleRate * header.NumChannels * header.BitsPerSample / 8;
    header.BlockAlign = header.NumChannels * header.BitsPerSample / 8;

    // Subchunk 2
    strncpy(header.Subchunk2ID, "data", 4);
    header.Subchunk2Size = Subchunk2Size;
}

void update_wav_header(FILE* fp, const int nb_data_bytes) {
    const int32_t Subchunk2Size = nb_data_bytes;
    const int32_t ChunkSize = 36 + Subchunk2Size;

    // Source: http://soundfile.sapp.org/doc/WaveFormat/
    // Refer to offset of each field
    // ChunkSize
    fseek(fp, 4, SEEK_SET);
    fwrite(&ChunkSize, sizeof(int32_t), 1, fp);
    // Subchunk2Size
    fseek(fp, 40, SEEK_SET);
    fwrite(&Subchunk2Size, sizeof(int32_t), 1, fp);

    fseek(fp, 0, SEEK_END);
}

void usage() {
    fprintf(stderr, 
        "append_wav_header, Adds wav header to raw IQ samples\n\n"
        "\t[-i input filename (default: None)]\n"
        "\t    If no file is provided then stdin is used\n"
        "\t[-o output filename (default: None)]\n"
        "\t    If no file is provided then stdout is used\n"
        "\t[-f sample_rate (default: 2'048'000)]\n"
        "\t[-F IQ format (default: s8) (options: u8, s8)]\n"
        "\t[-b block_size (default: 8192*16)]\n"
        "\t[-h (show usage)]\n"
    );
}

// Instructions 
// 1. ./generate_gps_data.sh
// 2. ./build/Release/append_wav_header.exe -i data/gpssim_s8.bin -o data/gpssim.wav 
int main(int argc, char** argv) {
    char* rd_filename = NULL;
    char* wr_filename = NULL;
    bool is_u8 = false;
    int32_t sample_rate = 2'048'000;
    int block_size = 8192*16;

    int opt; 
    while ((opt = getopt_custom(argc, argv, "i:o:f:F:b:h")) != -1) {
        switch (opt) {
        case 'i':
            rd_filename = optarg;
            break;
        case 'o':
            wr_filename = optarg;
            break;
        case 'f':
            sample_rate = (int32_t)atof(optarg);
            break;
        case 'F':
            if (strncmp("u8", optarg, 3) == 0) {
                is_u8 = true;
            } else if (strncmp("s8", optarg, 3) == 0) {
                is_u8 = false;
            }
            break;
        case 'b':
            block_size = (int)atof(optarg);
            break;
        case 'h':
        default:
            usage();
            return 0;
        }
    }

    if (sample_rate <= 0) {
        fprintf(stderr, "Got invalid sample rate %d <= 0\n", sample_rate);
        return 1;
    }

    if (block_size <= 0) {
        fprintf(stderr, "Got invalid block size %d <= 0\n", block_size);
        return 1;
    }

    FILE* fp_out = stdout;
    FILE* fp_in = stdin;

    if (rd_filename != NULL) {
        fp_in = fopen(rd_filename, "rb");
        if (fp_in == NULL) {
            fprintf(stderr, "Failed to open file for reading\n");
            return 1;
        }
    }

    if (wr_filename != NULL) {
        fp_out = fopen(wr_filename, "wb");
        if (fp_out == NULL) {
            fprintf(stderr, "Failed to open file for writing\n");
            return 1;
        }
    }

    #if defined(_WIN32)
    _setmode(_fileno(fp_in), _O_BINARY);
    _setmode(_fileno(fp_out), _O_BINARY);
    #endif

    WavHeader header;
    init_header(header, 0, sample_rate);
    fwrite(&header, sizeof(WavHeader), 1, fp_out); 
    
    auto rd_buf = std::vector<uint8_t>(block_size);
    size_t nb_data_bytes = 0;
    bool is_running = true;

    while (is_running) {
        const auto nb_read = fread(rd_buf.data(), sizeof(uint8_t), block_size, fp_in);
        if (nb_read != (size_t)block_size) {
            is_running = false;
        }
        nb_data_bytes += nb_read;

        // inplace convert
        if (!is_u8) {
            auto x = reinterpret_cast<int8_t*>(rd_buf.data());
            auto y = reinterpret_cast<uint8_t*>(rd_buf.data());
            for (size_t i = 0; i < nb_read; i++) {
                y[i] = (uint8_t)((int)x[i] + 127);
            }
        }

        fwrite(rd_buf.data(), sizeof(uint8_t), nb_read, fp_out);
    }
    
    update_wav_header(fp_out, (int)nb_data_bytes);
    fprintf(stderr, "Wrote %zu bytes with Fs=%d\n", nb_data_bytes, sample_rate);
    return 0;
}
