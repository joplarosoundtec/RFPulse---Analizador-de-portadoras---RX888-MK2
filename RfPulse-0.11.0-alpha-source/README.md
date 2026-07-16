# RfPulse

Analizador de espectro RF en tiempo real para **RX888 MK2**, enfocado
específicamente en la monitorización profesional de sistemas de audio
inalámbrico (IEM y micrófonos inalámbricos Shure, Sennheiser, Wisycom,
Lectrosonics, Audio-Technica, etc.) en el rango típico de trabajo
470–700 MHz.

RfPulse **no** es una herramienta SDR de propósito general, ni un sustituto
de SDR++, ni una herramienta para radioaficionados, ni un software de
coordinación de frecuencias. Es un instrumento de medida enfocado: ver de un
vistazo, en un span de hasta 32 MHz, qué frecuencias están limpias y cuáles
ya están ocupadas antes/durante un show.

## Características (0.11.0-alpha)

- Captura ancho de banda completo desde RX888 MK2 vía el SDK SDDC
  (fallback automático a un generador sintético de demostración si no hay
  hardware conectado — modo "DEMO"), con selector de dispositivo conectado
  (varios RX888, cambio en caliente) y diagnóstico de enlace USB2/USB3.
- Motor FFT en punto flotante simple (FFTW, AVX2) con tamaños seleccionables
  de 1024 a 32768 bins.
- Span de captura ajustable entre las tasas realmente alcanzables por el
  RX888 MK2 (2/4/8/16/32 MHz, según el reloj de su ADC — ver
  `ISdrDevice::availableSampleRates`), con el eje de frecuencias del
  espectro siempre calibrado exactamente a la tasa real capturada. Limitado
  además a lo probado fiable con hardware real por camino de entrada: hasta
  8 MHz en modo VHF/UHF (tuner R828D) y hasta 16 MHz en modo HF (muestreo
  directo) — con spans mayores en VHF la señal podía dejar de localizarse
  correctamente al cambiar de frecuencia.
- Espectro en tiempo real con traza actual, promedio configurable, max-hold,
  min-hold, marcador de pico automático, detección y anotación de varios
  picos visibles a la vez (MHz/dBFS), y tooltip de cursor. Los marcadores de
  pico se calculan sobre la traza promediada (no la traza en vivo), para que
  la frecuencia y el valor dBFS mostrados no reboten con el ruido de cada
  trama de FFT y se puedan leer de un vistazo. El promedio y los holds se
  reinician automáticamente al cambiar de frecuencia, span o dispositivo,
  para no arrastrar datos de una sintonización anterior.
- Navegación con clic en el espectro o directamente sobre un marcador de
  pico (se ajusta a su frecuencia exacta y recentra la vista), y lista de
  frecuencias favoritas personalizadas para saltar directamente a un canal
  conocido. La escucha (VFO/audio) solo se activa desde una frecuencia
  favorita, nunca con un clic suelto en el espectro — para no enganchar
  audio sin querer mientras se explora.
- Ancho de banda de canal de escucha ajustable (5-40 kHz), para adaptarlo a
  la desviación FM real de la marca/modelo de microfonía PMSE o de la radio
  bidireccional que se esté monitorizando, en vez de un valor fijo.
- Lectura de frecuencias corregida frente a artefactos propios del hardware
  del RX888 MK2 (no señales reales): offset tuning automático cuando la
  frecuencia central cae cerca de un múltiplo de 16 MHz (fuga del reloj de
  referencia del tuner R828D), enmascarado de esa misma espurea en cualquier
  punto del span visible, y recorte del margen exterior de cada borde del
  span donde el filtro de decimación del DDC no garantiza una banda de paso
  plana (ver [RELEASE_NOTES.md](RELEASE_NOTES.md) para el detalle técnico).
- Reintonización estable al escribir a mano en el campo de frecuencia: solo
  se manda un comando de sintonía al terminar de editar (no uno por cada
  pulsación mientras se escribe), evitando dejar el LO del tuner a medio
  asentar y el espectro etiquetado con la frecuencia equivocada.
- Atenuación RF y ganancia IF aplicadas correctamente desde el primer frame
  al arrancar (o al cambiar de dispositivo): antes podían quedarse en su
  valor de fábrica del chip hasta tocar un control a mano, aunque la UI ya
  mostrara el valor guardado.
- Suavizado configurable del espectro (box-car en potencia lineal, ajustable
  en número de bins).
- Waterfall con historial en GPU (textura D3D11), alineado en frecuencia con
  el espectro, tamaño ajustable, y 5 paletas de color incluyendo una paleta
  "Thermal" de estilo instrumento de laboratorio.
- Creación de receptores virtuales (VFO), con demodulación NFM/WFM,
  de-énfasis y squelch con histéresis.
- Salida de audio vía WASAPI (modo compartido).
- Persistencia de ajustes entre sesiones (archivo de configuración plano,
  sin dependencias externas).
- Tema oscuro de instrumento inspirado en analizadores de espectro
  profesionales (Signal Hound Spike, TinySA Ultra, Aaronia RTSA Suite PRO,
  SDR Console) — sin copiar literalmente ninguno.
- Icono propio de aplicación (barra de título, barra de tareas y el propio
  .exe en el Explorador) y pantalla de carga inicial con el logo del
  proyecto, mientras se abre el dispositivo SDR.

## Requisitos

- Windows 10/11 de 64 bits.
- [Visual C++ Redistributable x64](https://aka.ms/vs/17/release/vc_redist.x64.exe)
  más reciente instalado (RfPulse no enlaza el CRT estáticamente en esta
  alpha).
- GPU con soporte Direct3D 11 (hay fallback a WARP/software si no hay GPU
  compatible, útil para pruebas pero no para uso en directo).
- Opcional: un receptor **RX888 MK2** conectado por USB 3.0 y su driver
  (Cypress/CyUSB) instalado. Sin hardware, RfPulse arranca igualmente en
  modo DEMO con una señal sintética.
- Opcional (para escuchar el audio demodulado): un dispositivo de salida de
  audio WASAPI válido.

## Compilar desde el código fuente

Requiere Visual Studio 2022 o superior (probado con Visual Studio 18 2026
Insiders) con la carga de trabajo "Desarrollo para el escritorio con C++",
y CMake 3.24+.

```
cmake -S . -B build -G "Visual Studio 18 2026" -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

La primera compilación tarda varios minutos porque FFTW 3.3.10 se descarga
y se compila desde su fuente oficial (ver `cmake/FetchFftw.cmake`) para
garantizar los codelets AVX2, en vez de usar un paquete precompilado
limitado a SSE2.

El ejecutable resultante queda en `build/bin/Release/RfPulse.exe`.

### Tests

El proyecto incluye 19 suites de tests (ring buffers, FFT, DSP, VFO, audio,
settings, logging, detección de picos, etc.), ejecutables vía CTest:

```
ctest --test-dir build -C Release
```

## Estructura del proyecto

```
src/
  core/        Utilidades base: RingBuffer lock-free, TripleBuffer, Settings, Logger
  sdr/         Abstracción de dispositivo SDR (RX888 MK2 real + generador sintético)
  acquisition/ Reparto de bloques IQ del dispositivo a los distintos consumidores
  dsp/         Primitivas DSP: NCO, filtros FIR, aproximaciones rápidas (SIMD)
  fft/         Ventanas y motor FFT (FFTW)
  spectrum/    Procesado de espectro: promedio, holds, suavizado
  waterfall/   Paletas de color y buffer histórico del waterfall
  demod/       Demodulación FM (NFM/WFM), de-énfasis, squelch
  vfo/         Receptor virtual: NCO + decimación + demodulación + audio
  audio/       Salida WASAPI
  render/      Integración D3D11 + ImGui/ImPlot (espectro, waterfall)
  ui/          Ventana principal, tema visual
  app/         Integración de todos los módulos (Application)
tests/         Suites de test por módulo
third_party/   SDK SDDC vendorizado (Core, sin modificar)
cmake/         Scripts de FetchContent/ExternalProject para dependencias
```

## Licencia

RfPulse se distribuye bajo la **GNU General Public License v3.0** (ver
[LICENSE](LICENSE)), como consecuencia de enlazar estáticamente FFTW (GPL).
Ver [THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md) para el detalle de
todas las licencias de terceros (FFTW, Dear ImGui, ImPlot, SDDC Core).

## Estado del proyecto

Esta es una versión **0.11.0-alpha**: funcional y probada con hardware real
(RX888 MK2) en sesiones de 60-90+ segundos, pero sin el pulido ni las
pruebas extensivas de una release estable. Ver
[RELEASE_NOTES.md](RELEASE_NOTES.md) para el detalle de esta versión y sus
limitaciones conocidas.
