#include "IqAcquisition.h"

namespace rfpulse::acquisition {

bool IqAcquisition::start(double outputSampleRateHz)
{
    if (device_ == nullptr) {
        return false;
    }
    return device_->startStreaming(outputSampleRateHz, &IqAcquisition::deviceCallbackTrampoline, this);
}

void IqAcquisition::stop()
{
    if (device_ != nullptr) {
        device_->stopStreaming();
    }
}

void IqAcquisition::deviceCallbackTrampoline(void* context, const float* samples, std::uint32_t sampleCount)
{
    static_cast<IqAcquisition*>(context)->onIqBlock(samples, sampleCount);
}

void IqAcquisition::onIqBlock(const float* samples, std::uint32_t sampleCount)
{
    // samples apunta a sampleCount pares (I,Q) intercalados: layout identico
    // y garantizado por el estandar al de sampleCount std::complex<float>
    // contiguos (26.4.4), asi que el reparto es un memcpy puro, sin bucle de
    // conversion por muestra.
    const auto* complexSamples = reinterpret_cast<const std::complex<float>*>(samples);

    for (auto* consumer : consumers_) {
        const std::size_t pushed = consumer->tryPushBulk(complexSamples, sampleCount);
        if (pushed < sampleCount) {
            droppedBlocksTotal_.fetch_add(1, std::memory_order_relaxed);
        }
    }
}

} // namespace rfpulse::acquisition
