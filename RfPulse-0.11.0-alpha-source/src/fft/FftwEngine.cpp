#include "FftwEngine.h"

#include "dsp/FastMath.h"

namespace rfpulse::fft {

FftwEngine::SizePlan::SizePlan(std::size_t n, WindowType windowType)
    : size(n)
    , plan(nullptr)
    , fftInput(n)
    , fftOutput(n)
    , window(n)
    , powerLinear(n)
    , magnitudeDb(n)
{
    generateWindow(windowType, n, window.data());

    // fftwf_complex es `typedef float fftwf_complex[2]` en MSVC (no hay C99
    // <complex.h>), layout-compatible con std::complex<float> por garantia
    // del estandar (26.4.4): el reinterpret_cast es valido y es el uso
    // habitual documentado por el propio FFTW para interoperar con C++.
    plan = fftwf_plan_dft_1d(
        static_cast<int>(n),
        reinterpret_cast<fftwf_complex*>(fftInput.data()),
        reinterpret_cast<fftwf_complex*>(fftOutput.data()),
        FFTW_FORWARD,
        FFTW_MEASURE);
}

FftwEngine::SizePlan::~SizePlan()
{
    if (plan != nullptr) {
        fftwf_destroy_plan(plan);
    }
}

FftwEngine::FftwEngine(std::string wisdomPath, WindowType windowType)
    : wisdomPath_(std::move(wisdomPath))
{
    // Best-effort: en el primer arranque en una maquina nueva el archivo no
    // existe todavia y esta llamada simplemente no aporta wisdom previo (los
    // planes se calibran desde cero, ~1-2s con FFTW_MEASURE).
    fftwf_import_wisdom_from_filename(wisdomPath_.c_str());

    plans_.reserve(kSupportedFftSizes.size());
    for (std::size_t n : kSupportedFftSizes) {
        plans_.push_back(std::make_unique<SizePlan>(n, windowType));
    }

    fftwf_export_wisdom_to_filename(wisdomPath_.c_str());
}

FftwEngine::~FftwEngine() = default;

FftwEngine::SizePlan* FftwEngine::findPlan(std::size_t size) const
{
    for (const auto& p : plans_) {
        if (p->size == size) {
            return p.get();
        }
    }
    return nullptr;
}

bool FftwEngine::supportsSize(std::size_t size) const
{
    return findPlan(size) != nullptr;
}

FftResult FftwEngine::process(const std::complex<float>* input, std::size_t size)
{
    SizePlan* p = findPlan(size);
    if (p == nullptr) {
        return {};
    }

    for (std::size_t i = 0; i < size; ++i) {
        p->fftInput[i] = input[i] * p->window[i];
    }

    fftwf_execute(p->plan);

    // Normalizacion: para un tono complejo a plena escala (|A|=1), la FFT sin
    // normalizar de FFTW da |X[k]|=N en el bin correspondiente, asi que
    // dividir la potencia por N^2 deja un tono a plena escala en ~0 dBFS.
    // Reordena a la vez a frecuencia ascendente con DC en el bin central
    // (fftshift) en el mismo bucle, sin una pasada extra.
    const float invNSquared = 1.0f / (static_cast<float>(size) * static_cast<float>(size));
    const std::size_t half = size / 2;
    for (std::size_t k = 0; k < size; ++k) {
        const std::complex<float>& bin = p->fftOutput[k];
        const float power = (bin.real() * bin.real() + bin.imag() * bin.imag()) * invNSquared;
        const std::size_t dest = (k + half) % size;
        p->powerLinear[dest] = power;
    }

    rfpulse::dsp::powerToDb(p->powerLinear.data(), p->magnitudeDb.data(), size, referenceOffsetDb_);

    return FftResult{ p->powerLinear.data(), p->magnitudeDb.data(), size };
}

} // namespace rfpulse::fft
