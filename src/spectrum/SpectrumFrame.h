#pragma once

#include <cstddef>
#include <vector>

namespace rfpulse::spectrum {

// Trama lista para dibujar: todas las trazas ya en dB, ya en frecuencia
// ascendente (ver FftwEngine::process). Vive dentro de un
// core::TripleBuffer<SpectrumFrame> (ver SpectrumProcessor) — se
// redimensiona solo cuando cambia el tamano de FFT, nunca por trama.
struct SpectrumFrame {
    explicit SpectrumFrame(std::size_t n = 0)
        : currentDb(n, -200.0f)
        , averageDb(n, -200.0f)
        , maxHoldDb(n, -200.0f)
        , minHoldDb(n, 0.0f)
        , binCount(n)
    {
    }

    std::vector<float> currentDb;
    std::vector<float> averageDb;
    std::vector<float> maxHoldDb;
    std::vector<float> minHoldDb;
    std::size_t binCount;
    double centerFrequencyHz = 0.0;
    double spanHz = 0.0;
};

} // namespace rfpulse::spectrum
