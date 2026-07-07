#pragma once

#include "ISdrDevice.h"

#include <memory>
#include <vector>

namespace rfpulse::sdr {

enum class SdrDeviceType {
    // RX888 / RX888 MK2 / RX888 MK3 / HF103 / BBRF103 / RX999 / Lucy, todos
    // servidos por Core::RadioHandlerClass (el modelo concreto se detecta en
    // tiempo de ejecucion via SddcDevice::open()).
    SddcRx888,

    // Generador de IQ sintetico (dos tonos + ruido), sin hardware. Fallback
    // automatico de Application cuando SddcRx888::open() falla, para poder
    // ejercitar el resto de la aplicacion sin un RX888 conectado.
    Synthetic,
};

// Punto unico de extension para soportar otras familias de SDR en el futuro:
// anadir un valor a SdrDeviceType y un caso en createDevice, sin tocar
// ISdrDevice ni el resto del pipeline.
std::unique_ptr<ISdrDevice> createDevice(SdrDeviceType type);

// Enumera los RX888 (u otro hardware compatible con el SDK SDDC) conectados
// actualmente por USB, sin abrir ninguno de forma persistente. Pensado para
// poblar un selector de dispositivo en la UI antes de llamar a
// createDevice(SdrDeviceType::SddcRx888) + open(indice elegido). No incluye
// una entrada para el generador sintetico: esa opcion siempre esta
// disponible y la decide la UI, no la enumeracion de hardware.
std::vector<SdrDeviceInfo> enumerateSddcDevices();

} // namespace rfpulse::sdr
