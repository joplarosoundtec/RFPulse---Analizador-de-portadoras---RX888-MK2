# Scaffolding de tests: se anadira un add_executable(...) por modulo cuando ese
# modulo se implemente (empezando por test_ring_buffer.cpp en la tarea de
# Ring Buffers). GoogleTest se deja listo aqui para no repetir este fetch en
# cada tarea futura.

include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.15.2
    GIT_SHALLOW    TRUE
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

add_executable(test_ring_buffer test_ring_buffer.cpp)
target_include_directories(test_ring_buffer PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_ring_buffer PRIVATE rfpulse_compiler_options gtest_main)
gtest_discover_tests(test_ring_buffer)

add_executable(test_fast_math test_fast_math.cpp)
target_include_directories(test_fast_math PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_fast_math PRIVATE rfpulse_compiler_options gtest_main)
gtest_discover_tests(test_fast_math)

add_executable(test_window test_window.cpp ${CMAKE_SOURCE_DIR}/src/fft/Window.cpp)
target_include_directories(test_window PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_window PRIVATE rfpulse_compiler_options gtest_main)
gtest_discover_tests(test_window)

add_executable(test_fftw_engine
    test_fftw_engine.cpp
    ${CMAKE_SOURCE_DIR}/src/fft/Window.cpp
    ${CMAKE_SOURCE_DIR}/src/fft/FftwEngine.cpp
)
target_include_directories(test_fftw_engine PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_fftw_engine PRIVATE rfpulse_compiler_options FftwFloat::fftw3f gtest_main)
add_dependencies(test_fftw_engine fftw_build)
gtest_discover_tests(test_fftw_engine)

add_executable(test_iq_acquisition
    test_iq_acquisition.cpp
    ${CMAKE_SOURCE_DIR}/src/acquisition/IqAcquisition.cpp
)
target_include_directories(test_iq_acquisition PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_iq_acquisition PRIVATE rfpulse_compiler_options gtest_main)
gtest_discover_tests(test_iq_acquisition)

add_executable(test_triple_buffer test_triple_buffer.cpp)
target_include_directories(test_triple_buffer PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_triple_buffer PRIVATE rfpulse_compiler_options gtest_main)
gtest_discover_tests(test_triple_buffer)

add_executable(test_palette test_palette.cpp ${CMAKE_SOURCE_DIR}/src/waterfall/Palette.cpp)
target_include_directories(test_palette PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_palette PRIVATE rfpulse_compiler_options gtest_main)
gtest_discover_tests(test_palette)

add_executable(test_waterfall_engine
    test_waterfall_engine.cpp
    ${CMAKE_SOURCE_DIR}/src/waterfall/WaterfallEngine.cpp
)
target_include_directories(test_waterfall_engine PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_waterfall_engine PRIVATE rfpulse_compiler_options gtest_main)
gtest_discover_tests(test_waterfall_engine)

add_executable(test_spectrum_processor
    test_spectrum_processor.cpp
    ${CMAKE_SOURCE_DIR}/src/spectrum/SpectrumProcessor.cpp
)
target_include_directories(test_spectrum_processor PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_spectrum_processor PRIVATE rfpulse_compiler_options gtest_main)
gtest_discover_tests(test_spectrum_processor)

add_executable(test_peak_detector test_peak_detector.cpp ${CMAKE_SOURCE_DIR}/src/spectrum/PeakDetector.cpp)
target_include_directories(test_peak_detector PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_peak_detector PRIVATE rfpulse_compiler_options gtest_main)
gtest_discover_tests(test_peak_detector)

add_executable(test_nco test_nco.cpp ${CMAKE_SOURCE_DIR}/src/dsp/Nco.cpp)
target_include_directories(test_nco PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_nco PRIVATE rfpulse_compiler_options gtest_main)
gtest_discover_tests(test_nco)

add_executable(test_fir_filter test_fir_filter.cpp ${CMAKE_SOURCE_DIR}/src/dsp/FirFilter.cpp)
target_include_directories(test_fir_filter PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_fir_filter PRIVATE rfpulse_compiler_options gtest_main)
gtest_discover_tests(test_fir_filter)

add_executable(test_fm_demodulator test_fm_demodulator.cpp ${CMAKE_SOURCE_DIR}/src/demod/FmDemodulator.cpp)
target_include_directories(test_fm_demodulator PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_fm_demodulator PRIVATE rfpulse_compiler_options gtest_main)
gtest_discover_tests(test_fm_demodulator)

add_executable(test_deemphasis test_deemphasis.cpp ${CMAKE_SOURCE_DIR}/src/demod/Deemphasis.cpp)
target_include_directories(test_deemphasis PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_deemphasis PRIVATE rfpulse_compiler_options gtest_main)
gtest_discover_tests(test_deemphasis)

add_executable(test_squelch test_squelch.cpp ${CMAKE_SOURCE_DIR}/src/demod/Squelch.cpp)
target_include_directories(test_squelch PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_squelch PRIVATE rfpulse_compiler_options gtest_main)
gtest_discover_tests(test_squelch)

add_executable(test_vfo
    test_vfo.cpp
    ${CMAKE_SOURCE_DIR}/src/dsp/Nco.cpp
    ${CMAKE_SOURCE_DIR}/src/dsp/FirFilter.cpp
    ${CMAKE_SOURCE_DIR}/src/demod/FmDemodulator.cpp
    ${CMAKE_SOURCE_DIR}/src/demod/Deemphasis.cpp
    ${CMAKE_SOURCE_DIR}/src/demod/Squelch.cpp
    ${CMAKE_SOURCE_DIR}/src/vfo/Vfo.cpp
)
target_include_directories(test_vfo PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_vfo PRIVATE rfpulse_compiler_options gtest_main)
gtest_discover_tests(test_vfo)

add_executable(test_audio_output test_audio_output.cpp ${CMAKE_SOURCE_DIR}/src/audio/AudioOutput.cpp)
target_include_directories(test_audio_output PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_audio_output PRIVATE rfpulse_compiler_options gtest_main ole32)
gtest_discover_tests(test_audio_output)

add_executable(test_settings test_settings.cpp ${CMAKE_SOURCE_DIR}/src/core/Settings.cpp)
target_include_directories(test_settings PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_settings PRIVATE rfpulse_compiler_options gtest_main)
gtest_discover_tests(test_settings)

add_executable(test_logger test_logger.cpp ${CMAKE_SOURCE_DIR}/src/core/Logger.cpp)
target_include_directories(test_logger PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_logger PRIVATE rfpulse_compiler_options gtest_main)
gtest_discover_tests(test_logger)
