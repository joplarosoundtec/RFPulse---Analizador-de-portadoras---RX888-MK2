#pragma once

#include <string>
#include <unordered_map>

namespace rfpulse::core {

// Persistencia de preferencias en un archivo de texto plano "clave =
// valor" (una entrada por linea, # para comentarios). Deliberadamente no
// se usa TOML/JSON de terceros: lo que hace falta guardar es un puñado de
// pares clave-valor sin anidamiento (frecuencia, ganancia, tamaño de FFT,
// tema...), y anadir una dependencia nueva solo para eso no se justifica
// (mismo criterio que las paletas por puntos de control en vez de tablas
// exactas de terceros).
class Settings {
public:
    explicit Settings(std::string filePath);

    // Ambos son best-effort: si el archivo no existe (primer arranque) o
    // no se puede escribir, devuelven false pero no lanzan -- la
    // aplicacion debe poder seguir funcionando con los valores por
    // defecto.
    bool load();
    bool save() const;

    double getDouble(const std::string& key, double defaultValue) const;
    int getInt(const std::string& key, int defaultValue) const;
    bool getBool(const std::string& key, bool defaultValue) const;
    std::string getString(const std::string& key, const std::string& defaultValue) const;

    void setDouble(const std::string& key, double value);
    void setInt(const std::string& key, int value);
    void setBool(const std::string& key, bool value);
    void setString(const std::string& key, const std::string& value);

private:
    std::string filePath_;
    std::unordered_map<std::string, std::string> values_;
};

} // namespace rfpulse::core
