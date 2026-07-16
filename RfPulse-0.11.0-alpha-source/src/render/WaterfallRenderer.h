#pragma once

#include "GraphicsDevice.h"
#include "waterfall/Palette.h"
#include "waterfall/WaterfallEngine.h"

#include <array>
#include <cstdint>
#include <d3d11.h>
#include <vector>
#include <wrl/client.h>

namespace rfpulse::render {

// Dibuja el historial de WaterfallEngine como una textura RGBA8 en la GPU
// (una fila = una trama en el tiempo). updateRow() recolorea (paleta +
// rango dB, en CPU: un lookup de tabla de 256 entradas por bin, coste
// trivial ya que solo se hace por fila nueva, no para todo el historial) y
// sube cada fila nueva con UpdateSubresource + D3D11_BOX -- nunca la
// textura entera.
//
// El scroll (fila mas reciente siempre arriba) NO se consigue con una unica
// ImGui::Image con UV fuera de [0,1] confiando en direccionamiento WRAP: el
// sampler que crea el backend DX11 de ImGui (ver imgui_impl_dx11.cpp,
// ImGui_ImplDX11_CreateDeviceObjects) usa D3D11_TEXTURE_ADDRESS_CLAMP en
// los tres ejes, y es el UNICO sampler que usa (se enlaza una vez por
// frame para todas las imagenes, no hay forma de pedir uno distinto por
// textura). Un intento anterior asumia WRAP y quedaba con un bloque
// "congelado" (la fila en v=0 repetida) en vez de la parte mas antigua del
// historial cada vez que el rango de v se salia de [0,1]. Por eso draw()
// dibuja el bucle completo con DOS ImGui::Image (ver su comentario), cada
// una con un rango de v monotono dentro de [0,1], en vez de una sola con
// wraparound.
class WaterfallRenderer {
public:
    // D3D11 limita el tamaño de una dimension de Texture2D a 16384 en
    // cualquier feature level relevante (10_0 a 11_1); nuestro FFT size
    // maximo soportado es 32768, por encima de ese limite. Si binCount
    // supera kMaxTextureWidth, cada fila se decima (maximo por bloque, no
    // promedio: preserva espurias de un solo bin mejor) a este ancho antes
    // de subirla a la textura -- la textura nunca se crea mas ancha que
    // esto, sea cual sea el tamaño de FFT elegido.
    static constexpr std::size_t kMaxTextureWidth = 8192;

    WaterfallRenderer(GraphicsDevice& graphics, std::size_t binCount, std::size_t rowCount);

    void setPalette(rfpulse::waterfall::PaletteType type);
    void setDbRange(float minDb, float maxDb);

    // Recolorea y sube a la GPU TODAS las filas escritas por pushRow() desde
    // la ultima llamada, no solo la mas reciente: el hilo de espectro puede
    // producir filas mucho mas rapido que los frames de render (p.ej. ~488
    // filas/s con FFT de 32768 puntos a 16 Msps, frente a ~60 FPS de render),
    // asi que entre dos llamadas puede haber varias filas nuevas. Subir solo
    // la ultima dejaria el resto de la textura sin inicializar (memoria de
    // GPU sin definir en Direct3D 11 al crear la textura sin pInitialData),
    // visible como un bloque con datos viejos o basura en cuanto el
    // historial se muestra completo.
    void updateRow(const rfpulse::waterfall::WaterfallEngine& engine);

    // Dibuja como imagen ImGui de tamano widthPx x heightPx. leftScreenX es
    // la coordenada de pantalla (no relativa a la ventana) donde debe
    // empezar el borde izquierdo de la imagen -- normalmente
    // SpectrumInteraction::plotAreaScreenX, para que el waterfall quede
    // alineado en X con el area de datos del espectro dibujado justo
    // encima, en vez de con el ancho total del panel (que incluye el hueco
    // de las etiquetas del eje Y del espectro y desalinearia las columnas
    // de frecuencia entre ambos graficos). La coordenada Y se toma de la
    // posicion actual del cursor de ImGui (no se toca).
    //
    // usableSpanFraction recorta simetricamente el rango U (frecuencia) de
    // la textura antes de dibujarla, igual que SpectrumRenderer::draw hace
    // con su eje X y su busqueda de picos -- deben pasarse el mismo valor
    // (ver Application::kUsableSpanFraction) para que espectro y waterfall
    // seguir mostrando exactamente el mismo rango de frecuencias columna a
    // columna.
    void draw(const rfpulse::waterfall::WaterfallEngine& engine, float leftScreenX, float widthPx, float heightPx,
        float usableSpanFraction);

private:
    // Sube UNA fila (rowIndex, no necesariamente engine.currentRow()) a la
    // GPU: recolorea segun la paleta/rango dB actual y hace el
    // UpdateSubresource de esa fila concreta de la textura.
    void uploadRow(const rfpulse::waterfall::WaterfallEngine& engine, std::size_t rowIndex);

    GraphicsDevice& graphics_;
    std::size_t binCount_;
    std::size_t rowCount_;
    std::size_t textureWidth_;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> colorTexture_;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> colorSrv_;

    std::array<rfpulse::waterfall::RgbColor, 256> paletteLut_{};
    std::vector<float> decimatedDbScratch_;
    std::vector<std::uint32_t> rowScratch_;

    float minDb_ = -120.0f;
    float maxDb_ = 0.0f;

    // Estado de "hasta donde se ha subido a la GPU" (ver updateRow()).
    bool hasUploadedAny_ = false;
    std::size_t lastUploadedRow_ = 0;
};

} // namespace rfpulse::render
