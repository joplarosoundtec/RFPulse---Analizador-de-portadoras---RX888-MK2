#include "Window.h"

#include <cmath>
#include <numbers>

namespace rfpulse::fft {

namespace {

// Coeficientes estandar de la ventana Blackman-Harris de 4 terminos (ver
// cualquier referencia de DSP, p.ej. Harris 1978 "On the Use of Windows for
// Harmonic Analysis with the Discrete Fourier Transform"). No son un ajuste
// nuestro: son constantes de la literatura, sin necesidad de validacion
// numerica propia (a diferencia de dsp/FastMath.h).
constexpr float kA0 = 0.35875f;
constexpr float kA1 = 0.48829f;
constexpr float kA2 = 0.14128f;
constexpr float kA3 = 0.01168f;

void generateBlackmanHarris(std::size_t n, float* out)
{
    if (n == 0) {
        return;
    }
    if (n == 1) {
        out[0] = 1.0f;
        return;
    }

    const double denom = static_cast<double>(n - 1);
    for (std::size_t i = 0; i < n; ++i) {
        const double phase = 2.0 * std::numbers::pi * static_cast<double>(i) / denom;
        const double w = kA0
            - kA1 * std::cos(phase)
            + kA2 * std::cos(2.0 * phase)
            - kA3 * std::cos(3.0 * phase);
        out[i] = static_cast<float>(w);
    }
}

} // namespace

void generateWindow(WindowType type, std::size_t n, float* outCoefficients)
{
    switch (type) {
        case WindowType::BlackmanHarris:
            generateBlackmanHarris(n, outCoefficients);
            break;
    }

    if (n == 0) {
        return;
    }

    // Normaliza a ganancia coherente unidad (media de los coeficientes == 1)
    // para que un tono a plena escala siga leyendose ~0 dBFS tras la ventana.
    double sum = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        sum += outCoefficients[i];
    }
    const float mean = static_cast<float>(sum / static_cast<double>(n));
    if (mean > 0.0f) {
        const float invMean = 1.0f / mean;
        for (std::size_t i = 0; i < n; ++i) {
            outCoefficients[i] *= invMean;
        }
    }
}

} // namespace rfpulse::fft
