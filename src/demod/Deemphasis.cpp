#include "Deemphasis.h"

namespace rfpulse::demod {

Deemphasis::Deemphasis(double sampleRateHz, DeemphasisTimeConstant tc)
    : sampleRateHz_(sampleRateHz)
{
    setTimeConstant(tc);
}

void Deemphasis::setTimeConstant(DeemphasisTimeConstant tc)
{
    if (tc == DeemphasisTimeConstant::None) {
        alpha_ = 1.0f; // y[n] = x[n], sin filtrar
        return;
    }

    const double tauSeconds = (tc == DeemphasisTimeConstant::Us50) ? 50e-6 : 75e-6;
    const double dt = 1.0 / sampleRateHz_;
    alpha_ = static_cast<float>(dt / (tauSeconds + dt));
}

void Deemphasis::process(float* audio, std::size_t count)
{
    for (std::size_t i = 0; i < count; ++i) {
        state_ = state_ + alpha_ * (audio[i] - state_);
        audio[i] = state_;
    }
}

} // namespace rfpulse::demod
