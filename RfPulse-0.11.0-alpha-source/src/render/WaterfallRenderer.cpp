#include "WaterfallRenderer.h"

#include <imgui.h>

#include <algorithm>
#include <stdexcept>

using Microsoft::WRL::ComPtr;
using rfpulse::waterfall::generatePalette;
using rfpulse::waterfall::PaletteType;

namespace rfpulse::render {

WaterfallRenderer::WaterfallRenderer(GraphicsDevice& graphics, std::size_t binCount, std::size_t rowCount)
    : graphics_(graphics)
    , binCount_(binCount)
    , rowCount_(rowCount)
    , textureWidth_(std::min(binCount, kMaxTextureWidth))
    , decimatedDbScratch_(textureWidth_)
    , rowScratch_(textureWidth_)
{
    generatePalette(PaletteType::Viridis, paletteLut_);

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = static_cast<UINT>(textureWidth_);
    desc.Height = static_cast<UINT>(rowCount_);
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    HRESULT hr = graphics_.device()->CreateTexture2D(&desc, nullptr, colorTexture_.GetAddressOf());
    if (FAILED(hr)) {
        throw std::runtime_error("WaterfallRenderer: fallo al crear la textura de color");
    }
    hr = graphics_.device()->CreateShaderResourceView(colorTexture_.Get(), nullptr, colorSrv_.GetAddressOf());
    if (FAILED(hr)) {
        throw std::runtime_error("WaterfallRenderer: fallo al crear el shader resource view");
    }
}

void WaterfallRenderer::setPalette(PaletteType type)
{
    generatePalette(type, paletteLut_);
}

void WaterfallRenderer::setDbRange(float minDb, float maxDb)
{
    minDb_ = minDb;
    maxDb_ = maxDb;
}

void WaterfallRenderer::uploadRow(const rfpulse::waterfall::WaterfallEngine& engine, std::size_t rowIndex)
{
    const float* dbRow = engine.data() + rowIndex * engine.binCount();
    const float range = std::max(maxDb_ - minDb_, 0.0001f);

    // Si el tamaño de FFT supera el ancho maximo de textura soportado por
    // D3D11 (ver kMaxTextureWidth), se decima tomando el MAXIMO de cada
    // bloque de bins -- no el promedio -- para que una espurea estrecha
    // siga siendo visible en el waterfall en vez de diluirse.
    if (binCount_ > textureWidth_) {
        for (std::size_t col = 0; col < textureWidth_; ++col) {
            const std::size_t begin = (binCount_ * col) / textureWidth_;
            const std::size_t end = std::max(begin + 1, (binCount_ * (col + 1)) / textureWidth_);
            float maxV = dbRow[begin];
            for (std::size_t i = begin + 1; i < end; ++i) {
                maxV = std::max(maxV, dbRow[i]);
            }
            decimatedDbScratch_[col] = maxV;
        }
    } else {
        std::copy(dbRow, dbRow + binCount_, decimatedDbScratch_.begin());
    }

    for (std::size_t i = 0; i < textureWidth_; ++i) {
        const float t = std::clamp((decimatedDbScratch_[i] - minDb_) / range, 0.0f, 1.0f);
        const auto& c = paletteLut_[static_cast<std::size_t>(t * 255.0f + 0.5f)];
        rowScratch_[i] = (0xFFu << 24) | (static_cast<std::uint32_t>(c.b) << 16)
            | (static_cast<std::uint32_t>(c.g) << 8) | static_cast<std::uint32_t>(c.r);
    }

    D3D11_BOX box{};
    box.left = 0;
    box.right = static_cast<UINT>(textureWidth_);
    box.top = static_cast<UINT>(rowIndex);
    box.bottom = box.top + 1;
    box.front = 0;
    box.back = 1;

    graphics_.context()->UpdateSubresource(
        colorTexture_.Get(), 0, &box, rowScratch_.data(), static_cast<UINT>(textureWidth_ * sizeof(std::uint32_t)), 0);
}

void WaterfallRenderer::updateRow(const rfpulse::waterfall::WaterfallEngine& engine)
{
    if (engine.binCount() != binCount_) {
        return;
    }

    // El orden importa para la concurrencia: currentRow() sincroniza (via
    // release/acquire) con el ultimo pushRow(), lo que garantiza que
    // filledRowCount() -- leido despues -- tambien ve el valor mas
    // reciente, aunque su propio store en pushRow() sea relaxed (ver
    // WaterfallEngine.h).
    const std::size_t currentRow = engine.currentRow();
    const std::size_t filled = engine.filledRowCount();
    const std::size_t rowCount = engine.rowCount();

    if (filled == 0) {
        return; // el hilo de espectro todavia no ha escrito ninguna fila
    }

    if (!hasUploadedAny_) {
        // Primera subida: el historial de CPU (WaterfallEngine::history_)
        // puede tener ya varias filas escritas -- el hilo de espectro
        // corre a su propio ritmo desde el arranque, no espera al primer
        // frame de render -- que la textura de GPU nunca ha recibido
        // (creada sin pInitialData, contenido inicial indefinido). Se
        // suben todas de una vez: para r=1..filled, r%rowCount cubre
        // exactamente las filas ya escritas (1..filled si aun no ha dado
        // la vuelta, o 1..rowCount-1 y 0 si ya la dio, ver el comentario de
        // pushRow() sobre por que la primera fila escrita es la 1 y no la 0).
        for (std::size_t r = 1; r <= filled; ++r) {
            uploadRow(engine, r % rowCount);
        }
        hasUploadedAny_ = true;
        lastUploadedRow_ = currentRow;
        return;
    }

    if (currentRow == lastUploadedRow_) {
        return; // nada nuevo desde la ultima subida
    }

    // Puede haber mas de una fila nueva desde la ultima subida (ver el
    // comentario de mas arriba sobre la diferencia de ritmo entre el hilo
    // de espectro y los frames de render): se suben todas, en orden, para
    // que ninguna quede sin reflejarse en la textura.
    const std::size_t pending = (currentRow + rowCount - lastUploadedRow_) % rowCount;
    for (std::size_t i = 1; i <= pending; ++i) {
        uploadRow(engine, (lastUploadedRow_ + i) % rowCount);
    }
    lastUploadedRow_ = currentRow;
}

void WaterfallRenderer::draw(const rfpulse::waterfall::WaterfallEngine& engine, float leftScreenX, float widthPx, float heightPx,
    float usableSpanFraction)
{
    // Mismo recorte de borde que SpectrumRenderer::draw aplica a su eje X
    // (ver el comentario de esta funcion en el .h): uMin/uMax delimitan la
    // franja central de la textura que se muestra, dejando fuera la zona de
    // transicion del filtro de decimacion del DDC en ambos bordes.
    const float uMargin = (usableSpanFraction > 0.0f && usableSpanFraction < 1.0f) ? (1.0f - usableSpanFraction) / 2.0f : 0.0f;
    const float uMin = uMargin;
    const float uMax = 1.0f - uMargin;

    // Fija solo la X (absoluta, en pantalla); la Y se deja donde ya estaba
    // el cursor de ImGui (justo debajo del espectro). Hay que repetirlo
    // antes de CADA Image() de esta funcion: tras dibujar un Image, ImGui
    // mueve el cursor a la izquierda del contenido (no a leftScreenX) para
    // la siguiente linea.
    const auto alignCursorX = [leftScreenX]() {
        ImGui::SetCursorScreenPos(ImVec2(leftScreenX, ImGui::GetCursorScreenPos().y));
    };

    // El orden importa para la concurrencia: currentRow() hace un load()
    // con acquire que sincroniza con el release de pushRow(), lo que
    // garantiza que filledRowCount() (leido despues, aunque su propio
    // store en pushRow es relaxed) tambien ve el valor mas reciente -- ver
    // el comentario de filledRowCount_ en WaterfallEngine.h.
    const std::size_t currentRow = engine.currentRow();
    const std::size_t filled = engine.filledRowCount();
    const std::size_t rowCount = engine.rowCount();

    if (filled < rowCount) {
        // El historial aun no ha dado su primera vuelta completa: las filas
        // por encima de currentRow (en indice) todavia valen 0.0f (nunca
        // escritas), lo que el waterfall pintaria como una banda de color
        // caliente falsa si se mostrara junto a datos reales. En vez de
        // eso, solo se dibuja la porcion ya escrita (filas 1..currentRow,
        // contigua, sin necesidad de "envolver"), ocupando la parte
        // superior del widget en proporcion al llenado -- el waterfall
        // "crece" hacia abajo, como en un analizador real, y el resto del
        // widget se deja sin dibujar (fondo del tema).
        if (filled == 0) {
            return;
        }
        const float filledFraction = static_cast<float>(filled) / static_cast<float>(rowCount);
        const float vNewest = static_cast<float>(currentRow) / static_cast<float>(rowCount);
        const float vOldest = 1.0f / static_cast<float>(rowCount);
        alignCursorX();
        ImGui::Image(
            reinterpret_cast<ImTextureID>(colorSrv_.Get()),
            ImVec2(widthPx, heightPx * filledFraction),
            ImVec2(uMin, vNewest),
            ImVec2(uMax, vOldest));
        return;
    }

    // Historial completo: la fila mas reciente (currentRow) debe aparecer
    // arriba y la mas antigua (currentRow+1, la proxima en sobreescribirse)
    // abajo del todo. Como el sampler de ImGui usa CLAMP (no WRAP, ver el
    // comentario de la clase), no se puede pedir esto con una unica imagen
    // y un rango de v que se salga de [0,1] -- se partiria en el borde y
    // el trozo fuera de rango se veria como la fila del borde repetida en
    // vez de la parte antigua real del historial. En su lugar se dibujan
    // DOS imagenes contiguas (sin espacio entre ellas), cada una con un
    // rango de v estrictamente dentro de [0,1]:
    //   1) filas [currentRow .. 0]      -> v de currentRow/rowCount a 0
    //   2) filas [rowCount-1 .. currentRow+1] -> v de (rowCount-1)/rowCount
    //      a (currentRow+1)/rowCount
    // Los tamaños en pixeles de cada trozo son proporcionales al numero de
    // filas que contiene, para que ninguna fila quede mas o menos
    // "estirada" que las demas.
    const float rowOffset = static_cast<float>(currentRow) / static_cast<float>(rowCount);
    const std::size_t topRows = currentRow + 1; // filas currentRow..0
    const float topHeight = heightPx * (static_cast<float>(topRows) / static_cast<float>(rowCount));

    const ImGuiStyle& style = ImGui::GetStyle();
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(style.ItemSpacing.x, 0.0f));

    alignCursorX();
    ImGui::Image(
        reinterpret_cast<ImTextureID>(colorSrv_.Get()),
        ImVec2(widthPx, topHeight),
        ImVec2(uMin, rowOffset),
        ImVec2(uMax, 0.0f));

    if (topRows < rowCount) {
        const float bottomHeight = heightPx - topHeight;
        const float vBottomTop = static_cast<float>(rowCount - 1) / static_cast<float>(rowCount);
        const float vBottomEnd = static_cast<float>(currentRow + 1) / static_cast<float>(rowCount);
        alignCursorX();
        ImGui::Image(
            reinterpret_cast<ImTextureID>(colorSrv_.Get()),
            ImVec2(widthPx, bottomHeight),
            ImVec2(uMin, vBottomTop),
            ImVec2(uMax, vBottomEnd));
    }

    ImGui::PopStyleVar();
}

} // namespace rfpulse::render
