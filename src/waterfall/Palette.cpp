#include "Palette.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace rfpulse::waterfall {

namespace {

struct ControlPoint {
    float position; // 0..1
    RgbColor color;
};

const std::vector<ControlPoint>& controlPointsFor(PaletteType type)
{
    static const std::vector<ControlPoint> grayscale = {
        {0.00f, {0, 0, 0}},
        {1.00f, {255, 255, 255}},
    };
    static const std::vector<ControlPoint> viridis = {
        {0.00f, {68, 1, 84}},
        {0.13f, {72, 40, 120}},
        {0.25f, {62, 74, 137}},
        {0.38f, {49, 104, 142}},
        {0.50f, {38, 130, 142}},
        {0.63f, {31, 158, 137}},
        {0.75f, {53, 183, 121}},
        {0.88f, {109, 205, 89}},
        {1.00f, {253, 231, 37}},
    };
    static const std::vector<ControlPoint> inferno = {
        {0.00f, {0, 0, 4}},
        {0.13f, {31, 12, 72}},
        {0.25f, {85, 15, 109}},
        {0.38f, {136, 34, 106}},
        {0.50f, {186, 54, 85}},
        {0.63f, {227, 89, 51}},
        {0.75f, {249, 140, 10}},
        {0.88f, {249, 201, 45}},
        {1.00f, {252, 255, 164}},
    };
    static const std::vector<ControlPoint> turbo = {
        {0.00f, {48, 18, 59}},
        {0.13f, {70, 107, 227}},
        {0.25f, {36, 176, 240}},
        {0.38f, {30, 218, 198}},
        {0.50f, {98, 236, 137}},
        {0.63f, {183, 228, 68}},
        {0.75f, {240, 183, 42}},
        {0.88f, {240, 105, 33}},
        {1.00f, {122, 4, 3}},
    };
    // Gradiente termico purpura oscuro -> azul -> cian -> verde -> amarillo
    // -> naranja -> rojo, con negro por debajo del suelo de ruido: el
    // patron de gradiente que usan los analizadores de RF de laboratorio
    // (ver comentario en Palette.h sobre la fuente).
    static const std::vector<ControlPoint> thermal = {
        {0.00f, {0, 0, 0}},
        {0.10f, {25, 10, 60}},
        {0.25f, {60, 20, 130}},
        {0.40f, {20, 70, 200}},
        {0.55f, {0, 170, 200}},
        {0.68f, {0, 200, 100}},
        {0.80f, {230, 220, 30}},
        {0.90f, {245, 140, 20}},
        {1.00f, {230, 30, 20}},
    };

    switch (type) {
        case PaletteType::Grayscale:
            return grayscale;
        case PaletteType::Viridis:
            return viridis;
        case PaletteType::Inferno:
            return inferno;
        case PaletteType::Turbo:
            return turbo;
        case PaletteType::Thermal:
            return thermal;
    }
    return grayscale;
}

std::uint8_t lerpByte(std::uint8_t a, std::uint8_t b, float t)
{
    const float value = static_cast<float>(a) + (static_cast<float>(b) - static_cast<float>(a)) * t;
    return static_cast<std::uint8_t>(std::lround(std::clamp(value, 0.0f, 255.0f)));
}

} // namespace

void generatePalette(PaletteType type, std::array<RgbColor, 256>& outLut)
{
    const auto& points = controlPointsFor(type);

    for (int i = 0; i < 256; ++i) {
        const float t = static_cast<float>(i) / 255.0f;

        std::size_t segEnd = 1;
        while (segEnd < points.size() - 1 && t > points[segEnd].position) {
            ++segEnd;
        }
        const ControlPoint& a = points[segEnd - 1];
        const ControlPoint& b = points[segEnd];

        const float span = b.position - a.position;
        const float localT = (span > 0.0f) ? (t - a.position) / span : 0.0f;

        outLut[static_cast<std::size_t>(i)] = RgbColor{
            lerpByte(a.color.r, b.color.r, localT),
            lerpByte(a.color.g, b.color.g, localT),
            lerpByte(a.color.b, b.color.b, localT),
        };
    }
}

} // namespace rfpulse::waterfall
