#include "audio/AudioOutput.h"

#include <gtest/gtest.h>

#include <cmath>
#include <numbers>
#include <vector>

using rfpulse::audio::AudioOutput;

// AudioOutput depende de un dispositivo de audio real (WASAPI). En maquinas
// o entornos sin un endpoint de render por defecto (sandboxes, sesiones
// remotas sin audio), open() debe devolver false de forma limpia -- sin
// lanzar y sin dejar el objeto en un estado inconsistente -- para que el
// resto de la aplicacion siga funcionando sin audio. Estos tests se saltan
// (no fallan) cuando no hay dispositivo, y solo verifican el ciclo de vida
// basico cuando si lo hay: no pueden verificar automaticamente que el
// audio "suene bien".

TEST(AudioOutput, OpenFailsGracefullyOrSucceedsCleanly)
{
    AudioOutput audio;
    const bool opened = audio.open(48000);

    if (!opened) {
        GTEST_SKIP() << "No hay dispositivo de audio de salida por defecto en esta maquina/entorno.";
    }

    EXPECT_TRUE(audio.isOpen());
    EXPECT_EQ(audio.sampleRateHz(), 48000u);

    audio.close();
    EXPECT_FALSE(audio.isOpen());
}

TEST(AudioOutput, StartWriteStopDoesNotCrashWhenDeviceAvailable)
{
    AudioOutput audio;
    if (!audio.open(48000)) {
        GTEST_SKIP() << "No hay dispositivo de audio de salida por defecto en esta maquina/entorno.";
    }

    audio.start();

    std::vector<float> tone(4800);
    for (std::size_t n = 0; n < tone.size(); ++n) {
        tone[n] = 0.1f * static_cast<float>(std::sin(2.0 * std::numbers::pi * 440.0 * static_cast<double>(n) / 48000.0));
    }
    const std::size_t written = audio.write(tone.data(), tone.size());
    EXPECT_GT(written, 0u);

    audio.stop();
    audio.close();
}

TEST(AudioOutput, WriteBeforeOpenDoesNotCrash)
{
    AudioOutput audio;
    std::vector<float> silence(100, 0.0f);
    const std::size_t written = audio.write(silence.data(), silence.size());
    // El ring buffer interno existe desde la construccion, asi que esto no
    // deberia fallar aunque el dispositivo no este abierto todavia (el
    // audio simplemente no se reproduce porque el hilo de audio no esta
    // corriendo).
    EXPECT_EQ(written, silence.size());
}
