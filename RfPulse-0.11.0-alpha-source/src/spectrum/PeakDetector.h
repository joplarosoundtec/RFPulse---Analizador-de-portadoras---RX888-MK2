#pragma once

#include <cstddef>
#include <vector>

namespace rfpulse::spectrum {

// Un pico detectado: el bin mas fuerte dentro de un tramo contiguo de la
// traza que supera el umbral de deteccion.
struct DetectedPeak {
    std::size_t binIndex;
    float db;
};

// Encuentra todos los picos de `db` (tamaño binCount) que superan
// thresholdDb, no solo el maximo global -- pensado para detectar todas las
// señales activas en el span (marcadores secundarios del espectro), no solo
// la mas fuerte.
//
// Dos bins por encima del umbral que esten a `minSeparationBins` o menos de
// distancia se tratan como el mismo pico (se queda solo el de mayor
// potencia): sin esto, una señal ancha con ruido en la cima aparceria como
// varios picos vecinos en vez de uno. minSeparationBins deberia elegirse a
// partir del espaciado minimo esperado entre canales reales (ver
// kDefaultMinPeakSeparationHz en el llamador), no de la resolucion de bin.
//
// Devuelve los picos en orden de bin ascendente (frecuencia ascendente).
std::vector<DetectedPeak> detectPeaks(const float* db, std::size_t binCount, float thresholdDb, std::size_t minSeparationBins);

} // namespace rfpulse::spectrum
