#pragma once

#include "ISdrDevice.h"

#include <array>
#include <atomic>
#include <thread>

namespace rfpulse::sdr {

// Dispositivo SDR simulado: genera IQ sintetico (dos "transmisores" a
// frecuencias absolutas fijas + ruido) en vez de hablar con hardware real.
// Se usa como fallback automatico cuando no se encuentra un RX888
// conectado, para que el resto de la aplicacion (UI, espectro, waterfall,
// VFO/audio) se pueda ejercitar y verificar sin hardware -- es un
// ISdrDevice mas: el resto del pipeline no sabe ni le importa que esta
// "sintonizando" un generador de señales en vez de un tuner real.
class SyntheticSdrDevice final : public ISdrDevice {
public:
    SyntheticSdrDevice() = default;
    ~SyntheticSdrDevice() override;

    bool open(int /*deviceIndex*/) override { return true; }
    void close() override { stopStreaming(); }

    bool startStreaming(double outputSampleRateHz, IqBlockCallback callback, void* context) override;
    void stopStreaming() override;

    bool setCenterFrequency(double frequencyHz) override;
    double centerFrequency() const override;

    GainSteps rfAttenuationSteps() const override { return {}; }
    bool setRfAttenuationIndex(int) override { return true; }
    GainSteps ifGainSteps() const override { return {}; }
    bool setIfGainIndex(int) override { return true; }

    void setDither(bool) override { }
    bool dither() const override { return false; }
    void setRandomizer(bool) override { }
    bool randomizer() const override { return false; }
    void setBiasTeeHf(bool) override { }
    bool biasTeeHf() const override { return false; }
    void setBiasTeeVhf(bool) override { }
    bool biasTeeVhf() const override { return false; }

    const char* name() const override { return "Generador sintetico (sin hardware RX888 detectado)"; }
    std::uint16_t firmwareVersion() const override { return 0; }
    double adcSampleRate() const override { return outputSampleRateHz_; }

    // El generador sintetico puede producir en realidad CUALQUIER tasa
    // (es puro software, sin restriccion de reloj de ADC), pero se ofrecen
    // las mismas 5 opciones que un RX888 MK2 real (ADC de 64 Msps) para que
    // el selector de span de la UI se comporte igual en modo DEMO que con
    // hardware real -- util para probar esa parte de la UI sin RX888.
    SampleRateSteps availableSampleRates() const override
    {
        static constexpr std::array<double, 5> kDemoSampleRates = {
            2'000'000.0, 4'000'000.0, 8'000'000.0, 16'000'000.0, 32'000'000.0
        };
        return SampleRateSteps{kDemoSampleRates.data(), static_cast<int>(kDemoSampleRates.size())};
    }

private:
    void threadMain();

    std::atomic<double> centerFrequencyHz_{0.0};
    double outputSampleRateHz_ = 0.0;
    IqBlockCallback callback_ = nullptr;
    void* context_ = nullptr;
    std::thread thread_;
    std::atomic<bool> running_{false};
};

} // namespace rfpulse::sdr
