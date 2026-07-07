#pragma once

#include "SpectrumFrame.h"
#include "core/TripleBuffer.h"
#include "fft/IFftEngine.h"

#include <vector>

namespace rfpulse::spectrum {

enum class AveragingMode {
    None,
    Exponential,
};

// Consume salidas sucesivas de IFftEngine::process() (potencia lineal por
// bin) y mantiene la traza actual, el promedio (exponencial, en potencia
// LINEAL -- promediar directamente en dB seria matematicamente incorrecto),
// y Max/Min Hold persistentes. Publica la ultima trama completa a traves de
// un TripleBuffer, sin locks: el hilo que llama a submit() (el de FFT) y el
// que llama a latestFrame() (el de render) nunca se bloquean entre si.
//
// Nota de nomenclatura: el brief original distinguia "Peak Hold" de "Max
// Hold" como controles separados; en la practica son el mismo concepto
// (maximo persistente por bin desde el ultimo reset). Aqui se implementa
// una sola vez (maxHold) y la UI (tarea de Settings/UI) puede exponerlo bajo
// cualquiera de los dos nombres sin duplicar el calculo.
class SpectrumProcessor {
public:
    explicit SpectrumProcessor(std::size_t binCount);

    void setAveragingMode(AveragingMode mode) { averagingMode_ = mode; }
    void setAveragingAlpha(float alpha) { averagingAlpha_ = alpha; }

    // Ancho (en bins) de la media movil aplicada EN FRECUENCIA (potencia
    // lineal, entre bins vecinos de una misma trama) antes de repartirla a
    // las trazas actual/promedio/holds -- distinto del promediado, que es
    // en el TIEMPO (entre tramas sucesivas). 1 = sin suavizar; se fuerza a
    // impar (centrado) internamente. Es el "suavizado configurable" del
    // motor de FFT que pedia el brief original.
    void setSmoothingWidthBins(int widthBins);

    void setMaxHoldEnabled(bool enabled) { maxHoldEnabled_ = enabled; }
    void setMinHoldEnabled(bool enabled) { minHoldEnabled_ = enabled; }
    void resetHolds();

    // Si es > 0, los bins mas cercanos a cada multiplo EXACTO de
    // spurGridHz dentro del span visible se sustituyen por una
    // interpolacion lineal (en potencia, no en dB) de sus vecinos, ANTES
    // de repartir a la traza actual/promedio/holds -- para artefactos de
    // hardware conocidos de antemano y periodicos en frecuencia (p.ej. la
    // fuga del reloj de referencia de un tuner en multiplos fijos, ver
    // Application::applyCenterFrequency), no señales reales. 0 (por
    // defecto) desactiva el filtro sin coste alguno. Deliberadamente
    // generico (no sabe nada de RX888/R828D): quien conoce el hardware es
    // Application, que activa o no este valor segun corresponda.
    void setKnownSpurGridHz(double spurGridHz) { spurGridHz_ = spurGridHz; }

    // El pipeline nunca se detiene: frozen() solo deja de publicar tramas
    // nuevas al lector (submit() se sigue llamando, promedio/holds se
    // siguen actualizando), "congelando" la pantalla sin parar el sistema.
    void setFrozen(bool frozen) { frozen_ = frozen; }
    bool frozen() const { return frozen_; }

    void submit(const rfpulse::fft::FftResult& result, double centerFrequencyHz, double spanHz);

    // Adopta la ultima trama publicada si hay una mas reciente que la
    // devuelta la vez anterior, y devuelve la trama vigente (la nueva si la
    // habia, si no la misma que ya se tenia).
    const SpectrumFrame& latestFrame();

private:
    std::size_t binCount_;
    rfpulse::core::TripleBuffer<SpectrumFrame> frames_;

    AveragingMode averagingMode_ = AveragingMode::Exponential;
    float averagingAlpha_ = 0.2f;
    bool maxHoldEnabled_ = false;
    bool minHoldEnabled_ = false;
    bool frozen_ = false;
    bool firstFrame_ = true;

    std::vector<float> averagePowerLinear_;
    std::vector<float> maxHoldPowerLinear_;
    std::vector<float> minHoldPowerLinear_;

    int smoothingWidthBins_ = 1;
    std::vector<float> smoothedPowerLinear_;

    double spurGridHz_ = 0.0;
};

} // namespace rfpulse::spectrum
