#pragma once

#include "demod/Deemphasis.h"
#include "demod/FmDemodulator.h"
#include "demod/Squelch.h"
#include "dsp/FirFilter.h"
#include "dsp/Nco.h"

#include <complex>
#include <cstddef>
#include <vector>

namespace rfpulse::vfo {

// Receptor virtual completo: sintoniza una frecuencia dentro del span
// wideband sin tocar la frecuencia central del SDR (Nco), decima a la tasa
// de trabajo del demodulador mediante un FIR de canal (DecimatingFirFilter),
// demodula FM, aplica squelch y de-enfasis. Consume un bloque de IQ
// wideband por llamada a process() y produce audio mono listo para
// AudioOutput.
//
// No hay una clase "VfoDdc" separada: el DDC por VFO es exactamente
// Nco::mix seguido de DecimatingFirFilter, ya modulares por si mismos: una
// envoltura adicional no aportaria nada. Tampoco hay un "VfoManager" en
// esta tarea: multiples VFOs simultaneos esta en el roadmap de "futuras
// funciones" del brief original, no en el alcance actual; cuando haga
// falta, es tan simple como instanciar varios Vfo.
class Vfo {
public:
    // maxBlockSize dimensiona los buffers de trabajo internos (sin
    // asignaciones en process() mientras los bloques de entrada no superen
    // este tamaño; si lo superan, se redimensiona automaticamente, con el
    // coste de una asignacion puntual).
    Vfo(double widebandSampleRateHz, double audioSampleRateHz, double channelBandwidthHz,
        std::size_t maxBlockSize = 32768);

    // offsetFromWidebandCenterHz: frecuencia deseada menos la frecuencia
    // central actual del SDR. Positivo = por encima del centro, negativo =
    // por debajo. Debe estar dentro de +/- (tasa wideband)/2.
    void setOffsetHz(double offsetFromWidebandCenterHz);
    double offsetHz() const noexcept { return nco_.frequencyHz(); }

    void setMode(rfpulse::demod::FmMode mode);
    void setSquelchThresholdDb(float thresholdDb) { squelch_.setThresholdDb(thresholdDb); }
    void setDeemphasis(rfpulse::demod::DeemphasisTimeConstant tc) { deemphasis_.setTimeConstant(tc); }
    void setVolume(float volume) { volume_ = volume; }
    void setMuted(bool muted) { muted_ = muted; }

    // Procesa `count` muestras IQ wideband; escribe en `audioOut` (debe
    // tener espacio para al menos count/decimacion + 1 muestras) el audio
    // resultante y devuelve cuantas muestras de audio se generaron.
    std::size_t process(const std::complex<float>* widebandIq, std::size_t count, float* audioOut);

    bool squelchOpen() const noexcept { return squelch_.isOpen(); }
    double audioSampleRateHz() const noexcept { return audioSampleRateHz_; }
    int decimation() const noexcept { return decimation_; }

private:
    // decimation_ se calcula primero (a partir de la tasa wideband y la
    // tasa de audio deseada) y audioSampleRateHz_ guarda la tasa REAL que
    // resulta de esa decimacion entera (widebandSampleRateHz / decimation_),
    // no la tasa nominal pedida -- puede diferir un poco (p.ej. pedir 48000
    // Hz con una tasa wideband de 1 Msps da una decimacion de 21, y la tasa
    // real es ~47619 Hz, no 48000). Usar la tasa nominal en vez de la real
    // para el demodulador/de-enfasis introduciria un error sistematico
    // pequeño pero innecesario, y AudioOutput necesita la tasa real para
    // remuestrear correctamente.
    int decimation_;
    double audioSampleRateHz_;

    rfpulse::dsp::Nco nco_;
    rfpulse::dsp::DecimatingFirFilter channelFilter_;
    rfpulse::demod::FmDemodulator demodulator_;
    rfpulse::demod::Deemphasis deemphasis_;
    rfpulse::demod::Squelch squelch_;

    float volume_ = 1.0f;
    bool muted_ = false;

    std::vector<std::complex<float>> mixedScratch_;
    std::vector<std::complex<float>> decimatedScratch_;
};

} // namespace rfpulse::vfo
