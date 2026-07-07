# RfPulse 0.10.0-alpha

Décima alpha: identidad visual propia — icono de aplicación y pantalla de
carga con el logo del proyecto.

## Novedades de esta alpha

- **Icono de aplicación**: RfPulse ya no usa el icono genérico de Win32.
  Se generó un `.ico` multi-resolución (16/32/48/64/128/256 px) a partir de
  `icono.png` (no había ningún rasterizador de SVG disponible en la
  máquina de desarrollo para partir directamente de `icono.svg`, así que se
  usó la versión ya rasterizada) y se empotró en el ejecutable vía un
  recurso de Windows (`RfPulse.rc`) — se ve tanto en el propio `.exe` en el
  Explorador como en la barra de título/tareas mientras se ejecuta.
- **Pantalla de carga con logo**: la pantalla de carga inicial (introducida
  en la alpha anterior) ahora muestra el logo del proyecto (`logo.png`,
  cargado como textura D3D11 vía WIC — Windows Imaging Component, sin
  añadir ninguna dependencia nueva) en vez de un título de texto suelto, a
  un tamaño proporcional al de la ventana en vez de un máximo fijo que se
  quedaba pequeño y costaba leer (reportado por el usuario).

## Bugs corregidos en esta alpha

- El logo de la pantalla de carga se dibujaba a tamaño cero (invisible) en
  cuanto se calculaba su tamaño a partir de `ImGui::GetIO().DisplaySize`
  antes de que `ImGui_ImplWin32_NewFrame()` se hubiera llamado ni una vez
  -- ese campo vale `(0,0)` hasta el primer frame, y este es literalmente
  el primer frame de toda la aplicación. Corregido leyendo el rectángulo
  de cliente real de la ventana directamente (`GetClientRect`), que no
  depende del estado de ImGui.

---

# RfPulse 0.9.0-alpha

Novena alpha: corrige la legibilidad de los marcadores de pico, que
rebotaban a la velocidad del ruido de cada trama de FFT.

## Novedades de esta alpha

- **Marcadores de pico estables**: tanto el marcador de pico principal como
  los picos secundarios se buscaban sobre la traza en vivo (`currentDb`),
  una decisión deliberada de versiones anteriores para reaccionar rápido a
  un transmisor que se acaba de encender. En la práctica, esto hacía que la
  frecuencia y el valor dBFS mostrados "rebotaran" con el ruido de cada
  trama de FFT, incluso para una señal real y perfectamente estable —
  reportado por el usuario como difícil de leer. Ahora la búsqueda de picos
  usa `averageDb` (el promedio exponencial, ya calculado y ya visible como
  la traza cian "promedio"): con el promediado activado (por defecto), el
  marcador se queda quieto y legible; si se desactiva el promediado,
  `averageDb` pasa a ser una copia exacta de la traza en vivo en cada trama,
  así que el comportamiento reactivo de siempre sigue disponible sin
  ningún control nuevo que aprender.

## Bugs corregidos en esta alpha

- Los marcadores de frecuencia (pico principal y picos secundarios) del
  espectro rebotaban visiblemente, tanto en la frecuencia mostrada como en
  el valor dBFS, al ritmo del ruido de cada trama de FFT — reportado y
  confirmado por el usuario con hardware real (variación de varios cientos
  de kHz y varios dB entre tramas sucesivas de una misma señal estable).
  Corregido basando la búsqueda de picos en la traza promediada.

---

# RfPulse 0.8.0-alpha

Octava alpha: se centra en la escucha (VFO/audio) — restringe cuándo se
activa el audio y hace ajustable el ancho de canal para adaptarse a
distintos equipos de microfonía y radio.

## Novedades de esta alpha

- **Escucha solo desde frecuencias favoritas**: hasta ahora, un clic
  suelto en el espectro o el waterfall arrancaba directamente el VFO/audio
  en esa frecuencia. Eso hacía fácil enganchar audio sin querer mientras se
  exploraba el espectro. Ahora un clic en el espectro solo recentra la
  vista (con el mismo imán al marcador de pico más cercano de siempre) —
  la única forma de activar la escucha es seleccionar una frecuencia de la
  lista de Frecuencias favoritas.
- **Ancho de banda de canal ajustable**: nuevo control deslizante "Ancho
  canal (kHz)" en el panel del receptor virtual (5-40 kHz), para adaptar el
  filtro de canal a la desviación FM real de lo que se esté escuchando —
  distintas marcas/modelos de microfonía PMSE (Shure, Sennheiser, Wisycom,
  Lectrosonics, etc.) y radios bidireccionales LMR (12.5/25 kHz típico) no
  usan todas el mismo ancho, y antes estaba fijo en 12.5 kHz sin poder
  cambiarlo. Si hay un VFO activo escuchando cuando se cambia, se
  reconstruye en caliente a la misma frecuencia con el nuevo ancho. El
  límite superior (40 kHz) es una limitación real de la arquitectura actual
  del VFO (decima directo a la tasa de audio, ~48 kHz, sin una etapa
  intermedia separada): no alcanza para FM de radiodifusión real
  (~200 kHz), que necesitaría una decimación en dos etapas no implementada
  — fuera del alcance de RfPulse, pensado para PMSE/LMR.

---

# RfPulse 0.7.0-alpha

Séptima alpha: limita el span disponible según el camino de entrada del
RX888 MK2, a partir de una prueba con hardware real que muestra que los
spans anchos en modo VHF/UHF no localizan la señal de forma fiable.

## Novedades de esta alpha

- **Límite de span por modo HF/VHF**: el usuario reportó (probando con un
  generador de señal) que en modo VHF/UHF (tuner R828D) solo el span de
  8 MHz mantiene la señal generada correctamente localizada al cambiar de
  frecuencia central -- con 16 o 32 MHz la fundamental dejaba de encontrarse
  donde debería. En modo HF (muestreo directo, sin el tuner) se mantiene el
  límite en 16 MHz. El desplegable "Span" ahora solo ofrece las opciones
  que caben bajo el límite del modo actual, y cualquier cambio de frecuencia
  que cruce el umbral HF/VHF reajusta automáticamente el span si el
  seleccionado ya no cabe bajo el límite del modo nuevo (al escribir la
  frecuencia a mano, al hacer clic en una frecuencia favorita, al cambiar de
  dispositivo, o al arrancar con un span guardado de una sesión anterior).
  Deliberadamente conservador: RfPulse es una herramienta específica para el
  RX888 MK2, no un SDR de propósito general, así que se prioriza limitar las
  opciones a lo que se ha probado bien frente a dejar elegir un span que se
  sabe que da lecturas incorrectas.

---

# RfPulse 0.6.0-alpha

Sexta alpha: corrige un bug real de arranque en el que la atenuación RF y la
ganancia IF podían quedarse en el valor de fábrica del chip (no en el valor
guardado que mostraba la UI) hasta tocar un control a mano, y ajusta el
recorte del margen del filtro DDC para que el "Span" elegido se parezca
mucho más a la ventana de frecuencias realmente visible.

## Novedades de esta alpha

- **Atenuación RF/ganancia IF correctas desde el primer frame**: al
  arrancar (o cambiar de dispositivo), RfPulse aplica ahora la frecuencia
  central ANTES de aplicar la atenuación/ganancia guardada, en vez de al
  revés. La frecuencia de arranque por defecto (560 MHz) cae en banda
  VHF/UHF, y el primer salto a esa banda hace que el SDK del RX888 MK2
  cambie internamente de modo HF a VHF -- un cambio que aparca los
  registros de ganancia del camino HF a sus propios valores fijos sin
  tocar en absoluto los registros del camino VHF. Aplicar la ganancia antes
  de ese cambio de modo la mandaba al camino equivocado (el que todavía
  estaba activo en ese instante), dejando los registros VHF reales en lo
  que trajera el chip de fábrica -- normalmente el extremo de más
  atenuación/ganancia -- hasta que el usuario tocara un control a mano
  (momento en el que el cambio de modo ya había pasado y el comando por
  fin llegaba al camino correcto).
- **Ventana de span más fiel a lo seleccionado**: el recorte del margen de
  rolloff del filtro de decimación del DDC (introducido en la 0.4.0-alpha)
  pasa de ocultar el 15% del span (7.5% por borde) a solo el 5% (2.5% por
  borde). Seguía habiendo un desajuste confuso entre el "Span" que muestra
  el desplegable y la ventana de frecuencias realmente dibujada en pantalla
  (32 MHz seleccionados mostraban solo 27.2 MHz, sin ningún indicio en la
  UI de por qué) -- ahora la diferencia es mucho más pequeña (32 MHz
  seleccionados muestran ~30.4 MHz), conservando solo la protección frente
  al tramo más empinado de la rampa del filtro cerca del borde.

## Bugs corregidos en esta alpha

- La atenuación RF y la ganancia IF mostraban en la UI el valor guardado de
  la sesión anterior (p.ej. 0.0 dB) pero el hardware seguía realmente en su
  valor de fábrica (típicamente el extremo de más atenuación/ganancia)
  hasta que el usuario movía un control a mano una vez arrancada la
  aplicación -- reportado y diagnosticado con hardware real, causado por el
  orden de aplicación descrito arriba.
- El "Span" elegido en el desplegable no coincidía con la ventana de
  frecuencias mostrada en el espectro/waterfall (recortada un 15% de más
  desde la 0.4.0-alpha) -- reportado por el usuario, corregido reduciendo
  el recorte al 5%.

---

# RfPulse 0.5.0-alpha

Quinta alpha: corrige un bug real de reintonización descubierto probando con
un generador de señal — escribir a mano una frecuencia con decimales podía
dejar el espectro mostrando la señal real desplazada varios MHz de donde
estaba de verdad.

## Novedades de esta alpha

- **Reintonización estable al escribir la frecuencia a mano**: el campo de
  MHz de la barra superior mandaba un comando de sintonía al hardware en
  CADA pulsación que cambiara el valor (no solo al terminar de escribir) —
  escribir, por ejemplo, "241.3" caracter a caracter enviaba una ráfaga de
  reintonizaciones (241 → 241. → 241.3), cada una interrumpiendo el
  asentamiento del PLL del tuner R828D antes de que la anterior terminara,
  dejando el espectro etiquetado con la frecuencia pedida mientras el
  hardware aún no había asentado en ella. Ahora solo se manda un comando de
  sintonía al terminar de editar el campo (`Enter` o perder el foco), igual
  que un clic en un marcador de frecuencia favorita (que ya funcionaba bien
  al ser un único salto atómico).

## Bugs corregidos en esta alpha

- Escribir una frecuencia con decimales a mano (p.ej. 241.3 MHz) en el campo
  de MHz podía mostrar la señal real desplazada varios MHz de su posición
  verdadera, con el mismo nivel de señal pero mal etiquetada en el eje de
  frecuencias — reportado y reproducido por el usuario comparando una
  frecuencia "en redondo" (241.0, que mostraba bien) con la misma frecuencia
  con un decimal añadido (241.3, que no). Causado por la ráfaga de
  reintonizaciones descrita arriba, no por ningún artefacto de RF.
- `applyCenterFrequency()` no reiniciaba max-hold/min-hold al cambiar de
  frecuencia central (`applySpanHz()` sí lo hacía al cambiar el span): tras
  cualquier retonización, esas trazas podían seguir mostrando durante un
  rato el hold del espectro anterior mal proyectado sobre el nuevo eje de
  frecuencias.

---

# RfPulse 0.4.0-alpha

Cuarta alpha: dedicada por completo a la fiabilidad de la lectura de
frecuencias del RX888 MK2 — elimina dos artefactos generados por el propio
hardware (no señales reales) y evita que la zona de cada borde del span que
el filtro de decimación del dispositivo no garantiza plana se confunda con
una señal.

## Novedades de esta alpha

- **Offset tuning automático anti-espurea R828D**: el tuner VHF/UHF del
  RX888 MK2 (Rafael Micro R828D) deriva su PLL de una referencia de
  exactamente 16 MHz, y como toda la familia de tuners R820T/R820T2/R828D es
  conocido que esa misma referencia se filtra al camino de RF, generando una
  falsa señal ("espurea"/birdie) en cada múltiplo exacto de 16 MHz — no algo
  presente en el aire. Cuando la frecuencia central pedida cae a menos de
  250 kHz de uno de esos múltiplos, RfPulse desplaza automáticamente el LO
  real 300 kHz (la misma técnica de "offset tuning" que usan SDR#/GQRX para
  este mismo problema con tuners R820T) mientras mantiene el centro mostrado
  donde el usuario lo pidió; un aviso junto al campo de MHz indica cuándo se
  ha aplicado el ajuste.
- **Filtrado de la misma espurea en todo el span visible**: el ajuste
  anterior solo protegía la frecuencia central exacta. Ahora el
  procesado de espectro interpola (en potencia lineal, no en dB) sobre los
  bins más cercanos a cualquier múltiplo de 16 MHz dentro del span visible,
  antes de repartir a la traza actual/promedio/max-hold/min-hold, para que
  esta espurea nunca aparezca en pantalla independientemente de a qué
  frecuencia esté sintonizado el centro.
- **Recorte del margen de rolloff del filtro de decimación del DDC**: el
  propio SDK del RX888 MK2 documenta que su filtro FIR de decimación solo
  garantiza banda de paso plana en el 85% central de cualquier span
  capturado (diseño Kaiser con `relPass=0.85`/`relStop=1.1`) — el 7.5%
  exterior de cada borde es zona de transición real del filtro, no ruido ni
  señal fiable. Con spans anchos (16-32 MHz) esa franja es estrecha y pasa
  desapercibida; con spans estrechos (2 MHz) puede verse como una rampa que
  termina en una meseta elevada, fácil de confundir con el borde de una
  señal real. RfPulse ahora recorta esa franja tanto en la traza del
  espectro como en la búsqueda de picos (el marcador automático ya no puede
  caer ahí) y en el waterfall (recortado en la misma proporción para seguir
  alineado en frecuencia con el espectro).

## Bugs corregidos en esta alpha

- Al sintonizar 240 MHz aparecía una señal que no existía en el aire, que
  desaparecía al sintonizar 241 MHz — 240 MHz es exactamente 16×15 MHz, un
  múltiplo del reloj de referencia del tuner R828D (ver "Offset tuning
  automático anti-espurea R828D" arriba). Reportado por el usuario y
  reproducido de forma consistente antes de la corrección.
- Una señal ancha con forma de "meseta" cerca de 243.3 MHz (comparada contra
  un analizador de referencia RF Explorer) mostraba un borde con una rampa
  suave que no correspondía a ninguna característica real de la señal — era
  el filtro de decimación del DDC, no la señal, ya que solo aparecía con
  spans estrechos y desaparecía con spans anchos (ver "Recorte del margen de
  rolloff" arriba).

---

# RfPulse 0.3.0-alpha

Tercera alpha: corrige un desajuste real (no solo cosmético) entre lo que el
RX888 MK2 captura de verdad y lo que el espectro decía estar mostrando, y
expone el span como un ajuste explícito de la UI en vez de un valor fijo.

## Novedades de esta alpha

- **Span de captura ajustable y corregido**: el RX888 MK2 (como toda la
  familia SDDC) solo puede capturar en unas pocas tasas discretas fijas —
  2/4/8/16/32 MHz, derivadas de su reloj de ADC de 64 Msps — nunca un valor
  arbitrario. Versiones anteriores pedían 16 MHz de captura al hardware
  pero etiquetaban el eje de frecuencias como si fueran 10 MHz (un valor
  que ni siquiera es alcanzable en este hardware), así que toda lectura de
  frecuencia salvo el centro exacto quedaba desplazada, hasta 3 MHz de
  error en los bordes del span. Ahora hay un único valor de span, tomado
  siempre de las opciones reales que reporta el propio dispositivo
  (`ISdrDevice::availableSampleRates`), con un selector "Span" en la
  barra superior para cambiarlo en caliente. Se persiste entre sesiones.

## Bugs corregidos en esta alpha

- El eje de frecuencias del espectro asumía un span fijo de 10 MHz
  (`spanHz_`) totalmente desconectado de la tasa de captura real pedida al
  dispositivo (16 MHz, hardcodeada aparte). Con FFT de 16384 puntos esto
  significaba etiquetar cada bin con ~0.61 kHz de ancho cuando el ancho
  real era ~0.98 kHz — cualquier señal fuera del centro exacto se mostraba
  en la frecuencia equivocada. Corregido unificando ambos valores en uno
  solo, siempre tomado de una tasa realmente alcanzable por el hardware
  conectado.

---

# RfPulse 0.2.0-alpha

Segunda alpha: se centra en la usabilidad de la sintonización manual (clic
sobre picos, frecuencias favoritas) y en corregir varios problemas del
espectro/waterfall detectados usando la 0.1.0-alpha con hardware real.

## Novedades de esta alpha

- **Eje de frecuencia en MHz**: el espectro (y ahora también el waterfall)
  muestra la frecuencia en MHz con decimales ("555.0", "560.5"...) en vez de
  notación científica en Hz ("5.55e+08"), mucho más legible de un vistazo.
- **Detección y anotación de varios picos a la vez**: además del pico más
  fuerte (marcador y valor de siempre), ahora se marcan y anotan (MHz/dBFS)
  el resto de señales por encima de un umbral ajustable ("Umbral picos"),
  para poder leer el nivel de varias señales visibles sin pasar el ratón por
  cada una.
- **Clic sobre un marcador de pico para sintonizar**: un clic cerca (no
  necesariamente exacto) de un marcador de pico ajusta la sintonía a la
  frecuencia EXACTA de ese pico, no a donde cayó el ratón — útil para
  señales estrechas difíciles de apuntar con precisión. Un clic en cualquier
  otro punto del espectro sigue sintonizando esa frecuencia, como antes.
- **Frecuencias favoritas**: lista de marcadores personalizados en el panel
  lateral. Cada uno se guarda con la frecuencia (en MHz) que el usuario
  escribe explícitamente — nunca la frecuencia central actual salvo que el
  usuario la deje tal cual — y una etiqueta opcional. Un clic en un
  marcador guardado mueve la frecuencia central y sintoniza el VFO ahí
  directamente. Se persisten entre sesiones.
- **Tamaño del waterfall ajustable**: nuevo control deslizante ("Alto
  waterfall") para repartir la altura entre espectro y waterfall a gusto,
  en vez del reparto fijo a partes iguales de la 0.1.0-alpha.
- **Alineación exacta entre espectro y waterfall**: el waterfall se dibuja
  ahora en la misma columna de píxeles que el área de datos real del
  espectro (antes quedaba desplazado a la izquierda por el hueco que ImPlot
  reserva para las etiquetas del eje Y).
- Se retira el escaneo automático ("autoscan") de la 0.1.0-alpha: la
  detección de picos que usaba internamente se mantiene y ahora alimenta las
  anotaciones múltiples y el clic-sobre-marcador descritos arriba, con
  control manual en todo momento.

## Bugs corregidos en esta alpha

- El waterfall repetía visiblemente la fila más reciente en la parte que
  debía mostrar el historial más antiguo, con un tamaño que crecía y
  encogía cada ~1 segundo: el sampler que usa ImGui es CLAMP, no WRAP como
  se había asumido, así que el "envoltorio" del historial circular se
  clampeaba al borde en vez de mostrar datos reales. Corregido dibujando el
  historial con dos imágenes contiguas, cada una con un rango de
  coordenadas que nunca sale de [0,1] (detalle técnico completo en la
  sección de la 0.1.0-alpha más abajo).

## Limitaciones conocidas

- **Audio no verificado end-to-end en el entorno de desarrollo**: la ruta
  WASAPI se probó unitariamente (`test_audio_output`) y se integra
  correctamente en el pipeline, pero el entorno de compilación no dispone
  de un dispositivo de salida de audio real para una verificación
  auditiva completa.
- No se enlaza el CRT de Visual C++ de forma estática: requiere tener
  instalado el Visual C++ Redistributable x64 correspondiente en la
  máquina de destino (ver [README.md](README.md)).
- Sin soporte multi-VFO simultáneo probado bajo carga sostenida más allá de
  sesiones de prueba de 60-90 segundos.
- Sin firma de código (el binario no está firmado); Windows SmartScreen
  puede advertir al ejecutarlo por primera vez.
- Arquitectura preparada pero sin implementar todavía: grabación IQ,
  control remoto, sistema de plugins, soporte de otros SDRs.

## Requisitos de hardware/software

Ver [README.md](README.md).

## Cómo compilar

Ver [README.md](README.md) para las instrucciones completas de compilación
y test.

---

# RfPulse 0.1.0-alpha

Primera versión alpha del proyecto. Implementa el flujo completo descrito en
la arquitectura original: adquisición SDR → FFT → espectro/waterfall → VFO
con demodulación FM → audio, con una interfaz gráfica de estilo instrumento
profesional.

## Novedades de esta alpha

- Pipeline completo de captura, FFT (FFTW/AVX2), espectro (traza actual,
  promedio, max/min hold, suavizado por bins) y waterfall (5 paletas,
  incluida "Thermal").
- Soporte real de hardware RX888 MK2 (enumeración USB, apertura de
  dispositivo, control de ganancia RF/IF), con fallback automático a un
  generador sintético cuando no hay hardware disponible.
- VFOs por click sobre el espectro, con demodulación NFM/WFM, de-énfasis y
  squelch con histéresis, salida por WASAPI.
- Rediseño visual de estilo instrumento de laboratorio (inspirado en, sin
  copiar, Aaronia RTSA Suite PRO / Signal Hound Spike / TinySA Ultra):
  toolbar superior, panel de VFO lateral, status bar, marcador de pico y
  tooltip de cursor en el espectro.
- Selector de dispositivo conectado: combo en el panel lateral que enumera
  todos los RX888 (u otro hardware compatible con el SDK SDDC) detectados
  por USB, con un botón "Refrescar" para volver a escanear y la opción de
  generador sintético (DEMO) siempre disponible. Permite cambiar de
  dispositivo en caliente, sin reiniciar la aplicación.
- Diagnóstico de enlace USB en el selector: si el RX888 negocia USB 2.0
  High-Speed en vez de USB 3.0 SuperSpeed, el combo lo indica explícitamente
  ("USB2.0, sin S/N -- prueba un puerto/cable USB3") en vez de mostrar un
  número de serie ilegible. El firmware del RX888 solo incluye un número de
  serie real (derivado del ID de silicio del FX3) en su descriptor USB 3.0.
- Persistencia de ajustes entre sesiones.

## Limitaciones conocidas

- **Audio no verificado end-to-end en el entorno de desarrollo**: la ruta
  WASAPI se probó unitariamente (`test_audio_output`) y se integra
  correctamente en el pipeline, pero el entorno de compilación no dispone
  de un dispositivo de salida de audio real para una verificación
  auditiva completa. Se recomienda validar la escucha de audio demodulado
  en la primera sesión real con hardware.
- No se enlaza el CRT de Visual C++ de forma estática: requiere tener
  instalado el Visual C++ Redistributable x64 correspondiente en la
  máquina de destino (ver [README.md](README.md)).
- Sin soporte multi-VFO simultáneo probado bajo carga sostenida más allá de
  sesiones de prueba de 60-90 segundos.
- Sin firma de código (el binario no está firmado); Windows SmartScreen
  puede advertir al ejecutarlo por primera vez.
- Arquitectura preparada pero sin implementar todavía: grabación IQ,
  control remoto, sistema de plugins, soporte de otros SDRs.

## Bugs corregidos durante el desarrollo (relevantes para estabilidad)

- Falta de `fx3_->Enumerate()` antes de `Open()` en `SddcDevice`, que podía
  provocar un crash al cerrar la aplicación con hardware conectado.
- Límite de ancho de textura D3D11 (16384 px) superado por tamaños de FFT
  grandes (32768) en el waterfall — corregido con decimación por máximo a
  un ancho máximo de 8192 px.
- Reparto incorrecto de la altura entre espectro y waterfall que dejaba el
  waterfall fuera de la vista visible.
- El historial del waterfall se inicializaba a 0.0f dB, el extremo más
  "caliente" de la paleta por defecto, así que antes de la primera vuelta
  completa del buffer circular se veía una banda de color falsa que
  desaparecía de golpe al llenarse (parecía que el waterfall "se
  reiniciaba"). Corregido: ahora el waterfall crece desde arriba mostrando
  solo las filas con datos reales hasta que el historial se llena.
- El hilo de espectro puede producir filas del waterfall mucho más rápido
  que los frames de render (~488 filas/s con FFT de 32768 puntos a 16 Msps,
  frente a ~60 FPS), así que subir solo la fila más reciente a la textura
  dejaba la mayoría sin actualizar. Además, el "scroll" del waterfall
  asumía que el sampler de ImGui usaba direccionamiento WRAP, cuando en
  realidad usa CLAMP (el único sampler que crea el backend DX11 de ImGui,
  compartido por todas las imágenes) — el trozo de textura que debía
  "envolver" se veía como la fila del borde repetida en vez del historial
  real, con un tamaño que crecía y encogía en cada vuelta del buffer
  circular (~1 segundo). Corregido: `updateRow()` sube todas las filas
  pendientes desde la última subida, y `draw()` reconstruye el bucle
  completo con dos imágenes contiguas, cada una con un rango de coordenadas
  que nunca sale de [0,1].

## Requisitos de hardware/software

Ver [README.md](README.md).

## Cómo compilar

Ver [README.md](README.md) para las instrucciones completas de compilación
y test.
