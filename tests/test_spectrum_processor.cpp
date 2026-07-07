#include "spectrum/SpectrumProcessor.h"

#include "dsp/FastMath.h"

#include <gtest/gtest.h>

#include <cmath>
#include <vector>

using rfpulse::spectrum::AveragingMode;
using rfpulse::spectrum::SpectrumProcessor;

namespace {

rfpulse::fft::FftResult MakeResult(const std::vector<float>& power, std::vector<float>& dbScratch)
{
    dbScratch.resize(power.size());
    rfpulse::dsp::powerToDb(power.data(), dbScratch.data(), power.size(), 0.0f);
    return rfpulse::fft::FftResult{power.data(), dbScratch.data(), power.size()};
}

} // namespace

TEST(SpectrumProcessor, SmoothingAveragesSpikeWithNeighboringBins)
{
    SpectrumProcessor processor(5);
    processor.setSmoothingWidthBins(3); // ventana de 3: cada bin promedia con sus 2 vecinos

    // Un pico aislado de un solo bin en medio de bins a potencia muy baja.
    const std::vector<float> power = {1e-6f, 1e-6f, 1.0f, 1e-6f, 1e-6f};
    std::vector<float> dbScratch;
    const auto result = MakeResult(power, dbScratch);

    processor.submit(result, 0.0, 1.0);
    const auto& frame = processor.latestFrame();

    // El bin central ya no deberia leer ~0 dBFS (el pico sin suavizar):
    // promediado con sus 2 vecinos casi nulos, cae bastante mas abajo.
    EXPECT_LT(frame.currentDb[2], -3.0f);
    // Pero los bins vecinos, que antes eran ruido puro, ahora deberian
    // subir (reciben parte de la energia del pico via la media movil).
    EXPECT_GT(frame.currentDb[1], -100.0f);
    EXPECT_GT(frame.currentDb[3], -100.0f);
}

TEST(SpectrumProcessor, DefaultSmoothingIsIdentity)
{
    SpectrumProcessor processor(3);

    const std::vector<float> power = {1.0f, 0.01f, 1.0f};
    std::vector<float> dbScratch;
    const auto result = MakeResult(power, dbScratch);

    processor.submit(result, 0.0, 1.0);
    const auto& frame = processor.latestFrame();

    EXPECT_NEAR(frame.currentDb[0], 0.0f, 0.1f);
    EXPECT_NEAR(frame.currentDb[1], -20.0f, 0.1f);
    EXPECT_NEAR(frame.currentDb[2], 0.0f, 0.1f);
}

TEST(SpectrumProcessor, LatestFrameBeforeAnySubmitDoesNotCrash)
{
    SpectrumProcessor processor(4);
    const auto& frame = processor.latestFrame();
    EXPECT_EQ(frame.binCount, 4u);
}

TEST(SpectrumProcessor, SubmitPublishesCurrentTraceMatchingInput)
{
    SpectrumProcessor processor(3);
    const std::vector<float> power = {1.0f, 0.1f, 0.01f};
    std::vector<float> dbScratch;
    const auto result = MakeResult(power, dbScratch);

    processor.submit(result, 560'000'000.0, 10'000'000.0);
    const auto& frame = processor.latestFrame();

    ASSERT_EQ(frame.binCount, 3u);
    EXPECT_NEAR(frame.currentDb[0], 0.0f, 0.1f);
    EXPECT_NEAR(frame.currentDb[1], -10.0f, 0.1f);
    EXPECT_NEAR(frame.currentDb[2], -20.0f, 0.1f);
    EXPECT_DOUBLE_EQ(frame.centerFrequencyHz, 560'000'000.0);
    EXPECT_DOUBLE_EQ(frame.spanHz, 10'000'000.0);
}

TEST(SpectrumProcessor, ExponentialAverageConvergesTowardConstantInput)
{
    SpectrumProcessor processor(1);
    processor.setAveragingMode(AveragingMode::Exponential);
    processor.setAveragingAlpha(0.5f);

    const std::vector<float> power = {1.0f};
    std::vector<float> dbScratch;
    const auto result = MakeResult(power, dbScratch);

    for (int i = 0; i < 20; ++i) {
        processor.submit(result, 0.0, 1.0);
    }

    const auto& frame = processor.latestFrame();
    EXPECT_NEAR(frame.averageDb[0], 0.0f, 0.1f); // 10*log10(1.0) == 0 dB
}

TEST(SpectrumProcessor, MaxHoldTracksPeakAcrossSubmits)
{
    SpectrumProcessor processor(1);
    processor.setMaxHoldEnabled(true);

    std::vector<float> dbScratch;

    processor.submit(MakeResult({0.01f}, dbScratch), 0.0, 1.0);
    processor.submit(MakeResult({1.0f}, dbScratch), 0.0, 1.0);
    processor.submit(MakeResult({0.1f}, dbScratch), 0.0, 1.0);

    const auto& frame = processor.latestFrame();
    EXPECT_NEAR(frame.maxHoldDb[0], 0.0f, 0.1f); // deberia recordar el pico de 1.0 (0 dB)
}

TEST(SpectrumProcessor, MinHoldTracksTroughAcrossSubmits)
{
    SpectrumProcessor processor(1);
    processor.setMinHoldEnabled(true);

    std::vector<float> dbScratch;

    processor.submit(MakeResult({1.0f}, dbScratch), 0.0, 1.0);
    processor.submit(MakeResult({0.001f}, dbScratch), 0.0, 1.0);
    processor.submit(MakeResult({0.5f}, dbScratch), 0.0, 1.0);

    const auto& frame = processor.latestFrame();
    EXPECT_NEAR(frame.minHoldDb[0], -30.0f, 0.1f); // 10*log10(0.001) == -30 dB
}

TEST(SpectrumProcessor, ResetHoldsClearsMaxAndMinHold)
{
    SpectrumProcessor processor(1);
    processor.setMaxHoldEnabled(true);
    processor.setMinHoldEnabled(true);

    std::vector<float> dbScratch;
    processor.submit(MakeResult({1.0f}, dbScratch), 0.0, 1.0);

    processor.resetHolds();
    processor.submit(MakeResult({0.1f}, dbScratch), 0.0, 1.0);

    const auto& frame = processor.latestFrame();
    EXPECT_NEAR(frame.maxHoldDb[0], -10.0f, 0.1f); // ya no recuerda el 1.0 anterior al reset
    EXPECT_NEAR(frame.minHoldDb[0], -10.0f, 0.1f);
}

TEST(SpectrumProcessor, KnownSpurGridInterpolatesOverBinsAtExactMultiples)
{
    // 20 bins, span 2 MHz (100 kHz/bin), centrado en 1 MHz => freqStart=0,
    // asi que el bin i cae exactamente en i*100 kHz. Con spurGridHz=500 kHz,
    // el bin 10 (freq=1,000,000 Hz = 2*500,000) es un multiplo exacto.
    SpectrumProcessor processor(20);
    processor.setKnownSpurGridHz(500'000.0);

    std::vector<float> power(20);
    for (std::size_t i = 0; i < power.size(); ++i) {
        power[i] = 0.01f * static_cast<float>(i + 1); // rampa lineal conocida
    }
    power[10] = 1000.0f; // "espurea" artificial e injustificadamente alta

    std::vector<float> dbScratch;
    const auto result = MakeResult(power, dbScratch);
    processor.submit(result, 1'000'000.0, 2'000'000.0);
    const auto& frame = processor.latestFrame();

    // El bin de la espurea ya NO debe leer su pico artificial (+30 dBFS
    // para potencia 1000.0): el filtro lo sustituye por una interpolacion
    // de sus vecinos reales.
    EXPECT_LT(frame.currentDb[10], 0.0f);
    // Los bins lejos de cualquier multiplo de la rejilla no deberian
    // tocarse en absoluto.
    EXPECT_NEAR(frame.currentDb[2], 10.0f * std::log10(0.03f), 0.1f);
    EXPECT_NEAR(frame.currentDb[17], 10.0f * std::log10(0.18f), 0.1f);
}

TEST(SpectrumProcessor, SpurGridDisabledByDefaultLeavesSpikeUntouched)
{
    // Sin llamar a setKnownSpurGridHz, un pico en un bin que coincidiria
    // con un multiplo de 500 kHz no deberia verse afectado -- el filtro
    // esta desactivado (0.0) por defecto.
    SpectrumProcessor processor(20);

    std::vector<float> power(20, 0.01f);
    power[10] = 1000.0f;

    std::vector<float> dbScratch;
    const auto result = MakeResult(power, dbScratch);
    processor.submit(result, 1'000'000.0, 2'000'000.0);
    const auto& frame = processor.latestFrame();

    EXPECT_NEAR(frame.currentDb[10], 30.0f, 0.1f); // 10*log10(1000) == 30 dB, intacto
}

TEST(SpectrumProcessor, FrozenStopsPublishingButKeepsProcessing)
{
    SpectrumProcessor processor(1);
    processor.setMaxHoldEnabled(true);

    std::vector<float> dbScratch;
    processor.submit(MakeResult({0.1f}, dbScratch), 0.0, 1.0);
    const float dbBeforeFreeze = processor.latestFrame().currentDb[0];

    processor.setFrozen(true);
    processor.submit(MakeResult({1.0f}, dbScratch), 0.0, 1.0); // no deberia publicarse

    EXPECT_NEAR(processor.latestFrame().currentDb[0], dbBeforeFreeze, 0.01f);

    processor.setFrozen(false);
    processor.submit(MakeResult({1.0f}, dbScratch), 0.0, 1.0);
    EXPECT_NEAR(processor.latestFrame().currentDb[0], 0.0f, 0.1f);
}
