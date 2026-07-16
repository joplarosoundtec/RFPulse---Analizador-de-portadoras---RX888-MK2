#include "dsp/FastMath.h"

#include <gtest/gtest.h>

#include <cmath>
#include <vector>

using rfpulse::dsp::fastLog10;
using rfpulse::dsp::fastLog2;
using rfpulse::dsp::powerToDb;

namespace {

// Rango realista de potencia lineal para un bin de FFT normalizado a full
// scale: desde muy por debajo del suelo de ruido (1e-12) hasta un tono a
// plena escala (1.0), y algo de margen por encima (espurias/errores de
// calibracion). Muestreo logaritmico para cubrir muchas decadas.
std::vector<float> LogSpacedSamples(float minVal, float maxVal, int count)
{
    std::vector<float> out;
    out.reserve(static_cast<std::size_t>(count));
    const double logMin = std::log10(static_cast<double>(minVal));
    const double logMax = std::log10(static_cast<double>(maxVal));
    for (int i = 0; i < count; ++i) {
        const double t = static_cast<double>(i) / (count - 1);
        const double logVal = logMin + t * (logMax - logMin);
        out.push_back(static_cast<float>(std::pow(10.0, logVal)));
    }
    return out;
}

} // namespace

TEST(FastMath, Log2MatchesStdLog2WithinToleranceAcrossWideRange)
{
    const auto samples = LogSpacedSamples(1e-12f, 10.0f, 200'000);

    float maxAbsError = 0.0f;
    for (float x : samples) {
        const float reference = std::log2(x);
        const float approx = fastLog2(x);
        maxAbsError = std::max(maxAbsError, std::abs(approx - reference));
    }

    // La tecnica (Mineiro/"fastapprox") publica un error tipico bajo 0.01 en
    // unidades de log2; comprobamos con margen sobre lo medido realmente.
    EXPECT_LT(maxAbsError, 0.02f) << "error maximo medido en log2: " << maxAbsError;
}

TEST(FastMath, Log10MatchesStdLog10WithinToleranceAcrossWideRange)
{
    const auto samples = LogSpacedSamples(1e-12f, 10.0f, 200'000);

    float maxAbsError = 0.0f;
    for (float x : samples) {
        const float reference = std::log10(x);
        const float approx = fastLog10(x);
        maxAbsError = std::max(maxAbsError, std::abs(approx - reference));
    }

    EXPECT_LT(maxAbsError, 0.01f) << "error maximo medido en log10: " << maxAbsError;
}

TEST(FastMath, PowerToDbMatchesReferenceWithinDisplayTolerance)
{
    const auto samples = LogSpacedSamples(1e-12f, 10.0f, 200'000);
    std::vector<float> dbOut(samples.size());

    powerToDb(samples.data(), dbOut.data(), samples.size(), /*offsetDb=*/0.0f);

    float maxAbsErrorDb = 0.0f;
    for (std::size_t i = 0; i < samples.size(); ++i) {
        const float referenceDb = 10.0f * std::log10(samples[i]);
        maxAbsErrorDb = std::max(maxAbsErrorDb, std::abs(dbOut[i] - referenceDb));
    }

    // Umbral de calidad visual para un espectro/waterfall: un error asi de
    // pequeno es imperceptible frente al ruido de medida real de cualquier
    // receptor. Se deja constancia del valor medido en el mensaje de fallo
    // para poder ajustar los coeficientes de FastMath.h si esto cambia.
    EXPECT_LT(maxAbsErrorDb, 0.2f) << "error maximo medido en dB: " << maxAbsErrorDb;
}

TEST(FastMath, PowerToDbAppliesOffset)
{
    const std::vector<float> power = {1.0f, 0.1f, 0.01f};
    std::vector<float> dbOut(power.size());

    powerToDb(power.data(), dbOut.data(), power.size(), /*offsetDb=*/-30.0f);

    EXPECT_NEAR(dbOut[0], -30.0f, 0.05f);
    EXPECT_NEAR(dbOut[1], -40.0f, 0.05f);
    EXPECT_NEAR(dbOut[2], -50.0f, 0.05f);
}

TEST(FastMath, PowerToDbHandlesCountsNotMultipleOfEight)
{
    // 11 elementos ejercita el bloque AVX2 (8) + cola escalar (3).
    const std::vector<float> power = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.5f, 0.25f, 0.125f};
    std::vector<float> dbOut(power.size());

    powerToDb(power.data(), dbOut.data(), power.size(), 0.0f);

    for (std::size_t i = 0; i < 8; ++i) {
        EXPECT_NEAR(dbOut[i], 0.0f, 0.05f);
    }
    EXPECT_NEAR(dbOut[8], 10.0f * std::log10(0.5), 0.05f);
    EXPECT_NEAR(dbOut[9], 10.0f * std::log10(0.25), 0.05f);
    EXPECT_NEAR(dbOut[10], 10.0f * std::log10(0.125), 0.05f);
}
