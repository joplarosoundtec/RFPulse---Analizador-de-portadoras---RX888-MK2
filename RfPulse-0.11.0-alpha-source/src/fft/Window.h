#pragma once

#include <cstddef>

namespace rfpulse::fft {

enum class WindowType {
    BlackmanHarris,
};

// Genera los `n` coeficientes de la ventana pedida en `outCoefficients`
// (tamano `n`, sin alineamiento particular exigido por esta funcion), ya
// normalizados para que su ganancia coherente sea 1.0 (media de los
// coeficientes == 1): un tono a plena escala debe seguir leyendose como 0
// dBFS tras aplicar la ventana, sin necesidad de una correccion aparte en
// tiempo de ejecucion.
void generateWindow(WindowType type, std::size_t n, float* outCoefficients);

} // namespace rfpulse::fft
