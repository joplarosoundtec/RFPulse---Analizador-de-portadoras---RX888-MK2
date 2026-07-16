#include "GraphicsDevice.h"

#include <stdexcept>

using Microsoft::WRL::ComPtr;

namespace rfpulse::render {

GraphicsDevice::GraphicsDevice(HWND hwnd, std::uint32_t width, std::uint32_t height)
    : width_(width)
    , height_(height)
{
    UINT createFlags = 0;
#ifdef _DEBUG
    createFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
    HRESULT hr = D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createFlags,
        nullptr, 0, D3D11_SDK_VERSION,
        device_.GetAddressOf(), &featureLevel, context_.GetAddressOf());
    if (FAILED(hr)) {
        // Sin GPU compatible (maquinas virtuales, sesiones RDP sin
        // aceleracion, entornos de CI): recurre al rasterizador software
        // WARP en vez de fallar. Mas lento, pero mantiene la app utilizable
        // para desarrollo/pruebas en cualquier maquina.
        hr = D3D11CreateDevice(
            nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createFlags,
            nullptr, 0, D3D11_SDK_VERSION,
            device_.GetAddressOf(), &featureLevel, context_.GetAddressOf());
    }
    if (FAILED(hr)) {
        throw std::runtime_error("GraphicsDevice: D3D11CreateDevice fallo (hardware y WARP)");
    }

    ComPtr<IDXGIDevice> dxgiDevice;
    hr = device_.As(&dxgiDevice);
    if (FAILED(hr)) {
        throw std::runtime_error("GraphicsDevice: no se pudo obtener IDXGIDevice");
    }
    ComPtr<IDXGIAdapter> adapter;
    hr = dxgiDevice->GetAdapter(adapter.GetAddressOf());
    if (FAILED(hr)) {
        throw std::runtime_error("GraphicsDevice: IDXGIDevice::GetAdapter fallo");
    }
    ComPtr<IDXGIFactory2> factory;
    hr = adapter->GetParent(IID_PPV_ARGS(factory.GetAddressOf()));
    if (FAILED(hr)) {
        throw std::runtime_error("GraphicsDevice: IDXGIAdapter::GetParent fallo");
    }

    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.Width = width;
    desc.Height = height;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 2;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.Scaling = DXGI_SCALING_STRETCH;
    desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

    hr = factory->CreateSwapChainForHwnd(device_.Get(), hwnd, &desc, nullptr, nullptr, swapChain_.GetAddressOf());
    if (FAILED(hr)) {
        throw std::runtime_error("GraphicsDevice: CreateSwapChainForHwnd fallo");
    }
    factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);

    createRenderTarget();
}

GraphicsDevice::~GraphicsDevice() = default;

void GraphicsDevice::createRenderTarget()
{
    ComPtr<ID3D11Texture2D> backBuffer;
    swapChain_->GetBuffer(0, IID_PPV_ARGS(backBuffer.GetAddressOf()));
    device_->CreateRenderTargetView(backBuffer.Get(), nullptr, renderTargetView_.GetAddressOf());
}

void GraphicsDevice::releaseRenderTarget()
{
    renderTargetView_.Reset();
}

void GraphicsDevice::resize(std::uint32_t width, std::uint32_t height)
{
    if (width == 0 || height == 0) {
        return;
    }
    width_ = width;
    height_ = height;

    releaseRenderTarget();
    swapChain_->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    createRenderTarget();
}

void GraphicsDevice::beginFrame(const float clearColor[4])
{
    context_->OMSetRenderTargets(1, renderTargetView_.GetAddressOf(), nullptr);
    context_->ClearRenderTargetView(renderTargetView_.Get(), clearColor);

    D3D11_VIEWPORT viewport{};
    viewport.Width = static_cast<float>(width_);
    viewport.Height = static_cast<float>(height_);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    context_->RSSetViewports(1, &viewport);
}

void GraphicsDevice::present(bool vsync)
{
    swapChain_->Present(vsync ? 1 : 0, 0);
}

} // namespace rfpulse::render
