# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.0)
set(toolchainVersion 13_3_Rel1)
set(picotoolVersion 2.1.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.0)
set(toolchainVersion 13_3_Rel1)
set(picotoolVersion 2.1.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.0)
set(toolchainVersion 13_3_Rel1)
set(picotoolVersion 2.1.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(PicoHSP C CXX ASM)

set(PICO_CXX_ENABLE_EXCEPTIONS 1)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# HSP Configuration
# -DNDEBUG -DHSPDISH -DHSPDEBUG
add_compile_definitions(HSPDISH)
add_compile_definitions(HSPRPIPICO)
add_compile_definitions(HSPDEBUG)

# Add executable. Default name is the project name, version 0.1

add_executable(PicoHSP
    # PicoHSP.cpp

    src/hsp3/dpmread.cpp
    # src/hsp3dish/geometry.cpp
    src/hsp3/hsp3.cpp
    src/hsp3/hsp3code.cpp
    src/hsp3/hsp3debug.cpp
    src/hsp3/hsp3int.cpp
    src/hsp3/hspvar_core.cpp
    src/hsp3/hspvar_double.cpp
    src/hsp3/hspvar_int.cpp
    src/hsp3/hspvar_label.cpp
    src/hsp3/hspvar_str.cpp
    src/hsp3/hspvar_struct.cpp
    # src/hsp3dish/hspwnd_dish.cpp
    # src/hsp3dish/hspwnd_obj.cpp
    # src/hsp3dish/hspwnd_edit.cpp
    src/hsp3dish/random.cpp
    src/hsp3/stack.cpp
    src/hsp3/strbuf.cpp
    src/hsp3/strnote.cpp
    src/hsp3dish/rpipico/hsp3gr_rpipico.cpp
    src/hsp3/rpipico/hsp3ext_rpipico.cpp
    src/hsp3/rpipico/devctrl_io.cpp
    src/hsp3dish/sysreq.cpp
    # src/hsp3dish/texmes.cpp
    # src/hsp3dish/essprite.cpp
    # src/hsp3dish/emscripten/fontsystem.cpp
    # src/hsp3dish/emscripten/hgtex.cpp
    src/hsp3dish/rpipico/hgiox.cpp
    src/hsp3dish/rpipico/hsp3dish.cpp
    src/hsp3dish/rpipico/main.cpp
    # src/hsp3dish/emscripten/mmman.cpp
    # src/hsp3dish/emscripten/webtask_emscripten.cpp
    # src/hsp3dish/emscripten/stb_image.cpp
    src/hsp3dish/rpipico/supio_rpipico.cpp
)

pico_set_program_name(PicoHSP "PicoHSP")
pico_set_program_version(PicoHSP "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(PicoHSP 1)
pico_enable_stdio_usb(PicoHSP 1)

# Add the standard library to the build
target_link_libraries(PicoHSP
        pico_stdlib)

# Add the standard include files to the build
target_include_directories(PicoHSP PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}
  ../../src
)

target_compile_options(PicoHSP PRIVATE -Wno-write-strings)

# Add any user requested libraries
target_link_libraries(PicoHSP 
        hardware_spi
        hardware_i2c
        hardware_timer
        hardware_clocks
        pico_cyw43_arch_none
        )

pico_add_extra_outputs(PicoHSP)

