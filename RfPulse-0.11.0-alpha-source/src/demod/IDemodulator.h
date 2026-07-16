#pragma once

#include <complex>
#include <cstddef>

namespace rfpulse::demod {

class IDemodulator {
public:
    virtual ~IDemodulator() = default;

    // Demodula `count` muestras IQ en banda base (ya decimadas a la tasa de
    // trabajo del demodulador) y escribe `count` muestras de audio mono en
    // `out`. El remuestreo final a la tasa del dispositivo de audio lo hace
    // AudioOutput, no el demodulador.
    virtual void demodulate(const std::complex<float>* in, float* out, std::size_t count) = 0;

    virtual void reset() = 0;
};

} // namespace rfpulse::demod
