# Los modulos (core/, sdr/, acquisition/, dsp/, fft/, spectrum/, waterfall/,
# demod/, vfo/, audio/, recording/, render/, ui/, app/) se van anadiendo a
# APP_SOURCES a medida que se implementan, cada uno en su propia tarea
# aprobada.

set(APP_SOURCES
    main.cpp
    sdr/SddcDevice.cpp
    sdr/SdrDeviceFactory.cpp
    acquisition/IqAcquisition.cpp
    fft/Window.cpp
    fft/FftwEngine.cpp
    spectrum/SpectrumProcessor.cpp
    spectrum/PeakDetector.cpp
    waterfall/Palette.cpp
    waterfall/WaterfallEngine.cpp
    render/GraphicsDevice.cpp
    render/SpectrumRenderer.cpp
    render/WaterfallRenderer.cpp
    render/TextureLoader.cpp
    ui/MainWindow.cpp
    dsp/Nco.cpp
    dsp/FirFilter.cpp
    demod/FmDemodulator.cpp
    demod/Deemphasis.cpp
    demod/Squelch.cpp
    vfo/Vfo.cpp
    audio/AudioOutput.cpp
    core/Logger.cpp
    core/Settings.cpp
    sdr/SyntheticSdrDevice.cpp
    app/Application.cpp
    ui/Theme.cpp
    ui/RfPulse.rc
)

add_executable(RfPulse WIN32 ${APP_SOURCES})

target_include_directories(RfPulse PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(RfPulse PRIVATE
    rfpulse_compiler_options
    imgui
    implot
    FftwFloat::fftw3f
    SDDC_CORE
    d3d11
    dxgi
    d3dcompiler
    ole32
    windowscodecs
)

# logo.png (pantalla de carga, ver render/TextureLoader.h) se carga en
# tiempo de ejecucion desde el directorio de trabajo del ejecutable, no se
# empotra en el binario -- se copia junto a RfPulse.exe en cada build para
# que este disponible sin pasos manuales, igual que fftw_wisdom.dat.
add_custom_command(TARGET RfPulse POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/logo.png"
        "$<TARGET_FILE_DIR:RfPulse>/logo.png"
    COMMENT "Copiando logo.png junto al ejecutable"
)
