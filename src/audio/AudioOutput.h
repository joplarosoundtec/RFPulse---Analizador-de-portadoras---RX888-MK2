#pragma once

#include "core/RingBuffer.h"

#include <atomic>
#include <audioclient.h>
#include <cstdint>
#include <mmdeviceapi.h>
#include <thread>
#include <windows.h>
#include <wrl/client.h>

namespace rfpulse::audio {

// Salida de audio WASAPI en modo compartido (shared), orientada a baja
// latencia: un hilo propio, con prioridad de tiempo real, espera al evento
// de "hay hueco en el buffer" del dispositivo y lo rellena desde un
// RingBuffer<float> mono interno. El resto del pipeline (Vfo::process)
// solo llama a write(): nunca ve WASAPI ni bloquea si el hilo de audio va
// mas lento (las muestras que no quepan en el ring buffer se descartan).
//
// Se inicializa siempre con el "mix format" real del dispositivo (WASAPI
// garantiza que ese formato funciona en modo compartido sin negociacion
// adicional). Si la tasa a la que escribe el llamador (sampleRateHz en
// open()) no coincide con la del dispositivo, o si el dispositivo es
// estereo (lo habitual) y nuestro audio es mono, se adapta en el propio
// hilo de audio: remuestreo lineal simple (no un resampler polifasico --
// es audio de monitorizacion por voz, no la ruta de medida de RF) y
// duplicado del canal mono a todos los canales del dispositivo.
class AudioOutput {
public:
    explicit AudioOutput(std::size_t ringBufferCapacity = 1u << 16);
    ~AudioOutput();

    AudioOutput(const AudioOutput&) = delete;
    AudioOutput& operator=(const AudioOutput&) = delete;

    // Abre el dispositivo de audio de salida por defecto del sistema.
    // sampleRateHz es la tasa a la que el llamador escribira audio via
    // write(). Devuelve false (sin lanzar) si no hay dispositivo de audio
    // disponible -- el resto de la aplicacion debe poder seguir
    // funcionando sin audio.
    bool open(std::uint32_t sampleRateHz);
    void close();

    void start();
    void stop();

    // Encola muestras mono para reproduccion. Nunca bloquea: si el ring
    // buffer interno esta lleno, las muestras que no quepan se descartan.
    // Devuelve cuantas se encolaron realmente.
    std::size_t write(const float* samples, std::size_t count);

    std::uint32_t sampleRateHz() const noexcept { return sampleRateHz_; }
    bool isOpen() const noexcept { return audioClient_ != nullptr; }

private:
    void audioThreadMain();
    float nextResampledSample();

    Microsoft::WRL::ComPtr<IMMDeviceEnumerator> enumerator_;
    Microsoft::WRL::ComPtr<IMMDevice> device_;
    Microsoft::WRL::ComPtr<IAudioClient> audioClient_;
    Microsoft::WRL::ComPtr<IAudioRenderClient> renderClient_;

    rfpulse::core::RingBuffer<float> ring_;

    std::uint32_t sampleRateHz_ = 0;      // tasa a la que escribe el llamador
    std::uint32_t deviceSampleRateHz_ = 0; // tasa real del mix format del dispositivo
    std::uint32_t deviceChannelCount_ = 0;
    std::uint32_t bufferFrameCount_ = 0;
    HANDLE samplesReadyEvent_ = nullptr;

    std::thread audioThread_;
    std::atomic<bool> running_{false};

    double resamplePos_ = 0.0;
    double resampleStep_ = 1.0;
    float prevSample_ = 0.0f;
    float currSample_ = 0.0f;
};

} // namespace rfpulse::audio
