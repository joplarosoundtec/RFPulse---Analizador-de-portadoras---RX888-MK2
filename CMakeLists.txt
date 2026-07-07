cmake_minimum_required(VERSION 3.24)
project(RfPulse VERSION 0.10.0 LANGUAGES CXX)

if (NOT WIN32)
    message(FATAL_ERROR "RfPulse solo soporta Windows 10/11 (WASAPI, DirectX11, driver CyAPI del FX3).")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

option(RFPULSE_BUILD_TESTS "Compilar los tests unitarios" ON)

include(CompilerWarnings)
include(FetchFftw)
include(FetchImGui)
include(FetchImPlot)

# ---------------------------------------------------------------------------
# SDR Core vendorizado (RadioHandlerClass + motor r2iq + control FX3/CyAPI).
# Ver third_party/sddc_core/Core, copiado sin modificar desde ExtIO_sddc-master
# (SDK oficial de referencia para la familia RX888). USE_SIMD_OPTIMIZATIONS es
# leida (no declarada) por Core/CMakeLists.txt: si se deja indefinida, CMake la
# trata como falsa y desactivaria el SIMD del motor r2iq, justo lo contrario de
# lo que queremos.
# ---------------------------------------------------------------------------
set(USE_SIMD_OPTIMIZATIONS ON)
add_subdirectory(third_party/sddc_core/Core)
add_dependencies(SDDC_CORE fftw_build)

# Core/CMakeLists.txt (vendorizado, sin modificar) no expone sus propios
# directorios de include via target_include_directories: sus fuentes internas
# solo usan includes "entre comillas" relativos entre si. Para que nuestro
# codigo (src/sdr/SddcDevice.*) pueda incluir RadioHandler.h/FX3Class.h/
# Interface.h, lo anadimos aqui, en el punto donde ya conocemos el target.
target_include_directories(SDDC_CORE PUBLIC
    ${CMAKE_SOURCE_DIR}/third_party/sddc_core
    ${CMAKE_SOURCE_DIR}/third_party/sddc_core/Core
)

# El Core vendorizado se escribio y se probo contra /std:c++17 (asi lo compila
# el proyecto original). Nuestro CMAKE_CXX_STANDARD global es 20, y MSVC activa
# /permissive- automaticamente a partir de /std:c++20, lo que expone un bug
# real y preexistente en el propio SDK (arch/win32/FX3handler.cpp:303, un
# "return nullptr;" en una funcion que devuelve bool) que antes pasaba
# desapercibido bajo conformidad laxa. En vez de tocar el codigo vendorizado,
# fijamos el estandar de este target especifico a C++17, replicando el entorno
# en el que el Core realmente fue validado por su autor.
set_target_properties(SDDC_CORE PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

add_subdirectory(src)

if (RFPULSE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
