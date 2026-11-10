# Toolbox for Tab5 OS. Build with ./build.sh (it fetches the SDK and calls this).
# To use CMake directly: cmake -S . -B build -G Ninja [-DT5_SDK=<sdk dir>] && cmake --build build
cmake_minimum_required(VERSION 3.22)

# The SDK: -DT5_SDK, $T5_SDK, or the one ./build.sh fetched into .t5sdk/.
if(NOT T5_SDK)
    set(T5_SDK "$ENV{T5_SDK}")
endif()
if(NOT T5_SDK)
    file(STRINGS ${CMAKE_CURRENT_LIST_DIR}/tab5os-sdk.txt T5_SDK_VERSION LIMIT_COUNT 1)
    set(T5_SDK ${CMAKE_CURRENT_LIST_DIR}/.t5sdk/tab5os-sdk-${T5_SDK_VERSION}/sdk)
endif()
if(NOT EXISTS ${T5_SDK}/cmake/t5_app.cmake)
    message(FATAL_ERROR "No Tab5 OS SDK at ${T5_SDK}: run ./build.sh once, or set T5_SDK")
endif()
set(CMAKE_TOOLCHAIN_FILE ${T5_SDK}/cmake/toolchain-t5.cmake)

project(toolbox C)
include(${T5_SDK}/cmake/t5_app.cmake)

t5_app(toolbox
    SRCS
        main.c ui.c
        tools_conv.c tools_num.c tools_elec.c tools_time.c tools_colour.c tools_measure.c
        engine/fmt.c engine/units.c engine/numtools.c engine/elec.c engine/datecalc.c engine/colour.c
        engine/measure.c
        fonts/tb_sym_28.c fonts/tb_sym_32.c fonts/tb_sym_48.c fonts/tb_sym_68.c
    MANIFEST manifest.toml
    ICON icon.png)
target_compile_options(toolbox.elf PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-format-truncation -Werror)
# A map for turning a crash's app+0x... offset into a function name.
target_link_options(toolbox.elf PRIVATE -Wl,-Map=${CMAKE_BINARY_DIR}/toolbox.map)
