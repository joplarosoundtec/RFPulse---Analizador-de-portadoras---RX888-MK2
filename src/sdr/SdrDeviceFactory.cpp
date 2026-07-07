#include "SdrDeviceFactory.h"

#include "SddcDevice.h"
#include "SyntheticSdrDevice.h"

namespace rfpulse::sdr {

std::unique_ptr<ISdrDevice> createDevice(SdrDeviceType type)
{
    switch (type) {
        case SdrDeviceType::SddcRx888:
            return std::make_unique<SddcDevice>();
        case SdrDeviceType::Synthetic:
            return std::make_unique<SyntheticSdrDevice>();
    }
    return nullptr;
}

std::vector<SdrDeviceInfo> enumerateSddcDevices()
{
    return SddcDevice::enumerate();
}

} // namespace rfpulse::sdr
