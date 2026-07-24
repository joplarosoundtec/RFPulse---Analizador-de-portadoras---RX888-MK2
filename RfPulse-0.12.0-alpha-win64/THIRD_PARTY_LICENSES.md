# Licencias de terceros

RfPulse enlaza estáticamente o vendoriza el código de terceros listado abajo.
El binario distribuido de RfPulse se publica en su conjunto bajo la GNU
General Public License v3.0 (ver [LICENSE](LICENSE)), porque esa es la
licencia más restrictiva entre sus dependencias (FFTW). El resto de
dependencias usan licencias permisivas (MIT), compatibles con la GPLv3.

## FFTW 3.3.10

- **Uso:** motor de FFT (`fftw3f`, precisión simple), enlazado estáticamente.
- **Licencia:** GNU General Public License v2 "o, a su elección, cualquier
  versión posterior" — ver `https://www.fftw.org/fftw3_doc/License-and-Copyright.html`.
  Esta cláusula "or later version" es la que permite que el trabajo combinado
  se distribuya bajo GPLv3.
- **Copyright:** (c) 2003, 2007-2014 Matteo Frigo; (c) 2003, 2007-2014
  Massachusetts Institute of Technology.
- **Origen:** `https://www.fftw.org/fftw-3.3.10.tar.gz` (ver
  `cmake/FetchFftw.cmake`).

## Dear ImGui v1.91.5

- **Uso:** interfaz gráfica inmediata (paneles, controles, integración DX11/Win32).
- **Licencia:** MIT License.
- **Copyright:** (c) 2014-2024 Omar Cornut.
- **Origen:** `https://github.com/ocornut/imgui` (ver `cmake/FetchImGui.cmake`).

## ImPlot v0.16

- **Uso:** renderizado de las gráficas de espectro (traza actual, promedio,
  max/min hold, marcador de pico).
- **Licencia:** MIT License.
- **Copyright:** (c) 2020-2023 Evan Pezent.
- **Origen:** `https://github.com/epezent/implot` (ver `cmake/FetchImPlot.cmake`).

## SDDC Core (ExtIO_sddc, subdirectorio `Core`)

- **Uso:** control del hardware RX888 MK2 (FX3/CyAPI, DDC en FPGA, generación
  de bloques IQ). Vendorizado sin modificar en
  `third_party/sddc_core/Core` (ver también `Interface.h` y el firmware
  `SDDC_FX3.img`).
- **Licencia:** MIT License — ver `third_party/sddc_core/Core/license.txt`.
- **Copyright:** (c) 2017-2020 Oscar Steila (ik1xpv).
- **Nota:** el wrapper de alto nivel `libsddc.h`/`libsddc.cpp` del proyecto
  original ExtIO_sddc usa un SPDX header GPL-3.0-or-later, pero RfPulse **no**
  usa ese wrapper — se integra directamente contra las clases de `Core`
  (`RadioHandlerClass`, `fft_mt_r2iq`, `FX3Class`), que están bajo la licencia
  MIT reproducida arriba.

## Microsoft WASAPI / DirectX 11 / Windows SDK

- **Uso:** salida de audio (WASAPI shared-mode) y renderizado (D3D11,
  swapchain, texturas).
- Forman parte del Windows SDK; no se redistribuye código fuente de
  Microsoft, solo se enlaza contra las bibliotecas del sistema del usuario
  final (requiere Windows 10/11 con el Visual C++ Redistributable
  correspondiente).
