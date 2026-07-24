#pragma once

#include "spectrum/SpectrumFrame.h"

#include <cstddef>
#include <optional>
#include <vector>

namespace rfpulse::render {

// Resultado de un frame de dibujado: lo que el usuario hizo con el raton.
struct SpectrumInteraction {
    // Frecuencia (Hz) donde el usuario hizo clic con el boton izquierdo
    // dentro del area del grafico este frame, o nullopt si no hizo clic.
    // Si el clic cayo cerca (en pixeles) de un marcador de pico (el global o
    // uno de los secundarios), se ajusta a la frecuencia EXACTA de ese pico
    // en vez de la posicion cruda del raton -- ver kPeakSnapToleranceCol en
    // el .cpp -- para poder sintonizar con precision haciendo clic sobre el
    // marcador aunque el pico sea estrecho.
    std::optional<double> clickedFrequencyHz;

    // Rectangulo real (en coordenadas de pantalla/pixeles) donde ImPlot
    // dibujo el AREA DE DATOS del grafico -- no el ancho total pasado a
    // draw(), que tambien incluye el hueco que ImPlot reserva para las
    // etiquetas del eje Y ("-100", "dBFS", etc.). El waterfall se dibuja
    // justo debajo del espectro y necesita alinear su imagen exactamente
    // con esta area para que una frecuencia dada quede en la misma columna
    // de pixeles en los dos graficos; usar el ancho total sin corregir
    // desplaza el waterfall hacia la izquierda respecto al espectro (bug
    // real reportado: "los mhz no estan bien alineados en el waterfall").
    // Quedan a 0 si no se llego a abrir el plot (frame.binCount == 0).
    float plotAreaScreenX = 0.0f;
    float plotAreaWidthPx = 0.0f;
};

// Dibuja la traza de espectro (actual + opcionalmente promedio/max hold/min
// hold) con ImPlot. Antes de pasar los datos a ImPlot, los reduce a como
// mucho ~2 puntos (minimo y maximo) por columna de pixel visible: enviar
// hasta 32768 puntos a ImPlot cuando la pantalla tiene unos pocos miles de
// columnas es trabajo desperdiciado, y una espurea de un solo bin de ancho
// desaparece con un promedio o un submuestreo ingenuo pero NO con esta
// decimacion min/max (es la misma tecnica que usan los analizadores de
// espectro profesionales para su traza en tiempo real).
class SpectrumRenderer {
public:
    // El eje X se dibuja en MHz (mas legible que la notacion cientifica en
    // Hz que usaria ImPlot por defecto a estas magnitudes); el clic devuelto
    // en SpectrumInteraction se reconvierte a Hz (x1e6) para que el resto
    // del pipeline (tuneVfoAt, applyCenterFrequency, etc.) siga recibiendo
    // Hz sin tener que saber nada de esta conversion interna.
    //
    // detectionThresholdDb es el umbral (sobre frame.averageDb -- no la
    // traza en vivo, para que el marcador no rebote a la velocidad del
    // ruido de cada trama, ver el comentario junto a la busqueda de pico en
    // el .cpp) que decide que señales, ademas del pico global (que siempre
    // se marca), se anotan como picos secundarios con su propio marcador +
    // valor (MHz/dBFS).
    //
    // usableSpanFraction (0..1] recorta simetricamente los bins de ambos
    // extremos del span antes de fijar los limites del eje X, buscar el
    // pico global y detectar picos secundarios -- ver
    // Application::kUsableSpanFraction para el porque (zona de transicion
    // real del filtro de decimacion del DDC). 1.0 desactiva el recorte.
    SpectrumInteraction draw(const rfpulse::spectrum::SpectrumFrame& frame, float widthPx, float heightPx,
        bool showAverage, bool showMaxHold, bool showMinHold, float detectionThresholdDb, float usableSpanFraction);

private:
    void decimateMinMax(const float* src, std::size_t srcCount, int pixelColumns,
        std::vector<double>& outX, std::vector<double>& outY) const;

    std::vector<double> scratchX_;
    std::vector<double> scratchY_;
    std::vector<double> secondaryPeakX_;
    std::vector<double> secondaryPeakY_;

    // Posicion/valor MOSTRADOS en el cartel del pico principal (la
    // fundamental), suavizados por separado del marcador real -- ver el
    // comentario junto a su uso en el .cpp para el porque (el usuario
    // reporto que el cartel "daba botes" seguiendo al pico de trama en
    // trama). El marcador (PlotScatter) y el "iman" de clic siguen usando
    // el pico real sin suavizar; solo la caja de texto usa estos valores.
    bool hasDisplayedPeak_ = false;
    double displayedPeakFreqMHz_ = 0.0;
    double displayedPeakDb_ = 0.0;
};

} // namespace rfpulse::render
