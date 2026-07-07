#include "SyntheticSdrDevice.h"

#include <chrono>
#include <complex>
#include <numbers>
#include <random>
#include <vector>

namespace rfpulse::sdr {

namespace {
// Dos "transmisores" a frecuencias absolutas fijas, como si fueran dos
// petacas inalambricas reales emitiendo en la banda de trabajo; al cambiar
// la frecuencia central sintonizada se mueven por la pantalla exactamente
// igual que lo harian transmisores de verdad.
constexpr double kTone1AbsoluteHz = 558'000'000.0;
constexpr double kTone2AbsoluteHz = 561'000'000.0;
constexpr std::size_t kBlockSize = 32768;
} // namespace

SyntheticSdrDevice::~SyntheticSdrDevice()
{
    stopStreaming();
}

bool SyntheticSdrDevice::setCenterFrequency(double frequencyHz)
{
    centerFrequencyHz_.store(frequencyHz, std::memory_order_relaxed);
    return true;
}

double SyntheticSdrDevice::centerFrequency() const
{
    return centerFrequencyHz_.load(std::memory_order_relaxed);
}

bool SyntheticSdrDevice::startStreaming(double outputSampleRateHz, IqBlockCallback callback, void* context)
{
    stopStreaming();
    outputSampleRateHz_ = outputSampleRateHz;
    callback_ = callback;
    context_ = context;
    running_.store(true, std::memory_order_release);
    thread_ = std::thread([this]() { threadMain(); });
    return true;
}

void SyntheticSdrDevice::stopStreaming()
{
    if (running_.exchange(false, std::memory_order_acq_rel)) {
        if (thread_.joinable()) {
            thread_.join();
        }
    }
}

void SyntheticSdrDevice::threadMain()
{
    std::vector<std::complex<float>> block(kBlockSize);
    std::mt19937 rng(12345);
    std::normal_distribution<float> noise(0.0f, 0.02f);

    double phase1 = 0.0;
    double phase2 = 0.0;
    const auto blockDuration = std::chrono::duration<double>(static_cast<double>(kBlockSize) / outputSampleRateHz_);

    while (running_.load(std::memory_order_acquire)) {
        const double center = centerFrequencyHz_.load(std::memory_order_relaxed);
        const double offset1 = kTone1AbsoluteHz - center;
        const double offset2 = kTone2AbsoluteHz - center;
        const double step1 = 2.0 * std::numbers::pi * offset1 / outputSampleRateHz_;
        const double step2 = 2.0 * std::numbers::pi * offset2 / outputSampleRateHz_;

        for (std::size_t i = 0; i < kBlockSize; ++i) {
            const double p1 = phase1 + step1 * static_cast<double>(i);
            const double p2 = phase2 + step2 * static_cast<double>(i);
            const float i1 = 0.5f * static_cast<float>(std::cos(p1));
            const float q1 = 0.5f * static_cast<float>(std::sin(p1));
            const float i2 = 0.2f * static_cast<float>(std::cos(p2));
            const float q2 = 0.2f * static_cast<float>(std::sin(p2));
            block[i] = std::complex<float>(i1 + i2 + noise(rng), q1 + q2 + noise(rng));
        }
        phase1 += step1 * static_cast<double>(kBlockSize);
        phase2 += step2 * static_cast<double>(kBlockSize);

        if (callback_ != nullptr) {
            callback_(context_, reinterpret_cast<const float*>(block.data()), static_cast<std::uint32_t>(kBlockSize));
        }

        std::this_thread::sleep_for(blockDuration);
    }
}

} // namespace rfpulse::sdr
