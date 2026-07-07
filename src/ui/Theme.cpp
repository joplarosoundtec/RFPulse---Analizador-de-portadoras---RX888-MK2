#include "Theme.h"

namespace rfpulse::ui {

void applyDarkInstrumentTheme()
{
    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();

    // Esquinas afiladas, bordes finos, relleno ajustado: aspecto de panel
    // de instrumento, no de aplicacion de escritorio generica.
    style.WindowRounding = 2.0f;
    style.ChildRounding = 2.0f;
    style.FrameRounding = 2.0f;
    style.PopupRounding = 2.0f;
    style.ScrollbarRounding = 2.0f;
    style.GrabRounding = 2.0f;
    style.TabRounding = 2.0f;
    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.WindowPadding = ImVec2(8.0f, 8.0f);
    style.FramePadding = ImVec2(6.0f, 4.0f);
    style.ItemSpacing = ImVec2(6.0f, 5.0f);
    style.IndentSpacing = 14.0f;
    style.ScrollbarSize = 12.0f;
    style.GrabMinSize = 10.0f;

    ImVec4* colors = style.Colors;

    const ImVec4 bgBlack(0.035f, 0.038f, 0.043f, 1.0f);
    const ImVec4 bgPanel(0.07f, 0.075f, 0.082f, 1.0f);
    const ImVec4 bgWidget(0.10f, 0.108f, 0.118f, 1.0f);
    const ImVec4 bgWidgetHover(0.14f, 0.15f, 0.165f, 1.0f);
    const ImVec4 border(0.22f, 0.23f, 0.25f, 0.6f);
    const ImVec4 text(0.90f, 0.91f, 0.93f, 1.0f);
    const ImVec4 textDisabled(0.45f, 0.47f, 0.50f, 1.0f);

    // Acento ambar/naranja para los controles de la interfaz (checkboxes,
    // sliders, tabs activos...) -- deliberadamente distinto de los colores
    // de traza (ver TraceColors), para no confundir "cromo de interfaz" con
    // "datos medidos".
    const ImVec4 accent(0.92f, 0.62f, 0.13f, 1.0f);
    const ImVec4 accentHover(1.0f, 0.71f, 0.20f, 1.0f);
    const ImVec4 accentActive(0.78f, 0.52f, 0.10f, 1.0f);

    colors[ImGuiCol_Text] = text;
    colors[ImGuiCol_TextDisabled] = textDisabled;
    colors[ImGuiCol_WindowBg] = bgBlack;
    colors[ImGuiCol_ChildBg] = bgPanel;
    colors[ImGuiCol_PopupBg] = bgPanel;
    colors[ImGuiCol_Border] = border;
    colors[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);

    colors[ImGuiCol_FrameBg] = bgWidget;
    colors[ImGuiCol_FrameBgHovered] = bgWidgetHover;
    colors[ImGuiCol_FrameBgActive] = bgWidgetHover;

    colors[ImGuiCol_TitleBg] = bgBlack;
    colors[ImGuiCol_TitleBgActive] = bgBlack;
    colors[ImGuiCol_TitleBgCollapsed] = bgBlack;
    colors[ImGuiCol_MenuBarBg] = bgPanel;

    colors[ImGuiCol_ScrollbarBg] = bgBlack;
    colors[ImGuiCol_ScrollbarGrab] = bgWidget;
    colors[ImGuiCol_ScrollbarGrabHovered] = bgWidgetHover;
    colors[ImGuiCol_ScrollbarGrabActive] = accent;

    colors[ImGuiCol_CheckMark] = accent;
    colors[ImGuiCol_SliderGrab] = accent;
    colors[ImGuiCol_SliderGrabActive] = accentActive;

    colors[ImGuiCol_Button] = bgWidget;
    colors[ImGuiCol_ButtonHovered] = accentHover;
    colors[ImGuiCol_ButtonActive] = accentActive;

    colors[ImGuiCol_Header] = bgWidgetHover;
    colors[ImGuiCol_HeaderHovered] = accentHover;
    colors[ImGuiCol_HeaderActive] = accentActive;

    colors[ImGuiCol_Separator] = border;
    colors[ImGuiCol_SeparatorHovered] = accent;
    colors[ImGuiCol_SeparatorActive] = accentActive;

    colors[ImGuiCol_ResizeGrip] = bgWidgetHover;
    colors[ImGuiCol_ResizeGripHovered] = accentHover;
    colors[ImGuiCol_ResizeGripActive] = accentActive;

    colors[ImGuiCol_Tab] = bgPanel;
    colors[ImGuiCol_TabHovered] = accentHover;
    colors[ImGuiCol_TabActive] = bgWidgetHover;
    colors[ImGuiCol_TabUnfocused] = bgPanel;
    colors[ImGuiCol_TabUnfocusedActive] = bgWidget;

    colors[ImGuiCol_PlotLines] = accent;
    colors[ImGuiCol_PlotLinesHovered] = accentHover;
    colors[ImGuiCol_PlotHistogram] = accent;
    colors[ImGuiCol_PlotHistogramHovered] = accentHover;
}

} // namespace rfpulse::ui
