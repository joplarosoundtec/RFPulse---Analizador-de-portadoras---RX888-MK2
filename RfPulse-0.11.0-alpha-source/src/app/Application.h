#pragma once

#include "acquisition/IqAcquisition.h"
#include "audio/AudioOutput.h"
#include "core/Logger.h"
#include "core/RingBuffer.h"
#include "core/Settings.h"
#include "demod/Deemphasis.h"
#include "demod/FmDemodulator.h"
#include "fft/FftwEngine.h"
#include "render/GraphicsDevice.h"
#include "render/SpectrumRenderer.h"
#include "render/WaterfallRenderer.h"
#include "sdr/ISdrDevice.h"
#include "spectrum/SpectrumProcessor.h"
#include "vfo/Vfo.h"
#include "waterfall/Palette.h"
#include "waterfall/WaterfallEngine.h"

#include <atomic>
#include <complex>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace rfpulse::app {

// Junta todos los modulos del proyecto en una unica aplicacion: abre el
// dispositivo SDR (con fallback automatico a un generador sintetico si no
// hay hardware conectado), arranca los hilos de espectro y de VFO/audio,
// dibuja los paneles de control y el espectro/waterfall, y persiste las
// preferencias en Settings. Se llama una vez por frame desde
// ui::MainWindow::run() (ver update()).
//
// Modelo de hilos:
//  - captura: propiedad de Core (RadioHandlerClass::submit_thread) o del
//    hilo interno de SyntheticSdrDevice.
//  - espectro (spectrumThread_): acumula bloques de IqAcquisition hasta
//    tener un tamaño de FFT completo, ejecuta FftwEngine + SpectrumProcessor
//    y actualiza WaterfallEngine (solo CPU, sin tocar DirectX).
//  - VFO/audio (vfoThread_): drena el ring buffer de VFO y, si hay un VFO
//    activo, ejecuta Vfo::process + AudioOutput::write.
//  - render/UI (el hilo que llama a update(), ver MainWindow::run): es el
//    UNICO hilo que toca el ID3D11DeviceContext -- incluida la subida de
//    la fila de waterfall a la GPU (WaterfallRenderer::updateRow), que se
//    hace aqui y no en spectrumThread_, precisamente para no compartir el
//    contexto inmediato de D3D11 entre hilos (no es thread-safe).
class Application {
public:
    explicit Application(rfpulse::render::GraphicsDevice& graphics);
    ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    void update(float deltaSeconds);

private:
    void openDevice();
    void refreshDeviceList();
    void switchToDevice(int deviceIndex);
    void drawDeviceSelector(float widthPx);
    void applyCenterFrequency(double frequencyHz);
    void applyFftSize(std::size_t fftSize);
    void applySpanHz(double newSpanHz);
    double snapToNearestAvailableSpan(double desiredHz) const;
    void recreateSpectrumProcessor(std::size_t fftSize);

    // true si frequencyHz cae en el rango que hace al RX888 MK2 usar su
    // tuner R828D (VHF/UHF) en vez de muestreo directo (ver
    // Application::applyCenterFrequency y kSpurRefClockHz).
    bool isVhfModeAt(double frequencyHz) const;

    void startSpectrumThread();
    void stopSpectrumThread();
    void spectrumThreadMain();

    void startVfoThread();
    void stopVfoThread();
    void vfoThreadMain();
    void tuneVfoAt(double frequencyHz);

    void drawTopToolbar();
    void drawVfoPanel(float widthPx);
    void drawFrequencyPresets(float widthPx);
    void addFrequencyPreset(const std::string& label, double frequencyHz);
    void drawStatusBar();
    void drawSpectrumAndWaterfall(float widthPx, float heightPx);

    // Serializacion de frequencyPresets_ para Settings (que solo guarda
    // pares clave/valor sin anidamiento, ver core/Settings.h): un registro
    // por preset separado por ';', "MHz_en_Hz|etiqueta" dentro de cada uno.
    // Los caracteres ';' y '|' se descartan de la etiqueta al anadirla (ver
    // addFrequencyPreset) para que nunca aparezcan dentro de un campo.
    std::string serializeFrequencyPresets() const;
    void loadFrequencyPresets(const std::string& serialized);

    void loadSettings();
    void saveSettings();

    // Span (= tasa de muestreo IQ de banda ancha) por defecto al primer
    // arranque, antes de conocer las opciones reales del dispositivo. Se
    // ajusta a la opcion disponible mas cercana nada mas abrir cada
    // dispositivo (ver snapToNearestAvailableSpan) -- 16 MHz es una de las
    // 5 opciones reales de un RX888 MK2 (ver ISdrDevice::availableSampleRates),
    // asi que en el caso tipico coincide exacto.
    static constexpr double kDefaultSpanHz = 16'000'000.0;

    // El tuner VHF/UHF del RX888 MK2 (Rafael Micro R828D) deriva su PLL de
    // una referencia de EXACTAMENTE 16 MHz (ver R828D_FREQ en
    // third_party/sddc_core/Core/radio/RX888R2Radio.cpp). Es un
    // comportamiento bien conocido de la familia de tuners R820T/R820T2/
    // R828D que esa propia referencia se filtre al camino de RF, generando
    // una "espurea" (birdie) falsa exactamente en cada multiplo de 16 MHz
    // -- no una señal real (reportado por el usuario en 240 MHz = 16x15;
    // no se reproduce en las demas frecuencias probadas). La correccion es
    // "offset tuning", la misma tecnica que usan SDR#/GQRX para este mismo
    // problema con tuners R820T: se desplaza el LO real un poco para que
    // dejar de sintonizar justo un multiplo de 16 MHz, alejando la espurea
    // de la frecuencia de interes.
    static constexpr double kSpurRefClockHz = 16'000'000.0;
    static constexpr double kSpurAvoidanceThresholdHz = 250'000.0;
    static constexpr double kSpurAvoidanceOffsetHz = 300'000.0;

    // El filtro FIR de decimacion del DDC (third_party/sddc_core/Core/
    // fft_mt_r2iq.cpp, diseño Kaiser con relPass=0.85/relStop=1.1) solo
    // GARANTIZA banda de paso plana en el 85% central de Nyquist -- pero
    // entre ese 85% y el borde real del span (100%) la respuesta no cae en
    // seco, solo empieza a rodar gradualmente (el corte duro de verdad no
    // llega hasta el 110%, fuera ya de lo que se captura). Recortar el
    // 15% completo (kUsableSpanFraction=0.85 en la 0.4.0-alpha) ocultaba de
    // mas: el "Span" que elige el usuario dejaba de coincidir con la
    // ventana de frecuencias realmente visible en pantalla (32 MHz
    // seleccionados mostraban solo 27.2 MHz), una discrepancia confusa sin
    // ningun indicio en la UI (reportado por el usuario). Se recorta solo
    // el 5% exterior (2.5% por borde) -- suficiente para ocultar el tramo
    // mas empinado de la rampa cerca del borde, sin que el Span elegido y
    // lo que se ve en pantalla se separen tanto. SpectrumRenderer y
    // WaterfallRenderer recortan la franja exterior de la vista (traza,
    // deteccion de picos y waterfall) usando esta fraccion, para no marcar
    // como pico nada dentro de esa zona de transicion.
    static constexpr float kUsableSpanFraction = 0.95f;

    // Limite de span probado fiable con hardware real, por camino de
    // entrada del RX888 MK2: en modo VHF/UHF (tuner R828D, ver
    // isVhfModeAt) el usuario reporto que solo el span de 8 MHz mantiene la
    // señal generada localizada correctamente al cambiar de frecuencia --
    // con 16/32 MHz la fundamental dejaba de encontrarse donde deberia
    // tras retonizar. En modo HF (muestreo directo, sin el tuner) se
    // permite hasta 16 MHz. Deliberadamente conservador y especifico de
    // este proyecto (RfPulse no es una herramienta SDR generica): mejor
    // limitar las opciones del combo de Span a lo que se ha probado bien
    // que dejar elegir un span que se sabe que da lecturas incorrectas.
    static constexpr double kVhfMaxSpanHz = 8'000'000.0;
    static constexpr double kHfMaxSpanHz = 16'000'000.0;

    static constexpr std::size_t kWaterfallRows = 512;
    static constexpr double kAudioSampleRateHz = 48000.0;

    // Rango del control "Ancho canal (kHz)" (ver channelBandwidthKHz_):
    // el Vfo siempre decima directo a ~kAudioSampleRateHz (48 kHz, ver
    // Vfo::Vfo), sin una etapa intermedia separada para el filtro de canal
    // -- asi que el filtro de canal tiene que caber bajo el Nyquist de esa
    // tasa decimada (~24 kHz) con margen para la banda de transicion, o el
    // propio filtro introduce aliasing en vez de limpiarlo. 40 kHz dejan
    // margen de sobra para PMSE analogico (companders tipicos ~10-20 kHz de
    // desviacion) y radio bidireccional LMR de banda ancha (25 kHz de canal)
    // -- no alcanza para FM de radiodifusion real (~200 kHz de canal), que
    // necesitaria una arquitectura de doble decimacion no implementada aqui
    // (fuera del alcance de esta app, pensada para PMSE/LMR, no FM de
    // radiodifusion).
    static constexpr float kMinChannelBandwidthKHz = 5.0f;
    static constexpr float kMaxChannelBandwidthKHz = 40.0f;

    // Indice reservado (fuera del rango 0..MAXNDEV-1 que usa el SDK SDDC)
    // para representar "generador sintetico" en selectedDeviceIndex_ y en el
    // combo de la UI, sin necesitar un enum/variant aparte.
    static constexpr int kSyntheticDeviceIndex = -1;

    rfpulse::render::GraphicsDevice& graphics_;
    rfpulse::core::Logger logger_;
    rfpulse::core::Settings settings_;

    std::unique_ptr<rfpulse::sdr::ISdrDevice> device_;
    bool usingSyntheticDevice_ = false;
    std::vector<rfpulse::sdr::SdrDeviceInfo> availableDevices_;
    int selectedDeviceIndex_ = kSyntheticDeviceIndex;
    rfpulse::acquisition::IqAcquisition acquisition_;

    rfpulse::core::RingBuffer<std::complex<float>> spectrumRing_;
    rfpulse::core::RingBuffer<std::complex<float>> vfoRing_;

    std::unique_ptr<rfpulse::fft::FftwEngine> fftEngine_;
    std::unique_ptr<rfpulse::spectrum::SpectrumProcessor> spectrumProcessor_;
    std::unique_ptr<rfpulse::waterfall::WaterfallEngine> waterfallEngine_;
    rfpulse::render::SpectrumRenderer spectrumRenderer_;
    std::unique_ptr<rfpulse::render::WaterfallRenderer> waterfallRenderer_;

    std::thread spectrumThread_;
    std::atomic<bool> spectrumThreadRunning_{false};
    std::size_t fftSize_ = 16384;

    std::mutex vfoMutex_;
    std::unique_ptr<rfpulse::vfo::Vfo> activeVfo_;
    bool vfoActive_ = false;
    rfpulse::audio::AudioOutput audioOutput_;
    std::thread vfoThread_;
    std::atomic<bool> vfoThreadRunning_{false};

    // Parametros configurables desde la UI (fuente de verdad para Settings).
    // centerFrequencyHz_ es atomic: se escribe desde el hilo de UI
    // (applyCenterFrequency) y se lee desde spectrumThread_ (submit()) y
    // vfoThread_ (tuneVfoAt calcula el offset respecto a este valor).
    std::atomic<double> centerFrequencyHz_{560'000'000.0};
    // true si applyCenterFrequency() desplazo la ultima frecuencia pedida
    // por estar cerca de una espurea del tuner (ver kSpurRefClockHz);
    // centerFrequencyHz_ YA refleja el valor ajustado en ese caso -- este
    // flag es solo para mostrar un aviso en la UI, no cambia ningun calculo.
    bool centerFrequencySpurAdjusted_ = false;
    // Span REAL de captura (= tasa de muestreo IQ de banda ancha que se le
    // pide al dispositivo, ver applySpanHz/snapToNearestAvailableSpan):
    // unico valor tanto para la peticion de streaming/Vfo como para la
    // etiqueta del eje de frecuencias del espectro. Antes de esta tarea
    // eran dos numeros independientes (un span de exhibicion fijo en 10 MHz
    // y una tasa de captura fija en 16 MHz en el codigo), lo que descuadraba
    // el eje de frecuencias del espectro salvo justo en el centro -- ver
    // ISdrDevice::availableSampleRates para el porque del RX888 MK2 no
    // puede en realidad capturar un span arbitrario como esos 10 MHz.
    double spanHz_ = kDefaultSpanHz;
    int rfAttenuationIndex_ = 0;
    int ifGainIndex_ = 0;
    bool averagingEnabled_ = true;
    float averagingAlpha_ = 0.2f;
    bool maxHoldEnabled_ = false;
    bool minHoldEnabled_ = false;
    bool frozen_ = false;
    int smoothingWidthBins_ = 3; // suavizado ligero por defecto ("mas smooth" sin ocultar detalle real)
    int paletteIndex_ = 4;       // Viridis/Inferno/Turbo/Grayscale/Thermal -- Thermal por defecto
    float waterfallMinDb_ = -100.0f;
    float waterfallMaxDb_ = 0.0f;
    // Fraccion (0..1) del alto disponible que ocupa el waterfall; el resto
    // es para el espectro (ver drawSpectrumAndWaterfall). 0.35 por defecto:
    // el reparto a partes iguales original dejaba el waterfall "demasiado
    // grande" a costa del espectro (reportado por el usuario).
    float waterfallHeightRatio_ = 0.35f;

    float squelchThresholdDb_ = -50.0f;
    float volume_ = 0.5f;
    bool muted_ = false;
    int fmModeIndex_ = 0;       // 0 = NFM, 1 = WFM
    int deemphasisIndex_ = 1;   // 0 = None, 1 = 50us, 2 = 75us

    // Ancho del filtro de canal del Vfo, en kHz (ver Vfo::Vfo,
    // channelBandwidthHz). Editable por el usuario en vez de un valor fijo
    // (antes 12.5 kHz siempre) para poder ajustarlo a la desviacion FM real
    // de cada marca/modelo de microfonia PMSE o de radio bidireccional que
    // se este escuchando -- el usuario conoce mejor que nadie el ancho de
    // banda de su propio equipo. 12.5 kHz de fabrica coincide con el canal
    // LMR estrecho tipico, un punto de partida razonable. No hay
    // "setChannelBandwidth" en caliente sobre un Vfo ya activo (el filtro
    // de canal se fija en su construccion): cambiar este valor con un VFO
    // activo lo reconstruye a la misma frecuencia (ver drawVfoPanel).
    float channelBandwidthKHz_ = 12.5f;

    // Umbral (dBFS) por encima del cual una señal se marca y anota como
    // pico secundario en el espectro (ver SpectrumRenderer::draw); el pico
    // global siempre se marca sin depender de este umbral.
    float peakDisplayThresholdDb_ = -60.0f;

    // Lista de frecuencias favoritas del usuario (ver drawFrequencyPresets):
    // un clic en una de ellas mueve la frecuencia central Y sintoniza el VFO
    // ahi mismo, para pasar de canal en canal sin tener que buscarlo en el
    // espectro cada vez. Se persiste en Settings (ver serializeFrequencyPresets).
    struct FrequencyPreset {
        std::string label;
        double frequencyHz = 0.0;
    };
    std::vector<FrequencyPreset> frequencyPresets_;
    char presetLabelInput_[64] = {};
    // MHz que "Anadir" usara para el proximo marcador -- SIEMPRE lo que haya
    // en este campo, nunca la frecuencia central actual (el usuario puede
    // escribir cualquier frecuencia sin sintonizar antes hacia ella). Se
    // prerrellena con la frecuencia central al construir la Application
    // (ver Application::Application) solo como punto de partida comodo.
    double presetFreqInputMHz_ = 0.0;
};

} // namespace rfpulse::app
