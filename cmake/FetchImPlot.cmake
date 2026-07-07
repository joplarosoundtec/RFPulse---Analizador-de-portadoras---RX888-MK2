# ImPlot, igual que ImGui, se vendoriza como fuente via FetchContent. Depende
# unicamente de los headers de imgui (definido en FetchImGui.cmake, que debe
# incluirse antes que este modulo).

include(FetchContent)

FetchContent_Declare(
    implot_src
    GIT_REPOSITORY https://github.com/epezent/implot.git
    GIT_TAG        v0.16
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(implot_src)

add_library(implot STATIC
    ${implot_src_SOURCE_DIR}/implot.cpp
    ${implot_src_SOURCE_DIR}/implot_items.cpp
)

target_include_directories(implot PUBLIC ${implot_src_SOURCE_DIR})
target_link_libraries(implot PUBLIC imgui)
