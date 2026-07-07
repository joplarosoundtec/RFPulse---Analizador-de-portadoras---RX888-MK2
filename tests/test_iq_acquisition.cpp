#include "acquisition/IqAcquisition.h"
#include "core/RingBuffer.h"
#include "sdr/ISdrDevice.h"

#include <gtest/gtest.h>

#include <complex>
#include <vector>

using rfpulse::acquisition::IqAcquisition;
using rfpulse::core::RingBuffer;
using rfpulse::sdr::GainSteps;
using rfpulse::sdr::ISdrDevice;
using rfpulse::sdr::IqBlockCallback;

namespace {

// Doble de prueba de ISdrDevice: no habla con hardware real, solo permite
// inyectar bloques como si vinieran del callback de streaming, para poder
// probar la logica de reparto/descarte de IqAcquisition de forma aislada.
class FakeSdrDevice final : public ISdrDevice {
public:
    bool open(int /*deviceIndex*/) override { return true; }
    void close() override { }

    bool startStreaming(double, IqBlockCallback callback, void* context) override
    {
        callback_ = callback;
        context_ = context;
        return true;
    }

    void stopStreaming() override { callback_ = nullptr; }

    bool setCenterFrequency(double) override { return true; }
    double centerFrequency() const override { return 0.0; }

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

    const char* name() const override { return "Fake"; }
    std::uint16_t firmwareVersion() const override { return 0; }
    double adcSampleRate() const override { return 64'000'000.0; }
    rfpulse::sdr::SampleRateSteps availableSampleRates() const override { return {}; }

    void injectBlock(const float* samples, std::uint32_t count)
    {
        if (callback_ != nullptr) {
            callback_(context_, samples, count);
        }
    }

private:
    IqBlockCallback callback_ = nullptr;
    void* context_ = nullptr;
};

} // namespace

TEST(IqAcquisition, FansOutBlockToAllConsumers)
{
    FakeSdrDevice device;
    IqAcquisition acq;
    acq.attachDevice(&device);

    RingBuffer<std::complex<float>> consumerA(16);
    RingBuffer<std::complex<float>> consumerB(16);
    acq.addConsumer(&consumerA);
    acq.addConsumer(&consumerB);

    ASSERT_TRUE(acq.start(16'000'000.0));

    const float block[] = {1.0f, 2.0f, 3.0f, 4.0f}; // 2 muestras IQ: (1,2) (3,4)
    device.injectBlock(block, 2);

    std::complex<float> value;
    ASSERT_TRUE(consumerA.tryPop(value));
    EXPECT_FLOAT_EQ(value.real(), 1.0f);
    EXPECT_FLOAT_EQ(value.imag(), 2.0f);

    ASSERT_TRUE(consumerB.tryPop(value));
    EXPECT_FLOAT_EQ(value.real(), 1.0f);
    EXPECT_FLOAT_EQ(value.imag(), 2.0f);

    EXPECT_EQ(acq.droppedBlocksTotal(), 0u);
}

TEST(IqAcquisition, CountsDroppedBlocksWhenConsumerCannotFitWholeBlock)
{
    FakeSdrDevice device;
    IqAcquisition acq;
    acq.attachDevice(&device);

    RingBuffer<std::complex<float>> tinyConsumer(2); // capacidad real 2
    acq.addConsumer(&tinyConsumer);

    ASSERT_TRUE(acq.start(16'000'000.0));

    std::vector<float> block(8, 0.0f); // 4 muestras IQ; no caben 4 en un buffer de capacidad 2
    device.injectBlock(block.data(), 4);

    EXPECT_EQ(acq.droppedBlocksTotal(), 1u);
}

TEST(IqAcquisition, StartFailsWithoutAttachedDevice)
{
    IqAcquisition acq;
    EXPECT_FALSE(acq.start(16'000'000.0));
}
