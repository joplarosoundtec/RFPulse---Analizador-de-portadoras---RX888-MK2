#include "spectrum/PeakDetector.h"

#include <gtest/gtest.h>

using rfpulse::spectrum::detectPeaks;

TEST(PeakDetector, NoPeaksWhenAllBelowThreshold)
{
    const std::vector<float> db = {-90.0f, -95.0f, -92.0f, -100.0f};
    const auto peaks = detectPeaks(db.data(), db.size(), -50.0f, 2);
    EXPECT_TRUE(peaks.empty());
}

TEST(PeakDetector, FindsSingleIsolatedPeak)
{
    const std::vector<float> db = {-90.0f, -90.0f, -30.0f, -90.0f, -90.0f};
    const auto peaks = detectPeaks(db.data(), db.size(), -50.0f, 2);
    ASSERT_EQ(peaks.size(), 1u);
    EXPECT_EQ(peaks[0].binIndex, 2u);
    EXPECT_FLOAT_EQ(peaks[0].db, -30.0f);
}

TEST(PeakDetector, FindsTwoWellSeparatedPeaksInAscendingOrder)
{
    const std::vector<float> db = {-90.0f, -20.0f, -90.0f, -90.0f, -90.0f, -25.0f, -90.0f};
    const auto peaks = detectPeaks(db.data(), db.size(), -50.0f, 1);
    ASSERT_EQ(peaks.size(), 2u);
    EXPECT_EQ(peaks[0].binIndex, 1u);
    EXPECT_EQ(peaks[1].binIndex, 5u);
}

TEST(PeakDetector, MergesCloseBinsIntoSinglePeakKeepingStrongest)
{
    // Un mismo lobulo ancho con ruido: varios bins seguidos por encima del
    // umbral, con el maximo real en el medio. Debe salir un unico pico.
    const std::vector<float> db = {-90.0f, -40.0f, -35.0f, -20.0f, -38.0f, -42.0f, -90.0f};
    const auto peaks = detectPeaks(db.data(), db.size(), -50.0f, 2);
    ASSERT_EQ(peaks.size(), 1u);
    EXPECT_EQ(peaks[0].binIndex, 3u);
    EXPECT_FLOAT_EQ(peaks[0].db, -20.0f);
}

TEST(PeakDetector, ShortDipWithinMinSeparationDoesNotSplitPeak)
{
    // Valle corto (1 bin por debajo del umbral) dentro de la ventana de
    // fusion: sigue siendo UN pico, no dos.
    const std::vector<float> db = {-90.0f, -30.0f, -60.0f, -25.0f, -90.0f};
    const auto peaks = detectPeaks(db.data(), db.size(), -50.0f, 2);
    ASSERT_EQ(peaks.size(), 1u);
    EXPECT_EQ(peaks[0].binIndex, 3u);
    EXPECT_FLOAT_EQ(peaks[0].db, -25.0f);
}

TEST(PeakDetector, DipLargerThanMinSeparationSplitsIntoTwoPeaks)
{
    const std::vector<float> db = {-90.0f, -30.0f, -90.0f, -90.0f, -90.0f, -25.0f, -90.0f};
    const auto peaks = detectPeaks(db.data(), db.size(), -50.0f, 1);
    ASSERT_EQ(peaks.size(), 2u);
}

TEST(PeakDetector, PeakTouchingEndOfBufferIsDetected)
{
    const std::vector<float> db = {-90.0f, -90.0f, -90.0f, -20.0f};
    const auto peaks = detectPeaks(db.data(), db.size(), -50.0f, 2);
    ASSERT_EQ(peaks.size(), 1u);
    EXPECT_EQ(peaks[0].binIndex, 3u);
}

TEST(PeakDetector, EmptyInputProducesNoPeaks)
{
    const auto peaks = detectPeaks(nullptr, 0, -50.0f, 2);
    EXPECT_TRUE(peaks.empty());
}
