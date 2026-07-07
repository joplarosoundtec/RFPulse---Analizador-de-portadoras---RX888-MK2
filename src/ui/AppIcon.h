#pragma once

// Id de recurso compartido entre RfPulse.rc (donde se empotra el .ico en el
// ejecutable) y MainWindow.cpp (donde se carga ese mismo recurso via
// LoadIconW para fijarlo explicitamente como icono de la ventana) -- un
// unico numero para que ambos lados nunca puedan desincronizarse.
#define IDI_APP_ICON 101
