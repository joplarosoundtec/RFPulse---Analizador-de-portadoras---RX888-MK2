#include "AudioOutput.h"

using Microsoft::WRL::ComPtr;

namespace rfpulse::audio {

namespace {
constexpr REFERENCE_TIME kBufferDuration100ns = 200000; // 20 ms
}

AudioOutput::AudioOutput(std::size_t ringBufferCapacity)
    : ring_(ringBufferCapacity)
{
}

AudioOutput::~AudioOutput()
{
    close();
}

bool AudioOutput::open(std::uint32_t sampleRateHz)
{
    close();

    HRESULT hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(enumerator_.GetAddressOf()));
    if (FAILED(hr)) {
        return false;
    }

    hr = enumerator_->GetDefaultAudioEndpoint(eRender, eConsole, device_.GetAddressOf());
    if (FAILED(hr)) {
        return false;
    }

    hr = device_->Activate(
        __uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(audioClient_.GetAddressOf()));
    if (FAILED(hr)) {
        return false;
    }

    // Se usa siempre el mix format real del dispositivo: WASAPI garantiza
    // que ese formato funciona en modo compartido sin negociacion
    // adicional (a diferencia de pedir un formato propio y tener que
    // manejar el caso en que el driver no lo soporte).
    WAVEFORMATEX* mixFormat = nullptr;
    hr = audioClient_->GetMixFormat(&mixFormat);
    if (FAILED(hr) || mixFormat == nullptr) {
        return false;
    }
    deviceSampleRateHz_ = mixFormat->nSamplesPerSec;
    deviceChannelCount_ = mixFormat->nChannels;

    hr = audioClient_->Initialize(
        AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, kBufferDuration100ns, 0, mixFormat, nullptr);
    CoTaskMemFree(mixFormat);
    if (FAILED(hr)) {
        return false;
    }

    hr = audioClient_->GetBufferSize(&bufferFrameCount_);
    if (FAILED(hr)) {
        return false;
    }

    samplesReadyEvent_ = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (samplesReadyEvent_ == nullptr) {
        return false;
    }
    hr = audioClient_->SetEventHandle(samplesReadyEvent_);
    if (FAILED(hr)) {
        return false;
    }

    hr = audioClient_->GetService(
        __uuidof(IAudioRenderClient), reinterpret_cast<void**>(renderClient_.GetAddressOf()));
    if (FAILED(hr)) {
        return false;
    }

    sampleRateHz_ = sampleRateHz;
    resampleStep_ = static_cast<double>(sampleRateHz_) / static_cast<double>(deviceSampleRateHz_);
    resamplePos_ = 1.0; // fuerza a extraer una muestra real del ring buffer desde el primer frame
    prevSample_ = 0.0f;
    currSample_ = 0.0f;

    return true;
}

void AudioOutput::close()
{
    stop();

    if (samplesReadyEvent_ != nullptr) {
        CloseHandle(samplesReadyEvent_);
        samplesReadyEvent_ = nullptr;
    }

    renderClient_.Reset();
    audioClient_.Reset();
    device_.Reset();
    enumerator_.Reset();
}

void AudioOutput::start()
{
    if (audioClient_ == nullptr || running_.load(std::memory_order_acquire)) {
        return;
    }
    audioClient_->Start();
    running_.store(true, std::memory_order_release);
    audioThread_ = std::thread([this]() { audioThreadMain(); });
}

void AudioOutput::stop()
{
    if (!running_.exchange(false, std::memory_order_acq_rel)) {
        return;
    }
    if (samplesReadyEvent_ != nullptr) {
        SetEvent(samplesReadyEvent_); // despierta al hilo de audio para que vea running_==false
    }
    if (audioThread_.joinable()) {
        audioThread_.join();
    }
    if (audioClient_ != nullptr) {
        audioClient_->Stop();
    }
}

std::size_t AudioOutput::write(const float* samples, std::size_t count)
{
    return ring_.tryPushBulk(samples, count);
}

float AudioOutput::nextResampledSample()
{
    while (resamplePos_ >= 1.0) {
        prevSample_ = currSample_;
        float popped = 0.0f;
        if (ring_.tryPop(popped)) {
            currSample_ = popped;
        }
        // Si no hay dato nuevo, currSample_ se mantiene: se repite la
        // ultima muestra (silencio/nivel constante) en vez de un corte
        // brusco si el productor va temporalmente mas lento.
        resamplePos_ -= 1.0;
    }
    const float value = prevSample_ + static_cast<float>(resamplePos_) * (currSample_ - prevSample_);
    resamplePos_ += resampleStep_;
    return value;
}

void AudioOutput::audioThreadMain()
{
    const HRESULT hrCo = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool comInitialized = SUCCEEDED(hrCo);

    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

    while (running_.load(std::memory_order_acquire)) {
        const DWORD waitResult = WaitForSingleObject(samplesReadyEvent_, 100);
        if (!running_.load(std::memory_order_acquire)) {
            break;
        }
        if (waitResult != WAIT_OBJECT_0) {
            continue; // timeout: reintenta (permite volver a comprobar running_)
        }

        UINT32 paddingFrames = 0;
        if (FAILED(audioClient_->GetCurrentPadding(&paddingFrames))) {
            continue;
        }
        const UINT32 framesAvailable = bufferFrameCount_ - paddingFrames;
        if (framesAvailable == 0) {
            continue;
        }

        BYTE* data = nullptr;
        if (FAILED(renderClient_->GetBuffer(framesAvailable, &data))) {
            continue;
        }

        auto* floatData = reinterpret_cast<float*>(data);
        for (UINT32 frame = 0; frame < framesAvailable; ++frame) {
            const float sample = nextResampledSample();
            for (std::uint32_t ch = 0; ch < deviceChannelCount_; ++ch) {
                floatData[frame * deviceChannelCount_ + ch] = sample;
            }
        }

        renderClient_->ReleaseBuffer(framesAvailable, 0);
    }

    if (comInitialized) {
        CoUninitialize();
    }
}

} // namespace rfpulse::audio
