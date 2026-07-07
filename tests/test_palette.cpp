#include "waterfall/Palette.h"

#include <gtest/gtest.h>

using rfpulse::waterfall::generatePalette;
using rfpulse::waterfall::PaletteType;
using rfpulse::waterfall::RgbColor;

TEST(Palette, GrayscaleGoesFromBlackToWhite)
{
    std::array<RgbColor, 256> lut{};
    generatePalette(PaletteType::Grayscale, lut);

    EXPECT_EQ(lut.front().r, 0);
    EXPECT_EQ(lut.front().g, 0);
    EXPECT_EQ(lut.front().b, 0);
    EXPECT_EQ(lut.back().r, 255);
    EXPECT_EQ(lut.back().g, 255);
    EXPECT_EQ(lut.back().b, 255);
}

TEST(Palette, GrayscaleIsMonotonicallyIncreasing)
{
    std::array<RgbColor, 256> lut{};
    generatePalette(PaletteType::Grayscale, lut);

    for (std::size_t i = 1; i < lut.size(); ++i) {
        EXPECT_GE(lut[i].r, lut[i - 1].r);
    }
}

TEST(Palette, AllPalettesProduceValidEndpoints)
{
    for (auto type :
        {PaletteType::Viridis, PaletteType::Inferno, PaletteType::Turbo, PaletteType::Grayscale,
            PaletteType::Thermal}) {
        std::array<RgbColor, 256> lut{};
        generatePalette(type, lut);
        // No debe crashear ni dejar entradas sin inicializar de forma
        // detectable: al menos el primer y ultimo color deben diferir (una
        // paleta plana indicaria un fallo de generacion).
        const bool differs = lut.front().r != lut.back().r || lut.front().g != lut.back().g
            || lut.front().b != lut.back().b;
        EXPECT_TRUE(differs);
    }
}
