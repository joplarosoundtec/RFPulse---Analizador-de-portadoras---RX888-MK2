// Punto de entrada. Crea la ventana principal y la Application, que junta
// todos los modulos (SDR/generador sintetico, espectro, waterfall, VFO,
// audio, settings, logging) y se encarga de dibujar cada frame.

#include "app/Application.h"
#include "render/TextureLoader.h"
#include "ui/MainWindow.h"

#include <cstdio>
#include <stdexcept>
#include <windows.h>

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
try {
    rfpulse::ui::MainWindow window(L"RfPulse", 1600, 900);
    const auto logo = rfpulse::render::loadTextureFromFile(window.graphics(), L"logo.png");
    window.showSplash(logo, "RFPulse Analyzer", "Autor: Joplaro Sound Tech", 1500.0f);
    rfpulse::app::Application application(window.graphics());

    window.run([&](float deltaSeconds) { application.update(deltaSeconds); });

    return 0;
} catch (const std::exception& ex) {
    // Se deja constancia en un archivo ademas del MessageBox: si el fallo
    // ocurre en una maquina sin sesion interactiva (o el usuario cierra el
    // dialogo sin leerlo), el mensaje de error no se pierde.
    FILE* f = nullptr;
    fopen_s(&f, "rfpulse_fatal_error.log", "a");
    if (f != nullptr) {
        fprintf(f, "%s\n", ex.what());
        fclose(f);
    }
    MessageBoxA(nullptr, ex.what(), "RfPulse - error fatal", MB_OK | MB_ICONERROR);
    return 1;
}
