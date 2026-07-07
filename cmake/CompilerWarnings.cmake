# Opciones de compilación comunes para el código propio del proyecto (src/, tests/).
# Deliberadamente NO se aplican a third_party/sddc_core (Core ya trae sus propios
# flags por archivo para las variantes AVX/AVX2/AVX512 de fft_mt_r2iq) ni a las
# dependencias vendorizadas (FFTW/ImGui/ImPlot), que se compilan con su propia
# configuración óptima.

add_library(rfpulse_compiler_options INTERFACE)

target_compile_options(rfpulse_compiler_options INTERFACE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive- /arch:AVX2 /fp:fast /Zc:preprocessor /MP>
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:/O2 /Oi /GL>
)

target_link_options(rfpulse_compiler_options INTERFACE
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:/LTCG>
)

target_compile_definitions(rfpulse_compiler_options INTERFACE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    _CRT_SECURE_NO_WARNINGS
    # Todo el codigo propio usa las APIs Win32 "W" (RegisterClassExW,
    # CreateWindowExW...) explicitamente; definir UNICODE/_UNICODE hace que
    # macros TCHAR-generic como IDC_ARROW resuelvan tambien a su variante
    # ancha, en vez de a la ANSI por defecto.
    UNICODE
    _UNICODE
)

# /arch:AVX2 fija el mínimo de CPU soportado en Intel Haswell (2013+) / AMD
# Excavator (2015+). Es una decisión deliberada: el DSP propio (ventaneo,
# magnitud/dBFS, NCO, FIR) se escribe asumiendo AVX2 disponible en tiempo de
# compilación, sin dispatch en tiempo de ejecución a rutas SSE2 más lentas.
