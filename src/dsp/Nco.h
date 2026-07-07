#pragma once

#include <complex>
#include <cstddef>
#include <cstdint>

namespace rfpulse::dsp {

// Oscilador numerico controlado: mezcla (desplaza en frecuencia) una senal
// IQ multiplicando muestra a muestra por un vector unitario que rota a paso
// de fase constante -- mas barato que llamar a std::sin/cos por muestra.
// Es la implementacion del DDC por VFO: seleccionar cualquier frecuencia
// dentro del span wideband sin tocar la frecuencia central del SDR.
class Nco {
public:
    explicit Nco(double sampleRateHz);

    // frequencyHz es el offset (positivo o negativo) respecto al centro de
    // la banda base wideband que se quiere llevar a 0 Hz. No reinicia la
    // fase acumulada: cambiar de frecuencia no produce un salto de fase
    // audible, solo un cambio de pendiente.
    void setFrequency(double frequencyHz);
    double frequencyHz() const noexcept { return frequencyHz_; }

    // Mezcla `count` muestras de `in` y escribe el resultado en `out`
    // (puede ser el mismo puntero, la mezcla es in-place-safe muestra a
    // muestra).
    void mix(const std::complex<float>* in, std::complex<float>* out, std::size_t count);

    void reset();

private:
    void renormalize();

    double sampleRateHz_;
    double frequencyHz_ = 0.0;
    std::complex<double> rotator_{1.0, 0.0};
    std::complex<double> rotatorStep_{1.0, 0.0};
    std::uint32_t samplesSinceRenormalize_ = 0;
};

} // namespace rfpulse::dsp
