#pragma once

#include <cstddef>
#include <new>

namespace rfpulse::core {

// 256 bits = AVX2. Todo el DSP del proyecto asume este alineamiento como
// minimo para sus buffers (ver cmake/CompilerWarnings.cmake, /arch:AVX2).
inline constexpr std::size_t kSimdAlignment = 32;

// Bloque de `count` elementos T, alineado a kSimdAlignment, con construccion
// y destruccion explicita de cada elemento. No se usa `new T[count]`: en
// MSVC eso no garantiza una alineacion mayor que alignof(T), y necesitamos
// 32 bytes de forma fiable para que el DSP vectorizado (ventaneo, magnitud,
// FIR) pueda operar sobre este almacenamiento sin comprobaciones ni caminos
// escalares de reserva. Pensado como almacenamiento interno de RingBuffer<T>
// y para buffers de DSP reutilizables que no deben pasar por el heap dentro
// del bucle de procesamiento.
template <typename T>
class AlignedBuffer {
public:
    explicit AlignedBuffer(std::size_t count)
        : count_(count)
        , data_(static_cast<T*>(::operator new[](count * sizeof(T), std::align_val_t{kSimdAlignment})))
    {
        for (std::size_t i = 0; i < count_; ++i) {
            ::new (static_cast<void*>(data_ + i)) T();
        }
    }

    ~AlignedBuffer()
    {
        for (std::size_t i = 0; i < count_; ++i) {
            data_[i].~T();
        }
        ::operator delete[](static_cast<void*>(data_), std::align_val_t{kSimdAlignment});
    }

    AlignedBuffer(const AlignedBuffer&) = delete;
    AlignedBuffer& operator=(const AlignedBuffer&) = delete;
    AlignedBuffer(AlignedBuffer&&) = delete;
    AlignedBuffer& operator=(AlignedBuffer&&) = delete;

    [[nodiscard]] T* data() noexcept { return data_; }
    [[nodiscard]] const T* data() const noexcept { return data_; }
    [[nodiscard]] std::size_t size() const noexcept { return count_; }

    [[nodiscard]] T& operator[](std::size_t idx) noexcept { return data_[idx]; }
    [[nodiscard]] const T& operator[](std::size_t idx) const noexcept { return data_[idx]; }

private:
    std::size_t count_;
    T* data_;
};

} // namespace rfpulse::core
