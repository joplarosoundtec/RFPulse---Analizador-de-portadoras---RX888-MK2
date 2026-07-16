#include "Nco.h"

#include <cmath>
#include <numbers>

namespace rfpulse::dsp {

Nco::Nco(double sampleRateHz)
    : sampleRateHz_(sampleRateHz)
{
}

void Nco::setFrequency(double frequencyHz)
{
    frequencyHz_ = frequencyHz;
    // Multiplicar por e^{-j*2*pi*f*n/Fs} lleva la componente que esta a
    // +f Hz hasta 0 Hz (banda base).
    const double phaseIncrement = -2.0 * std::numbers::pi * frequencyHz / sampleRateHz_;
    rotatorStep_ = std::polar(1.0, phaseIncrement);
}

void Nco::mix(const std::complex<float>* in, std::complex<float>* out, std::size_t count)
{
    for (std::size_t i = 0; i < count; ++i) {
        const auto rot = std::complex<float>(static_cast<float>(rotator_.real()), static_cast<float>(rotator_.imag()));
        out[i] = in[i] * rot;
        rotator_ *= rotatorStep_;

        if (++samplesSinceRenormalize_ >= 1024) {
            renormalize();
        }
    }
}

void Nco::reset()
{
    rotator_ = {1.0, 0.0};
    samplesSinceRenormalize_ = 0;
}

void Nco::renormalize()
{
    // La multiplicacion repetida de numeros complejos acumula error de
    // punto flotante en la magnitud del rotador (deberia ser siempre 1);
    // sin esto, tras millones de muestras el rotador podria crecer o
    // encogerse de forma perceptible.
    const double magnitude = std::abs(rotator_);
    if (magnitude > 1e-9) {
        rotator_ /= magnitude;
    }
    samplesSinceRenormalize_ = 0;
}

} // namespace rfpulse::dsp
