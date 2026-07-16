#include "dsp/Nco.h"

#include <gtest/gtest.h>

#include <cmath>
#include <complex>
#include <numbers>
#include <vector>

using rfpulse::dsp::Nco;

TEST(Nco, MixingDcSignalProducesExpectedSinusoid)
{
    constexpr double sampleRateHz = 48000.0;
    constexpr double frequencyHz = 1000.0;
    constexpr std::size_t count = 2000;

    Nco nco(sampleRateHz);
    nco.setFrequency(frequencyHz);

    std::vector<std::complex<float>> in(count, std::complex<float>(1.0f, 0.0f));
    std::vector<std::complex<float>> out(count);
    nco.mix(in.data(), out.data(), count);

    // Multiplicar 1+0i por e^{-j*2*pi*f*n/Fs} debe dar exactamente esa
    // sinusoide compleja.
    for (std::size_t n = 0; n < count; n += 97) {
        const double phase = -2.0 * std::numbers::pi * frequencyHz * static_cast<double>(n) / sampleRateHz;
        const float expectedReal = static_cast<float>(std::cos(phase));
        const float expectedImag = static_cast<float>(std::sin(phase));
        EXPECT_NEAR(out[n].real(), expectedReal, 1e-3f) << "n=" << n;
        EXPECT_NEAR(out[n].imag(), expectedImag, 1e-3f) << "n=" << n;
    }
}

TEST(Nco, RotatorMagnitudeStaysUnitaryOverManySamples)
{
    constexpr double sampleRateHz = 48000.0;
    Nco nco(sampleRateHz);
    nco.setFrequency(1234.5);

    // Suficientes muestras para disparar la renormalizacion muchas veces
    // (cada 1024 muestras).
    constexpr std::size_t count = 2'000'000;
    std::vector<std::complex<float>> in(1, std::complex<float>(1.0f, 0.0f));
    std::vector<std::complex<float>> out(1);

    float lastMagnitude = 1.0f;
    for (std::size_t n = 0; n < count; ++n) {
        nco.mix(in.data(), out.data(), 1);
        lastMagnitude = std::abs(out[0]);
    }

    EXPECT_NEAR(lastMagnitude, 1.0f, 1e-4f);
}

TEST(Nco, ZeroFrequencyIsIdentity)
{
    Nco nco(48000.0);
    nco.setFrequency(0.0);

    std::vector<std::complex<float>> in = {{1.0f, 2.0f}, {-3.0f, 0.5f}, {0.0f, -1.0f}};
    std::vector<std::complex<float>> out(in.size());
    nco.mix(in.data(), out.data(), in.size());

    for (std::size_t i = 0; i < in.size(); ++i) {
        EXPECT_NEAR(out[i].real(), in[i].real(), 1e-5f);
        EXPECT_NEAR(out[i].imag(), in[i].imag(), 1e-5f);
    }
}
