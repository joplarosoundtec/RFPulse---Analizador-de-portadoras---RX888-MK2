#pragma once

#include "core/AlignedBuffer.h"

#include <complex>
#include <cstddef>
#include <vector>

namespace rfpulse::dsp {

// Diseña un FIR paso bajo (ventana Kaiser) con corte en cutoffHz a la tasa
// de muestreo sampleRateHz. numTaps debe ser impar (fase lineal,
// simetrico). Los coeficientes se normalizan para ganancia DC unidad
// (sum(taps) == 1): una señal a plena escala dentro de la banda de paso no
// debe cambiar de amplitud al atravesar el filtro.
std::vector<float> designLowpassFir(double sampleRateHz, double cutoffHz, int numTaps, double kaiserBeta = 6.0);

// FIR complejo con decimacion integrada: por cada `decimation` muestras de
// entrada consumidas produce como mucho 1 muestra de salida -- nunca
// calcula las salidas intermedias que se descartarian igualmente (a
// diferencia de "filtrar todo y luego decimar"). Mantiene su propia linea
// de retardo entre llamadas, asi que puede alimentarse con bloques de
// tamaño arbitrario sin perder continuidad en los bordes.
class DecimatingFirFilter {
public:
    DecimatingFirFilter(std::vector<float> taps, int decimation);

    // Escribe en `out` las muestras decimadas correspondientes a `inCount`
    // muestras de `in`. `out` debe tener espacio para al menos
    // inCount/decimation + 1 muestras. Devuelve cuantas se escribieron
    // realmente.
    std::size_t process(const std::complex<float>* in, std::size_t inCount, std::complex<float>* out);

    int decimation() const noexcept { return decimation_; }
    std::size_t numTaps() const noexcept { return taps_.size(); }

    void reset();

private:
    std::vector<float> taps_;
    int decimation_;
    rfpulse::core::AlignedBuffer<std::complex<float>> history_;
    std::size_t historyWritePos_ = 0;
    int samplesUntilNextOutput_;
};

} // namespace rfpulse::dsp
