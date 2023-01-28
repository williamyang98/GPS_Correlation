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

void usage() {
    fprintf(stderr, 
        "convert_s8_to_u8, Converts raw IQ signed 8bit values to unsigned 8bit values\n\n"
        "\t[-i input filename (default: None)]\n"
        "\t    If no file is provided then stdin is used\n"
        "\t[-o output filename (default: None)]\n"
        "\t    If no file is provided then stdout is used\n"
        "\t[-b block_size (default: 8192*16)]\n"
        "\t[-h (show usage)]\n"
    );
}

// Instructions
// ./generate_gps_data.sh
// ./convert_s8_to_u8.exe -i data/gpssim_s8.bin -o data/gpssim_u8.bin 
int main(int argc, char** argv) {
    char* rd_filename = NULL;
    char* wr_filename = NULL;
    int block_size = 8192*16;

    int opt; 
    while ((opt = getopt_custom(argc, argv, "i:o:b:h")) != -1) {
        switch (opt) {
        case 'i':
            rd_filename = optarg;
            break;
        case 'o':
            wr_filename = optarg;
            break;
        case 'b':
            block_size = (int32_t)atof(optarg);
            break;
        case 'h':
        default:
            usage();
            return 0;
        }
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

    auto rd_buf = std::vector<int8_t>(block_size);
    auto wr_buf = std::vector<uint8_t>(block_size);
    size_t nb_data_bytes = 0;
    bool is_running = true;

    while (is_running) {
        const auto nb_read = fread(rd_buf.data(), sizeof(int8_t), (size_t)block_size, fp_in);
        if (nb_read != (size_t)block_size) {
            is_running = false;
        }
        nb_data_bytes += nb_read;

        for (size_t i = 0; i < nb_read; i++) {
            auto& v0 = rd_buf[i];
            auto& v1 = wr_buf[i];
            v1 = (uint8_t)((int)v0 + 127);
        }

        fwrite(wr_buf.data(), sizeof(uint8_t), nb_read, fp_out);
    }
    
    fprintf(stderr, "Wrote %zu bytes\n", nb_data_bytes);
    return 0;
}
