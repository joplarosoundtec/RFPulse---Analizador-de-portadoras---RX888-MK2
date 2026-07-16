#include "demod/Squelch.h"

#include <gtest/gtest.h>

#include <complex>
#include <vector>

using rfpulse::demod::Squelch;

namespace {

std::vector<std::complex<float>> MakeConstantPower(float amplitude, std::size_t count)
{
    return std::vector<std::complex<float>>(count, std::complex<float>(amplitude, 0.0f));
}

} // namespace

TEST(Squelch, OpensOnStrongSignal)
{
    Squelch squelch(-50.0f);
    const auto strong = MakeConstantPower(1.0f, 100); // 0 dBFS, muy por encima del umbral
    std::vector<float> audio(100, 0.5f);

    const bool open = squelch.process(strong.data(), strong.size(), audio.data());

    EXPECT_TRUE(open);
    for (float v : audio) {
        EXPECT_FLOAT_EQ(v, 0.5f); // no silenciado
    }
}

TEST(Squelch, ClosesAndMutesOnWeakSignal)
{
    Squelch squelch(-50.0f);
    const auto weak = MakeConstantPower(0.0001f, 100); // ~ -80 dBFS, por debajo del umbral
    std::vector<float> audio(100, 0.5f);

    const bool open = squelch.process(weak.data(), weak.size(), audio.data());

    EXPECT_FALSE(open);
    for (float v : audio) {
        EXPECT_FLOAT_EQ(v, 0.0f); // silenciado
    }
}

TEST(Squelch, HysteresisPreventsImmediateClosingNearThreshold)
{
    Squelch squelch(-50.0f, /*hysteresisDb=*/3.0f);

    const auto strong = MakeConstantPower(1.0f, 100); // abre claramente
    std::vector<float> audio(100, 1.0f);
    ASSERT_TRUE(squelch.process(strong.data(), strong.size(), audio.data()));

    // Potencia justo por debajo del umbral de apertura pero por encima del
    // umbral de cierre (umbral - histeresis): debe seguir abierto.
    const auto borderline = MakeConstantPower(0.0056f, 100); // ~ -45 dBFS aprox
    const bool stillOpen = squelch.process(borderline.data(), borderline.size(), audio.data());

    EXPECT_TRUE(stillOpen);
}

TEST(Squelch, ClosesWhenBelowHysteresisMargin)
{
    Squelch squelch(-50.0f, /*hysteresisDb=*/3.0f);

    const auto strong = MakeConstantPower(1.0f, 100);
    std::vector<float> audio(100, 1.0f);
    ASSERT_TRUE(squelch.process(strong.data(), strong.size(), audio.data()));

    const auto veryWeak = MakeConstantPower(0.0001f, 100); // muy por debajo, incluso con histeresis
    const bool nowClosed = squelch.process(veryWeak.data(), veryWeak.size(), audio.data());

    EXPECT_FALSE(nowClosed);
}
