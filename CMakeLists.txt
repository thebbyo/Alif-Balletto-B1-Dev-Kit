cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(ai_sine_wave)

target_sources(app PRIVATE src/main.cpp)

# Vela compiler memory modes mapping for Ethos-U55
zephyr_compile_definitions(NPU_REGIONCFG_0=3)
zephyr_compile_definitions(NPU_REGIONCFG_1=0)
zephyr_compile_definitions(NPU_REGIONCFG_2=0)
zephyr_compile_definitions(NPU_QCONFIG=3)
