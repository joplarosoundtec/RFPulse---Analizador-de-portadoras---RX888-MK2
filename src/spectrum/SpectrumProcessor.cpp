#include "SpectrumProcessor.h"

#include "dsp/FastMath.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace rfpulse::spectrum {

namespace {

// Media movil centrada sobre bins vecinos, en potencia LINEAL (suavizar en
// dB seria matematicamente incorrecto, igual que con el promediado
// temporal). windowBins=1 es identidad. O(n*ventana): con ventanas tipicas
// de hasta ~15-21 bins y n hasta 32768, son unas pocas decenas de miles de
// sumas por trama de FFT (no por muestra ni por frame de render), coste
// insignificante -- no hace falta la version O(n) de ventana deslizante.
void smoothPowerLinear(const float* in, float* out, std::size_t n, int windowBins)
{
    if (windowBins <= 1 || n == 0) {
        std::copy(in, in + n, out);
        return;
    }

    const int half = windowBins / 2;
    for (std::size_t i = 0; i < n; ++i) {
        double sum = 0.0;
        int count = 0;
        const long begin = static_cast<long>(i) - half;
        const long end = static_cast<long>(i) + half;
        for (long idx = begin; idx <= end; ++idx) {
            if (idx >= 0 && idx < static_cast<long>(n)) {
                sum += in[static_cast<std::size_t>(idx)];
                ++count;
            }
        }
        out[i] = static_cast<float>(sum / count);
    }
}

// Ancho (a cada lado del bin exacto) del hueco que se interpola por cada
// espurea, en Hz -- fijo en Hz (no en bins) porque la resolucion por bin
// cambia mucho con el tamaño de FFT elegido (de ~61 Hz/bin con 32768 puntos
// a ~31 kHz/bin con 1024), y el ancho real de una fuga de reloj en el
// espectro no depende de cuantos bins use la FFT para medirla.
constexpr double kSpurNotchHalfWidthHz = 15'000.0;

// Sustituye por interpolacion lineal (en potencia LINEAL, ver el comentario
// de smoothPowerLinear sobre por que no en dB) los bins mas cercanos a cada
// multiplo EXACTO de spurGridHz dentro de [freqStart, freqStart+n*hzPerBin).
// Si un hueco cae demasiado cerca del borde de la traza (no hay bin vecino
// valido a un lado), se deja esa espurea concreta sin tocar en vez de
// arriesgarse a extrapolar.
void maskKnownSpurs(float* powerLinear, std::size_t n, double freqStart, double hzPerBin, double spurGridHz)
{
    if (spurGridHz <= 0.0 || hzPerBin <= 0.0 || n == 0) {
        return;
    }

    const double freqEnd = freqStart + static_cast<double>(n) * hzPerBin;
    const long halfWidthBins = std::max<long>(1, std::lround(kSpurNotchHalfWidthHz / hzPerBin));

    const double firstMultiple = std::ceil(freqStart / spurGridHz) * spurGridHz;
    for (double spurHz = firstMultiple; spurHz <= freqEnd; spurHz += spurGridHz) {
        const long centerBin = std::lround((spurHz - freqStart) / hzPerBin);
        const long lo = centerBin - halfWidthBins;
        const long hi = centerBin + halfWidthBins;
        const long loNeighbor = lo - 1;
        const long hiNeighbor = hi + 1;
        if (loNeighbor < 0 || hiNeighbor >= static_cast<long>(n)) {
            continue;
        }

        const float a = powerLinear[static_cast<std::size_t>(loNeighbor)];
        const float b = powerLinear[static_cast<std::size_t>(hiNeighbor)];
        const long span = hiNeighbor - loNeighbor;
        for (long idx = lo; idx <= hi; ++idx) {
            const float t = static_cast<float>(idx - loNeighbor) / static_cast<float>(span);
            powerLinear[static_cast<std::size_t>(idx)] = a + t * (b - a);
        }
    }
}

} // namespace

SpectrumProcessor::SpectrumProcessor(std::size_t binCount)
    : binCount_(binCount)
    , frames_(binCount)
    , averagePowerLinear_(binCount, 0.0f)
    , maxHoldPowerLinear_(binCount, 0.0f)
    , minHoldPowerLinear_(binCount, std::numeric_limits<float>::max())
    , smoothedPowerLinear_(binCount, 0.0f)
{
}

void SpectrumProcessor::setSmoothingWidthBins(int widthBins)
{
    if (widthBins < 1) {
        widthBins = 1;
    }
    if (widthBins % 2 == 0) {
        ++widthBins;
    }
    smoothingWidthBins_ = widthBins;
}

void SpectrumProcessor::resetHolds()
{
    std::fill(maxHoldPowerLinear_.begin(), maxHoldPowerLinear_.end(), 0.0f);
    std::fill(minHoldPowerLinear_.begin(), minHoldPowerLinear_.end(), std::numeric_limits<float>::max());
}

void SpectrumProcessor::submit(const rfpulse::fft::FftResult& result, double centerFrequencyHz, double spanHz)
{
    if (result.binCount == 0 || result.powerLinear == nullptr) {
        return;
    }

    if (result.binCount != binCount_) {
        // El tamano de FFT ha cambiado (p.ej. el usuario cambio el FFT
        // size): redimensiona el estado interno y reinicia promedio/holds.
        binCount_ = result.binCount;
        averagePowerLinear_.assign(binCount_, 0.0f);
        maxHoldPowerLinear_.assign(binCount_, 0.0f);
        minHoldPowerLinear_.assign(binCount_, std::numeric_limits<float>::max());
        smoothedPowerLinear_.assign(binCount_, 0.0f);
        firstFrame_ = true;
    }

    // Suavizado EN FRECUENCIA (bins vecinos de esta misma trama), antes de
    // repartir a promedio/holds/traza actual -- todas las trazas publicadas
    // se benefician por igual del suavizado, no solo la traza en vivo.
    smoothPowerLinear(result.powerLinear, smoothedPowerLinear_.data(), binCount_, smoothingWidthBins_);

    // Filtro de espureas conocidas (ver setKnownSpurGridHz): tambien ANTES
    // de repartir a promedio/holds/traza actual, con el mismo criterio que
    // el suavizado -- si no se hiciera aqui, un max hold conservaria el
    // artefacto indefinidamente aunque la traza en vivo lo ocultara.
    if (spurGridHz_ > 0.0) {
        const double freqStart = centerFrequencyHz - spanHz / 2.0;
        const double hzPerBin = spanHz / static_cast<double>(binCount_);
        maskKnownSpurs(smoothedPowerLinear_.data(), binCount_, freqStart, hzPerBin, spurGridHz_);
    }

    for (std::size_t i = 0; i < binCount_; ++i) {
        const float power = smoothedPowerLinear_[i];

        if (averagingMode_ == AveragingMode::Exponential && !firstFrame_) {
            averagePowerLinear_[i] += averagingAlpha_ * (power - averagePowerLinear_[i]);
        } else {
            averagePowerLinear_[i] = power;
        }

        if (maxHoldEnabled_) {
            maxHoldPowerLinear_[i] = std::max(maxHoldPowerLinear_[i], power);
        }
        if (minHoldEnabled_) {
            minHoldPowerLinear_[i] = std::min(minHoldPowerLinear_[i], power);
        }
    }
    firstFrame_ = false;

    if (frozen_) {
        // El pipeline sigue vivo (promedio/holds ya actualizados arriba),
        // solo se deja de publicar al lector: la pantalla queda congelada
        // sin detener el resto del sistema.
        return;
    }

    SpectrumFrame& out = frames_.writable();
    if (out.binCount != binCount_) {
        out = SpectrumFrame(binCount_);
    }

    // out.currentDb se recalcula desde smoothedPowerLinear_ (no se copia
    // result.magnitudeDb directamente) para que tambien refleje el
    // suavizado en frecuencia, igual que promedio/holds. offset 0.0f en
    // los cuatro por igual: la calibracion de referencia (si algun dia se
    // usa FftwEngine::setReferenceOffsetDb) tendria que aplicarse aqui, de
    // forma consistente, no solo en la traza actual.
    rfpulse::dsp::powerToDb(smoothedPowerLinear_.data(), out.currentDb.data(), binCount_, 0.0f);
    rfpulse::dsp::powerToDb(averagePowerLinear_.data(), out.averageDb.data(), binCount_, 0.0f);
    rfpulse::dsp::powerToDb(maxHoldPowerLinear_.data(), out.maxHoldDb.data(), binCount_, 0.0f);
    rfpulse::dsp::powerToDb(minHoldPowerLinear_.data(), out.minHoldDb.data(), binCount_, 0.0f);

    out.centerFrequencyHz = centerFrequencyHz;
    out.spanHz = spanHz;

    frames_.publish();
}

const SpectrumFrame& SpectrumProcessor::latestFrame()
{
    frames_.consumeLatest();
    return frames_.latest();
}

} // namespace rfpulse::spectrum
