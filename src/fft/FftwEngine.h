#pragma once

#include "IFftEngine.h"
#include "Window.h"
#include "core/AlignedBuffer.h"

#include <array>
#include <complex>
#include <fftw3.h>
#include <memory>
#include <string>
#include <vector>

namespace rfpulse::fft {

// Tamanos de FFT soportados por el pipeline de espectro. 16384 es el valor
// por defecto recomendado para la ventana de 10 MHz (ver la tarea de
// arquitectura: con Fs=16 Msps da ~0.98 kHz/bin, resolucion adecuada para
// canales PMSE de 200-600 kHz de BW ocupado); 8192 es el modo ligero, 32768
// el modo de alta resolucion/caza de espurias.
inline constexpr std::array<std::size_t, 6> kSupportedFftSizes = {
    1024, 2048, 4096, 8192, 16384, 32768
};

// Motor de FFT del pipeline de espectro: ventaneo (Blackman-Harris) + FFT
// compleja (FFTW, precision simple) + conversion a potencia lineal/dBFS.
// Precalcula un fftwf_plan por cada tamano soportado en el constructor (con
// FFTW_MEASURE, cacheando la calibracion en wisdomPath entre ejecuciones);
// process() no crea, destruye ni reserva nada, solo ejecuta planes ya
// listos sobre buffers ya reservados.
class FftwEngine final : public IFftEngine {
public:
    explicit FftwEngine(std::string wisdomPath, WindowType windowType = WindowType::BlackmanHarris);
    ~FftwEngine() override;

    FftwEngine(const FftwEngine&) = delete;
    FftwEngine& operator=(const FftwEngine&) = delete;

    FftResult process(const std::complex<float>* input, std::size_t size) override;
    bool supportsSize(std::size_t size) const override;

    // Offset aditivo en dB aplicado tras la conversion a dBFS (calibracion
    // de referencia frente al nivel real del front-end; 0 dB por defecto,
    // es decir, dBFS puro relativo a plena escala digital). La calibracion
    // a dBm real depende del factor de ganancia que ya aplica Core::r2iq
    // (ver GAINFACTOR en third_party/sddc_core/Core/config.h) y queda para
    // la tarea de Settings/calibracion, no se inventa aqui un valor.
    void setReferenceOffsetDb(float offsetDb) { referenceOffsetDb_ = offsetDb; }
    float referenceOffsetDb() const { return referenceOffsetDb_; }

private:
    struct SizePlan {
        explicit SizePlan(std::size_t n, WindowType windowType);
        ~SizePlan();

        SizePlan(const SizePlan&) = delete;
        SizePlan& operator=(const SizePlan&) = delete;

        std::size_t size;
        fftwf_plan plan;
        rfpulse::core::AlignedBuffer<std::complex<float>> fftInput;
        rfpulse::core::AlignedBuffer<std::complex<float>> fftOutput;
        rfpulse::core::AlignedBuffer<float> window;
        rfpulse::core::AlignedBuffer<float> powerLinear;
        rfpulse::core::AlignedBuffer<float> magnitudeDb;
    };

    SizePlan* findPlan(std::size_t size) const;

    std::string wisdomPath_;
    float referenceOffsetDb_ = 0.0f;
    std::vector<std::unique_ptr<SizePlan>> plans_;
};

} // namespace rfpulse::fft
