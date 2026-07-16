#pragma once

#include <complex>
#include <cstddef>

namespace rfpulse::fft {

// Resultado de procesar un bloque: magnitudes en potencia lineal y en dBFS,
// ya reordenadas en frecuencia ascendente (DC en el bin central, ver
// FftwEngine::process). Los punteros son validos hasta la siguiente llamada
// a process() con el mismo motor (buffers internos reutilizados, sin
// asignaciones). Este motor no promedia ni mantiene Peak/Max/Min Hold entre
// llamadas: eso es responsabilidad de SpectrumProcessor (modulo aparte).
struct FftResult {
    const float* powerLinear = nullptr;
    const float* magnitudeDb = nullptr;
    std::size_t binCount = 0;
};

class IFftEngine {
public:
    virtual ~IFftEngine() = default;

    // `input` debe tener exactamente `size` muestras IQ. `size` debe ser uno
    // de los tamanos soportados (ver supportsSize); si no lo es, devuelve un
    // FftResult vacio (binCount == 0).
    virtual FftResult process(const std::complex<float>* input, std::size_t size) = 0;

    virtual bool supportsSize(std::size_t size) const = 0;
};

} // namespace rfpulse::fft
