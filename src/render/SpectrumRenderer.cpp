#include "SpectrumRenderer.h"

#include "spectrum/PeakDetector.h"
#include "ui/Theme.h"

#include <imgui.h>
#include <implot.h>

#include <algorithm>
#include <cmath>

namespace rfpulse::render {

namespace {
// Separacion minima entre dos señales para contarlas como picos distintos
// (ver PeakDetector::detectPeaks). Se expresa en Hz, no en bins, porque el
// numero de bins por Hz cambia con el tamaño de FFT elegido: 150 kHz es un
// espaciado razonable para no fundir dos transmisores PMSE vecinos en uno
// solo, sin partir el lobulo ancho de un unico transmisor en varios picos.
constexpr double kMinPeakSeparationHz = 150'000.0;

// Radio de "iman" para hacer clic sobre un marcador de pico, en columnas de
// pixel: un pico puede ocupar un unico bin de ancho, mas estrecho que lo que
// un dedo/raton puede apuntar con precision, asi que un clic a pocos
// pixeles del marcador (no solo exactamente encima) se ajusta a su
// frecuencia exacta en vez de a la posicion cruda del raton.
constexpr double kPeakSnapToleranceCol = 10.0;

// Rango del eje Y (dBFS) con el que arranca el espectro, en vez de dejar
// que ImPlot lo autoajuste al rango de datos de cada trama. Sin esto, si el
// span visible no tiene ninguna señal por encima del ruido de fondo, el
// autoajuste encoge el eje al rango exacto (a veces minusculo y desplazado,
// p.ej. -190 a -120 dBFS) de ese ruido, dejando la parte util del espectro
// -- donde aparecera cualquier señal real -- fuera de la vista o aplastada
// en una franja diminuta (reportado por el usuario: "la grafica aparece muy
// abajo" al arrancar). -110/-40 dBFS es un encuadre razonable de fabrica
// para RF de PMSE/LMR con ganancia por defecto. Solo se aplica UNA vez
// (ImGuiCond_Once, no en cada trama) para no pelearse con un zoom/pan
// manual posterior del usuario sobre ese mismo eje.
constexpr double kDefaultYAxisMinDb = -110.0;
constexpr double kDefaultYAxisMaxDb = -40.0;

// Suavizado del CARTEL del pico principal (no del marcador ni de la traza,
// ver el comentario junto a su uso mas abajo). kLabelSmoothingAlpha=0.08 a
// ~60 fps (la app va vsincronizada, ver GraphicsDevice::present) da una
// constante de tiempo de ~0.2s -- nota pero no lenta. Por debajo de los
// umbrales de "salto grande" se desliza hacia el valor real en vez de
// saltar de golpe; por encima (un retonizado real, o una señal
// completamente distinta pasa a ser la mas fuerte) se salta directo, para
// no perseguir el valor nuevo a camara lenta durante casi un segundo.
constexpr double kLabelSmoothingAlpha = 0.08;
constexpr double kLabelSnapFractionOfSpan = 0.03;
constexpr double kLabelSnapDeltaDb = 10.0;
} // namespace

void SpectrumRenderer::decimateMinMax(const float* src, std::size_t srcCount, int pixelColumns,
    std::vector<double>& outX, std::vector<double>& outY) const
{
    outX.clear();
    outY.clear();

    if (pixelColumns <= 0 || srcCount == 0) {
        return;
    }

    if (static_cast<std::size_t>(pixelColumns) * 2 >= srcCount) {
        // Menos bins que columnas de pantalla: no hace falta decimar.
        outX.reserve(srcCount);
        outY.reserve(srcCount);
        for (std::size_t i = 0; i < srcCount; ++i) {
            outX.push_back(static_cast<double>(i));
            outY.push_back(static_cast<double>(src[i]));
        }
        return;
    }

    outX.reserve(static_cast<std::size_t>(pixelColumns) * 2);
    outY.reserve(static_cast<std::size_t>(pixelColumns) * 2);

    for (int col = 0; col < pixelColumns; ++col) {
        const auto begin = (srcCount * static_cast<std::size_t>(col)) / static_cast<std::size_t>(pixelColumns);
        const auto end = (srcCount * static_cast<std::size_t>(col + 1)) / static_cast<std::size_t>(pixelColumns);
        if (begin >= end) {
            continue;
        }

        float minV = src[begin];
        float maxV = src[begin];
        std::size_t minIdx = begin;
        std::size_t maxIdx = begin;
        for (std::size_t i = begin + 1; i < end; ++i) {
            if (src[i] < minV) {
                minV = src[i];
                minIdx = i;
            }
            if (src[i] > maxV) {
                maxV = src[i];
                maxIdx = i;
            }
        }

        // Se anaden en orden de indice (no de valor) para que la linea que
        // dibuja ImPlot no zigzaguee hacia atras dentro de la columna.
        if (minIdx <= maxIdx) {
            outX.push_back(static_cast<double>(minIdx));
            outY.push_back(static_cast<double>(minV));
            outX.push_back(static_cast<double>(maxIdx));
            outY.push_back(static_cast<double>(maxV));
        } else {
            outX.push_back(static_cast<double>(maxIdx));
            outY.push_back(static_cast<double>(maxV));
            outX.push_back(static_cast<double>(minIdx));
            outY.push_back(static_cast<double>(minV));
        }
    }
}

SpectrumInteraction SpectrumRenderer::draw(const rfpulse::spectrum::SpectrumFrame& frame, float widthPx, float heightPx,
    bool showAverage, bool showMaxHold, bool showMinHold, float detectionThresholdDb, float usableSpanFraction)
{
    SpectrumInteraction interaction;
    if (frame.binCount == 0) {
        return interaction;
    }

    const int pixelColumns = std::max(1, static_cast<int>(widthPx));
    const double freqStart = frame.centerFrequencyHz - frame.spanHz / 2.0;
    const double hzPerBin = frame.spanHz / static_cast<double>(frame.binCount);

    // Recorta simetricamente los bins de ambos extremos que caen en la zona
    // de transicion real del filtro de decimacion del DDC (ver
    // Application::kUsableSpanFraction) -- ni se muestran en la traza (via
    // los limites del eje X) ni participan en la busqueda de picos, para
    // que esa franja nunca se confunda con una señal real. Con fallback a
    // el rango completo si el recorte calculado dejara una ventana vacia
    // (binCount muy pequeño).
    std::size_t firstUsableBin = 0;
    std::size_t lastUsableBin = frame.binCount;
    if (usableSpanFraction > 0.0f && usableSpanFraction < 1.0f) {
        const auto marginBins = static_cast<std::size_t>(
            std::lround(static_cast<double>(frame.binCount) * (1.0 - static_cast<double>(usableSpanFraction)) / 2.0));
        if (marginBins > 0 && frame.binCount - 2 * marginBins > 0) {
            firstUsableBin = marginBins;
            lastUsableBin = frame.binCount - marginBins;
        }
    }

    // El eje X se dibuja en MHz (no en Hz crudos): con valores del orden de
    // 5.6e+08, ImPlot etiqueta el eje en notacion cientifica ("5.55e+08"),
    // que un tecnico de RF no lee de un vistazo tan rapido como "555.0". El
    // resto del pipeline (SpectrumFrame, centerFrequencyHz_, tuneVfoAt, etc.)
    // sigue trabajando en Hz; la conversion es solo para lo que se le pasa a
    // ImPlot y para lo que se devuelve al hacer click.
    auto binIndexToHz = [&](double binIndex) { return freqStart + binIndex * hzPerBin; };
    auto binIndexToMHz = [&](double binIndex) { return binIndexToHz(binIndex) / 1e6; };
    auto plotTrace = [&](const char* label, const std::vector<float>& db, const ImVec4& color, float thickness) {
        decimateMinMax(db.data(), frame.binCount, pixelColumns, scratchX_, scratchY_);
        for (double& x : scratchX_) {
            x = binIndexToMHz(x);
        }
        ImPlot::SetNextLineStyle(color, thickness);
        ImPlot::PlotLine(label, scratchX_.data(), scratchY_.data(), static_cast<int>(scratchX_.size()));
    };

    const double usableFreqStart = binIndexToHz(static_cast<double>(firstUsableBin));
    const double usableFreqEnd = binIndexToHz(static_cast<double>(lastUsableBin));

    // Marcador de pico ("peak search"): el punto mas fuerte, buscado en
    // resolucion completa (no en la version decimada para pantalla) y solo
    // dentro del rango util (ver arriba), tal como hacen los analizadores
    // de espectro profesionales. Se muestra siempre, sin depender del
    // umbral de deteccion (util incluso si lo mas fuerte del span util es
    // solo ruido de fondo).
    //
    // Se busca sobre frame.averageDb (el promedio exponencial), NO sobre
    // frame.currentDb (la traza en vivo): buscar sobre la traza en vivo
    // hacia que el marcador "rebotara" a la velocidad del ruido de cada
    // trama de FFT, tanto en el valor dBFS mostrado como en el propio bin
    // ganador cuando dos bins vecinos tenian una potencia muy parecida --
    // ilegible en movimiento (reportado por el usuario). averageDb se
    // recalcula en cada trama igual que currentDb (ver
    // SpectrumProcessor::submit), asi que sigue reaccionando a un
    // transmisor que se acaba de encender en un puñado de tramas, no
    // instantaneamente pero sin el parpadeo -- y si el usuario desactiva
    // el promediado (averagingEnabled_ a false en Application), averageDb
    // pasa a ser una copia exacta de currentDb en cada trama, asi que el
    // comportamiento en vivo de siempre sigue disponible sin mas que
    // apagar "Prom.".
    std::size_t peakBin = firstUsableBin;
    float peakDb = frame.averageDb[firstUsableBin];
    for (std::size_t i = firstUsableBin + 1; i < lastUsableBin; ++i) {
        if (frame.averageDb[i] > peakDb) {
            peakDb = frame.averageDb[i];
            peakBin = i;
        }
    }
    const double peakFreqMHz = binIndexToMHz(static_cast<double>(peakBin));

    // Deteccion de TODAS las señales activas por encima de
    // detectionThresholdDb (no solo la mas fuerte): es la base del "mapa de
    // canales" visual (marcadores secundarios mas abajo, con su propio
    // valor MHz/dBFS) y de a que pico exacto se ajusta un clic cercano (ver
    // kPeakSnapToleranceCol). minSeparationBins se deriva de
    // kMinPeakSeparationHz (constante en Hz) en vez de un numero fijo de
    // bins, porque el numero de bins por Hz cambia con el tamaño de FFT.
    // Se busca solo dentro de [firstUsableBin, lastUsableBin) por el mismo
    // motivo que el pico global, y sobre averageDb por el mismo motivo de
    // estabilidad de arriba; detectPeaks devuelve indices relativos a ese
    // subrango, asi que se les suma firstUsableBin al usarlos.
    const auto minSeparationBins =
        std::max<std::size_t>(1, static_cast<std::size_t>(kMinPeakSeparationHz / hzPerBin));
    const auto detectedPeaks = rfpulse::spectrum::detectPeaks(frame.averageDb.data() + firstUsableBin,
        lastUsableBin - firstUsableBin, detectionThresholdDb, minSeparationBins);

    if (ImPlot::BeginPlot("##spectrum", ImVec2(widthPx, heightPx), ImPlotFlags_NoMouseText)) {
        ImPlot::SetupAxes("Frecuencia (MHz)", "dBFS");
        ImPlot::SetupAxisLimits(ImAxis_X1, usableFreqStart / 1e6, usableFreqEnd / 1e6, ImGuiCond_Always);
        ImPlot::SetupAxisLimits(ImAxis_Y1, kDefaultYAxisMinDb, kDefaultYAxisMaxDb, ImGuiCond_Once);

        // Se captura DESPUES de SetupAxes/SetupAxisLimits (ImPlot ya sabe el
        // ancho de las etiquetas del eje Y en este punto) para que el
        // waterfall pueda alinearse exactamente con el area de datos real,
        // no con el ancho total del widget (ver el comentario de
        // SpectrumInteraction::plotAreaScreenX).
        const ImVec2 plotPos = ImPlot::GetPlotPos();
        const ImVec2 plotSize = ImPlot::GetPlotSize();
        interaction.plotAreaScreenX = plotPos.x;
        interaction.plotAreaWidthPx = plotSize.x;

        if (showMaxHold) {
            plotTrace("max hold", frame.maxHoldDb, rfpulse::ui::TraceColors::kMaxHold, 1.0f);
        }
        if (showMinHold) {
            plotTrace("min hold", frame.minHoldDb, rfpulse::ui::TraceColors::kMinHold, 1.0f);
        }
        if (showAverage) {
            plotTrace("promedio", frame.averageDb, rfpulse::ui::TraceColors::kAverage, 1.5f);
        }
        plotTrace("actual", frame.currentDb, rfpulse::ui::TraceColors::kCurrent, 1.75f);

        double peakX = peakFreqMHz;
        double peakY = static_cast<double>(peakDb);
        ImPlot::SetNextMarkerStyle(
            ImPlotMarker_Down, 7.0f, ImVec4(1.0f, 1.0f, 1.0f, 1.0f), 1.5f, rfpulse::ui::TraceColors::kCurrent);
        ImPlot::PlotScatter("##peak", &peakX, &peakY, 1);

        // El cartel (caja de texto) del pico principal usa una posicion/
        // valor suavizados aparte del marcador de arriba: aunque el pico ya
        // se busca sobre averageDb (ver el comentario de peakBin), puede
        // seguir oscilando entre dos bins de potencia casi identica de una
        // trama a otra, y esa oscilacion -- por pequeña que sea en MHz/dB --
        // hacia que la caja "diera botes" en pantalla, dificil de leer
        // (reportado por el usuario). El marcador y el iman de clic (mas
        // abajo) siguen usando peakX/peakY reales sin tocar.
        const double peakDbDouble = static_cast<double>(peakDb);
        if (!hasDisplayedPeak_) {
            displayedPeakFreqMHz_ = peakFreqMHz;
            displayedPeakDb_ = peakDbDouble;
            hasDisplayedPeak_ = true;
        } else {
            const double visibleSpanMHz = (usableFreqEnd - usableFreqStart) / 1e6;
            const bool bigJump =
                std::abs(peakFreqMHz - displayedPeakFreqMHz_) > visibleSpanMHz * kLabelSnapFractionOfSpan
                || std::abs(peakDbDouble - displayedPeakDb_) > kLabelSnapDeltaDb;
            if (bigJump) {
                displayedPeakFreqMHz_ = peakFreqMHz;
                displayedPeakDb_ = peakDbDouble;
            } else {
                displayedPeakFreqMHz_ += kLabelSmoothingAlpha * (peakFreqMHz - displayedPeakFreqMHz_);
                displayedPeakDb_ += kLabelSmoothingAlpha * (peakDbDouble - displayedPeakDb_);
            }
        }
        ImPlot::Annotation(
            displayedPeakFreqMHz_, displayedPeakDb_, ImVec4(1.0f, 1.0f, 1.0f, 1.0f), ImVec2(0.0f, -18.0f), true,
            "%.4f MHz  %.1f dBFS", displayedPeakFreqMHz_, displayedPeakDb_);

        // Marcadores secundarios: el resto de señales detectadas (aparte de
        // la mas fuerte, que ya tiene su propio marcador de arriba) se
        // señalan con un circulo y su propio valor MHz/dBFS, igual que el
        // pico principal pero sin el marcador en forma de flecha -- asi se
        // puede leer de un vistazo el nivel de varias señales visibles a la
        // vez, no solo la mas fuerte.
        secondaryPeakX_.clear();
        secondaryPeakY_.clear();
        for (const auto& peak : detectedPeaks) {
            const std::size_t absoluteBin = peak.binIndex + firstUsableBin;
            if (absoluteBin == peakBin) {
                continue;
            }
            secondaryPeakX_.push_back(binIndexToMHz(static_cast<double>(absoluteBin)));
            secondaryPeakY_.push_back(static_cast<double>(peak.db));
        }
        if (!secondaryPeakX_.empty()) {
            ImPlot::SetNextMarkerStyle(ImPlotMarker_Circle, 5.0f, rfpulse::ui::TraceColors::kMaxHold, 1.0f,
                rfpulse::ui::TraceColors::kMaxHold);
            ImPlot::PlotScatter(
                "##scan_peaks", secondaryPeakX_.data(), secondaryPeakY_.data(), static_cast<int>(secondaryPeakX_.size()));
            for (std::size_t i = 0; i < secondaryPeakX_.size(); ++i) {
                ImPlot::Annotation(secondaryPeakX_[i], secondaryPeakY_[i], rfpulse::ui::TraceColors::kMaxHold,
                    ImVec2(0.0f, -14.0f), true, "%.4f MHz  %.1f dBFS", secondaryPeakX_[i], secondaryPeakY_[i]);
            }
        }

        if (ImPlot::IsPlotHovered()) {
            const ImPlotPoint mouse = ImPlot::GetPlotMousePos();
            ImGui::BeginTooltip();
            ImGui::Text("%.4f MHz", mouse.x);
            ImGui::Text("%.1f dBFS", mouse.y);
            ImGui::EndTooltip();

            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                // "Iman" de picos: si el clic cae cerca (en pixeles, no en
                // MHz -- el span puede ser de 1 o de 20 MHz) de un marcador
                // de pico, se sintoniza su frecuencia exacta en vez de la
                // posicion cruda del raton, para poder apuntar con
                // precision a una señal estrecha (ver kPeakSnapToleranceCol).
                const double mhzPerPixel = (usableFreqEnd - usableFreqStart) / 1e6 / std::max(widthPx, 1.0f);
                const double snapToleranceMHz = mhzPerPixel * kPeakSnapToleranceCol;

                double bestFreqMHz = peakFreqMHz;
                double bestDistance = std::abs(mouse.x - peakFreqMHz);
                for (std::size_t i = 0; i < secondaryPeakX_.size(); ++i) {
                    const double distance = std::abs(mouse.x - secondaryPeakX_[i]);
                    if (distance < bestDistance) {
                        bestDistance = distance;
                        bestFreqMHz = secondaryPeakX_[i];
                    }
                }

                const double snappedMHz = (bestDistance <= snapToleranceMHz) ? bestFreqMHz : mouse.x;
                interaction.clickedFrequencyHz = snappedMHz * 1e6; // eje en MHz; el resto del pipeline espera Hz
            }
        }

        ImPlot::EndPlot();
    }

    return interaction;
}

} // namespace rfpulse::render
