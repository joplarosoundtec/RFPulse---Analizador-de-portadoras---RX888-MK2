#pragma once

#include "GraphicsDevice.h"

#include <d3d11.h>
#include <wrl/client.h>

namespace rfpulse::render {

// Textura D3D11 cargada desde disco, lista para ImGui::Image() (que espera
// un ID3D11ShaderResourceView* disfrazado de ImTextureID, ver
// WaterfallRenderer::draw para el mismo patron).
struct LoadedTexture {
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
    int width = 0;
    int height = 0;

    bool valid() const noexcept { return srv != nullptr; }
};

// Decodifica una imagen (PNG/JPEG/BMP/etc., lo que soporte WIC -- Windows
// Imaging Component, ya disponible en el sistema, sin añadir ninguna
// dependencia nueva al proyecto) y la sube como textura D3D11 inmutable.
// Devuelve un LoadedTexture vacío (valid() == false) si el archivo no
// existe o no se puede decodificar, en vez de lanzar una excepción: un
// logo/asset puramente decorativo que falte no debería impedir arrancar el
// resto de la aplicación.
LoadedTexture loadTextureFromFile(GraphicsDevice& graphics, const wchar_t* path);

} // namespace rfpulse::render
