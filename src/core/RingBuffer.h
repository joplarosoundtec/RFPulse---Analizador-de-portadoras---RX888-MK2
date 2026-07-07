#pragma once

#include "AlignedBuffer.h"

#include <algorithm>
#include <atomic>
#include <bit>
#include <cstddef>
#include <utility>

namespace rfpulse::core {

// Cola SPSC (single-producer / single-consumer) lock-free. Un unico hilo
// puede llamar a tryPush/tryPushBulk, y un unico hilo (distinto del anterior)
// a tryPop/tryPopBulk; usar mas de un productor o mas de un consumidor
// concurrentemente rompe las garantias de esta implementacion (para eso
// haria falta MPSC/MPMC, que no es lo que necesita este pipeline).
//
// Es el conector entre cada etapa: captura SDR -> DDC wideband, DDC -> FFT de
// espectro, DDC -> cada VFO, demodulador -> audio. Toda la memoria se reserva
// en el constructor (ver AlignedBuffer); tryPush/tryPop nunca asignan ni
// liberan memoria, y nunca bloquean (sin mutex, sin condition_variable):
// devuelven false/0 si no hay hueco o dato disponible, en vez de esperar.
template <typename T>
class RingBuffer {
public:
    // capacity se redondea hacia arriba a la siguiente potencia de 2: el
    // indexado usa una mascara de bits (head & mask_), no el operador %.
    explicit RingBuffer(std::size_t capacity)
        : capacity_(std::bit_ceil(std::max<std::size_t>(capacity, 2)))
        , mask_(capacity_ - 1)
        , buffer_(capacity_)
    {
    }

    RingBuffer(const RingBuffer&) = delete;
    RingBuffer& operator=(const RingBuffer&) = delete;
    RingBuffer(RingBuffer&&) = delete;
    RingBuffer& operator=(RingBuffer&&) = delete;

    // ---- lado productor (un unico hilo) ----

    bool tryPush(const T& value)
    {
        const std::size_t head = head_.load(std::memory_order_relaxed);
        if (head - tail_.load(std::memory_order_acquire) >= capacity_) {
            return false; // lleno
        }
        buffer_[head & mask_] = value;
        head_.store(head + 1, std::memory_order_release);
        return true;
    }

    bool tryPush(T&& value)
    {
        const std::size_t head = head_.load(std::memory_order_relaxed);
        if (head - tail_.load(std::memory_order_acquire) >= capacity_) {
            return false;
        }
        buffer_[head & mask_] = std::move(value);
        head_.store(head + 1, std::memory_order_release);
        return true;
    }

    // Copia hasta `count` elementos contiguos de `values`. Devuelve cuantos
    // caben realmente en el hueco libre actual (0..count); nunca bloquea ni
    // escribe parcialmente mas alla de lo que devuelve.
    std::size_t tryPushBulk(const T* values, std::size_t count)
    {
        const std::size_t head = head_.load(std::memory_order_relaxed);
        const std::size_t tail = tail_.load(std::memory_order_acquire);
        const std::size_t freeSlots = capacity_ - (head - tail);
        const std::size_t n = std::min(count, freeSlots);

        for (std::size_t i = 0; i < n; ++i) {
            buffer_[(head + i) & mask_] = values[i];
        }
        head_.store(head + n, std::memory_order_release);
        return n;
    }

    // ---- lado consumidor (un unico hilo, distinto del productor) ----

    bool tryPop(T& value)
    {
        const std::size_t tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire)) {
            return false; // vacio
        }
        value = std::move(buffer_[tail & mask_]);
        tail_.store(tail + 1, std::memory_order_release);
        return true;
    }

    std::size_t tryPopBulk(T* out, std::size_t maxCount)
    {
        const std::size_t tail = tail_.load(std::memory_order_relaxed);
        const std::size_t head = head_.load(std::memory_order_acquire);
        const std::size_t available = head - tail;
        const std::size_t n = std::min(maxCount, available);

        for (std::size_t i = 0; i < n; ++i) {
            out[i] = std::move(buffer_[(tail + i) & mask_]);
        }
        tail_.store(tail + n, std::memory_order_release);
        return n;
    }

    // Aproximado: solo valido como metrica/telemetria (nivel de ocupacion
    // para el Logger o la UI). El productor y el consumidor pueden estar
    // modificando head_/tail_ concurrentemente en el instante de la lectura.
    [[nodiscard]] std::size_t sizeApprox() const noexcept
    {
        return head_.load(std::memory_order_relaxed) - tail_.load(std::memory_order_relaxed);
    }

    [[nodiscard]] std::size_t capacity() const noexcept { return capacity_; }

private:
    std::size_t capacity_;
    std::size_t mask_;
    AlignedBuffer<T> buffer_;

    // Cada indice en su propia linea de cache: el productor solo escribe
    // head_ (y lee tail_), el consumidor solo escribe tail_ (y lee head_).
    // Sin este separado, ambos hilos generarian false sharing constante al
    // estar head_/tail_ en la misma linea de 64 bytes.
    alignas(64) std::atomic<std::size_t> head_{0};
    alignas(64) std::atomic<std::size_t> tail_{0};
};

} // namespace rfpulse::core
