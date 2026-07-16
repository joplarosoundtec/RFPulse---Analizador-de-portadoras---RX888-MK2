#include "TextureLoader.h"

#include <wincodec.h>

#include <cstdint>
#include <vector>

namespace rfpulse::render {

namespace {

// WIC exige COM inicializado en el hilo que lo usa. loadTextureFromFile()
// solo se llama desde el hilo principal, antes de que arranque cualquier
// otro hilo de la aplicación (ver main.cpp), así que basta con
// inicializarlo una vez por proceso -- static local, construcción
// perezosa segura entre hilos garantizada por el propio lenguaje.
struct ComInit {
    HRESULT hr;
    ComInit()
        : hr(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))
    {
    }
    ~ComInit()
    {
        if (SUCCEEDED(hr)) {
            CoUninitialize();
        }
    }
};

} // namespace

LoadedTexture loadTextureFromFile(GraphicsDevice& graphics, const wchar_t* path)
{
    LoadedTexture result;

    static ComInit comInit;
    // RPC_E_CHANGED_MODE: COM ya estaba inicializado (p.ej. con otro modelo
    // de threading) por otra parte del proceso -- no es un fallo real, WIC
    // sigue siendo utilizable a través de la instancia ya inicializada.
    if (FAILED(comInit.hr) && comInit.hr != RPC_E_CHANGED_MODE) {
        return result;
    }

    Microsoft::WRL::ComPtr<IWICImagingFactory> factory;
    HRESULT hr =
        CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (FAILED(hr)) {
        return result;
    }

    Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
    hr = factory->CreateDecoderFromFilename(
        path, nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder);
    if (FAILED(hr)) {
        return result;
    }

    Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr)) {
        return result;
    }

    // Se normaliza a RGBA de 8 bits por canal sea cual sea el formato de
    // origen (el PNG del logo es RGB de 8 bits sin alfa, pero esto tambien
    // cubre JPEG, formatos con paleta, etc. sin casos especiales): coincide
    // directamente con DXGI_FORMAT_R8G8B8A8_UNORM, sin conversion manual de
    // canales.
    Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
    hr = factory->CreateFormatConverter(&converter);
    if (FAILED(hr)) {
        return result;
    }
    hr = converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr,
        0.0, WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) {
        return result;
    }

    UINT width = 0;
    UINT height = 0;
    hr = converter->GetSize(&width, &height);
    if (FAILED(hr) || width == 0 || height == 0) {
        return result;
    }

    const UINT stride = width * 4;
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(stride) * height);
    hr = converter->CopyPixels(nullptr, stride, static_cast<UINT>(pixels.size()), pixels.data());
    if (FAILED(hr)) {
        return result;
    }

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA initData{};
    initData.pSysMem = pixels.data();
    initData.SysMemPitch = stride;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    hr = graphics.device()->CreateTexture2D(&desc, &initData, &texture);
    if (FAILED(hr)) {
        return result;
    }

    hr = graphics.device()->CreateShaderResourceView(texture.Get(), nullptr, &result.srv);
    if (FAILED(hr)) {
        result.srv.Reset();
        return result;
    }

    result.width = static_cast<int>(width);
    result.height = static_cast<int>(height);
    return result;
}

} // namespace rfpulse::render
