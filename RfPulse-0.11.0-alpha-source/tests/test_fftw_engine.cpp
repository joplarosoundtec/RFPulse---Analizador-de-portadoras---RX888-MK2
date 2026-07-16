#include "fft/FftwEngine.h"

#include <gtest/gtest.h>

#include <cmath>
#include <complex>
#include <numbers>
#include <vector>

using rfpulse::fft::FftwEngine;
using rfpulse::fft::WindowType;

namespace {

std::vector<std::complex<float>> GenerateOnGridTone(std::size_t n, std::size_t binIndex)
{
    std::vector<std::complex<float>> signal(n);
    const double phaseStep = 2.0 * std::numbers::pi * static_cast<double>(binIndex) / static_cast<double>(n);
    for (std::size_t i = 0; i < n; ++i) {
        const double phase = phaseStep * static_cast<double>(i);
        signal[i] = std::complex<float>(static_cast<float>(std::cos(phase)), static_cast<float>(std::sin(phase)));
    }
    return signal;
}

} // namespace

TEST(FftwEngine, SupportsOnlyDeclaredSizes)
{
    FftwEngine engine("test_fftw_wisdom.dat");
    EXPECT_TRUE(engine.supportsSize(1024));
    EXPECT_TRUE(engine.supportsSize(16384));
    EXPECT_FALSE(engine.supportsSize(1000));
    EXPECT_FALSE(engine.supportsSize(0));
}

TEST(FftwEngine, UnsupportedSizeReturnsEmptyResult)
{
    FftwEngine engine("test_fftw_wisdom.dat");
    const std::vector<std::complex<float>> signal(1000, std::complex<float>(1.0f, 0.0f));
    const auto result = engine.process(signal.data(), signal.size());
    EXPECT_EQ(result.binCount, 0u);
    EXPECT_EQ(result.powerLinear, nullptr);
    EXPECT_EQ(result.magnitudeDb, nullptr);
}

TEST(FftwEngine, OnGridFullScaleToneReadsZeroDbfsAtExpectedShiftedBin)
{
    constexpr std::size_t n = 1024;
    constexpr std::size_t rawBin = 100; // frecuencia positiva, lejos de DC y de los bordes

    FftwEngine engine("test_fftw_wisdom.dat");
    const auto signal = GenerateOnGridTone(n, rawBin);

    const auto result = engine.process(signal.data(), signal.size());
    ASSERT_EQ(result.binCount, n);

    const std::size_t expectedShiftedBin = (rawBin + n / 2) % n;

    // El pico debe estar exactamente en el bin esperado tras el fftshift.
    std::size_t peakBin = 0;
    float peakDb = result.magnitudeDb[0];
    for (std::size_t i = 1; i < n; ++i) {
        if (result.magnitudeDb[i] > peakDb) {
            peakDb = result.magnitudeDb[i];
            peakBin = i;
        }
    }
    EXPECT_EQ(peakBin, expectedShiftedBin);

    // Ventana normalizada a ganancia coherente unidad: un tono a plena
    // escala exactamente sobre un bin debe leerse ~0 dBFS (ver derivacion en
    // el comentario de FftwEngine::process: Y[k] = N * mean(w) = N).
    EXPECT_NEAR(peakDb, 0.0f, 0.5f);

    // Un bin lejano del pico debe quedar muy por debajo (supresion de
    // lobulos laterales de la ventana Blackman-Harris).
    const std::size_t farBin = (expectedShiftedBin + n / 4) % n;
    EXPECT_LT(result.magnitudeDb[farBin], peakDb - 40.0f);
}

TEST(FftwEngine, ReferenceOffsetShiftsReportedLevel)
{
    constexpr std::size_t n = 1024;
    constexpr std::size_t rawBin = 50;

    FftwEngine engine("test_fftw_wisdom.dat");
    engine.setReferenceOffsetDb(-10.0f);
    EXPECT_FLOAT_EQ(engine.referenceOffsetDb(), -10.0f);

    const auto signal = GenerateOnGridTone(n, rawBin);
    const auto result = engine.process(signal.data(), signal.size());

    const std::size_t expectedShiftedBin = (rawBin + n / 2) % n;
    EXPECT_NEAR(result.magnitudeDb[expectedShiftedBin], -10.0f, 0.5f);
}
