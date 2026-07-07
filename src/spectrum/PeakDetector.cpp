#include "PeakDetector.h"

namespace rfpulse::spectrum {

std::vector<DetectedPeak> detectPeaks(const float* db, std::size_t binCount, float thresholdDb, std::size_t minSeparationBins)
{
    std::vector<DetectedPeak> peaks;

    std::size_t i = 0;
    while (i < binCount) {
        if (db[i] <= thresholdDb) {
            ++i;
            continue;
        }

        // Un tramo empieza aqui: se extiende mientras el bin siguiente siga
        // por encima del umbral, o mientras el hueco por debajo del umbral
        // no supere minSeparationBins (para no partir un mismo pico ancho
        // en dos si tiene un valle corto de ruido en la cima).
        std::size_t lastAboveThreshold = i;
        std::size_t peakIdx = i;
        float peakDb = db[i];

        std::size_t j = i + 1;
        while (j < binCount) {
            if (db[j] > thresholdDb) {
                lastAboveThreshold = j;
                if (db[j] > peakDb) {
                    peakDb = db[j];
                    peakIdx = j;
                }
                ++j;
            } else if (j - lastAboveThreshold <= minSeparationBins) {
                ++j;
            } else {
                break;
            }
        }

        peaks.push_back(DetectedPeak{peakIdx, peakDb});
        i = j;
    }

    return peaks;
}

} // namespace rfpulse::spectrum
