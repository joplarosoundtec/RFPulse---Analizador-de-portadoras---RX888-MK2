#pragma once

#include <imgui.h>

namespace rfpulse::ui {

// Tema oscuro "instrumento de laboratorio": fondo casi negro, esquinas
// afiladas (sin el redondeado de "app generica"), bordes finos, acento
// ambar/naranja para los controles (coherente con el aspecto de marca de
// los analizadores de RF profesionales de gama alta que sirven de
// inspiracion) y colores de traza fijos y con buen contraste sobre negro
// para los datos (espectro/waterfall), separados deliberadamente del color
// de acento de la interfaz.
void applyDarkInstrumentTheme();

// Colores de traza del espectro: fijos, no los que ImPlot cicla por
// defecto. Verde vivo = traza en vivo (color clasico de fosforo de
// analizador de espectro), cian = promedio, rojo = max hold, azul acero =
// min hold -- convencion habitual en instrumentacion de RF.
namespace TraceColors {
inline constexpr ImVec4 kCurrent(0.68f, 1.0f, 0.20f, 1.0f);
inline constexpr ImVec4 kAverage(0.15f, 0.85f, 1.0f, 1.0f);
inline constexpr ImVec4 kMaxHold(1.0f, 0.30f, 0.25f, 1.0f);
inline constexpr ImVec4 kMinHold(0.35f, 0.55f, 0.95f, 1.0f);
} // namespace TraceColors

} // namespace rfpulse::ui
