#include "fft/Window.h"

#include <gtest/gtest.h>

#include <vector>

using rfpulse::fft::generateWindow;
using rfpulse::fft::WindowType;

TEST(Window, BlackmanHarrisHasUnityCoherentGain)
{
    constexpr std::size_t n = 16384;
    std::vector<float> w(n);
    generateWindow(WindowType::BlackmanHarris, n, w.data());

    double sum = 0.0;
    for (float v : w) {
        sum += v;
    }
    const double mean = sum / static_cast<double>(n);

    EXPECT_NEAR(mean, 1.0, 1e-4);
}

TEST(Window, BlackmanHarrisTapersTowardEdgesAndPeaksAtCenter)
{
    constexpr std::size_t n = 1024;
    std::vector<float> w(n);
    generateWindow(WindowType::BlackmanHarris, n, w.data());

    EXPECT_LT(w.front(), w[n / 2]);
    EXPECT_LT(w.back(), w[n / 2]);

    // Simetria aproximada respecto al centro (la ventana es simetrica por
    // construccion, salvo redondeo de punto flotante).
    for (std::size_t i = 0; i < 10; ++i) {
        EXPECT_NEAR(w[i], w[n - 1 - i], 1e-4f);
    }
}
