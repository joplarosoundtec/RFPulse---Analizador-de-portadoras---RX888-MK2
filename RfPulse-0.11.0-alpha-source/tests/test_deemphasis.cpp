#include "demod/Deemphasis.h"

#include <gtest/gtest.h>

#include <cmath>
#include <numbers>
#include <vector>

using rfpulse::demod::Deemphasis;
using rfpulse::demod::DeemphasisTimeConstant;

namespace {

double RmsAfterSettling(Deemphasis& filter, double sampleRateHz, double toneHz, std::size_t count)
{
    std::vector<float> audio(count);
    for (std::size_t n = 0; n < count; ++n) {
        audio[n] = static_cast<float>(std::sin(2.0 * std::numbers::pi * toneHz * static_cast<double>(n) / sampleRateHz));
    }
    filter.process(audio.data(), count);

    double sumSquares = 0.0;
    std::size_t settled = 0;
    for (std::size_t n = count / 2; n < count; ++n) { // ignora la primera mitad (asentamiento)
        sumSquares += static_cast<double>(audio[n]) * audio[n];
        ++settled;
    }
    return std::sqrt(sumSquares / static_cast<double>(settled));
}

} // namespace

TEST(Deemphasis, NoneModeIsPassthrough)
{
    Deemphasis filter(48000.0, DeemphasisTimeConstant::None);
    std::vector<float> audio = {0.1f, -0.5f, 0.9f, -1.0f, 0.3f};
    const std::vector<float> original = audio;

    filter.process(audio.data(), audio.size());

    for (std::size_t i = 0; i < audio.size(); ++i) {
        EXPECT_FLOAT_EQ(audio[i], original[i]);
    }
}

TEST(Deemphasis, AttenuatesHighFrequencyMoreThanLowFrequency)
{
    constexpr double sampleRateHz = 48000.0;

    Deemphasis lowFilter(sampleRateHz, DeemphasisTimeConstant::Us75);
    Deemphasis highFilter(sampleRateHz, DeemphasisTimeConstant::Us75);

    const double lowRms = RmsAfterSettling(lowFilter, sampleRateHz, 200.0, 4800);
    const double highRms = RmsAfterSettling(highFilter, sampleRateHz, 8000.0, 4800);

    EXPECT_GT(lowRms, highRms);
}

TEST(Deemphasis, Us50HasShorterTimeConstantThanUs75)
{
    // Con la misma tasa y la misma frecuencia de tono, Us50 (tau mas corta)
    // debe atenuar MENOS que Us75 (tau mas larga corta antes en frecuencia).
    constexpr double sampleRateHz = 48000.0;
    constexpr double toneHz = 6000.0;

    Deemphasis filter50(sampleRateHz, DeemphasisTimeConstant::Us50);
    Deemphasis filter75(sampleRateHz, DeemphasisTimeConstant::Us75);

    const double rms50 = RmsAfterSettling(filter50, sampleRateHz, toneHz, 4800);
    const double rms75 = RmsAfterSettling(filter75, sampleRateHz, toneHz, 4800);

    EXPECT_GT(rms50, rms75);
}
