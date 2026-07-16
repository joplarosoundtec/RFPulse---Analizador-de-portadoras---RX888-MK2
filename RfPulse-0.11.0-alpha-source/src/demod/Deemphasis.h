#pragma once

#include <cstddef>

namespace rfpulse::demod {

enum class DeemphasisTimeConstant {
    None,
    Us50,
    Us75,
};

// Filtro de-enfasis IIR de un polo: y[n] = y[n-1] + alpha*(x[n]-y[n-1]),
// alpha = dt/(tau+dt). Revierte el pre-enfasis aplicado en el transmisor
// (estandar en FM analogica: refuerza los agudos en TX para mejorar la
// relacion señal/ruido percibida; este filtro los atenua de vuelta en RX).
class Deemphasis {
public:
    Deemphasis(double sampleRateHz, DeemphasisTimeConstant tc);

    void setTimeConstant(DeemphasisTimeConstant tc);
    void process(float* audio, std::size_t count);
    void reset() { state_ = 0.0f; }

private:
    double sampleRateHz_;
    float alpha_ = 1.0f;
    float state_ = 0.0f;
};

} // namespace rfpulse::demod
