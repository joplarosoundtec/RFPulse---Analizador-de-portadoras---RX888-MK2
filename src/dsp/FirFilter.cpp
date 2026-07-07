#include "FirFilter.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace rfpulse::dsp {

namespace {

// I0: funcion de Bessel modificada de orden 0, por serie de potencias.
// 25 terminos son sobradamente suficientes para los valores de beta
// (0..~12) que se usan en ventanas Kaiser practicas, con precision doble.
double besselI0(double x)
{
    double sum = 1.0;
    double term = 1.0;
    for (int k = 1; k <= 25; ++k) {
        term *= (x * x) / (4.0 * static_cast<double>(k) * static_cast<double>(k));
        sum += term;
    }
    return sum;
}

} // namespace

std::vector<float> designLowpassFir(double sampleRateHz, double cutoffHz, int numTaps, double kaiserBeta)
{
    std::vector<float> taps(static_cast<std::size_t>(numTaps), 0.0f);
    if (numTaps <= 0) {
        return taps;
    }

    const int m = numTaps - 1;
    const double fc = cutoffHz / sampleRateHz; // normalizada, ciclos/muestra
    const double i0Beta = besselI0(kaiserBeta);
    const double halfM = static_cast<double>(m) / 2.0;

    double sum = 0.0;
    for (int n = 0; n < numTaps; ++n) {
        const double x = static_cast<double>(n) - halfM;

        double sincVal;
        if (x == 0.0) {
            sincVal = 2.0 * fc;
        } else {
            sincVal = std::sin(2.0 * std::numbers::pi * fc * x) / (std::numbers::pi * x);
        }

        double window = 0.0;
        if (halfM > 0.0) {
            const double ratio = x / halfM;
            const double windowArg = 1.0 - ratio * ratio;
            window = (windowArg >= 0.0) ? besselI0(kaiserBeta * std::sqrt(windowArg)) / i0Beta : 0.0;
        } else {
            window = 1.0;
        }

        const double h = sincVal * window;
        taps[static_cast<std::size_t>(n)] = static_cast<float>(h);
        sum += h;
    }

    if (sum != 0.0) {
        for (float& t : taps) {
            t = static_cast<float>(static_cast<double>(t) / sum);
        }
    }
    return taps;
}

DecimatingFirFilter::DecimatingFirFilter(std::vector<float> taps, int decimation)
    : taps_(std::move(taps))
    , decimation_(decimation)
    , history_(std::max<std::size_t>(taps_.size(), 1))
    , samplesUntilNextOutput_(decimation)
{
}

void DecimatingFirFilter::reset()
{
    for (std::size_t i = 0; i < history_.size(); ++i) {
        history_[i] = std::complex<float>(0.0f, 0.0f);
    }
    historyWritePos_ = 0;
    samplesUntilNextOutput_ = decimation_;
}

std::size_t DecimatingFirFilter::process(const std::complex<float>* in, std::size_t inCount, std::complex<float>* out)
{
    std::size_t outCount = 0;
    const std::size_t n = taps_.size();
    if (n == 0) {
        return 0;
    }

    for (std::size_t i = 0; i < inCount; ++i) {
        history_[historyWritePos_] = in[i];
        historyWritePos_ = (historyWritePos_ + 1) % n;

        if (--samplesUntilNextOutput_ <= 0) {
            samplesUntilNextOutput_ = decimation_;

            // taps_[k] se aplica a la muestra que entro hace (n-1-k) pasos;
            // recorriendo k=0..n-1 partiendo de historyWritePos_ (la
            // posicion mas antigua justo despues de la escritura circular)
            // se obtiene el orden causal correcto de la convolucion.
            std::complex<float> acc(0.0f, 0.0f);
            for (std::size_t k = 0; k < n; ++k) {
                const std::size_t idx = (historyWritePos_ + k) % n;
                acc += history_[idx] * taps_[k];
            }
            out[outCount++] = acc;
        }
    }
    return outCount;
}

} // namespace rfpulse::dsp
