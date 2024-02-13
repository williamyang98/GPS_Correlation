# Introduction
[![x86-windows](https://github.com/williamyang98/GPS_Correlation/actions/workflows/x86-windows.yml/badge.svg)](https://github.com/williamyang98/GPS_Correlation/actions/workflows/x86-windows.yml)

Displays the correlation peaks when attempting to acquire a GPS signal.

Useful to debug in realtime whether a gps signal is present allowing for quick adjustments to location or orientation of antenna. 

This is quite useful when using software like [GNSS-SDRLIB](https://github.com/taroz/GNSS-SDRLIB) which doesn't expose this data in realtime. Instead limited information is only shown in a text window which only updates every few seconds.

This is not GPS decoding software, it is only used for testing whether or not your gps receiver has a valid signal.

# Gallery
![GUI](docs/screenshot_v1.png)

# Useful links
1. How GPS PRN codes are generated [link](https://natronics.github.io/blag/2014/gps-prn)
2. How GPS spread codes work [link](https://natronics.github.io/blag/2014/gps-spreading/)
3. Instructions for running SDR software for decoding GPS signals [youtube-link](https://www.youtube.com/watch?v=YG2fJRTAoHA)
4. Website for determining which satellites are visible from your location [gnss-radar](http://taroz.net/GNSS-Radar.html)

# Build instructions
1. Use windows with C++ Visual Studio development tools and vcpkg installed.
2. Clone repository with submodules.
3. Open up the x64 C++ developer environment for Visual Studio.
4. Configure cmake: ```cmake . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=C:\tools\vcpkg\scripts\buildsystems\vcpkg.cmake```
5. Build: ```cmake --build build --config Release```

# Run instructions
Refer to ```./build/Release/gps_corr.exe -h``` for instructions.

Check each PRN code from 1 to 32 and see if there are any correlation peaks. If there is a stable and prominent peak then a satellite is visible. You can then adjust your antenna's position for the best receptin in realtime.

| Usage | Command |
| --- | --- | 
| Running from RTLSDR v3 dongle | ```./get_live_samples.sh \| ./gps_corr.exe -F u8``` | 
| Generating synthetic GPS data | ```./generate_gps_data.sh``` |
| Running on synthetic GPS data | ```./gps_corr.exe -i data/gpssim_s8.bin -A -F s8``` |

**NOTE**: Synthetic GPS data has satellites with PRNs of ```[2,5,12,13,14,15,18,21,22,24,25,26,29]```

**NOTE**: Use [gnss-radar](http://taroz.net/GNSS-Radar.html) to quickly skip to the most likely satellites in your location when reading from your RTLSDR v3 blog dongle.
