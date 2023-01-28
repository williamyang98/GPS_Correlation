cmake_minimum_required(VERSION 3.10)
project(gps_corr)

set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${CMAKE_CURRENT_SOURCE_DIR}/cmake")
find_package(imgui REQUIRED)
find_package(implot REQUIRED)
find_package(FFTW3f REQUIRED)
find_package(fmt REQUIRED)

if(MSVC)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /fp:fast /arch:AVX2")
# NOTE: If we are on a platform without AVX2
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /fp:fast")
add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx2 -mfma -ffast-math")
# NOTE: If we are compiling with ssse3 not avx2+fma (affects pll and viterbi decoder)
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mssse3 -ffast-math")
endif()

set(SRC_DIR ${CMAKE_SOURCE_DIR}/src)

add_library(gps_lib STATIC
    ${SRC_DIR}/dsp/calculate_fft.cpp
    ${SRC_DIR}/gps/gps_correlator.cpp
    ${SRC_DIR}/gps/gps_app.cpp)
target_include_directories(gps_lib PRIVATE ${SRC_DIR})
target_compile_features(gps_lib PRIVATE cxx_std_17)
target_link_libraries(gps_lib PRIVATE FFTW3::fftw3f)

add_executable(gps_corr 
    ${SRC_DIR}/gps_corr.cpp
    ${SRC_DIR}/gui/imgui_skeleton.cpp
    ${SRC_DIR}/utility/getopt/getopt.c
)
target_include_directories(gps_corr PRIVATE ${SRC_DIR})
target_compile_features(gps_corr PRIVATE cxx_std_17)
target_link_libraries(gps_corr 
    gps_lib
    imgui implot fmt::fmt
)

add_executable(append_wav_header 
    ${SRC_DIR}/append_wav_header.cpp
    ${SRC_DIR}/utility/getopt/getopt.c
)
target_include_directories(append_wav_header PRIVATE ${SRC_DIR})
target_compile_features(append_wav_header PRIVATE cxx_std_17)

add_executable(convert_s8_to_u8 
    ${SRC_DIR}/convert_s8_to_u8.cpp
    ${SRC_DIR}/utility/getopt/getopt.c
)
target_include_directories(convert_s8_to_u8 PRIVATE ${SRC_DIR})
target_compile_features(convert_s8_to_u8 PRIVATE cxx_std_17)

if (WIN32)
target_compile_options(gps_lib              PRIVATE "/MP")
target_compile_options(gps_corr             PRIVATE "/MP")
target_compile_options(append_wav_header    PRIVATE "/MP")
target_compile_options(convert_s8_to_u8     PRIVATE "/MP")
endif (WIN32)
