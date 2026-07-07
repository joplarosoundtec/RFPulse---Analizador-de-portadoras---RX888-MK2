#include "vfo/Vfo.h"

#include <gtest/gtest.h>

#include <cmath>
#include <complex>
#include <numbers>
#include <vector>

using rfpulse::vfo::Vfo;

// Prueba de integracion de extremo a extremo del pipeline por VFO: mezcla
// (Nco) -> filtro de canal decimador (DecimatingFirFilter) -> demodulacion
// FM -> de-enfasis -> squelch. Genera una senal wideband con un tono puro
// desplazado respecto al centro sintonizado del VFO (equivalente a una
// señal FM con una desviacion instantanea constante) y comprueba que el
// audio demodulado converge al valor esperado, verificando que TODA la
// cadena (no solo cada pieza por separado) esta bien conectada.
TEST(Vfo, DemodulatesConstantOffsetToneToExpectedAudioLevel)
{
    constexpr double widebandSampleRateHz = 1'000'000.0;
    constexpr double targetAudioSampleRateHz = 48000.0;
    constexpr double channelBandwidthHz = 12500.0;
    constexpr double tunedOffsetHz = 200000.0; // el VFO se sintoniza 200 kHz por encima del centro wideband
    constexpr double toneOffsetWithinChannelHz = 1000.0; // "desviacion" residual dentro del canal
    constexpr double maxDeviationHzNarrowband = 5000.0;

    constexpr std::size_t widebandBlockSize = 60000;
    Vfo vfo(widebandSampleRateHz, targetAudioSampleRateHz, channelBandwidthHz, widebandBlockSize);
    vfo.setOffsetHz(tunedOffsetHz);
    vfo.setSquelchThresholdDb(-80.0f); // asegura que el squelch esta abierto para este tono a plena escala

    std::vector<std::complex<float>> widebandIq(widebandBlockSize);
    const double toneFrequencyHz = tunedOffsetHz + toneOffsetWithinChannelHz;
    for (std::size_t n = 0; n < widebandBlockSize; ++n) {
        const double phase = 2.0 * std::numbers::pi * toneFrequencyHz * static_cast<double>(n) / widebandSampleRateHz;
        widebandIq[n] = std::complex<float>(static_cast<float>(std::cos(phase)), static_cast<float>(std::sin(phase)));
    }

    std::vector<float> audioOut(widebandBlockSize);
    const std::size_t audioCount = vfo.process(widebandIq.data(), widebandBlockSize, audioOut.data());

    ASSERT_GT(audioCount, 300u);
    EXPECT_TRUE(vfo.squelchOpen());

    const float expected = static_cast<float>(toneOffsetWithinChannelHz / maxDeviationHzNarrowband);

    // Se ignoran las primeras muestras (transitorio de asentamiento del
    // filtro de canal FIR) y se comprueba que el resto converge al nivel
    // esperado (0.2 = 1000 Hz / 5000 Hz de desviacion maxima NFM).
    for (std::size_t i = 200; i < audioCount; ++i) {
        EXPECT_NEAR(audioOut[i], expected, 0.02f) << "i=" << i;
    }
}

TEST(Vfo, MuteSilencesOutputRegardlessOfSignal)
{
    constexpr double widebandSampleRateHz = 1'000'000.0;
    constexpr double targetAudioSampleRateHz = 48000.0;
    constexpr double channelBandwidthHz = 12500.0;
    constexpr std::size_t widebandBlockSize = 20000;

    Vfo vfo(widebandSampleRateHz, targetAudioSampleRateHz, channelBandwidthHz, widebandBlockSize);
    vfo.setOffsetHz(0.0);
    vfo.setSquelchThresholdDb(-80.0f);
    vfo.setMuted(true);

    std::vector<std::complex<float>> widebandIq(widebandBlockSize);
    for (std::size_t n = 0; n < widebandBlockSize; ++n) {
        const double phase = 2.0 * std::numbers::pi * 1000.0 * static_cast<double>(n) / widebandSampleRateHz;
        widebandIq[n] = std::complex<float>(static_cast<float>(std::cos(phase)), static_cast<float>(std::sin(phase)));
    }

    std::vector<float> audioOut(widebandBlockSize, 1.0f);
    const std::size_t audioCount = vfo.process(widebandIq.data(), widebandBlockSize, audioOut.data());

    ASSERT_GT(audioCount, 0u);
    for (std::size_t i = 0; i < audioCount; ++i) {
        EXPECT_FLOAT_EQ(audioOut[i], 0.0f);
    }
}

TEST(Vfo, SquelchClosesAndMutesOnNoSignal)
{
    constexpr double widebandSampleRateHz = 1'000'000.0;
    constexpr double targetAudioSampleRateHz = 48000.0;
    constexpr double channelBandwidthHz = 12500.0;
    constexpr std::size_t widebandBlockSize = 20000;

    Vfo vfo(widebandSampleRateHz, targetAudioSampleRateHz, channelBandwidthHz, widebandBlockSize);
    vfo.setOffsetHz(0.0);
    vfo.setSquelchThresholdDb(-50.0f);

    // Ruido termico muy debil (muy por debajo del umbral): el squelch debe
    // permanecer cerrado.
    std::vector<std::complex<float>> widebandIq(widebandBlockSize, std::complex<float>(0.00001f, 0.0f));
    std::vector<float> audioOut(widebandBlockSize);

    const std::size_t audioCount = vfo.process(widebandIq.data(), widebandBlockSize, audioOut.data());

    ASSERT_GT(audioCount, 0u);
    EXPECT_FALSE(vfo.squelchOpen());
    for (std::size_t i = 0; i < audioCount; ++i) {
        EXPECT_FLOAT_EQ(audioOut[i], 0.0f);
    }
}
