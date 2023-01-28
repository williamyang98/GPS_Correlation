#!/bin/sh
# This script generates fake gps signals using gps-sdr-sim
./tools/gps-sdr-sim/gps-sdr-sim.exe -e tools/gps-sdr-sim/brdc3540.14n -o data/gpssim_s8.bin -s 2048000 -b 8