#pragma once

#include "core/RingBuffer.h"
#include "sdr/ISdrDevice.h"

#include <atomic>
#include <complex>
#include <cstdint>
#include <vector>

namespace rfpulse::acquisition {

// Puente entre el callback de IQ del dispositivo (sdr::ISdrDevice) y los
// ring buffers de consumo de los pipelines (espectro y cada VFO). Cada
// bloque que entrega el dispositivo se reparte (memcpy via tryPushBulk, sin
// conversion) a todos los consumidores registrados; si alguno esta lleno,
// el bloque se descarta SOLO para ese consumidor concreto — el hilo de
// captura del dispositivo (propiedad de Core) nunca se bloquea.
//
// Esto es el "Wideband DDC" de la arquitectura: Core::r2iq ya hace la
// conversion real ADC -> IQ (ver la nota de la tarea de SDR Device), asi que
// esta clase no reimplementa esa conversion, solo distribuye su salida.
class IqAcquisition {
public:
    IqAcquisition() = default;
    ~IqAcquisition() = default;

    IqAcquisition(const IqAcquisition&) = delete;
    IqAcquisition& operator=(const IqAcquisition&) = delete;

    // No toma ownership: el dispositivo lo posee quien construya la
    // aplicacion (Application, ver tarea de UI/app).
    void attachDevice(sdr::ISdrDevice* device) { device_ = device; }

    // Registra un consumidor (tampoco toma ownership). Debe llamarse antes
    // de start(): anadir consumidores mientras el streaming esta activo no
    // es seguro (el vector no esta protegido para escritura concurrente).
    void addConsumer(rfpulse::core::RingBuffer<std::complex<float>>* consumer) { consumers_.push_back(consumer); }

    bool start(double outputSampleRateHz);
    void stop();

    // Bloques descartados en total (suma sobre todos los consumidores)
    // desde el arranque. Metrica para Logging/UI, no logica de control.
    std::uint64_t droppedBlocksTotal() const noexcept { return droppedBlocksTotal_.load(std::memory_order_relaxed); }

private:
    static void deviceCallbackTrampoline(void* context, const float* samples, std::uint32_t sampleCount);
    void onIqBlock(const float* samples, std::uint32_t sampleCount);

    sdr::ISdrDevice* device_ = nullptr;
    std::vector<rfpulse::core::RingBuffer<std::complex<float>>*> consumers_;
    std::atomic<std::uint64_t> droppedBlocksTotal_{0};
};

} // namespace rfpulse::acquisition
