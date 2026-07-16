# Dear ImGui no distribuye un CMakeLists.txt de biblioteca reutilizable (solo
# ejemplos), asi que se vendoriza la fuente via FetchContent y se define el
# target de compilacion manualmente. Se compilan unicamente los backends que
# usamos (Win32 + DirectX11); se omite deliberadamente imgui_demo.cpp: es
# codigo de ejemplo, no pertenece a un binario comercial.

include(FetchContent)

FetchContent_Declare(
    imgui_src
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.5
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(imgui_src)

add_library(imgui STATIC
    ${imgui_src_SOURCE_DIR}/imgui.cpp
    ${imgui_src_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_src_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_src_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_src_SOURCE_DIR}/backends/imgui_impl_win32.cpp
    ${imgui_src_SOURCE_DIR}/backends/imgui_impl_dx11.cpp
)

target_include_directories(imgui PUBLIC
    ${imgui_src_SOURCE_DIR}
    ${imgui_src_SOURCE_DIR}/backends
)

target_link_libraries(imgui PUBLIC d3d11 dxgi d3dcompiler)
target_compile_definitions(imgui PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
