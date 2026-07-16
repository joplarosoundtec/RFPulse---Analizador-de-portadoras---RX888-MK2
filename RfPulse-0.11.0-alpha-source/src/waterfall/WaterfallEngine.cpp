#include "WaterfallEngine.h"

#include <cstring>

namespace rfpulse::waterfall {

WaterfallEngine::WaterfallEngine(std::size_t rowCount, std::size_t binCount)
    : rowCount_(rowCount)
    , binCount_(binCount)
    , history_(rowCount * binCount)
{
}

void WaterfallEngine::pushRow(const float* magnitudeDb, std::size_t binCount)
{
    if (binCount != binCount_) {
        return;
    }
    const std::size_t newRow = (currentRow_.load(std::memory_order_relaxed) + 1) % rowCount_;
    std::memcpy(history_.data() + newRow * binCount_, magnitudeDb, binCount_ * sizeof(float));

    const std::size_t filledSoFar = filledRowCount_.load(std::memory_order_relaxed);
    if (filledSoFar < rowCount_) {
        filledRowCount_.store(filledSoFar + 1, std::memory_order_relaxed);
    }

    // Publica la fila solo despues de que la escritura haya terminado, para
    // que un lector concurrente que vea el nuevo indice tambien vea sus
    // datos completos (happens-before via release/acquire).
    currentRow_.store(newRow, std::memory_order_release);
}

} // namespace rfpulse::waterfall
