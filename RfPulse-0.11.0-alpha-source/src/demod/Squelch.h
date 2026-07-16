#pragma once

#include <complex>
#include <cstddef>

namespace rfpulse::demod {

// Silencia el audio cuando la potencia de la señal de entrada cae por
// debajo de thresholdDb (dBFS, misma referencia que el resto del
// pipeline). Usa un pequeño margen de histeresis (abre en thresholdDb,
// cierra en thresholdDb - hysteresisDb) para evitar aperturas/cierres
// rapidos cuando la señal esta justo en el borde del umbral.
class Squelch {
public:
    explicit Squelch(float thresholdDb = -50.0f, float hysteresisDb = 3.0f)
        : thresholdDb_(thresholdDb)
        , hysteresisDb_(hysteresisDb)
    {
    }

    void setThresholdDb(float thresholdDb) { thresholdDb_ = thresholdDb; }
    void setHysteresisDb(float hysteresisDb) { hysteresisDb_ = hysteresisDb; }

    // Evalua la potencia media (dBFS) de `in` y, si el squelch esta
    // cerrado, silencia `audio` (mismo tamaño que in). Devuelve true si el
    // squelch esta abierto (hay señal) tras evaluar este bloque.
    bool process(const std::complex<float>* in, std::size_t count, float* audio);

    bool isOpen() const noexcept { return open_; }

private:
    float thresholdDb_;
    float hysteresisDb_;
    bool open_ = false;
};

} // namespace rfpulse::demod
