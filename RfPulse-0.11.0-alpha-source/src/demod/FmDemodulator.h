#pragma once

#include "IDemodulator.h"

namespace rfpulse::demod {

enum class FmMode {
    Narrowband, // ~5 kHz de desviacion: PMSE analogico tipico (Shure/Sennheiser/Wisycom/Lectrosonics)
    Wideband,   // ~75 kHz de desviacion: FM de radiodifusion, mono
};

// Discriminador FM por producto cruzado: para cada par de muestras
// consecutivas x[n], x[n-1], el angulo de x[n]*conj(x[n-1]) es
// proporcional a la desviacion de frecuencia instantanea. NFM y WFM son la
// misma matematica con distinta desviacion esperada (y por tanto distinta
// ganancia de normalizacion) -- el brief original los trataba como
// controles separados, pero duplicar la clase no aportaria nada; aqui es
// una unica clase parametrizada por FmMode (forMode) o por ganancia
// explicita (setGain).
class FmDemodulator final : public IDemodulator {
public:
    static FmDemodulator forMode(FmMode mode, double sampleRateHz);

    explicit FmDemodulator(float gain = 1.0f)
        : gain_(gain)
    {
    }

    void demodulate(const std::complex<float>* in, float* out, std::size_t count) override;
    void reset() override
    {
        havePrevious_ = false;
        previous_ = {};
    }

    void setGain(float gain) { gain_ = gain; }
    float gain() const noexcept { return gain_; }

private:
    std::complex<float> previous_{0.0f, 0.0f};
    bool havePrevious_ = false;
    float gain_;
};

} // namespace rfpulse::demod
