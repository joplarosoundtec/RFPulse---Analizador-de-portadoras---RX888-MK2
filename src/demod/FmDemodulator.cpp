#include "FmDemodulator.h"

#include <numbers>

namespace rfpulse::demod {

FmDemodulator FmDemodulator::forMode(FmMode mode, double sampleRateHz)
{
    const double maxDeviationHz = (mode == FmMode::Narrowband) ? 5000.0 : 75000.0;
    // El discriminador entrega radianes/muestra; a la desviacion maxima
    // esperada, la fase entre muestras consecutivas es
    // 2*pi*maxDeviationHz/sampleRateHz. Esta ganancia normaliza eso a
    // amplitud de audio ~1.0 en el pico de desviacion.
    const double gain = sampleRateHz / (2.0 * std::numbers::pi * maxDeviationHz);
    return FmDemodulator(static_cast<float>(gain));
}

void FmDemodulator::demodulate(const std::complex<float>* in, float* out, std::size_t count)
{
    for (std::size_t i = 0; i < count; ++i) {
        if (!havePrevious_) {
            out[i] = 0.0f;
        } else {
            const std::complex<float> product = in[i] * std::conj(previous_);
            out[i] = gain_ * std::arg(product);
        }
        previous_ = in[i];
        havePrevious_ = true;
    }
}

} // namespace rfpulse::demod
