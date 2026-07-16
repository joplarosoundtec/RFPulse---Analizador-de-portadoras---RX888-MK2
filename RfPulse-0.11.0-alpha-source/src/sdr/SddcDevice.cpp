#include "SddcDevice.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace rfpulse::sdr {

SddcDevice::SddcDevice() = default;

SddcDevice::~SddcDevice()
{
    close();
}

std::vector<SdrDeviceInfo> SddcDevice::enumerate()
{
    std::vector<SdrDeviceInfo> devices;

    std::unique_ptr<fx3class> probe(CreateUsbHandler());
    if (!probe) {
        return devices;
    }

    // Mismo patron que SoapySDDC::findSDDC y el selector de dispositivo del
    // ExtIO original: se reutiliza la MISMA instancia de fx3class para cada
    // indice creciente (Enumerate abre/cierra el USB internamente en cada
    // llamada) hasta que uno falla, lo que fx3handler::Enumerate garantiza
    // que ocurre en el primer indice sin dispositivo presente.
    for (unsigned char idx = 0; idx < MAXNDEV; ++idx) {
        char nameBuffer[256] = {};
        if (!probe->Enumerate(idx, nameBuffer)) {
            break;
        }

        // probe->isSuperSpeed() debe leerse AHORA: refleja el Open() interno
        // que Enumerate() acaba de hacer sobre este mismo idx, y el siguiente
        // idx del bucle lo sobrescribira en la proxima vuelta.
        const bool superSpeed = probe->isSuperSpeed();
        std::string label(nameBuffer);
        if (!superSpeed) {
            // El firmware del RX888 solo expone un numero de serie real
            // (derivado del ID de silicio unico del FX3) en su descriptor
            // USB 3.0; el descriptor USB 2.0 (el que usa el enlace si no
            // llega a negociar SuperSpeed) no tiene indice de cadena de
            // serie en absoluto (ver ExtIO_sddc-master/SDDC_FX3/
            // USBdescriptor.c), asi que el "sn:" que compone Enumerate()
            // aqui siempre es basura (tipicamente un unico caracter de
            // reemplazo '?'). Se sustituye por un aviso accionable.
            const auto snPos = label.find("sn:");
            if (snPos != std::string::npos) {
                label.erase(snPos);
            }
            label += "(USB2.0, sin S/N -- prueba un puerto/cable USB3)";
        }

        devices.push_back(SdrDeviceInfo{static_cast<int>(idx), label, superSpeed});
    }

    return devices;
}

bool SddcDevice::open(int deviceIndex)
{
    if (opened_) {
        return true;
    }

    fx3_.reset(CreateUsbHandler());
    if (!fx3_) {
        return false;
    }

    // Enumerate() es quien de verdad asigna el objeto CyAPI interno
    // (fx3dev) y hace el bootstrap de firmware/enumeracion en el bus; sin
    // llamarlo antes, Open() (via GetFx3DeviceStreamer) siempre falla
    // porque ese puntero interno sigue siendo nulo -- y, peor, destruir el
    // handler mas tarde (fx3_.reset()) desreferencia ese puntero nulo
    // dentro de fx3handler::Close() y crashea (bug real, encontrado
    // ejecutando la app sin hardware conectado). deviceIndex selecciona
    // cual de los dispositivos FX3 compatibles enumerados por el driver
    // Cypress abrir (ver SdrDeviceInfo::index / SddcDevice::enumerate()).
    auto idx = static_cast<unsigned char>(deviceIndex);
    char deviceNameBuffer[256] = {};
    if (!fx3_->Enumerate(idx, deviceNameBuffer)) {
        fx3_.reset();
        return false;
    }

    // fx3class::Open() sube el firmware embebido (ver firmware.h generado por
    // bin2h a partir de SDDC_FX3.img) y completa la inicializacion del
    // dispositivo ya enumerado. No hace falta ningun archivo externo en
    // tiempo de ejecucion.
    if (!fx3_->Open()) {
        fx3_.reset();
        return false;
    }

    handler_ = std::make_unique<RadioHandlerClass>();

    // r2iqCntrl = nullptr => RadioHandlerClass crea internamente su
    // fft_mt_r2iq por defecto (el motor DDC ancho AVX2/AVX512 ya presente en
    // Core). No necesitamos aportar una implementacion propia de
    // r2iqControlClass.
    if (!handler_->Init(fx3_.get(), &SddcDevice::coreCallbackTrampoline, nullptr, this)) {
        handler_.reset();
        fx3_.reset();
        return false;
    }

    opened_ = true;
    refreshSampleRateSteps();
    return true;
}

void SddcDevice::close()
{
    if (!opened_) {
        return;
    }

    stopStreaming();

    if (handler_) {
        handler_->Close();
        handler_.reset();
    }
    fx3_.reset();

    opened_ = false;
}

bool SddcDevice::startStreaming(double outputSampleRateHz, IqBlockCallback callback, void* context)
{
    if (!opened_ || !handler_) {
        return false;
    }

    userCallback_ = callback;
    userContext_ = context;

    const int idx = pickSampleRateIndex(outputSampleRateHz);
    streaming_ = handler_->Start(idx);
    return streaming_;
}

void SddcDevice::stopStreaming()
{
    if (handler_ && streaming_) {
        handler_->Stop();
    }
    streaming_ = false;
}

bool SddcDevice::setCenterFrequency(double frequencyHz)
{
    if (!handler_) {
        return false;
    }

    const auto freq = static_cast<std::uint64_t>(frequencyHz);
    const rf_mode mode = handler_->PrepareLo(freq);
    if (mode == NOMODE) {
        return false;
    }

    if (handler_->GetmodeRF() != mode) {
        handler_->UpdatemodeRF(mode);
    }

    // TuneLO calcula y aplica internamente el offset fino de IF (necesario
    // porque el LO real del R828D no cae exactamente en la frecuencia
    // pedida) para que la salida IQ quede centrada en frequencyHz.
    handler_->TuneLO(freq);
    centerFrequencyHz_ = frequencyHz;
    return true;
}

double SddcDevice::centerFrequency() const
{
    return centerFrequencyHz_;
}

GainSteps SddcDevice::rfAttenuationSteps() const
{
    GainSteps steps;
    if (handler_) {
        steps.count = handler_->GetRFAttSteps(&steps.values);
    }
    return steps;
}

bool SddcDevice::setRfAttenuationIndex(int index)
{
    if (!handler_) {
        return false;
    }
    // RadioHandlerClass::UpdateattRF devuelve el propio indice tanto si tuvo
    // exito como 0 si fallo, indistinguible de "indice 0 aplicado con exito".
    // Es una limitacion de la API vendorizada, no la corregimos aqui.
    handler_->UpdateattRF(index);
    return true;
}

GainSteps SddcDevice::ifGainSteps() const
{
    GainSteps steps;
    if (handler_) {
        steps.count = handler_->GetIFGainSteps(&steps.values);
    }
    return steps;
}

bool SddcDevice::setIfGainIndex(int index)
{
    if (!handler_) {
        return false;
    }
    handler_->UpdateIFGain(index);
    return true;
}

void SddcDevice::setDither(bool enabled)
{
    if (handler_) {
        handler_->UptDither(enabled);
    }
}

bool SddcDevice::dither() const
{
    return handler_ ? handler_->GetDither() : false;
}

void SddcDevice::setRandomizer(bool enabled)
{
    if (handler_) {
        handler_->UptRand(enabled);
    }
}

bool SddcDevice::randomizer() const
{
    return handler_ ? handler_->GetRand() : false;
}

void SddcDevice::setBiasTeeHf(bool enabled)
{
    if (handler_) {
        handler_->UpdBiasT_HF(enabled);
    }
}

bool SddcDevice::biasTeeHf() const
{
    return handler_ ? handler_->GetBiasT_HF() : false;
}

void SddcDevice::setBiasTeeVhf(bool enabled)
{
    if (handler_) {
        handler_->UpdBiasT_VHF(enabled);
    }
}

bool SddcDevice::biasTeeVhf() const
{
    return handler_ ? handler_->GetBiasT_VHF() : false;
}

const char* SddcDevice::name() const
{
    return handler_ ? handler_->getName() : "(closed)";
}

std::uint16_t SddcDevice::firmwareVersion() const
{
    return handler_ ? handler_->GetFirmware() : 0;
}

double SddcDevice::adcSampleRate() const
{
    return handler_ ? static_cast<double>(handler_->getSampleRate()) : 0.0;
}

SampleRateSteps SddcDevice::availableSampleRates() const
{
    return SampleRateSteps{sampleRateStepsCache_.data(), sampleRateStepsCount_};
}

void SddcDevice::refreshSampleRateSteps()
{
    const double adcRate = adcSampleRate();
    const int bandCount = (adcRate > static_cast<double>(N2_BANDSWITCH)) ? 6 : 5;

    // Mismo idx -> tasa que pickSampleRateIndex (adcRate / 2^(idx+1)), pero
    // aqui se guardan TODAS las opciones en orden ascendente (idx mas alto
    // primero: idx=bandCount-1 es la tasa mas baja) para un selector de UI,
    // en vez de buscar solo la mas cercana a un valor deseado.
    sampleRateStepsCount_ = bandCount;
    for (int i = 0; i < bandCount; ++i) {
        const int idx = bandCount - 1 - i;
        sampleRateStepsCache_[static_cast<std::size_t>(i)] = adcRate / static_cast<double>(1u << (idx + 1));
    }
}

void SddcDevice::coreCallbackTrampoline(void* context, const float* samples, std::uint32_t sampleCount)
{
    auto* self = static_cast<SddcDevice*>(context);
    if (self->userCallback_ != nullptr) {
        self->userCallback_(self->userContext_, samples, sampleCount);
    }
}

int SddcDevice::pickSampleRateIndex(double desiredOutputHz) const
{
    const double adcRate = adcSampleRate();
    const int bandCount = (adcRate > static_cast<double>(N2_BANDSWITCH)) ? 6 : 5;

    // idx -> tasa de salida es adcRate / 2^(idx+1); ver el comentario
    // "0,1,2,3,4 => 32,16,8,4,2 MHz" en RadioHandlerClass::Start.
    int bestIdx = bandCount - 1;
    double bestDiff = std::numeric_limits<double>::max();
    for (int idx = 0; idx < bandCount; ++idx) {
        const double rate = adcRate / static_cast<double>(1u << (idx + 1));
        const double diff = std::abs(rate - desiredOutputHz);
        if (diff < bestDiff) {
            bestDiff = diff;
            bestIdx = idx;
        }
    }
    return bestIdx;
}

} // namespace rfpulse::sdr
