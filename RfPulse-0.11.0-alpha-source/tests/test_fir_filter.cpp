#include "dsp/FirFilter.h"

#include <gtest/gtest.h>

#include <cmath>
#include <complex>
#include <numbers>
#include <vector>

using rfpulse::dsp::designLowpassFir;
using rfpulse::dsp::DecimatingFirFilter;

TEST(FirDesign, HasUnityDcGain)
{
    const auto taps = designLowpassFir(48000.0, 5000.0, 63);
    double sum = 0.0;
    for (float t : taps) {
        sum += t;
    }
    EXPECT_NEAR(sum, 1.0, 1e-4);
}

TEST(FirDesign, IsSymmetric)
{
    const auto taps = designLowpassFir(48000.0, 5000.0, 63);
    for (std::size_t i = 0; i < taps.size(); ++i) {
        EXPECT_NEAR(taps[i], taps[taps.size() - 1 - i], 1e-6f);
    }
}

TEST(DecimatingFirFilter, ProducesExpectedOutputCountForDecimation)
{
    const auto taps = designLowpassFir(48000.0, 5000.0, 31);
    DecimatingFirFilter filter(taps, 4);

    constexpr std::size_t inCount = 4000;
    std::vector<std::complex<float>> in(inCount, std::complex<float>(1.0f, 0.0f));
    std::vector<std::complex<float>> out(inCount);

    const std::size_t outCount = filter.process(in.data(), inCount, out.data());
    EXPECT_EQ(outCount, inCount / 4);
}

TEST(DecimatingFirFilter, PassesDcThroughAfterSettling)
{
    const auto taps = designLowpassFir(48000.0, 5000.0, 63);
    DecimatingFirFilter filter(taps, 4);

    constexpr std::size_t inCount = 4000;
    std::vector<std::complex<float>> in(inCount, std::complex<float>(1.0f, 0.0f));
    std::vector<std::complex<float>> out(inCount);

    const std::size_t outCount = filter.process(in.data(), inCount, out.data());
    ASSERT_GT(outCount, 10u);

    // Se ignoran las primeras muestras (transitorio de arranque del FIR) y
    // se comprueba que la DC (ganancia unidad) pasa correctamente.
    for (std::size_t i = 10; i < outCount; ++i) {
        EXPECT_NEAR(out[i].real(), 1.0f, 0.02f) << "i=" << i;
        EXPECT_NEAR(out[i].imag(), 0.0f, 0.02f) << "i=" << i;
    }
}

TEST(DecimatingFirFilter, AttenuatesOutOfBandTone)
{
    constexpr double sampleRateHz = 48000.0;
    constexpr double cutoffHz = 2000.0;
    constexpr double toneHz = 15000.0; // muy por encima del corte

    const auto taps = designLowpassFir(sampleRateHz, cutoffHz, 127);
    DecimatingFirFilter filter(taps, 1); // sin decimar, solo se prueba la atenuacion

    constexpr std::size_t count = 2000;
    std::vector<std::complex<float>> in(count);
    for (std::size_t n = 0; n < count; ++n) {
        const double phase = 2.0 * std::numbers::pi * toneHz * static_cast<double>(n) / sampleRateHz;
        in[n] = std::complex<float>(static_cast<float>(std::cos(phase)), static_cast<float>(std::sin(phase)));
    }
    std::vector<std::complex<float>> out(count);
    const std::size_t outCount = filter.process(in.data(), count, out.data());

    double inPower = 0.0;
    double outPower = 0.0;
    for (std::size_t i = 200; i < outCount; ++i) { // se ignora el transitorio inicial
        inPower += std::norm(in[i]);
        outPower += std::norm(out[i]);
    }

    EXPECT_LT(outPower, inPower * 0.01); // al menos 20 dB de atenuacion
}
