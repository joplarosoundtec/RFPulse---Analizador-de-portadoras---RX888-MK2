#pragma once

#include "core/AlignedBuffer.h"

#include <atomic>
#include <cstddef>

namespace rfpulse::waterfall {

// Mantiene el historial de magnitudes (dB) del waterfall como una matriz
// logicamente circular: `rowCount` filas de `binCount` valores cada una.
// pushRow() sobreescribe la fila mas antigua (indice modulo rowCount) en
// vez de desplazar todo el buffer; el "scroll" visual lo hace
// WaterfallRenderer desplazando las coordenadas de textura al dibujar (ver
// ese modulo), asi que aqui nunca se mueve memoria salvo la fila nueva.
//
// Pensado para un unico escritor (el hilo de espectro, via pushRow) y un
// unico lector (el hilo de render, via currentRow()/data()) concurrentes:
// currentRow_ es atomico y se publica con release DESPUES de escribir la
// fila, y se lee con acquire ANTES de leer history_, para que el lector
// nunca vea una fila a medio escribir.
class WaterfallEngine {
public:
    WaterfallEngine(std::size_t rowCount, std::size_t binCount);

    // Ignora la llamada si binCount no coincide con el configurado (p.ej.
    // tras un cambio de tamano de FFT sin haber recreado el motor). Solo
    // debe llamarlo el hilo escritor (el de espectro).
    void pushRow(const float* magnitudeDb, std::size_t binCount);

    // Solo debe llamarlos el hilo lector (el de render).
    const float* data() const noexcept { return history_.data(); }
    std::size_t rowCount() const noexcept { return rowCount_; }
    std::size_t binCount() const noexcept { return binCount_; }

    // Fila (0..rowCount()-1) donde se escribio el ultimo pushRow: la fila
    // mas reciente del historial.
    std::size_t currentRow() const noexcept { return currentRow_.load(std::memory_order_acquire); }

    // Cuantas filas del historial contienen datos reales (saturado en
    // rowCount()). Antes de la primera vuelta completa del buffer circular,
    // las filas todavia no escritas quedan a su valor inicial (0.0f dB, ver
    // AlignedBuffer), que el waterfall pintaria como una banda de color
    // caliente falsa si se mostrara sin distinguirla de datos reales; el
    // renderer usa este valor para dibujar solo la parte ya escrita mientras
    // el historial se llena por primera vez.
    std::size_t filledRowCount() const noexcept { return filledRowCount_.load(std::memory_order_acquire); }

private:
    std::size_t rowCount_;
    std::size_t binCount_;
    std::atomic<std::size_t> currentRow_{0};
    std::atomic<std::size_t> filledRowCount_{0};
    rfpulse::core::AlignedBuffer<float> history_;
};

} // namespace rfpulse::waterfall
