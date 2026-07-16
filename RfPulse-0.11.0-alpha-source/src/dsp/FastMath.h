#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <immintrin.h>

namespace rfpulse::dsp {

// Aproximacion rapida de log2(x), x > 0. Tecnica clasica (Mineiro/
// "fastapprox"): el patron de bits IEEE-754 de un float, leido como entero,
// es ya de por si una aproximacion aproximadamente lineal de log2(x); esta
// funcion corrige el error sistematico de esa aproximacion con un termino
// racional calibrado. La precision real (no la nominal de la tecnica
// original) esta medida en tests/test_fast_math.cpp — consultar ese archivo
// antes de tocar los coeficientes.
inline float fastLog2(float x) noexcept
{
    std::uint32_t bits;
    std::memcpy(&bits, &x, sizeof(bits));

    const std::uint32_t mantissaBits = (bits & 0x007FFFFFu) | 0x3F000000u;
    float mx;
    std::memcpy(&mx, &mantissaBits, sizeof(mx));

    float y = static_cast<float>(static_cast<std::int32_t>(bits));
    y *= 1.1920928955078125e-7f; // 2^-23

    return y - 124.22551499f - 1.498030302f * mx - 1.72587999f / (0.3520887068f + mx);
}

// 1 / log2(10) == log10(2); se deja como constante nombrada por claridad en
// los dos usos (log10 = log2 * este factor).
inline constexpr float kLog10Of2 = 0.30102999566398120f;

inline float fastLog10(float x) noexcept
{
    return fastLog2(x) * kLog10Of2;
}

// Version AVX2 de fastLog2: procesa 8 floats (x > 0) por llamada, misma
// tecnica aplicada carril a carril.
inline __m256 fastLog2Avx2(__m256 x) noexcept
{
    const __m256i xi = _mm256_castps_si256(x);

    const __m256i mantissaBits = _mm256_or_si256(
        _mm256_and_si256(xi, _mm256_set1_epi32(0x007FFFFF)),
        _mm256_set1_epi32(0x3F000000));
    const __m256 mx = _mm256_castsi256_ps(mantissaBits);

    __m256 y = _mm256_cvtepi32_ps(xi);
    y = _mm256_mul_ps(y, _mm256_set1_ps(1.1920928955078125e-7f));

    const __m256 term2 = _mm256_mul_ps(_mm256_set1_ps(1.498030302f), mx);
    const __m256 denom = _mm256_add_ps(_mm256_set1_ps(0.3520887068f), mx);
    const __m256 term3 = _mm256_div_ps(_mm256_set1_ps(1.72587999f), denom);

    __m256 result = _mm256_sub_ps(y, _mm256_set1_ps(124.22551499f));
    result = _mm256_sub_ps(result, term2);
    result = _mm256_sub_ps(result, term3);
    return result;
}

inline __m256 fastLog10Avx2(__m256 x) noexcept
{
    return _mm256_mul_ps(fastLog2Avx2(x), _mm256_set1_ps(kLog10Of2));
}

// Convierte `count` valores de potencia lineal (> 0) a dB: 10*log10(power) +
// offsetDb. Bloques de 8 por AVX2 y cola escalar para el resto. Pensado para
// el paso potencia->dBFS de FftwEngine: se ejecuta una vez por trama
// renderizada (tras el promediado), no por cada FFT en bruto, asi que el
// coste real ya es bajo incluso en escalar — esto lo hace, ademas, gratuito.
inline void powerToDb(const float* power, float* dbOut, std::size_t count, float offsetDb) noexcept
{
    const float dbScale = 10.0f * kLog10Of2;
    const __m256 dbScaleV = _mm256_set1_ps(dbScale);
    const __m256 offset = _mm256_set1_ps(offsetDb);

    std::size_t i = 0;
    for (; i + 8 <= count; i += 8) {
        const __m256 p = _mm256_loadu_ps(power + i);
        const __m256 log2p = fastLog2Avx2(p);
        const __m256 db = _mm256_add_ps(_mm256_mul_ps(log2p, dbScaleV), offset);
        _mm256_storeu_ps(dbOut + i, db);
    }
    for (; i < count; ++i) {
        dbOut[i] = fastLog2(power[i]) * dbScale + offsetDb;
    }
}

} // namespace rfpulse::dsp
