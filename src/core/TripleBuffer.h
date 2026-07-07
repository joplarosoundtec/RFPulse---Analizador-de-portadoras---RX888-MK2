#pragma once

#include <array>
#include <atomic>
#include <cstdint>

namespace rfpulse::core {

// Primitiva SPSC lock-free "ultimo valor publicado": un hilo productor
// escribe en writable() y publica con publish(); un hilo consumidor (uno
// solo, distinto del productor) llama a consumeLatest() y lee via latest().
// A diferencia de RingBuffer, el consumidor nunca ve un backlog: si llega
// una trama nueva antes de que el consumidor haya leido la anterior, la
// anterior simplemente se pierde — es exactamente lo que quiere un
// renderer (dibujar siempre el ultimo frame disponible, no arrastrar una
// cola de frames atrasados).
//
// Implementacion: 3 instancias de T rotan de propietario mediante un unico
// atomic<uint8_t> (2 bits de indice + 1 bit de "hay dato nuevo"). En todo
// momento hay exactamente 3 indices distintos en juego: el que escribe el
// productor (privado), el que lee el consumidor (privado), y el que "flota"
// en el atomico compartido; publish()/consumeLatest() intercambian la
// propiedad del que flota mediante un unico exchange atomico cada uno.
template <typename T>
class TripleBuffer {
public:
    template <typename... Args>
    explicit TripleBuffer(Args&&... args)
        : slots_{ T(args...), T(args...), T(args...) }
    {
    }

    // Solo debe llamarlo el hilo productor.
    T& writable() { return slots_[static_cast<std::size_t>(writeIndex_)]; }

    // Publica lo escrito en writable() como el valor mas reciente
    // disponible para el consumidor. Solo debe llamarlo el hilo productor.
    void publish()
    {
        const auto newState = static_cast<std::uint8_t>(writeIndex_) | kDirtyFlag;
        const std::uint8_t oldState = shared_.exchange(newState, std::memory_order_acq_rel);
        writeIndex_ = static_cast<int>(oldState & kIndexMask);
    }

    // Si hay una trama mas nueva que la ultima leida, la adopta (latest()
    // pasara a referenciarla) y devuelve true. Si no hay nada nuevo desde
    // la ultima llamada, devuelve false y latest() no cambia. Solo debe
    // llamarlo el hilo consumidor.
    bool consumeLatest()
    {
        const std::uint8_t current = shared_.load(std::memory_order_acquire);
        if ((current & kDirtyFlag) == 0) {
            return false;
        }
        const auto newState = static_cast<std::uint8_t>(readIndex_);
        const std::uint8_t oldState = shared_.exchange(newState, std::memory_order_acq_rel);
        readIndex_ = static_cast<int>(oldState & kIndexMask);
        return true;
    }

    const T& latest() const { return slots_[static_cast<std::size_t>(readIndex_)]; }

private:
    static constexpr std::uint8_t kDirtyFlag = 0b100;
    static constexpr std::uint8_t kIndexMask = 0b011;

    std::array<T, 3> slots_;
    int writeIndex_ = 0;
    int readIndex_ = 1;
    std::atomic<std::uint8_t> shared_{ static_cast<std::uint8_t>(2) }; // indice 2, sin dirty todavia
};

} // namespace rfpulse::core
