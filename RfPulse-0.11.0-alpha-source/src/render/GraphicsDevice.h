#pragma once

#include <cstdint>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <windows.h>
#include <wrl/client.h>

namespace rfpulse::render {

// Dispositivo DirectX11 + swapchain de la ventana principal. Flip-model
// (DXGI_SWAP_EFFECT_FLIP_DISCARD) para baja latencia de presentacion, en
// vez del modelo BitBlt heredado. present(vsync=false) permite tearing
// (FPS no limitado) para la sensacion de fluidez maxima cuando el usuario
// lo pida; por defecto se llama con vsync=true desde MainWindow.
class GraphicsDevice {
public:
    GraphicsDevice(HWND hwnd, std::uint32_t width, std::uint32_t height);
    ~GraphicsDevice();

    GraphicsDevice(const GraphicsDevice&) = delete;
    GraphicsDevice& operator=(const GraphicsDevice&) = delete;

    void resize(std::uint32_t width, std::uint32_t height);
    void beginFrame(const float clearColor[4]);
    void present(bool vsync);

    ID3D11Device* device() const noexcept { return device_.Get(); }
    ID3D11DeviceContext* context() const noexcept { return context_.Get(); }

private:
    void createRenderTarget();
    void releaseRenderTarget();

    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> renderTargetView_;

    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;
};

} // namespace rfpulse::render
