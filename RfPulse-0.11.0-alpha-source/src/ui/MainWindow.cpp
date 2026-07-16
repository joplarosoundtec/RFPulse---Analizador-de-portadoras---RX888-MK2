#include "MainWindow.h"

#include "AppIcon.h"
#include "Theme.h"

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <implot.h>

#include <algorithm>
#include <chrono>
#include <cstdio>

// imgui_impl_win32.h deja esta declaracion comentada a proposito (para no
// arrastrar <windows.h> desde ese header) y documenta copiarla aqui, en el
// .cpp que si incluye <windows.h> (via MainWindow.h).
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace rfpulse::ui {

namespace {
constexpr wchar_t kWindowClassName[] = L"RfPulseMainWindow";
}

LRESULT CALLBACK MainWindow::wndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam) != 0) {
        return true;
    }

    auto* self = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (self != nullptr) {
        return self->handleMessage(msg, wparam, lparam);
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

LRESULT MainWindow::handleMessage(UINT msg, WPARAM wparam, LPARAM lparam)
{
    switch (msg) {
        case WM_SIZE:
            if (graphics_ && wparam != SIZE_MINIMIZED) {
                graphics_->resize(LOWORD(lparam), HIWORD(lparam));
            }
            return 0;
        case WM_DESTROY:
            running_ = false;
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(hwnd_, msg, wparam, lparam);
    }
}

MainWindow::MainWindow(const wchar_t* title, std::uint32_t width, std::uint32_t height)
{
    // El icono de la ventana (barra de titulo, Alt+Tab, barra de tareas
    // mientras se ejecuta) se carga explicitamente del recurso empotrado en
    // el .exe (ver AppIcon.h/RfPulse.rc) -- Windows a veces reutiliza el
    // icono del propio .exe por defecto igualmente, pero fijarlo aqui es
    // explicito y no depende de ese comportamiento implicito. Si el recurso
    // no estuviera presente (build sin RfPulse.rc por alguna razon),
    // LoadIconW devuelve nullptr y Windows usa su icono generico de
    // ventana, sin que esto sea un error fatal.
    HICON appIcon = LoadIconW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(IDI_APP_ICON));

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = &MainWindow::wndProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = appIcon;
    wc.hIconSm = appIcon;
    wc.lpszClassName = kWindowClassName;
    RegisterClassExW(&wc);

    RECT rect{0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

    hwnd_ = CreateWindowExW(
        0, kWindowClassName, title, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top,
        nullptr, nullptr, wc.hInstance, nullptr);

    SetWindowLongPtrW(hwnd_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

    graphics_ = std::make_unique<rfpulse::render::GraphicsDevice>(hwnd_, width, height);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    applyDarkInstrumentTheme();
    ImGui_ImplWin32_Init(hwnd_);
    ImGui_ImplDX11_Init(graphics_->device(), graphics_->context());

    // ImPlot exige su propio contexto, creado despues del de ImGui (lo dice
    // implot.h explicitamente). Sin esto, ImPlot::BeginPlot desreferencia un
    // puntero de contexto global nulo. Las lineas ya se dibujan suavizadas
    // por defecto (ImGuiStyle::AntiAliasedLines, activo de serie), que es
    // el mecanismo real de antialiasing que usa el draw list subyacente de
    // ImPlot -- no existe un flag separado en ImPlotStyle en esta version.
    ImPlot::CreateContext();

    ShowWindow(hwnd_, SW_SHOW);
    UpdateWindow(hwnd_);
}

MainWindow::~MainWindow()
{
    ImPlot::DestroyContext();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    if (hwnd_ != nullptr) {
        DestroyWindow(hwnd_);
    }
}

void MainWindow::run(const std::function<void(float deltaSeconds)>& onFrame)
{
    using clock = std::chrono::steady_clock;
    auto lastTime = clock::now();

    MSG msg{};
    while (running_) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE) != 0) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (msg.message == WM_QUIT) {
                running_ = false;
            }
        }
        if (!running_) {
            break;
        }

        const auto now = clock::now();
        const float deltaSeconds = std::chrono::duration<float>(now - lastTime).count();
        lastTime = now;

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        const float clearColor[4] = {0.06f, 0.06f, 0.07f, 1.0f};
        graphics_->beginFrame(clearColor);

        if (onFrame) {
            onFrame(deltaSeconds);
        }

        ImGui::Render();
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        graphics_->present(true);
    }
}

void MainWindow::showSplash(
    const rfpulse::render::LoadedTexture& logo, const char* fallbackTitle, const char* author,
    float minDurationMs)
{
    using clock = std::chrono::steady_clock;
    const auto start = clock::now();

    // Tamaño del logo en pantalla: proporcional al lado mas corto de la
    // ventana (no un numero de pixeles fijo, para que siga viendose bien
    // sea cual sea el tamaño de ventana) preservando su relacion de
    // aspecto, nunca agrandado por encima de su tamaño real. Antes era un
    // maximo fijo de 340px, que con la ventana 1600x900 se quedaba pequeño
    // y costaba leerlo (reportado por el usuario); 0.6 del lado mas corto
    // (540px en esa misma ventana) lo deja bien grande conservando margen
    // suficiente para el texto de autor/carga de debajo, dado que el
    // centro del logo esta anclado al 42% de la altura (ver logoY). Se lee
    // el rectangulo de cliente directamente de Win32 (GetClientRect), NO de
    // ImGui::GetIO().DisplaySize -- ese campo solo lo rellena
    // ImGui_ImplWin32_NewFrame(), que todavia no se ha llamado ni una vez
    // aqui (es el primer frame de toda la aplicacion), asi que leerlo antes
    // del bucle devolvia (0,0) y el logo se dibujaba a tamaño cero (bug
    // real: el logo desaparecia por completo en cuanto se calculaba su
    // tamaño desde fuera del bucle).
    RECT clientRect{};
    GetClientRect(hwnd_, &clientRect);
    const float maxLogoSizePx =
        static_cast<float>(std::min(clientRect.right - clientRect.left, clientRect.bottom - clientRect.top)) * 0.6f;
    ImVec2 logoSize(0.0f, 0.0f);
    if (logo.valid()) {
        const float scale =
            std::min(1.0f, maxLogoSizePx / static_cast<float>(std::max(logo.width, logo.height)));
        logoSize = ImVec2(static_cast<float>(logo.width) * scale, static_cast<float>(logo.height) * scale);
    }

    MSG msg{};
    while (running_) {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE) != 0) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (msg.message == WM_QUIT) {
                running_ = false;
            }
        }
        if (!running_) {
            break;
        }

        const float elapsedMs = std::chrono::duration<float, std::milli>(clock::now() - start).count();

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        const float clearColor[4] = {0.06f, 0.06f, 0.07f, 1.0f};
        graphics_->beginFrame(clearColor);

        const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
        ImGui::SetNextWindowSize(displaySize);
        ImGui::Begin(
            "##splash", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
                | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse
                | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings);

        // El logo trae el nombre del producto integrado en la propia
        // imagen, asi que sustituye por completo al titulo de texto -- solo
        // se usa fallbackTitle si logo.png no se pudo cargar (ver
        // TextureLoader::loadTextureFromFile), para no dejar la pantalla de
        // carga vacia de marca en ese caso.
        float contentBottomY;
        if (logo.valid()) {
            const float logoY = displaySize.y * 0.42f - logoSize.y * 0.5f;
            ImGui::SetCursorPos(ImVec2((displaySize.x - logoSize.x) * 0.5f, logoY));
            ImGui::Image(reinterpret_cast<ImTextureID>(logo.srv.Get()), logoSize);
            contentBottomY = logoY + logoSize.y;
        } else {
            ImGui::SetWindowFontScale(2.5f);
            const ImVec2 titleSize = ImGui::CalcTextSize(fallbackTitle);
            const float titleY = displaySize.y * 0.42f;
            ImGui::SetCursorPos(ImVec2((displaySize.x - titleSize.x) * 0.5f, titleY));
            ImGui::TextColored(rfpulse::ui::TraceColors::kMaxHold, "%s", fallbackTitle);
            ImGui::SetWindowFontScale(1.0f);
            contentBottomY = titleY + titleSize.y;
        }

        const ImVec2 authorSize = ImGui::CalcTextSize(author);
        ImGui::SetCursorPos(ImVec2((displaySize.x - authorSize.x) * 0.5f, contentBottomY + 16.0f));
        ImGui::TextDisabled("%s", author);

        // Indicador de carga simple (puntos suspensivos animados, no una
        // barra de progreso real: no hay ninguna tarea con progreso
        // medible que reportar aqui, esta pantalla es puramente de marca/
        // transicion mientras Application hace su arranque, ver el
        // comentario de showSplash en el .h).
        char loadingText[16];
        const int dotCount = 1 + (static_cast<int>(elapsedMs / 400.0f) % 3);
        std::snprintf(loadingText, sizeof(loadingText), "Cargando%.*s", dotCount, "...");
        const ImVec2 loadingSize = ImGui::CalcTextSize(loadingText);
        ImGui::SetCursorPos(
            ImVec2((displaySize.x - loadingSize.x) * 0.5f, contentBottomY + authorSize.y + 40.0f));
        ImGui::TextDisabled("%s", loadingText);

        ImGui::End();

        ImGui::Render();
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        graphics_->present(true);

        if (elapsedMs >= minDurationMs) {
            break;
        }
    }
}

} // namespace rfpulse::ui
