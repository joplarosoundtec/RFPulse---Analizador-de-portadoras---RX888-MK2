#pragma once

#include "ISdrDevice.h"

#include <array>
#include <memory>
#include <vector>

// Headers del SDK vendorizado (third_party/sddc_core/Core). RadioHandler.h
// arrastra FX3Class.h, config.h e Interface.h transitivamente.
#include "RadioHandler.h"

namespace rfpulse::sdr {

// Implementacion de ISdrDevice sobre el motor real del SDK RX888
// (Core::RadioHandlerClass + su motor r2iq interno, ver ExtIO_sddc-master).
//
// Deliberadamente NO se usa libsddc.h/.cpp (el wrapper C del mismo repo): esa
// capa es un esqueleto incompleto cuyo callback interno nunca reenvia datos
// (ver notas de la tarea de arquitectura). Aqui se llama directamente a
// RadioHandlerClass::Init/Start/TuneLO, tal como hace el propio ExtIO_sddc.dll
// de referencia.
//
// RadioHandlerClass::Init recibe un callback en forma de puntero a funcion en
// C (no admite capturas), asi que este wrapper registra una funcion estatica
// (coreCallbackTrampoline) que reenvia al callback de usuario guardado en la
// instancia. Init tambien recibe un puntero a r2iqControlClass opcional; se
// deja en nullptr para que RadioHandlerClass cree su fft_mt_r2iq por defecto
// (el motor DDC ancho ya optimizado en AVX2/AVX512, ver Core/fft_mt_r2iq*).
class SddcDevice final : public ISdrDevice {
public:
    SddcDevice();
    ~SddcDevice() override;

    SddcDevice(const SddcDevice&) = delete;
    SddcDevice& operator=(const SddcDevice&) = delete;
    SddcDevice(SddcDevice&&) = delete;
    SddcDevice& operator=(SddcDevice&&) = delete;

    // Enumera los dispositivos SDDC (RX888/MK2/MK3/HF103/etc.) conectados
    // actualmente por USB, sin abrir ninguno de forma persistente. Usa un
    // fx3class temporal propio y el mismo patron idx=0,1,2... que
    // fx3handler::Enumerate espera (parar en el primer indice que falle);
    // ver MAXNDEV en Core/config.h para el limite superior. Cada llamada a
    // Enumerate() abre y cierra brevemente el dispositivo para leer su
    // nombre/serie, asi que no debe invocarse mientras ese mismo dispositivo
    // esta en uso por otra instancia (streaming activo) para evitar
    // contencion sobre el mismo handle USB.
    static std::vector<SdrDeviceInfo> enumerate();

    bool open(int deviceIndex = 0) override;
    void close() override;

    bool startStreaming(double outputSampleRateHz, IqBlockCallback callback, void* context) override;
    void stopStreaming() override;

    bool setCenterFrequency(double frequencyHz) override;
    double centerFrequency() const override;

    GainSteps rfAttenuationSteps() const override;
    bool setRfAttenuationIndex(int index) override;

    GainSteps ifGainSteps() const override;
    bool setIfGainIndex(int index) override;

    void setDither(bool enabled) override;
    bool dither() const override;

    void setRandomizer(bool enabled) override;
    bool randomizer() const override;

    void setBiasTeeHf(bool enabled) override;
    bool biasTeeHf() const override;
    void setBiasTeeVhf(bool enabled) override;
    bool biasTeeVhf() const override;

    const char* name() const override;
    std::uint16_t firmwareVersion() const override;

    double adcSampleRate() const override;

    SampleRateSteps availableSampleRates() const override;

private:
    static void coreCallbackTrampoline(void* context, const float* samples, std::uint32_t sampleCount);

    // Traduce una tasa de salida deseada al indice discreto que acepta
    // RadioHandlerClass::Start (0..4, o 0..5 por encima de N2_BANDSWITCH),
    // eligiendo el mas cercano. La correspondencia idx -> tasa es
    // adcRate / 2^(idx+1); ver RadioHandlerClass::Start en RadioHandler.cpp.
    int pickSampleRateIndex(double desiredOutputHz) const;

    // Recalcula sampleRateStepsCache_/sampleRateStepsCount_ a partir de
    // adcSampleRate() (solo valido una vez abierto el dispositivo, que es
    // cuando se conoce el reloj real del ADC). Mismo umbral N2_BANDSWITCH
    // que pickSampleRateIndex, para que ambos coincidan siempre.
    void refreshSampleRateSteps();

    std::unique_ptr<fx3class> fx3_;
    std::unique_ptr<RadioHandlerClass> handler_;

    IqBlockCallback userCallback_ = nullptr;
    void* userContext_ = nullptr;

    double centerFrequencyHz_ = 0.0;
    bool streaming_ = false;
    bool opened_ = false;

    // Hasta 6 anchos de banda posibles (ver N2_BANDSWITCH); en orden
    // ascendente, poblado por refreshSampleRateSteps() al abrir.
    std::array<double, 6> sampleRateStepsCache_{};
    int sampleRateStepsCount_ = 0;
};

} // namespace rfpulse::sdr
