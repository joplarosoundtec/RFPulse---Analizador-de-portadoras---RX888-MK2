#include "demod/FmDemodulator.h"

#include <gtest/gtest.h>

#include <cmath>
#include <complex>
#include <numbers>
#include <vector>

using rfpulse::demod::FmDemodulator;
using rfpulse::demod::FmMode;

TEST(FmDemodulator, ConstantOffsetToneProducesConstantOutputProportionalToOffset)
{
    constexpr double sampleRateHz = 48000.0;
    constexpr double maxDeviationHz = 5000.0; // Narrowband
    constexpr double offsetHz = 1000.0;       // dentro del rango de desviacion

    auto demod = FmDemodulator::forMode(FmMode::Narrowband, sampleRateHz);

    constexpr std::size_t count = 1000;
    std::vector<std::complex<float>> in(count);
    for (std::size_t n = 0; n < count; ++n) {
        const double phase = 2.0 * std::numbers::pi * offsetHz * static_cast<double>(n) / sampleRateHz;
        in[n] = std::complex<float>(static_cast<float>(std::cos(phase)), static_cast<float>(std::sin(phase)));
    }
    std::vector<float> out(count);
    demod.demodulate(in.data(), out.data(), count);

    const float expected = static_cast<float>(offsetHz / maxDeviationHz);
    for (std::size_t i = 1; i < count; ++i) { // la muestra 0 es 0 (sin muestra anterior)
        EXPECT_NEAR(out[i], expected, 1e-3f) << "i=" << i;
    }
    EXPECT_FLOAT_EQ(out[0], 0.0f);
}

TEST(FmDemodulator, NegativeOffsetProducesNegativeOutput)
{
    constexpr double sampleRateHz = 48000.0;
    constexpr double maxDeviationHz = 5000.0;
    constexpr double offsetHz = -2000.0;

    auto demod = FmDemodulator::forMode(FmMode::Narrowband, sampleRateHz);

    constexpr std::size_t count = 500;
    std::vector<std::complex<float>> in(count);
    for (std::size_t n = 0; n < count; ++n) {
        const double phase = 2.0 * std::numbers::pi * offsetHz * static_cast<double>(n) / sampleRateHz;
        in[n] = std::complex<float>(static_cast<float>(std::cos(phase)), static_cast<float>(std::sin(phase)));
    }
    std::vector<float> out(count);
    demod.demodulate(in.data(), out.data(), count);

    const float expected = static_cast<float>(offsetHz / maxDeviationHz);
    for (std::size_t i = 1; i < count; ++i) {
        EXPECT_NEAR(out[i], expected, 1e-3f);
    }
}

TEST(FmDemodulator, WidebandGainMatchesLargerDeviation)
{
    // Misma señal, mismo offset, pero en modo Wideband (desviacion maxima
    // 75 kHz en vez de 5 kHz): la ganancia debe ser 15 veces menor, asi que
    // el mismo offset produce una salida 15 veces mas pequeña.
    constexpr double sampleRateHz = 48000.0;
    constexpr double offsetHz = 1000.0;

    auto nfm = FmDemodulator::forMode(FmMode::Narrowband, sampleRateHz);
    auto wfm = FmDemodulator::forMode(FmMode::Wideband, sampleRateHz);

    constexpr std::size_t count = 10;
    std::vector<std::complex<float>> in(count);
    for (std::size_t n = 0; n < count; ++n) {
        const double phase = 2.0 * std::numbers::pi * offsetHz * static_cast<double>(n) / sampleRateHz;
        in[n] = std::complex<float>(static_cast<float>(std::cos(phase)), static_cast<float>(std::sin(phase)));
    }
    std::vector<float> outNfm(count);
    std::vector<float> outWfm(count);
    nfm.demodulate(in.data(), outNfm.data(), count);
    wfm.demodulate(in.data(), outWfm.data(), count);

    EXPECT_NEAR(outNfm[5] / outWfm[5], 15.0f, 0.01f);
}

TEST(FmDemodulator, ResetClearsPreviousSampleState)
{
    auto demod = FmDemodulator::forMode(FmMode::Narrowband, 48000.0);

    std::vector<std::complex<float>> in = {{1.0f, 0.0f}, {0.0f, 1.0f}};
    std::vector<float> out(2);
    demod.demodulate(in.data(), out.data(), 2);
    EXPECT_NE(out[1], 0.0f);

    demod.reset();
    std::vector<float> out2(1);
    demod.demodulate(in.data(), out2.data(), 1);
    EXPECT_FLOAT_EQ(out2[0], 0.0f); // tras reset, la primera muestra vuelve a ser "sin anterior"
}
