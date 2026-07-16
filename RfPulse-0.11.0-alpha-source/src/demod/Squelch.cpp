#include "Squelch.h"

#include "dsp/FastMath.h"

#include <algorithm>

namespace rfpulse::demod {

bool Squelch::process(const std::complex<float>* in, std::size_t count, float* audio)
{
    double sumSquares = 0.0;
    for (std::size_t i = 0; i < count; ++i) {
        const double re = in[i].real();
        const double im = in[i].imag();
        sumSquares += re * re + im * im;
    }
    const double meanPower = (count > 0) ? sumSquares / static_cast<double>(count) : 0.0;
    const float powerDb = 10.0f * rfpulse::dsp::fastLog10(static_cast<float>(std::max(meanPower, 1e-12)));

    if (open_) {
        if (powerDb < thresholdDb_ - hysteresisDb_) {
            open_ = false;
        }
    } else {
        if (powerDb > thresholdDb_) {
            open_ = true;
        }
    }

    if (!open_) {
        std::fill(audio, audio + count, 0.0f);
    }
    return open_;
}

} // namespace rfpulse::demod
