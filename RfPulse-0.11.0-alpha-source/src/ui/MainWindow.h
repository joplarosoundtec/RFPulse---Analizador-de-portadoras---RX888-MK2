#pragma once

#include "render/GraphicsDevice.h"
#include "render/TextureLoader.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <windows.h>

namespace rfpulse::ui {

// Ventana Win32 + dispositivo DirectX11 + backend ImGui (Win32 + DX11). El
// bucle de mensajes vive en run(): procesa los mensajes pendientes de la
// ventana y luego invoca onFrame una vez por frame renderizado, entre
// ImGui::NewFrame() y ImGui::Render() (onFrame es donde va todo el
// contenido ImGui/ImPlot del frame). Vuelve cuando se cierra la ventana.
class MainWindow {
public:
    MainWindow(const wchar_t* title, std::uint32_t width, std::uint32_t height);
    ~MainWindow();

    MainWindow(const MainWindow&) = delete;
    MainWindow& operator=(const MainWindow&) = delete;

    void run(const std::function<void(float deltaSeconds)>& onFrame);

    // Pantalla de carga inicial: logo (si logo.valid(), ver
    // render::loadTextureFromFile) + autor centrados, durante al menos
    // minDurationMs (aunque la ventana ya este lista, para que sea legible
    // incluso cuando arranca en modo DEMO, donde no hay ninguna carga real
    // que esperar). Bloquea (bombea mensajes de ventana igual que run(), asi
    // que la ventana sigue respondiendo) hasta cumplir esa duracion o hasta
    // que el usuario cierre la ventana. Pensada para llamarse UNA vez, ANTES
    // de construir Application (que es la parte realmente lenta del
    // arranque: abrir el dispositivo SDR, FFTW, etc.) -- deliberadamente
    // secuencial, no en paralelo con esa carga real, para no tener que
    // sincronizar el ID3D11DeviceContext entre dos hilos. Si logo no es
    // valido (archivo no encontrado, etc.), se cae a un titulo de texto en
    // su lugar en vez de dejar la pantalla vacia.
    void showSplash(
        const rfpulse::render::LoadedTexture& logo, const char* fallbackTitle, const char* author,
        float minDurationMs);

    HWND handle() const noexcept { return hwnd_; }
    rfpulse::render::GraphicsDevice& graphics() { return *graphics_; }

private:
    static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
    LRESULT handleMessage(UINT msg, WPARAM wparam, LPARAM lparam);

    HWND hwnd_ = nullptr;
    std::unique_ptr<rfpulse::render::GraphicsDevice> graphics_;
    bool running_ = true;
};

} // namespace rfpulse::ui
