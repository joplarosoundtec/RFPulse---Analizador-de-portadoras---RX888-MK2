#pragma once

#include <cstdint>
#include <string>

namespace rfpulse::sdr {

// Describe un dispositivo SDR fisico detectado por enumeracion USB, antes de
// abrirlo. `index` es el indice de enumeracion (0..MAXNDEV-1 para el SDK
// SDDC, ver fx3class::Enumerate) que hay que pasar a ISdrDevice::open() para
// abrir precisamente ese dispositivo. `label` es el nombre + numero de serie
// tal como lo reporta el propio hardware (ej. "RX888 MK2  sn:0123456789"),
// pensado para mostrarse directamente en un selector de la UI.
//
// `superSpeed` indica si el enlace nego USB 3.x (SuperSpeed) en vez de
// USB 2.0 High-Speed. Es relevante porque el firmware del RX888 solo incluye
// un numero de serie real (derivado del ID de silicio unico del FX3) en su
// descriptor USB 3.0; el descriptor USB 2.0 no tiene indice de cadena de
// serie en absoluto (ver ExtIO_sddc-master/SDDC_FX3/USBdescriptor.c), asi
// que por debajo de SuperSpeed nunca hay un S/N real que leer -- `label` ya
// viene ajustado por SddcDevice::enumerate() para reflejar esto en vez de
// mostrar un valor basura.
struct SdrDeviceInfo {
    int index = 0;
    std::string label;
    bool superSpeed = true;
};

// Invocado desde el hilo interno de captura del dispositivo (para SddcDevice,
// el submit_thread de Core::RadioHandlerClass) cada vez que hay un bloque
// nuevo de IQ ya convertido a banda base. Firma de function pointer puro (no
// std::function) a proposito: es una interfaz de tiempo real, sin
// asignaciones ni type erasure en el hot path.
//
// IMPORTANTE: `sampleCount` es el numero de MUESTRAS COMPLEJAS, no de floats.
// `samples` apunta a `sampleCount` pares (I,Q) intercalados: I0,Q0,I1,Q1,...
// (2 * sampleCount floats en total). Este detalle no es obvio a partir de la
// firma y es facil de malinterpretar.
using IqBlockCallback = void (*)(void* context, const float* samples, std::uint32_t sampleCount);

struct GainSteps {
    // Punteros a las tablas de pasos en dB expuestas por el hardware
    // (dependen del modelo de radio concreto). Validos mientras el
    // dispositivo este abierto; no cachear mas alla de eso.
    const float* values = nullptr;
    int count = 0;
};

// Anchos de banda (Hz) de salida REALMENTE alcanzables por este hardware en
// concreto, en orden ascendente. El motor DDC del SDK SDDC (fft_mt_r2iq)
// solo decima en potencias de 2 a partir del reloj del ADC -- para un
// RX888 MK2 tipico (ADC a 64 Msps) eso son exactamente 2/4/8/16/32 MHz, no
// un rango continuo (ver SddcDevice::availableSampleRates() para el
// detalle y RadioHandlerClass::Start en el SDK vendorizado). Pedir un
// valor que no sea uno de estos hace que startStreaming() se quede en
// silencio con el mas cercano de verdad; por eso el resto del pipeline
// (SpectrumProcessor, Vfo) debe usar siempre uno de estos valores, nunca
// uno arbitrario, para que el eje de frecuencias no se descuadre.
struct SampleRateSteps {
    const double* valuesHz = nullptr;
    int count = 0;
};

// Abstraccion de un receptor SDR fisico. Una implementacion (SddcDevice) por
// familia de hardware; SdrDeviceFactory decide cual instanciar. Todo el
// control aqui es de "setup", no de tiempo real: el unico camino de tiempo
// real es IqBlockCallback.
class ISdrDevice {
public:
    virtual ~ISdrDevice() = default;

    // Abre el dispositivo fisico (USB + firmware + identificacion de
    // hardware). Debe llamarse una unica vez antes de cualquier otro metodo.
    // `deviceIndex` selecciona cual de los dispositivos enumerados abrir
    // (ver SdrDeviceInfo::index / SdrDeviceFactory::enumerateSddcDevices);
    // las implementaciones que no soportan varios dispositivos (p.ej.
    // SyntheticSdrDevice) lo ignoran.
    virtual bool open(int deviceIndex = 0) = 0;
    virtual void close() = 0;

    // Arranca (o reconfigura en caliente) el streaming de IQ a la tasa de
    // salida deseada. Puede llamarse de nuevo mientras ya esta corriendo para
    // cambiar de tasa; el propio dispositivo detiene y relanza el streaming
    // internamente. El callback se invoca desde el hilo de captura interno
    // del dispositivo, nunca desde el hilo que llama a startStreaming.
    virtual bool startStreaming(double outputSampleRateHz, IqBlockCallback callback, void* context) = 0;
    virtual void stopStreaming() = 0;

    // Ajusta la frecuencia central deseada. La implementacion decide y aplica
    // internamente el modo HF/VHF y cualquier correccion fina de IF
    // necesaria para que el IQ de salida quede centrado exactamente en
    // frequencyHz. Devuelve false si la frecuencia esta fuera del rango que
    // soporta el hardware.
    virtual bool setCenterFrequency(double frequencyHz) = 0;
    virtual double centerFrequency() const = 0;

    // No existe un control de AGC real en este hardware (el tuner solo
    // expone atenuador RF + ganancia IF manuales, ver GainSteps). Un AGC de
    // software que ajuste estos indices automaticamente segun el nivel de
    // señal es una capa a construir mas adelante sobre este mismo dispositivo,
    // no un metodo de ISdrDevice.
    virtual GainSteps rfAttenuationSteps() const = 0;
    virtual bool setRfAttenuationIndex(int index) = 0;

    virtual GainSteps ifGainSteps() const = 0;
    virtual bool setIfGainIndex(int index) = 0;

    virtual void setDither(bool enabled) = 0;
    virtual bool dither() const = 0;

    virtual void setRandomizer(bool enabled) = 0;
    virtual bool randomizer() const = 0;

    virtual void setBiasTeeHf(bool enabled) = 0;
    virtual bool biasTeeHf() const = 0;
    virtual void setBiasTeeVhf(bool enabled) = 0;
    virtual bool biasTeeVhf() const = 0;

    virtual const char* name() const = 0;
    virtual std::uint16_t firmwareVersion() const = 0;

    virtual double adcSampleRate() const = 0;

    // Anchos de banda de salida realmente alcanzables (ver SampleRateSteps).
    // Los punteros son validos mientras el dispositivo este abierto, igual
    // que GainSteps.
    virtual SampleRateSteps availableSampleRates() const = 0;
};

} // namespace rfpulse::sdr
