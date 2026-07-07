#include "Vfo.h"

#include <algorithm>
#include <cmath>

namespace rfpulse::vfo {

Vfo::Vfo(double widebandSampleRateHz, double audioSampleRateHz, double channelBandwidthHz, std::size_t maxBlockSize)
    : decimation_(std::max(1, static_cast<int>(std::lround(widebandSampleRateHz / audioSampleRateHz))))
    , audioSampleRateHz_(widebandSampleRateHz / static_cast<double>(decimation_))
    , nco_(widebandSampleRateHz)
    , channelFilter_(
          rfpulse::dsp::designLowpassFir(
              widebandSampleRateHz, channelBandwidthHz / 2.0,
              std::clamp(4 * decimation_ + 1, 31, 255)),
          decimation_)
    , demodulator_(rfpulse::demod::FmDemodulator::forMode(rfpulse::demod::FmMode::Narrowband, audioSampleRateHz_))
    , deemphasis_(audioSampleRateHz_, rfpulse::demod::DeemphasisTimeConstant::Us50)
    , mixedScratch_(maxBlockSize)
    , decimatedScratch_(maxBlockSize / static_cast<std::size_t>(std::max(1, decimation_)) + 1)
{
}

void Vfo::setOffsetHz(double offsetFromWidebandCenterHz)
{
    nco_.setFrequency(offsetFromWidebandCenterHz);
}

void Vfo::setMode(rfpulse::demod::FmMode mode)
{
    demodulator_ = rfpulse::demod::FmDemodulator::forMode(mode, audioSampleRateHz_);
}

std::size_t Vfo::process(const std::complex<float>* widebandIq, std::size_t count, float* audioOut)
{
    if (mixedScratch_.size() < count) {
        mixedScratch_.resize(count);
    }
    nco_.mix(widebandIq, mixedScratch_.data(), count);

    const std::size_t maxDecimatedCount = count / static_cast<std::size_t>(decimation_) + 1;
    if (decimatedScratch_.size() < maxDecimatedCount) {
        decimatedScratch_.resize(maxDecimatedCount);
    }
    const std::size_t decimatedCount = channelFilter_.process(mixedScratch_.data(), count, decimatedScratch_.data());

    demodulator_.demodulate(decimatedScratch_.data(), audioOut, decimatedCount);
    squelch_.process(decimatedScratch_.data(), decimatedCount, audioOut);
    deemphasis_.process(audioOut, decimatedCount);

    if (muted_) {
        std::fill(audioOut, audioOut + decimatedCount, 0.0f);
    } else if (volume_ != 1.0f) {
        for (std::size_t i = 0; i < decimatedCount; ++i) {
            audioOut[i] *= volume_;
        }
    }

    return decimatedCount;
}

} // namespace rfpulse::vfo
