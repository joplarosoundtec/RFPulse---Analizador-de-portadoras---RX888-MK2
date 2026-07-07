#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <utility>

namespace rfpulse::core {

enum class LogLevel {
    Debug,
    Info,
    Warning,
    Error,
};

// Logger asincrono: log() encola el mensaje y devuelve de inmediato (un
// lock breve sobre una cola en memoria, nunca toca disco); un hilo propio
// drena la cola y escribe al archivo. Pensado para poder llamarse desde
// cualquier hilo sin que una escritura a disco lenta bloquee el pipeline
// -- aunque en la practica el DSP/audio no deberian loguear en su bucle
// caliente, solo en eventos (arranque, cambios de frecuencia, errores).
class Logger {
public:
    explicit Logger(std::string filePath);
    ~Logger();

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void log(LogLevel level, const std::string& message);

    void debug(const std::string& message) { log(LogLevel::Debug, message); }
    void info(const std::string& message) { log(LogLevel::Info, message); }
    void warning(const std::string& message) { log(LogLevel::Warning, message); }
    void error(const std::string& message) { log(LogLevel::Error, message); }

private:
    void writerThreadMain();

    std::string filePath_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<std::pair<LogLevel, std::string>> queue_;
    std::thread writerThread_;
    std::atomic<bool> running_{true};
};

} // namespace rfpulse::core
