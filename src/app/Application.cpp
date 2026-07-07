#include "Application.h"

#include "sdr/SdrDeviceFactory.h"
#include "ui/Theme.h"

#include <imgui.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

namespace rfpulse::app {

namespace {

bool isSupportedFftSize(std::size_t n)
{
    for (std::size_t v : rfpulse::fft::kSupportedFftSizes) {
        if (v == n) {
            return true;
        }
    }
    return false;
}

constexpr rfpulse::waterfall::PaletteType kPaletteValues[] = {
    rfpulse::waterfall::PaletteType::Viridis,
    rfpulse::waterfall::PaletteType::Inferno,
    rfpulse::waterfall::PaletteType::Turbo,
    rfpulse::waterfall::PaletteType::Grayscale,
    rfpulse::waterfall::PaletteType::Thermal,
};
constexpr int kPaletteCount = 5;

rfpulse::demod::DeemphasisTimeConstant deemphasisFromIndex(int index)
{
    switch (index) {
        case 0:
            return rfpulse::demod::DeemphasisTimeConstant::None;
        case 2:
            return rfpulse::demod::DeemphasisTimeConstant::Us75;
        default:
            return rfpulse::demod::DeemphasisTimeConstant::Us50;
    }
}

} // namespace

Application::Application(rfpulse::render::GraphicsDevice& graphics)
    : graphics_(graphics)
    , logger_("rfpulse.log")
    , settings_("rfpulse_settings.txt")
    , spectrumRing_(1u << 20)
    , vfoRing_(1u << 20)
{
    loadSettings();
    presetFreqInputMHz_ = centerFrequencyHz_.load(std::memory_order_relaxed) / 1e6;
    logger_.info("RfPulse iniciando");

    openDevice();
    if (device_) {
        // El primer setCenterFrequency() en banda VHF/UHF (el caso tipico:
        // 560 MHz por defecto) hace que el SDK cambie de modo HF a VHF (ver
        // RX888R2Radio::UpdatemodeRF) -- y ese cambio de modo aparca los
        // registros de ganancia del camino HF a valores propios fijos, sin
        // tocar los registros VHF (R82XX_ATTENUATOR/R82XX_VGA) en absoluto.
        // Si se aplica rfAttenuationIndex_/ifGainIndex_ ANTES de este primer
        // cambio de modo, esas llamadas van al camino de ganancia
        // EQUIVOCADO (HF, el que todavia esta activo en ese instante) y los
        // registros VHF se quedan en lo que traiga el chip de fabrica (bug
        // real reportado: atenuacion/ganancia real al maximo pese a que la
        // UI muestra 0 dB, hasta tocar a mano un slider una vez que ya se
        // esta en modo VHF). Por eso applyCenterFrequency() va aqui, ANTES
        // de aplicar la ganancia guardada -- para que, si el arranque cae en
        // VHF, el cambio de modo ya haya pasado y la ganancia se aplique al
        // camino correcto.
        applyCenterFrequency(centerFrequencyHz_.load(std::memory_order_relaxed));
        device_->setRfAttenuationIndex(rfAttenuationIndex_);
        device_->setIfGainIndex(ifGainIndex_);
    }
    // El span persistido (o el de fabrica, kDefaultSpanHz) puede no ser una
    // de las opciones que este dispositivo concreto puede dar de verdad
    // (ver ISdrDevice::availableSampleRates); se ajusta ANTES de arrancar
    // la adquisicion para que lo que se le pide al dispositivo y lo que
    // asume el resto del pipeline (SpectrumProcessor, Vfo) sea siempre el
    // mismo numero.
    spanHz_ = snapToNearestAvailableSpan(spanHz_);

    fftEngine_ = std::make_unique<rfpulse::fft::FftwEngine>("fftw_wisdom.dat");
    recreateSpectrumProcessor(fftSize_);

    waterfallEngine_ = std::make_unique<rfpulse::waterfall::WaterfallEngine>(kWaterfallRows, fftSize_);
    waterfallRenderer_ = std::make_unique<rfpulse::render::WaterfallRenderer>(graphics_, fftSize_, kWaterfallRows);
    waterfallRenderer_->setPalette(kPaletteValues[paletteIndex_ % kPaletteCount]);
    waterfallRenderer_->setDbRange(waterfallMinDb_, waterfallMaxDb_);

    acquisition_.attachDevice(device_.get());
    acquisition_.addConsumer(&spectrumRing_);
    acquisition_.addConsumer(&vfoRing_);
    acquisition_.start(spanHz_);

    if (!audioOutput_.open(static_cast<std::uint32_t>(kAudioSampleRateHz))) {
        logger_.warning("No se pudo abrir ningun dispositivo de audio de salida; el VFO funcionara sin sonido.");
    }
    audioOutput_.start();

    startSpectrumThread();
    startVfoThread();

    logger_.info(std::string("RfPulse listo (") + (usingSyntheticDevice_ ? "modo demostracion" : "hardware real") + ")");
}

Application::~Application()
{
    saveSettings();

    stopSpectrumThread();
    stopVfoThread();

    acquisition_.stop();
    audioOutput_.stop();
    audioOutput_.close();

    logger_.info("RfPulse cerrado");
}

void Application::refreshDeviceList()
{
    availableDevices_ = rfpulse::sdr::enumerateSddcDevices();
}

void Application::openDevice()
{
    refreshDeviceList();

    if (!availableDevices_.empty()) {
        device_ = rfpulse::sdr::createDevice(rfpulse::sdr::SdrDeviceType::SddcRx888);
        if (device_ && device_->open(availableDevices_.front().index)) {
            usingSyntheticDevice_ = false;
            selectedDeviceIndex_ = availableDevices_.front().index;
            logger_.info(std::string("Dispositivo SDR detectado: ") + device_->name());
            return;
        }
    }

    logger_.warning("No se detecto ningun RX888 conectado; usando generador sintetico de demostracion.");
    device_ = rfpulse::sdr::createDevice(rfpulse::sdr::SdrDeviceType::Synthetic);
    device_->open(0);
    usingSyntheticDevice_ = true;
    selectedDeviceIndex_ = kSyntheticDeviceIndex;
}

void Application::switchToDevice(int deviceIndex)
{
    if (deviceIndex == selectedDeviceIndex_) {
        return;
    }

    // Se detiene el streaming ANTES de tocar device_: acquisition_ solo
    // guarda un puntero prestado (no tiene ownership, ver
    // IqAcquisition::attachDevice), asi que destruir el dispositivo viejo
    // mientras su hilo de captura interno (propiedad de Core) sigue
    // entregando bloques dejaria ese puntero colgando.
    acquisition_.stop();

    std::unique_ptr<rfpulse::sdr::ISdrDevice> newDevice;
    bool newIsSynthetic = false;

    if (deviceIndex == kSyntheticDeviceIndex) {
        newDevice = rfpulse::sdr::createDevice(rfpulse::sdr::SdrDeviceType::Synthetic);
        newDevice->open(0);
        newIsSynthetic = true;
    } else {
        newDevice = rfpulse::sdr::createDevice(rfpulse::sdr::SdrDeviceType::SddcRx888);
        if (!newDevice || !newDevice->open(deviceIndex)) {
            logger_.warning(
                "No se pudo abrir el dispositivo SDR elegido (indice " + std::to_string(deviceIndex)
                + "); se mantiene el dispositivo anterior.");
            newDevice.reset();
        }
    }

    if (newDevice) {
        device_ = std::move(newDevice);
        usingSyntheticDevice_ = newIsSynthetic;
        selectedDeviceIndex_ = deviceIndex;
        logger_.info(std::string("Dispositivo SDR cambiado a: ") + device_->name());

        // applyCenterFrequency() ANTES de aplicar la ganancia guardada, por
        // el mismo motivo que en el constructor (ver su comentario): el
        // primer setCenterFrequency() en VHF/UHF hace que el SDK cambie de
        // modo HF a VHF, lo que aparca los registros de ganancia del
        // camino HF sin tocar los del camino VHF -- si se aplicara la
        // ganancia ANTES de ese cambio de modo, iria al camino equivocado y
        // los registros VHF se quedarian en su valor de fabrica.
        applyCenterFrequency(centerFrequencyHz_.load(std::memory_order_relaxed));
        device_->setRfAttenuationIndex(rfAttenuationIndex_);
        device_->setIfGainIndex(ifGainIndex_);
    }

    // Un dispositivo distinto (otro modelo, u otra unidad con un reloj de
    // ADC distinto) puede tener opciones de span distintas -- se reajusta
    // por si acaso, igual que en el constructor (ver
    // ISdrDevice::availableSampleRates).
    const double previousSpanHz = spanHz_;
    spanHz_ = snapToNearestAvailableSpan(spanHz_);
    if (spanHz_ != previousSpanHz && spectrumProcessor_) {
        spectrumProcessor_->resetHolds();
    }

    // Si la apertura del nuevo dispositivo fallo, device_ sigue apuntando al
    // dispositivo anterior (nunca se toco), asi que reenganchar y arrancar
    // de nuevo simplemente restaura el streaming previo sin dejar la app sin
    // dispositivo.
    acquisition_.attachDevice(device_.get());
    acquisition_.start(spanHz_);
}

bool Application::isVhfModeAt(double frequencyHz) const
{
    // Por debajo de este umbral el RX888 MK2 muestrea directo (sin el
    // tuner R828D ni su PLL), asi que ni el ajuste de LO ni el filtro de
    // espureas (ambos ligados al reloj de referencia del tuner, ver
    // kSpurRefClockHz) tienen sentido ahi.
    const double vhfThresholdHz = device_ ? device_->adcSampleRate() / 2.0 : 32'000'000.0;
    return frequencyHz >= vhfThresholdHz;
}

void Application::applyCenterFrequency(double frequencyHz)
{
    double adjustedHz = frequencyHz;
    centerFrequencySpurAdjusted_ = false;

    const bool vhfMode = isVhfModeAt(frequencyHz);
    if (vhfMode) {
        const double nearestMultiple = std::round(frequencyHz / kSpurRefClockHz) * kSpurRefClockHz;
        if (std::abs(frequencyHz - nearestMultiple) < kSpurAvoidanceThresholdHz) {
            adjustedHz = nearestMultiple + kSpurAvoidanceOffsetHz;
            centerFrequencySpurAdjusted_ = true;
            logger_.info(
                "Frecuencia central ajustada de " + std::to_string(frequencyHz / 1e6) + " a "
                + std::to_string(adjustedHz / 1e6) + " MHz para evitar espurea del tuner (multiplo de 16 MHz)");
        }
    }

    // centerFrequencyHz_ (y por tanto el eje del espectro, ver
    // spectrumThreadMain) refleja SIEMPRE la frecuencia REALMENTE aplicada
    // al hardware, nunca la pedida sin ajustar -- lo mismo que exige la
    // correccion del span (ver ISdrDevice::availableSampleRates): un solo
    // numero, coherente en todo el pipeline.
    centerFrequencyHz_.store(adjustedHz, std::memory_order_relaxed);
    if (device_) {
        device_->setCenterFrequency(adjustedHz);
    }

    // El filtro de espureas de toda la traza (no solo evitar que el CENTRO
    // coincida con una, ver kSpurAvoidanceOffsetHz mas arriba) solo tiene
    // sentido en modo VHF/UHF: con spans anchos (hasta 32 MHz) puede haber
    // varios multiplos de 16 MHz visibles a la vez, lejos del centro.
    if (spectrumProcessor_) {
        spectrumProcessor_->setKnownSpurGridHz(vhfMode ? kSpurRefClockHz : 0.0);
        // Max-hold/min-hold acumulan potencia por INDICE de bin, que tras un
        // cambio de centro pasa a representar una frecuencia distinta a la
        // de antes -- sin resetear, seguirian mostrando el hold del
        // espectro anterior mal etiquetado sobre el eje nuevo, igual que ya
        // se hacia al cambiar el span (ver applySpanHz). Misma limitacion
        // preexistente que applySpanHz: el promedio exponencial no tiene un
        // reset propio y solo se realinea gradualmente, frame a frame.
        spectrumProcessor_->resetHolds();
    }
}

void Application::recreateSpectrumProcessor(std::size_t fftSize)
{
    spectrumProcessor_ = std::make_unique<rfpulse::spectrum::SpectrumProcessor>(fftSize);
    spectrumProcessor_->setAveragingMode(
        averagingEnabled_ ? rfpulse::spectrum::AveragingMode::Exponential : rfpulse::spectrum::AveragingMode::None);
    spectrumProcessor_->setAveragingAlpha(averagingAlpha_);
    spectrumProcessor_->setMaxHoldEnabled(maxHoldEnabled_);
    spectrumProcessor_->setMinHoldEnabled(minHoldEnabled_);
    spectrumProcessor_->setFrozen(frozen_);
    spectrumProcessor_->setSmoothingWidthBins(smoothingWidthBins_);

    const bool vhfMode = isVhfModeAt(centerFrequencyHz_.load(std::memory_order_relaxed));
    spectrumProcessor_->setKnownSpurGridHz(vhfMode ? kSpurRefClockHz : 0.0);
}

void Application::applyFftSize(std::size_t fftSize)
{
    if (fftSize == fftSize_ || !isSupportedFftSize(fftSize)) {
        return;
    }

    stopSpectrumThread();

    fftSize_ = fftSize;
    recreateSpectrumProcessor(fftSize_);

    waterfallEngine_ = std::make_unique<rfpulse::waterfall::WaterfallEngine>(kWaterfallRows, fftSize_);
    waterfallRenderer_ = std::make_unique<rfpulse::render::WaterfallRenderer>(graphics_, fftSize_, kWaterfallRows);
    waterfallRenderer_->setPalette(kPaletteValues[paletteIndex_ % kPaletteCount]);
    waterfallRenderer_->setDbRange(waterfallMinDb_, waterfallMaxDb_);

    startSpectrumThread();
    logger_.info("Tamano de FFT cambiado a " + std::to_string(fftSize_));
}

double Application::snapToNearestAvailableSpan(double desiredHz) const
{
    if (!device_) {
        return desiredHz;
    }
    const auto steps = device_->availableSampleRates();
    if (steps.count <= 0) {
        // Dispositivo sin tabla de opciones (no deberia pasar con las
        // implementaciones actuales, pero es un valor razonable si pasara):
        // se deja el valor pedido tal cual en vez de fallar.
        return desiredHz;
    }

    // Solo se consideran las opciones que caben bajo el limite probado
    // fiable del modo ACTUAL (ver kVhfMaxSpanHz/kHfMaxSpanHz) -- asi el
    // combo de Span nunca puede acabar en un valor que se sabe que da
    // lecturas incorrectas para la frecuencia central de momento.
    const bool vhfMode = isVhfModeAt(centerFrequencyHz_.load(std::memory_order_relaxed));
    const double capHz = vhfMode ? kVhfMaxSpanHz : kHfMaxSpanHz;

    double bestHz = 0.0;
    double bestDiff = std::numeric_limits<double>::max();
    for (int i = 0; i < steps.count; ++i) {
        if (steps.valuesHz[i] > capHz + 1.0) {
            continue;
        }
        const double diff = std::abs(steps.valuesHz[i] - desiredHz);
        if (diff < bestDiff) {
            bestDiff = diff;
            bestHz = steps.valuesHz[i];
        }
    }
    if (bestDiff == std::numeric_limits<double>::max()) {
        // Ningun paso del dispositivo cabe bajo el limite de este modo (no
        // deberia pasar con las tasas reales del RX888 MK2): mejor devolver
        // la opcion mas cercana sin limite que dejar el span sin snapear.
        bestHz = steps.valuesHz[0];
        bestDiff = std::abs(bestHz - desiredHz);
        for (int i = 1; i < steps.count; ++i) {
            const double diff = std::abs(steps.valuesHz[i] - desiredHz);
            if (diff < bestDiff) {
                bestDiff = diff;
                bestHz = steps.valuesHz[i];
            }
        }
    }
    return bestHz;
}

void Application::applySpanHz(double newSpanHz)
{
    const double snapped = snapToNearestAvailableSpan(newSpanHz);
    if (snapped == spanHz_) {
        return;
    }

    // A diferencia de applyFftSize(), no hace falta parar el hilo de
    // espectro: su tamano de acumulacion depende de fftSize_ (sin cambios
    // aqui), y spectrumProcessor_->submit() ya lee spanHz_ de fresco en
    // cada llamada, asi que en cuanto acquisition_ vuelva a entregar datos
    // a la nueva tasa, el resto del pipeline lo recoge solo.
    acquisition_.stop();
    spanHz_ = snapped;
    acquisition_.start(spanHz_);

    // El significado de cada bin (que Hz representa) cambia con el span,
    // asi que los holds/promedio acumulados con el span anterior ya no
    // corresponden a las mismas frecuencias -- se limpian para no mezclar
    // datos de dos escalas distintas en la misma traza.
    if (spectrumProcessor_) {
        spectrumProcessor_->resetHolds();
    }

    logger_.info("Span cambiado a " + std::to_string(spanHz_ / 1e6) + " MHz");
}

void Application::startSpectrumThread()
{
    spectrumThreadRunning_.store(true, std::memory_order_release);
    spectrumThread_ = std::thread([this]() { spectrumThreadMain(); });
}

void Application::stopSpectrumThread()
{
    spectrumThreadRunning_.store(false, std::memory_order_release);
    if (spectrumThread_.joinable()) {
        spectrumThread_.join();
    }
}

void Application::spectrumThreadMain()
{
    std::vector<std::complex<float>> accum(fftSize_, std::complex<float>(0.0f, 0.0f));
    std::size_t filled = 0;

    while (spectrumThreadRunning_.load(std::memory_order_acquire)) {
        const std::size_t popped = spectrumRing_.tryPopBulk(accum.data() + filled, fftSize_ - filled);
        filled += popped;

        if (filled < fftSize_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        const auto result = fftEngine_->process(accum.data(), fftSize_);
        if (result.binCount > 0) {
            spectrumProcessor_->submit(result, centerFrequencyHz_.load(std::memory_order_relaxed), spanHz_);
            waterfallEngine_->pushRow(result.magnitudeDb, result.binCount);
        }
        filled = 0;
    }
}

void Application::startVfoThread()
{
    vfoThreadRunning_.store(true, std::memory_order_release);
    vfoThread_ = std::thread([this]() { vfoThreadMain(); });
}

void Application::stopVfoThread()
{
    vfoThreadRunning_.store(false, std::memory_order_release);
    if (vfoThread_.joinable()) {
        vfoThread_.join();
    }
}

void Application::vfoThreadMain()
{
    std::vector<std::complex<float>> iqScratch(65536);
    std::vector<float> audioScratch(65536);

    while (vfoThreadRunning_.load(std::memory_order_acquire)) {
        const std::size_t popped = vfoRing_.tryPopBulk(iqScratch.data(), iqScratch.size());
        if (popped == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        std::lock_guard<std::mutex> lock(vfoMutex_);
        if (vfoActive_ && activeVfo_) {
            const std::size_t audioCount = activeVfo_->process(iqScratch.data(), popped, audioScratch.data());
            audioOutput_.write(audioScratch.data(), audioCount);
        }
    }
}

void Application::tuneVfoAt(double frequencyHz)
{
    const double offsetHz = frequencyHz - centerFrequencyHz_.load(std::memory_order_relaxed);

    auto newVfo =
        std::make_unique<rfpulse::vfo::Vfo>(spanHz_, kAudioSampleRateHz, channelBandwidthKHz_ * 1000.0);
    newVfo->setOffsetHz(offsetHz);
    newVfo->setMode(fmModeIndex_ == 0 ? rfpulse::demod::FmMode::Narrowband : rfpulse::demod::FmMode::Wideband);
    newVfo->setSquelchThresholdDb(squelchThresholdDb_);
    newVfo->setDeemphasis(deemphasisFromIndex(deemphasisIndex_));
    newVfo->setVolume(volume_);
    newVfo->setMuted(muted_);

    {
        std::lock_guard<std::mutex> lock(vfoMutex_);
        activeVfo_ = std::move(newVfo);
        vfoActive_ = true;
    }

    logger_.info(
        "VFO sintonizado a " + std::to_string(frequencyHz / 1e6) + " MHz (offset " + std::to_string(offsetHz / 1e3) + " kHz)");
}

void Application::update(float deltaSeconds)
{
    (void)deltaSeconds;

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::Begin(
        "RfPulse", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse
            | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    drawTopToolbar();
    ImGui::Separator();

    constexpr float kSidePanelWidth = 300.0f;
    constexpr float kStatusBarHeight = 24.0f;
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const float centralHeight = avail.y - kStatusBarHeight - ImGui::GetStyle().ItemSpacing.y;

    ImGui::BeginChild("CentralArea", ImVec2(avail.x - kSidePanelWidth - spacing, centralHeight), false);
    drawSpectrumAndWaterfall(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y);
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("SidePanel", ImVec2(kSidePanelWidth, centralHeight), true);
    drawVfoPanel(kSidePanelWidth);
    ImGui::EndChild();

    drawStatusBar();

    ImGui::End();
}

void Application::drawTopToolbar()
{
    ImGui::TextColored(rfpulse::ui::TraceColors::kMaxHold, "RfPulse");
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();

    double centerMHz = centerFrequencyHz_.load(std::memory_order_relaxed) / 1e6;
    ImGui::SetNextItemWidth(140.0f);
    // InputDouble devuelve true en CADA pulsacion que cambia el valor
    // parseado (no solo al terminar de escribir) -- si se llamara a
    // applyCenterFrequency() ahi mismo, escribir "241.3" caracter a caracter
    // mandaria una rafaga de reintonizaciones al hardware (241 -> 241. ->
    // 241.3), cada una interrumpiendo el asentamiento de la anterior en el
    // PLL del R828D antes de que termine de fijar frecuencia, dejando la
    // ultima trama de IQ etiquetada con la frecuencia pedida pero capturada
    // con el LO todavia sin asentar en ella (bug real reportado: escribir
    // "241.3" a mano mostraba la señal desplazada ~4.8 MHz, mientras que un
    // solo salto atomico -- p.ej. un marcador de frecuencia favorita -- iba
    // bien). IsItemDeactivatedAfterEdit() solo dispara UNA vez, al terminar
    // de editar (Enter o perder el foco), asi que solo se manda un comando
    // de sintonia por edicion.
    ImGui::InputDouble("MHz", &centerMHz, 0.0, 0.0, "%.4f");
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        applyCenterFrequency(centerMHz * 1e6);
        // Si la nueva frecuencia cruza el umbral HF/VHF, el span actual
        // puede dejar de caber bajo el limite del modo nuevo (ver
        // kVhfMaxSpanHz/kHfMaxSpanHz) -- applySpanHz() reajusta solo si
        // hace falta (es un no-op si el span actual ya es valido).
        applySpanHz(spanHz_);
    }
    if (centerFrequencySpurAdjusted_) {
        ImGui::SameLine();
        ImGui::TextColored(rfpulse::ui::TraceColors::kMaxHold, "(ajustada, espurea tuner)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(
                "Desplazada automaticamente ~%.0f kHz: el tuner del RX888 MK2 puede\n"
                "mostrar una espurea falsa justo en multiplos de 16 MHz.",
                kSpurAvoidanceOffsetHz / 1e3);
        }
    }
    ImGui::SameLine();
    if (device_) {
        const auto rateSteps = device_->availableSampleRates();
        if (rateSteps.count > 0) {
            // El RX888 MK2 (y familia SDDC en general) solo puede capturar
            // en unas pocas tasas discretas fijas por su DDC en FPGA (ver
            // ISdrDevice::availableSampleRates) -- de ahi un combo con las
            // opciones reales, no un slider continuo que sugeriria que
            // cualquier valor intermedio es alcanzable. Ademas, solo se
            // listan las opciones que caben bajo el limite probado fiable
            // del modo actual (ver kVhfMaxSpanHz/kHfMaxSpanHz): no tiene
            // sentido dejar elegir un span que se sabe que da la señal
            // desplazada de su sitio para esta frecuencia central.
            const bool vhfMode = isVhfModeAt(centerFrequencyHz_.load(std::memory_order_relaxed));
            const double capHz = vhfMode ? kVhfMaxSpanHz : kHfMaxSpanHz;

            char labels[6][16];
            const char* labelPtrs[6];
            double labelValuesHz[6];
            int currentSpanIndex = 0;
            int count = 0;
            for (int i = 0; i < rateSteps.count && count < 6; ++i) {
                if (rateSteps.valuesHz[i] > capHz + 1.0) {
                    continue;
                }
                std::snprintf(labels[count], sizeof(labels[count]), "%.0f MHz", rateSteps.valuesHz[i] / 1e6);
                labelPtrs[count] = labels[count];
                labelValuesHz[count] = rateSteps.valuesHz[i];
                if (std::abs(rateSteps.valuesHz[i] - spanHz_) < 1.0) {
                    currentSpanIndex = count;
                }
                ++count;
            }
            if (count > 0) {
                ImGui::SetNextItemWidth(90.0f);
                if (ImGui::Combo("Span", &currentSpanIndex, labelPtrs, count)) {
                    applySpanHz(labelValuesHz[currentSpanIndex]);
                }
            }
        }
    }
    ImGui::SameLine();
    ImGui::TextDisabled("|");

    ImGui::SameLine();
    static const char* kFftSizeLabels[] = {"1024", "2048", "4096", "8192", "16384", "32768"};
    int fftSizeCurrentIndex = 4;
    for (int i = 0; i < 6; ++i) {
        if (rfpulse::fft::kSupportedFftSizes[static_cast<std::size_t>(i)] == fftSize_) {
            fftSizeCurrentIndex = i;
        }
    }
    ImGui::SetNextItemWidth(90.0f);
    if (ImGui::Combo("FFT", &fftSizeCurrentIndex, kFftSizeLabels, 6)) {
        applyFftSize(rfpulse::fft::kSupportedFftSizes[static_cast<std::size_t>(fftSizeCurrentIndex)]);
    }

    ImGui::SameLine();
    ImGui::SetNextItemWidth(110.0f);
    if (ImGui::SliderInt("Suavizado", &smoothingWidthBins_, 1, 21)) {
        spectrumProcessor_->setSmoothingWidthBins(smoothingWidthBins_);
    }

    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    ImGui::Checkbox("Prom.", &averagingEnabled_);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(90.0f);
    ImGui::SliderFloat("Alpha", &averagingAlpha_, 0.01f, 1.0f);
    spectrumProcessor_->setAveragingMode(
        averagingEnabled_ ? rfpulse::spectrum::AveragingMode::Exponential : rfpulse::spectrum::AveragingMode::None);
    spectrumProcessor_->setAveragingAlpha(averagingAlpha_);

    ImGui::SameLine();
    if (ImGui::Checkbox("Max Hold", &maxHoldEnabled_)) {
        spectrumProcessor_->setMaxHoldEnabled(maxHoldEnabled_);
    }
    ImGui::SameLine();
    if (ImGui::Checkbox("Min Hold", &minHoldEnabled_)) {
        spectrumProcessor_->setMinHoldEnabled(minHoldEnabled_);
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset")) {
        spectrumProcessor_->resetHolds();
    }
    ImGui::SameLine();
    if (ImGui::Checkbox("Pausa", &frozen_)) {
        spectrumProcessor_->setFrozen(frozen_);
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(110.0f);
    ImGui::SliderFloat("Umbral picos", &peakDisplayThresholdDb_, -100.0f, 0.0f);

    // Segunda fila: ganancias del hardware + paleta/rango del waterfall.
    if (device_) {
        const auto rfSteps = device_->rfAttenuationSteps();
        if (rfSteps.count > 0) {
            rfAttenuationIndex_ = std::clamp(rfAttenuationIndex_, 0, rfSteps.count - 1);
            ImGui::SetNextItemWidth(150.0f);
            if (ImGui::SliderInt("Atenuacion RF", &rfAttenuationIndex_, 0, rfSteps.count - 1)) {
                device_->setRfAttenuationIndex(rfAttenuationIndex_);
            }
            ImGui::SameLine();
            ImGui::TextDisabled("(%.1f dB)", rfSteps.values[rfAttenuationIndex_]);
            ImGui::SameLine();
        }

        const auto ifSteps = device_->ifGainSteps();
        if (ifSteps.count > 0) {
            ifGainIndex_ = std::clamp(ifGainIndex_, 0, ifSteps.count - 1);
            ImGui::SetNextItemWidth(150.0f);
            if (ImGui::SliderInt("Ganancia IF", &ifGainIndex_, 0, ifSteps.count - 1)) {
                device_->setIfGainIndex(ifGainIndex_);
            }
            ImGui::SameLine();
            ImGui::TextDisabled("(%.1f dB)", ifSteps.values[ifGainIndex_]);
            ImGui::SameLine();
        }
        ImGui::TextDisabled("|");
        ImGui::SameLine();
    }

    static const char* kPaletteLabels[] = {"Viridis", "Inferno", "Turbo", "Grayscale", "Thermal"};
    ImGui::SetNextItemWidth(110.0f);
    if (ImGui::Combo("Paleta", &paletteIndex_, kPaletteLabels, kPaletteCount)) {
        waterfallRenderer_->setPalette(kPaletteValues[paletteIndex_ % kPaletteCount]);
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(90.0f);
    bool dbRangeChanged = ImGui::SliderFloat("dB min", &waterfallMinDb_, -140.0f, 0.0f);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(90.0f);
    dbRangeChanged |= ImGui::SliderFloat("dB max", &waterfallMaxDb_, -140.0f, 0.0f);
    if (dbRangeChanged) {
        waterfallRenderer_->setDbRange(waterfallMinDb_, waterfallMaxDb_);
    }

    ImGui::SameLine();
    ImGui::SetNextItemWidth(110.0f);
    ImGui::SliderFloat("Alto waterfall", &waterfallHeightRatio_, 0.15f, 0.70f, "%.2f");
}

void Application::drawDeviceSelector(float widthPx)
{
    // Item 0 del combo es siempre "DEMO" (kSyntheticDeviceIndex); los
    // siguientes son los dispositivos SDDC detectados en la ULTIMA llamada a
    // refreshDeviceList() (arranque + boton "Refrescar"), no una enumeracion
    // en vivo por frame -- reabrir el USB en cada frame seria costoso e
    // innecesario para algo que solo cambia cuando el usuario conecta o
    // desconecta hardware.
    // -1 (ningun elemento de availableDevices_ coincide) puede pasar de forma
    // legitima justo tras un Refrescar que ya no incluye el dispositivo
    // actualmente abierto (se desconecto, o el USB lo reenumero con otro
    // indice); en ese caso no asumimos DEMO, mostramos el nombre real del
    // dispositivo activo como preview.
    int matchedItem = -1;
    for (std::size_t i = 0; i < availableDevices_.size(); ++i) {
        if (availableDevices_[i].index == selectedDeviceIndex_) {
            matchedItem = static_cast<int>(i);
            break;
        }
    }

    std::string preview;
    if (matchedItem >= 0) {
        preview = availableDevices_[static_cast<std::size_t>(matchedItem)].label;
    } else if (usingSyntheticDevice_) {
        preview = "Generador sintetico (DEMO)";
    } else {
        preview = device_ ? device_->name() : "(ninguno)";
    }

    (void)widthPx;
    // Se calcula el ancho del combo a partir del espacio realmente
    // disponible dentro del panel (no del ancho exterior del BeginChild,
    // que no descuenta el borde/padding) para que el boton "Refrescar" que
    // va justo despues en la misma linea no quede cortado en el borde.
    const ImGuiStyle& style = ImGui::GetStyle();
    const float buttonWidth = ImGui::CalcTextSize("Refrescar").x + style.FramePadding.x * 2.0f;
    const float comboWidth = std::max(80.0f, ImGui::GetContentRegionAvail().x - buttonWidth - style.ItemSpacing.x);

    ImGui::SetNextItemWidth(comboWidth);
    if (ImGui::BeginCombo("##DeviceSelector", preview.c_str())) {
        if (ImGui::Selectable("Generador sintetico (DEMO)", usingSyntheticDevice_)) {
            switchToDevice(kSyntheticDeviceIndex);
        }
        for (std::size_t i = 0; i < availableDevices_.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            const bool isSelected = (matchedItem == static_cast<int>(i));
            if (ImGui::Selectable(availableDevices_[i].label.c_str(), isSelected)) {
                switchToDevice(availableDevices_[i].index);
            }
            ImGui::PopID();
        }
        if (availableDevices_.empty()) {
            ImGui::TextDisabled("(no se detecto ningun RX888)");
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (ImGui::Button("Refrescar")) {
        refreshDeviceList();
    }

    if (usingSyntheticDevice_) {
        ImGui::TextColored(rfpulse::ui::TraceColors::kMaxHold, "Modo demostracion (sin hardware RX888)");
    } else {
        ImGui::TextWrapped("Activo: %s", device_ ? device_->name() : "(ninguno)");
    }
}

void Application::addFrequencyPreset(const std::string& label, double frequencyHz)
{
    std::string cleanLabel = label;
    // ';' y '|' son los separadores del formato de serializacion (ver
    // serializeFrequencyPresets): se descartan aqui para que una etiqueta
    // nunca pueda corromper el resto de la lista guardada.
    cleanLabel.erase(std::remove(cleanLabel.begin(), cleanLabel.end(), ';'), cleanLabel.end());
    cleanLabel.erase(std::remove(cleanLabel.begin(), cleanLabel.end(), '|'), cleanLabel.end());
    if (cleanLabel.empty()) {
        cleanLabel = std::to_string(frequencyHz / 1e6) + " MHz";
    }
    frequencyPresets_.push_back(FrequencyPreset{std::move(cleanLabel), frequencyHz});
}

void Application::drawFrequencyPresets(float widthPx)
{
    (void)widthPx;
    ImGui::TextWrapped("Frecuencias favoritas");
    ImGui::TextDisabled("(clic para ir directamente y sintonizar)");

    int indexToRemove = -1;
    for (std::size_t i = 0; i < frequencyPresets_.size(); ++i) {
        const auto& preset = frequencyPresets_[i];
        ImGui::PushID(static_cast<int>(i));

        const float removeButtonWidth = ImGui::CalcTextSize("x").x + ImGui::GetStyle().FramePadding.x * 2.0f;
        const float selectableWidth =
            std::max(40.0f, ImGui::GetContentRegionAvail().x - removeButtonWidth - ImGui::GetStyle().ItemSpacing.x);

        char rowLabel[96];
        std::snprintf(rowLabel, sizeof(rowLabel), "%s (%.4f MHz)", preset.label.c_str(), preset.frequencyHz / 1e6);
        if (ImGui::Selectable(rowLabel, false, 0, ImVec2(selectableWidth, 0.0f))) {
            applyCenterFrequency(preset.frequencyHz);
            // Ver el comentario junto al campo de MHz: un preset puede
            // cruzar el umbral HF/VHF igual que escribir la frecuencia a
            // mano.
            applySpanHz(spanHz_);
            tuneVfoAt(preset.frequencyHz);
        }
        ImGui::SameLine();
        if (ImGui::Button("x")) {
            indexToRemove = static_cast<int>(i);
        }
        ImGui::PopID();
    }
    if (indexToRemove >= 0) {
        frequencyPresets_.erase(frequencyPresets_.begin() + indexToRemove);
    }

    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    ImGui::InputTextWithHint("##PresetLabel", "Etiqueta (opcional)", presetLabelInput_, sizeof(presetLabelInput_));

    const float addButtonWidth = ImGui::CalcTextSize("Anadir").x + ImGui::GetStyle().FramePadding.x * 2.0f;
    ImGui::SetNextItemWidth(std::max(60.0f, ImGui::GetContentRegionAvail().x - addButtonWidth - ImGui::GetStyle().ItemSpacing.x));
    ImGui::InputDouble("##PresetFreqMHz", &presetFreqInputMHz_, 0.0, 0.0, "%.4f MHz");
    ImGui::SameLine();
    if (ImGui::Button("Anadir")) {
        // La frecuencia del marcador es SIEMPRE la que escribio el usuario
        // en este campo, no la frecuencia central actual: se prerrellena
        // con esta ultima (ver el constructor) solo como punto de partida
        // comodo, pero el usuario puede escribir cualquier otra antes de
        // anadir sin que "Anadir" la sobrescriba.
        addFrequencyPreset(presetLabelInput_, presetFreqInputMHz_ * 1e6);
        presetLabelInput_[0] = '\0';
    }
}

std::string Application::serializeFrequencyPresets() const
{
    std::string result;
    for (const auto& preset : frequencyPresets_) {
        if (!result.empty()) {
            result += ';';
        }
        result += std::to_string(preset.frequencyHz);
        result += '|';
        result += preset.label;
    }
    return result;
}

void Application::loadFrequencyPresets(const std::string& serialized)
{
    frequencyPresets_.clear();
    std::size_t recordStart = 0;
    while (recordStart <= serialized.size()) {
        const std::size_t recordEnd = serialized.find(';', recordStart);
        const std::string record =
            serialized.substr(recordStart, recordEnd == std::string::npos ? std::string::npos : recordEnd - recordStart);

        const std::size_t sep = record.find('|');
        if (sep != std::string::npos) {
            try {
                const double frequencyHz = std::stod(record.substr(0, sep));
                frequencyPresets_.push_back(FrequencyPreset{record.substr(sep + 1), frequencyHz});
            } catch (...) {
                // Registro corrupto (edicion manual del archivo, etc.): se
                // ignora en vez de romper la carga del resto de la lista.
            }
        }

        if (recordEnd == std::string::npos) {
            break;
        }
        recordStart = recordEnd + 1;
    }
}

void Application::drawVfoPanel(float widthPx)
{
    drawDeviceSelector(widthPx);
    ImGui::Separator();

    ImGui::TextWrapped("Receptor virtual");
    ImGui::TextDisabled("(clic en el espectro centra la vista; la escucha");
    ImGui::TextDisabled("solo se activa desde Frecuencias favoritas)");
    {
        std::lock_guard<std::mutex> lock(vfoMutex_);
        if (activeVfo_) {
            ImGui::Text("Offset: %.1f kHz", activeVfo_->offsetHz() / 1e3);
            if (activeVfo_->squelchOpen()) {
                ImGui::TextColored(rfpulse::ui::TraceColors::kCurrent, "Squelch: ABIERTO");
            } else {
                ImGui::TextDisabled("Squelch: cerrado");
            }
        } else {
            ImGui::TextDisabled("(ninguno)");
        }
    }
    ImGui::Separator();

    drawFrequencyPresets(widthPx);
    ImGui::Separator();

    static const char* kFmModeLabels[] = {"NFM (PMSE analogico)", "WFM (radiodifusion)"};
    if (ImGui::Combo("Modo", &fmModeIndex_, kFmModeLabels, 2)) {
        std::lock_guard<std::mutex> lock(vfoMutex_);
        if (activeVfo_) {
            activeVfo_->setMode(fmModeIndex_ == 0 ? rfpulse::demod::FmMode::Narrowband : rfpulse::demod::FmMode::Wideband);
        }
    }

    if (ImGui::SliderFloat(
            "Ancho canal (kHz)", &channelBandwidthKHz_, kMinChannelBandwidthKHz, kMaxChannelBandwidthKHz, "%.1f")) {
        // A diferencia de Modo/De-enfasis/Squelch/Volumen, el filtro de
        // canal del Vfo se fija en su construccion (no hay un
        // "setChannelBandwidth" en caliente, ver Vfo::channelFilter_): si
        // hay un VFO activo, se reconstruye a la misma frecuencia absoluta
        // que ya estaba sintonizada -- leer el offset y soltar el lock
        // ANTES de llamar a tuneVfoAt() (que toma el mismo mutex por su
        // cuenta) para no bloquear dos veces sobre un mutex no reentrante.
        double retuneAbsoluteHz = 0.0;
        bool shouldRetune = false;
        {
            std::lock_guard<std::mutex> lock(vfoMutex_);
            if (activeVfo_) {
                retuneAbsoluteHz = centerFrequencyHz_.load(std::memory_order_relaxed) + activeVfo_->offsetHz();
                shouldRetune = true;
            }
        }
        if (shouldRetune) {
            tuneVfoAt(retuneAbsoluteHz);
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(
            "Ajusta al ancho de canal real de lo que estes escuchando:\n"
            "PMSE analogico (Shure/Sennheiser/Wisycom/Lectrosonics, etc.)\n"
            "o radio bidireccional LMR (12.5/25 kHz tipico). No llega para\n"
            "FM de radiodifusion (~200 kHz) -- fuera del alcance de RfPulse.");
    }

    static const char* kDeemphasisLabels[] = {"Ninguno", "50 us", "75 us"};
    if (ImGui::Combo("De-enfasis", &deemphasisIndex_, kDeemphasisLabels, 3)) {
        std::lock_guard<std::mutex> lock(vfoMutex_);
        if (activeVfo_) {
            activeVfo_->setDeemphasis(deemphasisFromIndex(deemphasisIndex_));
        }
    }

    if (ImGui::SliderFloat("Squelch (dBFS)", &squelchThresholdDb_, -100.0f, 0.0f)) {
        std::lock_guard<std::mutex> lock(vfoMutex_);
        if (activeVfo_) {
            activeVfo_->setSquelchThresholdDb(squelchThresholdDb_);
        }
    }
    if (ImGui::SliderFloat("Volumen", &volume_, 0.0f, 1.0f)) {
        std::lock_guard<std::mutex> lock(vfoMutex_);
        if (activeVfo_) {
            activeVfo_->setVolume(volume_);
        }
    }
    if (ImGui::Checkbox("Mute", &muted_)) {
        std::lock_guard<std::mutex> lock(vfoMutex_);
        if (activeVfo_) {
            activeVfo_->setMuted(muted_);
        }
    }
}

void Application::drawStatusBar()
{
    ImGui::Separator();
    ImGui::Text("FPS %.1f", static_cast<double>(ImGui::GetIO().Framerate));
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    ImGui::TextColored(usingSyntheticDevice_ ? rfpulse::ui::TraceColors::kMaxHold : rfpulse::ui::TraceColors::kCurrent,
        usingSyntheticDevice_ ? "DEMO" : "HW");
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    ImGui::TextDisabled("Bloques descartados: %llu", static_cast<unsigned long long>(acquisition_.droppedBlocksTotal()));
}

void Application::drawSpectrumAndWaterfall(float widthPx, float heightPx)
{
    // El espectro y el waterfall se reparten la altura disponible segun
    // waterfallHeightRatio_ (ajustable desde la UI, ver drawTopToolbar),
    // no a partes iguales: heightPx es el total, no el de cada uno.
    const float waterfallHeightPx = heightPx * waterfallHeightRatio_;
    const float spectrumHeightPx = heightPx - waterfallHeightPx;

    const auto& frame = spectrumProcessor_->latestFrame();
    const auto interaction = spectrumRenderer_.draw(frame, widthPx, spectrumHeightPx, averagingEnabled_,
        maxHoldEnabled_, minHoldEnabled_, peakDisplayThresholdDb_, kUsableSpanFraction);
    if (interaction.clickedFrequencyHz.has_value()) {
        // El clic en el espectro/waterfall solo navega (recentra la vista,
        // con iman al pico mas cercano si se hace clic cerca de un
        // marcador, ver SpectrumRenderer::draw) -- YA NO arranca el VFO ni
        // el audio. El usuario pidio escuchar solo frecuencias favoritas
        // (ver drawFrequencyPresets/tuneVfoAt), para no enganchar audio por
        // error explorando el espectro.
        applyCenterFrequency(*interaction.clickedFrequencyHz);
        applySpanHz(spanHz_);
    }

    waterfallRenderer_->updateRow(*waterfallEngine_);
    // plotAreaScreenX/plotAreaWidthPx (no widthPx) alinean el waterfall con
    // el area de datos real del espectro, que queda mas estrecha que
    // widthPx por el hueco que ImPlot reserva para las etiquetas del eje Y
    // (ver el comentario de SpectrumInteraction). kUsableSpanFraction debe
    // ser el mismo que se le paso a spectrumRenderer_.draw arriba para que
    // ambos recorten exactamente el mismo rango de frecuencias.
    waterfallRenderer_->draw(*waterfallEngine_, interaction.plotAreaScreenX, interaction.plotAreaWidthPx,
        waterfallHeightPx, kUsableSpanFraction);
}

void Application::loadSettings()
{
    settings_.load();

    centerFrequencyHz_.store(
        settings_.getDouble("center_frequency_hz", centerFrequencyHz_.load(std::memory_order_relaxed)),
        std::memory_order_relaxed);
    // Se ajusta a una opcion real del dispositivo mas tarde en el
    // constructor (snapToNearestAvailableSpan), una vez abierto -- aqui
    // solo se recupera el valor guardado tal cual.
    spanHz_ = settings_.getDouble("span_hz", spanHz_);

    const std::size_t loadedFftSize = static_cast<std::size_t>(settings_.getInt("fft_size", static_cast<int>(fftSize_)));
    fftSize_ = isSupportedFftSize(loadedFftSize) ? loadedFftSize : fftSize_;

    averagingEnabled_ = settings_.getBool("averaging_enabled", averagingEnabled_);
    averagingAlpha_ = static_cast<float>(settings_.getDouble("averaging_alpha", averagingAlpha_));
    smoothingWidthBins_ = settings_.getInt("smoothing_width_bins", smoothingWidthBins_);
    maxHoldEnabled_ = settings_.getBool("max_hold_enabled", maxHoldEnabled_);
    minHoldEnabled_ = settings_.getBool("min_hold_enabled", minHoldEnabled_);
    paletteIndex_ = settings_.getInt("palette_index", paletteIndex_);
    waterfallMinDb_ = static_cast<float>(settings_.getDouble("waterfall_min_db", waterfallMinDb_));
    waterfallMaxDb_ = static_cast<float>(settings_.getDouble("waterfall_max_db", waterfallMaxDb_));
    waterfallHeightRatio_ = static_cast<float>(settings_.getDouble("waterfall_height_ratio", waterfallHeightRatio_));
    squelchThresholdDb_ = static_cast<float>(settings_.getDouble("squelch_threshold_db", squelchThresholdDb_));
    volume_ = static_cast<float>(settings_.getDouble("volume", volume_));
    muted_ = settings_.getBool("muted", muted_);
    fmModeIndex_ = settings_.getInt("fm_mode_index", fmModeIndex_);
    deemphasisIndex_ = settings_.getInt("deemphasis_index", deemphasisIndex_);
    channelBandwidthKHz_ = static_cast<float>(settings_.getDouble("channel_bandwidth_khz", channelBandwidthKHz_));
    channelBandwidthKHz_ = std::clamp(channelBandwidthKHz_, kMinChannelBandwidthKHz, kMaxChannelBandwidthKHz);
    rfAttenuationIndex_ = settings_.getInt("rf_attenuation_index", rfAttenuationIndex_);
    ifGainIndex_ = settings_.getInt("if_gain_index", ifGainIndex_);
    peakDisplayThresholdDb_ = static_cast<float>(settings_.getDouble("peak_threshold_db", peakDisplayThresholdDb_));
    loadFrequencyPresets(settings_.getString("frequency_presets", ""));
}

void Application::saveSettings()
{
    settings_.setDouble("center_frequency_hz", centerFrequencyHz_.load(std::memory_order_relaxed));
    settings_.setDouble("span_hz", spanHz_);
    settings_.setInt("fft_size", static_cast<int>(fftSize_));
    settings_.setBool("averaging_enabled", averagingEnabled_);
    settings_.setDouble("averaging_alpha", averagingAlpha_);
    settings_.setInt("smoothing_width_bins", smoothingWidthBins_);
    settings_.setBool("max_hold_enabled", maxHoldEnabled_);
    settings_.setBool("min_hold_enabled", minHoldEnabled_);
    settings_.setInt("palette_index", paletteIndex_);
    settings_.setDouble("waterfall_min_db", waterfallMinDb_);
    settings_.setDouble("waterfall_max_db", waterfallMaxDb_);
    settings_.setDouble("waterfall_height_ratio", waterfallHeightRatio_);
    settings_.setDouble("squelch_threshold_db", squelchThresholdDb_);
    settings_.setDouble("volume", volume_);
    settings_.setBool("muted", muted_);
    settings_.setInt("fm_mode_index", fmModeIndex_);
    settings_.setInt("deemphasis_index", deemphasisIndex_);
    settings_.setDouble("channel_bandwidth_khz", channelBandwidthKHz_);
    settings_.setInt("rf_attenuation_index", rfAttenuationIndex_);
    settings_.setInt("if_gain_index", ifGainIndex_);
    settings_.setDouble("peak_threshold_db", peakDisplayThresholdDb_);
    settings_.setString("frequency_presets", serializeFrequencyPresets());
    settings_.save();
}

} // namespace rfpulse::app
