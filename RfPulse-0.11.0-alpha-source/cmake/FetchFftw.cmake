# Compila FFTW (precision simple, fftw3f) desde el codigo fuente oficial, con
# los codelets AVX/AVX2 habilitados explicitamente.
#
# Decision deliberada: NO usamos el paquete vcpkg ni una DLL precompilada de
# fftw.org. Ambos caminos suelen limitarse a SSE2 (o, en el mejor de los casos,
# a un dispatch en runtime no garantizado) en Windows, lo que anularia buena
# parte de la razon por la que elegimos FFTW frente a KissFFT/PFFFT: rendimiento
# maximo en las FFT grandes (16384/32768) del pipeline de espectro.
#
# Se construye como ExternalProject (no FetchContent_MakeAvailable) porque el
# propio CMakeLists.txt de FFTW no expone nombres de target estables entre
# versiones; en su lugar generamos variables explicitas y predecibles a partir
# de un CMAKE_INSTALL_PREFIX que controlamos nosotros. Esto tiene una ventaja
# adicional: third_party/sddc_core/Core/CMakeLists.txt (vendorizado, sin tocar)
# ya espera exactamente estas tres variables:
#   LIBFFTW_INCLUDE_DIRS / LIBFFTW_LIBRARY_DIRS / LIBFFTW_LIBRARIES
# por lo que no hace falta modificar ni una linea del Core vendorizado.

include(ExternalProject)

set(FFTW_INSTALL_DIR "${CMAKE_BINARY_DIR}/third_party/fftw")

ExternalProject_Add(fftw_build
    # Deliberadamente NO se usa GIT_REPOSITORY: el repositorio git de FFTW no
    # incluye los "codelets" generados (miles de .c generados por su
    # herramienta interna genfft, p.ej. rdft/scalar/r2cf/*.c) — solo trae los
    # Makefile.am que los generarian en un build autotools completo con OCaml
    # instalado, algo que nuestro ExternalProject no ejecuta. El tarball de
    # release oficial de fftw.org si los incluye pre-generados, que es lo que
    # de verdad necesitamos para enlazar (comprobado: sin esto, el linker
    # falla con "unresolved external symbol fftwf_solvtab_rdft_r2cf" y
    # variantes, porque esos .c nunca llegaron a existir).
    URL      https://www.fftw.org/fftw-3.3.10.tar.gz
    URL_HASH SHA256=56c932549852cddcfafdab3820b0200c7742675be92179e59e6215b340e26467
    CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX=${FFTW_INSTALL_DIR}
        -DCMAKE_BUILD_TYPE=Release
        # El CMakeLists.txt de FFTW 3.3.10 declara un cmake_minimum_required
        # muy antiguo; CMake >= 4.0 elimino el soporte de compatibilidad para
        # eso. Este flag le dice a CMake que aplique las politicas de la 3.5
        # sin fallar por la version declarada.
        -DCMAKE_POLICY_VERSION_MINIMUM=3.5
        -DENABLE_FLOAT=ON
        -DENABLE_SSE2=ON
        -DENABLE_AVX=ON
        -DENABLE_AVX2=ON
        -DENABLE_THREADS=ON
        -DBUILD_SHARED_LIBS=OFF
        -DBUILD_TESTS=OFF
        -DDISABLE_FORTRAN=ON
    BUILD_BYPRODUCTS
        "${FFTW_INSTALL_DIR}/lib/fftw3f.lib"
    INSTALL_DIR ${FFTW_INSTALL_DIR}
)

set(LIBFFTW_INCLUDE_DIRS "${FFTW_INSTALL_DIR}/include")
set(LIBFFTW_LIBRARY_DIRS "${FFTW_INSTALL_DIR}/lib")
set(LIBFFTW_LIBRARIES fftw3f)

# CMake exige que INTERFACE_INCLUDE_DIRECTORIES de un target IMPORTED exista ya
# en tiempo de generacion, pero fftw3.h no se instala hasta que fftw_build
# corre en tiempo de build. Se crea el directorio vacio de antemano para que
# la configuracion no falle; el contenido real llega despues, en el build.
file(MAKE_DIRECTORY "${LIBFFTW_INCLUDE_DIRS}")

add_library(FftwFloat::fftw3f STATIC IMPORTED GLOBAL)
set_target_properties(FftwFloat::fftw3f PROPERTIES
    IMPORTED_LOCATION "${FFTW_INSTALL_DIR}/lib/fftw3f.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${LIBFFTW_INCLUDE_DIRS}"
)
add_dependencies(FftwFloat::fftw3f fftw_build)
