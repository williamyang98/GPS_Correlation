#!/bin/sh
# This script is used to get data from a bias T powered GPS patch antenna
# Expects the RTLSDR Blog v3 as the receiver
./tools/rtl_sdr/rtl_sdr.exe -f 1575420000 -g 0 -s 2048000 -T