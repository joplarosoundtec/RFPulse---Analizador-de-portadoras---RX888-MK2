#pragma once

#include <array>
#include <cstdint>

namespace rfpulse::waterfall {

enum class PaletteType {
    Viridis,
    Inferno,
    Turbo,
    Grayscale,
    // Gradiente termico purpura->azul->cian->verde->amarillo->naranja->rojo,
    // como el que usan los analizadores de RF de laboratorio de gama alta
    // (confirmado a partir de los metadatos de gradiente CMYK publicados
    // por Aaronia para su RTSA Suite PRO, no transcrito de memoria).
    Thermal,
};

struct RgbColor {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
};

// Genera una tabla de 256 colores para la paleta pedida, interpolando
// linealmente entre un conjunto reducido de puntos de control que
// aproximan visualmente el colormap con ese nombre. No son las tablas
// oficiales publicadas byte a byte (Viridis/Inferno/Turbo tienen 256+
// entradas exactas publicadas que no se reproducen aqui de memoria para no
// arriesgar una transcripcion incorrecta) — son una aproximacion curada,
// visualmente muy similar, suficiente para un waterfall de monitorizacion
// de RF.
void generatePalette(PaletteType type, std::array<RgbColor, 256>& outLut);

} // namespace rfpulse::waterfall
