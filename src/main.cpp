// Punto de entrada. Crea la ventana principal y la Application, que junta
// todos los modulos (SDR/generador sintetico, espectro, waterfall, VFO,
// audio, settings, logging) y se encarga de dibujar cada frame.

#include "app/Application.h"
#include "render/TextureLoader.h"
#include "ui/MainWindow.h"

#include <array>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <windows.h>

namespace {

// Carpeta donde esta el propio .exe (con la barra final incluida), NO el
// directorio de trabajo actual del proceso -- son lo mismo al lanzar el
// .exe con doble clic desde el Explorador, pero NO al ejecutarlo desde el
// depurador de Visual Studio (que por defecto usa el directorio del
// proyecto como CWD) ni desde muchos otros lanzadores. logo.png se copia
// junto al .exe en cada build (ver el post-build de src/CMakeLists.txt),
// asi que hay que resolverlo relativo a DONDE ESTA EL EXE, no al CWD, o
// desaparece en cuanto el CWD no coincide (reportado por el usuario:
// visible en el precompilado que distribuyo -- siempre lanzado con el CWD
// igual a su carpeta -- pero no en su propia compilacion).
std::wstring exeDirectory()
{
    std::array<wchar_t, MAX_PATH> buffer{};
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0 || length == buffer.size()) {
        return L"";
    }
    const std::wstring path(buffer.data(), length);
    const auto lastSlash = path.find_last_of(L"\\/");
    return (lastSlash != std::wstring::npos) ? path.substr(0, lastSlash + 1) : L"";
}

} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
try {
    rfpulse::ui::MainWindow window(L"RfPulse", 1600, 900);
    const std::wstring logoPath = exeDirectory() + L"logo.png";
    const auto logo = rfpulse::render::loadTextureFromFile(window.graphics(), logoPath.c_str());
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
